#pragma once

#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "esphome/core/component.h"

#include "RFF60Emulator.h"

namespace esphome {
namespace rea131b {

class REA131B : public Component {
   public:
    RFF60Emulator *thermoMixer, *thermoMain;
    RFF60Emulator::ThermoReadings _receivedReadings{NAN, NAN, NAN, NAN};

    std::string verbose_logging_state_;
    std::string remote_control_state_;
    std::string mixer_circuit_selector_posn_state_;
    std::string mixer_circuit_use_room_temp_state_;
    float mixer_circuit_temp_offset_state_;
    float mixer_circuit_meas_temp_state_;
    std::string main_circuit_selector_posn_state_;
    std::string main_circuit_use_room_temp_state_;
    float main_circuit_temp_offset_state_;
    float main_circuit_meas_temp_state_;

    std::map<const std::string, bool> enableDisableMap{
        {"DISABLED", false},
        {"ENABLED", true}};

    bool _initialized = false;

    float get_setup_priority() const override;
    static void rea131bCommsTask(void *);
    void setup() override; // setup() sets up the thermometer instances and creates the background communications task
    void loop() override; // loop() does nothing except push sensor readings
    void dump_config() override;
    void on_hello_world();
    void queueSendMixer();
    void queueSendMain();
};

}  // namespace rea131b
}  // namespace esphome
