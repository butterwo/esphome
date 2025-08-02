# rea-131b
Interface with an OE-tronic REA-131B heating boiler regulator over RS485.
Emulates an RFF60 room thermostat to:
- control the temperature setpoint (+/- 6 deg C)
- communicate the value of an external room temperature sensor to be used by the regulator
- commute between Reduced, Comfort and Timer temperature presets

Config:

```
external_components:
  - source:
      type: git
      url: https://github.com/butterwo/esphome.git
    refresh: 0s

rea131b:
  id: my_rea131b_id

select:
  - platform: template
    name: "Mixer circuit selector position"
    id: mixer_circuit_selector_posn
    options:
      - "TIMER"
      - "COMFORT"
      - "ECO"
    initial_option: "TIMER"
    optimistic: true
    restore_value: true
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->mixer_circuit_selector_posn_state_ = id(mixer_circuit_selector_posn).state;
        id(my_rea131b_id)->queueSendMixer();

  - platform: template
    name: "Main circuit selector position"
    options:
     - "TIMER"
     - "COMFORT"
     - "ECO"
    initial_option: "TIMER"
    optimistic: true
    restore_value: true
    id: main_circuit_selector_posn
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->main_circuit_selector_posn_state_ = id(main_circuit_selector_posn).state;
        id(my_rea131b_id)->queueSendMain();

  - platform: template
    name: "Verbose logging"
    options:
     - "OFF"
     - "API"
     - "SERIAL"
     - "BOTH"
    initial_option: "OFF"
    optimistic: true
    restore_value: true
    id: verbose_logging
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->verbose_logging_state_ = id(verbose_logging).state;
        id(my_rea131b_id)->queueSendMixer();
        id(my_rea131b_id)->queueSendMain();

  - platform: template
    name: "Remote control"
    options:
     - "DISABLED"
     - "ENABLED"
    initial_option: "DISABLED"
    optimistic: true
    restore_value: true
    id: remote_control
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->remote_control_state_ = id(remote_control).state;
        id(my_rea131b_id)->queueSendMixer();
        id(my_rea131b_id)->queueSendMain();

  - platform: template
    name: "Mixer circuit use room temperature"
    options:
     - "DISABLED"
     - "ENABLED"
    initial_option: "DISABLED"
    optimistic: true
    restore_value: true
    id: mixer_circuit_use_room_temp
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->mixer_circuit_use_room_temp_state_ = id(mixer_circuit_use_room_temp).state;
        id(my_rea131b_id)->queueSendMixer();

  - platform: template
    name: "Main circuit use room temperature"
    options:
     - "DISABLED"
     - "ENABLED"
    initial_option: "DISABLED"
    optimistic: true
    restore_value: true
    id: main_circuit_use_room_temp
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->main_circuit_use_room_temp_state_ = id(main_circuit_use_room_temp).state;
        id(my_rea131b_id)->queueSendMain();

number:
  - platform: template
    name: "Mixer circuit temperature offset"
    optimistic: true
    restore_value: true
    min_value: -6
    max_value: 6
    step: 0.5
    initial_value: -2
    unit_of_measurement: °C
    id: mixer_circuit_temp_offset
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->mixer_circuit_temp_offset_state_ = id(mixer_circuit_temp_offset).state;
        id(my_rea131b_id)->queueSendMixer();

  - platform: template
    name: "Main circuit temperature offset"
    optimistic: true
    restore_value: true
    min_value: -6
    max_value: 6
    step: 0.5
    initial_value: -2
    unit_of_measurement: °C
    id: main_circuit_temp_offset
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->main_circuit_temp_offset_state_ = id(main_circuit_temp_offset).state;
        id(my_rea131b_id)->queueSendMain();

  - platform: template
    name: "Mixer circuit measured temperature"
    optimistic: true
    restore_value: true
    min_value: 0
    max_value: 30
    step: 0.5
    initial_value: 18
    unit_of_measurement: °C
    id: mixer_circuit_meas_temp
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->mixer_circuit_meas_temp_state_ = id(mixer_circuit_meas_temp).state;
        id(my_rea131b_id)->queueSendMixer();

  - platform: template
    name: "Main circuit measured temperature"
    optimistic: true
    restore_value: true
    min_value: 0
    max_value: 30
    step: 0.5
    initial_value: 18
    unit_of_measurement: °C
    id: main_circuit_meas_temp
    on_value:
      lambda: !lambda |-
        id(my_rea131b_id)->main_circuit_meas_temp_state_ = id(main_circuit_meas_temp).state;
        id(my_rea131b_id)->queueSendMain();

sensor:
  - platform: template
    name: "Outside temperature"
    device_class: TEMPERATURE
    unit_of_measurement: °C
    lambda: |-
      return id(my_rea131b_id)->_receivedReadings.outsideTemp;
    update_interval: 300s

  - platform: template
    name: "Hot water temperature"
    device_class: TEMPERATURE
    unit_of_measurement: °C
    lambda: |-
      return id(my_rea131b_id)->_receivedReadings.hotWaterTemp;
    update_interval: 30s

  - platform: template
    name: "Mixer temperature"
    device_class: TEMPERATURE
    unit_of_measurement: °C
    lambda: |-
      return id(my_rea131b_id)->_receivedReadings.mixerTemp;
    update_interval: 30s

  - platform: template
    name: "Boiler temperature"
    device_class: TEMPERATURE
    unit_of_measurement: °C
    lambda: |-
      return id(my_rea131b_id)->_receivedReadings.boilerTemp;
    update_interval: 30s
```



