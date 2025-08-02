#include "REA131B.h"

#include "esphome/core/log.h"

namespace esphome {
namespace rea131b {

static const char *TAG = "rea131b.component";

float REA131B::get_setup_priority() const {
    return esphome::setup_priority::AFTER_CONNECTION;
}

// this is the background task for communication with REA-131B
void REA131B::rea131bCommsTask(void *pvParameters) {
    for (;;) {
        RFF60Emulator *firstThermo = RFF60Emulator::waitUntilPolled();
        if (firstThermo) {
            if (firstThermo->doDataExchangeWithHeader() == 0) {
                ESP_LOGD("custom", "Did data exchange");
            }
        }
    }
}

// setup() sets up the thermometer instances and creates the background communications task
void REA131B::setup() {

    RFF60Emulator::setup();
    // address, polling address, regulator polling address
    // polling addresses are 7 bit with MSB = even parity bit
    thermoMixer = RFF60Emulator::addInstance(0x21, 0x21, 0x90);
    thermoMain = RFF60Emulator::addInstance(0x23, 0xa3, 0x90);

    // Create the background communications task, storing the handle.
    // Note that the passed parameter ucParameterToPass
    // must exist for the lifetime of the task, so in this case is declared static.
 
    static uint8_t ucParameterToPass = 0;
    TaskHandle_t xHandle = NULL;

    BaseType_t rc = xTaskCreate(rea131bCommsTask, "REA131B_COMMS", 50000, &ucParameterToPass, configMAX_PRIORITIES - 1, &xHandle);
    configASSERT(xHandle);

    if (rc == pdPASS) ESP_LOGD("custom", "Task REA131B_COMMS successfully created");

    _initialized = true;

    // send values from the ESPhome Components
    queueSendMixer();
    queueSendMain();
}

// loop() does nothing except push sensor readings
void REA131B::loop() {
    RFF60Emulator::readingsQueueReceive(&_receivedReadings);
    // vTaskDelay(10);
}

void REA131B::dump_config() {
      ESP_LOGCONFIG(TAG, "REA131B");
}

void REA131B::on_hello_world() {
    ESP_LOGD("custom", "Hello World!");
}

void REA131B::queueSendMixer() {
    if (_initialized) {
        RFF60Emulator::SELECTOR_POSN selectorPosn = RFF60Emulator::selectorPosnMap.find(mixer_circuit_selector_posn_state_)->second;
        RFF60Emulator::VERBOSE_LOGGING verboseLogging = RFF60Emulator::verboseLoggingMap.find(verbose_logging_state_)->second;
        bool remoteControl = enableDisableMap.find(remote_control_state_)->second;
        bool ignoreMeasTemp = !(enableDisableMap.find(mixer_circuit_use_room_temp_state_)->second);
        RFF60Emulator::ThermoSettings settings{selectorPosn, mixer_circuit_temp_offset_state_, mixer_circuit_meas_temp_state_,
                                                ignoreMeasTemp, verboseLogging, remoteControl};
        ESP_LOGD("custom", "settings:\n  selectorPosition: %d\n  temperatureOffset: %f\n  temperatureMeasurement: %f\n  ignoreMeasTemp: %d\n  verboseLogging: %d",
                    settings.selectorPosition, settings.temperatureOffset, settings.temperatureMeasurement, settings.ignoreMeasTemp, settings.verboseLogging);
        RFF60Emulator::ThermoSettings *pSettings = &settings;
        if (thermoMixer) {
            thermoMixer->settingsQueueSend(pSettings);
        }
    }
}

void REA131B::queueSendMain() {
    if (_initialized) {
        RFF60Emulator::SELECTOR_POSN selectorPosn = RFF60Emulator::selectorPosnMap.find(main_circuit_selector_posn_state_)->second;
        RFF60Emulator::VERBOSE_LOGGING verboseLogging = RFF60Emulator::verboseLoggingMap.find(verbose_logging_state_)->second;
        bool remoteControl = enableDisableMap.find(remote_control_state_)->second;
        bool ignoreMeasTemp = !(enableDisableMap.find(main_circuit_use_room_temp_state_)->second);
        RFF60Emulator::ThermoSettings settings{selectorPosn, main_circuit_temp_offset_state_, main_circuit_meas_temp_state_,
                                                ignoreMeasTemp, verboseLogging, remoteControl};
        ESP_LOGD("custom", "settings:\n  selectorPosition: %d\n  temperatureOffset: %f\n  temperatureMeasurement: %f\n  ignoreMeasTemp: %d\n  verboseLogging: %d",
                    settings.selectorPosition, settings.temperatureOffset, settings.temperatureMeasurement, settings.ignoreMeasTemp, settings.verboseLogging);
        RFF60Emulator::ThermoSettings *pSettings = &settings;
        if (thermoMain) {
            thermoMain->settingsQueueSend(pSettings);
        }
    }
}

} // namespace rea131b
} // namespace esphome
