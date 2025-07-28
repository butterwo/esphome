#include <RFF60Emulator.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "esphome.h"

class REA131B : public Component {
   public:
    RFF60Emulator *thermoMixer, *thermoMain;
    RFF60Emulator::ThermoReadings _receivedReadings{NAN, NAN, NAN, NAN};

    std::map<const std::string, bool> enableDisableMap{
        {"DISABLED", false},
        {"ENABLED", true}};

    bool _initialized = false;
    // static QueueHandle_t _readingsQueue;

    float get_setup_priority() const override {
        return esphome::setup_priority::AFTER_CONNECTION;
    }

    // this is the background task for communication with REA-131B
    static void rea131bCommsTask(void *pvParameters) {
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
    void setup() override {
        // selectorPosnMap["TIMER"] = RFF60Emulator::SELECTOR_POSN::TIMER;
        // selectorPosnMap["COMFORT"] = RFF60Emulator::SELECTOR_POSN::COMFORT;
        // selectorPosnMap["ECO"] = RFF60Emulator::SELECTOR_POSN::ECO;

        register_service(&REA131B::on_hello_world, "hello_world");

        //_readingsQueue = xQueueCreate(1, sizeof(RFF60Emulator::ThermoReadings));

        RFF60Emulator::setup();
        // address, polling address, regulator polling address
        // polling addresses are 7 bit with MSB = even parity bit
        thermoMixer = RFF60Emulator::addInstance(0x21, 0x21, 0x90);
        // thermoMixer = NULL;
        thermoMain = RFF60Emulator::addInstance(0x23, 0xa3, 0x90);
        // thermoMain = NULL;

        // Create the background communications task, storing the handle.
        // Note that the passed parameter ucParameterToPass
        // must exist for the lifetime of the task, so in this case is declared static.
        // xTaskCreate( vTaskCode, "NAME", STACK_SIZE, &ucParameterToPass, tskIDLE_PRIORITY, &xHandle );

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
    void loop() override {
        RFF60Emulator::readingsQueueReceive(&_receivedReadings);
        // vTaskDelay(10);
    }

    void on_hello_world() {
        ESP_LOGD("custom", "Hello World!");
    }

    void queueSendMixer() {
        if (_initialized) {
            RFF60Emulator::SELECTOR_POSN selectorPosn = RFF60Emulator::selectorPosnMap.find(id(mixer_circuit_selector_posn).state)->second;
            RFF60Emulator::VERBOSE_LOGGING verboseLogging = RFF60Emulator::verboseLoggingMap.find(id(verbose_logging).state)->second;
            bool remoteControl = enableDisableMap.find(id(remote_control).state)->second;
            bool ignoreMeasTemp = !(enableDisableMap.find(id(mixer_circuit_use_room_temp).state)->second);
            RFF60Emulator::ThermoSettings settings{selectorPosn, id(mixer_circuit_temp_offset).state, id(mixer_circuit_meas_temp).state,
                                                   ignoreMeasTemp, verboseLogging, remoteControl};
            ESP_LOGD("custom", "settings:\n  selectorPosition: %d\n  temperatureOffset: %f\n  temperatureMeasurement: %f\n  ignoreMeasTemp: %d\n  verboseLogging: %d",
                     settings.selectorPosition, settings.temperatureOffset, settings.temperatureMeasurement, settings.ignoreMeasTemp, settings.verboseLogging);
            RFF60Emulator::ThermoSettings *pSettings = &settings;
            if (thermoMixer) {
                thermoMixer->settingsQueueSend(pSettings);
            }
        }
    }

    void queueSendMain() {
        if (_initialized) {
            RFF60Emulator::SELECTOR_POSN selectorPosn = RFF60Emulator::selectorPosnMap.find(id(main_circuit_selector_posn).state)->second;
            RFF60Emulator::VERBOSE_LOGGING verboseLogging = RFF60Emulator::verboseLoggingMap.find(id(verbose_logging).state)->second;
            bool remoteControl = enableDisableMap.find(id(remote_control).state)->second;
            bool ignoreMeasTemp = !(enableDisableMap.find(id(main_circuit_use_room_temp).state)->second);
            RFF60Emulator::ThermoSettings settings{selectorPosn, id(main_circuit_temp_offset).state, id(main_circuit_meas_temp).state,
                                                   ignoreMeasTemp, verboseLogging, remoteControl};
            ESP_LOGD("custom", "settings:\n  selectorPosition: %d\n  temperatureOffset: %f\n  temperatureMeasurement: %f\n  ignoreMeasTemp: %d\n  verboseLogging: %d",
                     settings.selectorPosition, settings.temperatureOffset, settings.temperatureMeasurement, settings.ignoreMeasTemp, settings.verboseLogging);
            RFF60Emulator::ThermoSettings *pSettings = &settings;
            if (thermoMain) {
                thermoMain->settingsQueueSend(pSettings);
            }
        }
    }
};
