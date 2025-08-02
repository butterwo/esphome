#include "RFF60Emulator.h"

#include "esphome/core/log.h"

namespace esphome {
namespace rea131b {

using namespace EspSoftwareSerial;

SoftwareSerial RFF60Emulator::_uart;
uint8_t RFF60Emulator::_recvBuf[1024];
CRC16 *RFF60Emulator::_crc = new CRC16(0x1021, 0, 0, true, true);  // CRC16-KERMIT algorithm
std::map<uint8_t, RFF60Emulator *> RFF60Emulator::_instances;
std::map<const std::string, RFF60Emulator::SELECTOR_POSN> RFF60Emulator::selectorPosnMap{
    {"TIMER", SELECTOR_POSN::TIMER},
    {"COMFORT", SELECTOR_POSN::COMFORT},
    {"ECO", SELECTOR_POSN::ECO}};
std::map<const std::string, RFF60Emulator::VERBOSE_LOGGING> RFF60Emulator::verboseLoggingMap{
    {"OFF", VERBOSE_LOGGING::VERBOSE_OFF},
    {"API", VERBOSE_LOGGING::VERBOSE_API},
    {"SERIAL", VERBOSE_LOGGING::VERBOSE_SERIAL},
    {"BOTH", VERBOSE_LOGGING::VERBOSE_BOTH}};
bool RFF60Emulator::_apiLogging = false;
bool RFF60Emulator::_serialLogging = false;
bool RFF60Emulator::_remoteControl = false;

QueueHandle_t RFF60Emulator::_readingsQueue;
// std::ostringstream RFF60Emulator::stream("", std::ios_base::app);

RFF60Emulator::RFF60Emulator(uint8_t addr, uint8_t pollingAddress, uint8_t regulatorAddr) {
    _addr = addr;
    _addr7e = pollingAddress;
    _regulatorAddr = regulatorAddr;
}

void RFF60Emulator::setup() {

    _readingsQueue = xQueueCreate(1, sizeof(ThermoReadings));

    pinMode(TX_ENABLE_PIN, OUTPUT);
    digitalWrite(TX_ENABLE_PIN, LOW);
    // set up serial ports:
    _uart.begin(9600, SWSERIAL_8S1, 23, 19, false);  // Rx = 23, Tx = 19
    _uart.setTimeout(POLLING_TIMEOUT);
    // clear the UART buffers
    while (_uart.available() > 0) {
        _uart.read();
    }
}

RFF60Emulator *RFF60Emulator::addInstance(uint8_t addr, uint8_t pollingAddr, uint8_t regulatorAddr) {
    _instances[pollingAddr] = new RFF60Emulator(addr, pollingAddr, regulatorAddr);
    _instances[pollingAddr]->_settingsQueue = xQueueCreate(1, sizeof(ThermoSettings));
    return _instances[pollingAddr];
}

RFF60Emulator *RFF60Emulator::waitUntilPolled() {
    size_t recvLen = 0;
    int prevAddr = 0;
    std::map<uint8_t, RFF60Emulator *>::iterator it;

    ESP_LOGD("custom", "Waiting for polling...");

    static TickType_t taskDelay = 10;

    _uart.setTimeout(READ_TIMEOUT);

    for (;;) {
        for (it = _instances.begin(); it != _instances.end(); it++) {
            RFF60Emulator *thermo = it->second;
            if (thermo->settingsQueueReceive()) {
                ESP_LOGD("custom", "Updating settings...");
                thermo->setSelector(thermo->_receivedSettings.selectorPosition);
                thermo->setKnobSetting(thermo->_receivedSettings.temperatureOffset);
                thermo->setMeasTemp(thermo->_receivedSettings.temperatureMeasurement);
                thermo->setIgnoreMeasTemp(thermo->_receivedSettings.ignoreMeasTemp);
                _remoteControl = thermo->_receivedSettings.remoteControl;
                _apiLogging = (thermo->_receivedSettings.verboseLogging == VERBOSE_LOGGING::VERBOSE_API) ||
                              (thermo->_receivedSettings.verboseLogging == VERBOSE_LOGGING::VERBOSE_BOTH);
                _serialLogging = (thermo->_receivedSettings.verboseLogging == VERBOSE_LOGGING::VERBOSE_SERIAL) ||
                                 (thermo->_receivedSettings.verboseLogging == VERBOSE_LOGGING::VERBOSE_BOTH);
                ESP_LOGD("custom", "Address 0x%02x set members:\n    _selector = 0x%02x\n    _knobSetting = 0x%02x\n    _measTemp = 0x%02x\n    _dipSwitch = 0x%02x\n    _apiLogging = %d\n    _serialLogging = %d\n    _remoteControl = %d",
                         thermo->_addr, thermo->_selector, thermo->_knobSetting, thermo->_measTemp, thermo->_dipSwitch, _apiLogging, _serialLogging, _remoteControl);
            }
        }

        if (!_remoteControl) {
            ESP_LOGD("custom", "Remote control is disabled");
            vTaskDelay(1000);
            return 0;
        }

        recvLen = receiveData(_recvBuf, 1024);
        if (recvLen == 1) {
            it = _instances.find(_recvBuf[0]);
            if (it != _instances.end()) {
                if (_recvBuf[0] == prevAddr) {  // if same address twice
                    break;
                }
                prevAddr = _recvBuf[0];
            }
        }

        vTaskDelay(taskDelay);
    }

    _uart.setTimeout(LONG_TIMEOUT);
    return it->second;
}

int RFF60Emulator::doDataExchangeWithHeader() {
    _uart.setTimeout(LONG_TIMEOUT);
    size_t recvLen = 0;
    // send 2 bytes {06 _addr}
    uint8_t msg_06_addr[]{0x06, 0};
    msg_06_addr[1] = _addr7e;
    transmitData(msg_06_addr, 2);
    // receive 9 bytes, _skipThermostatsFlag = byte 5 & 0x01
    recvLen = receiveData(_recvBuf, 9);
    if (!isMessageValid(_recvBuf, recvLen)) {
        return 1;
    };
    // check message contents before reading flag
    uint8_t replyHeader[]{0x82, 0x10, 0xaa, 0x01, 0x00};
    if (memcmp(_recvBuf, replyHeader, 5)) return 1;
    _skipThermostatsFlag = _recvBuf[5] & 0x01;
    return doDataExchange();
}

int RFF60Emulator::doDataExchange() {
    _uart.setTimeout(LONG_TIMEOUT);
    size_t recvLen = 0;
    // send 1 byte {06}
    uint8_t msg_06[]{0x06};
    transmitData(msg_06, 1);
    vTaskDelay(70);
    // send 1 byte {90}
    uint8_t msg_90[]{0x90};
    transmitData(msg_90, 1, PARITY_MARK);
    // receive 2 bytes {06 90}
    recvLen = receiveData(_recvBuf, 2);
    if (!isReplyValid(_recvBuf, 2)) return 1;
    vTaskDelay(1);
    // send 9 bytes {82 _addr 10 01 02 10 CRC 03}
    uint8_t msg4[]{0x82, 0, 0x10, 0x01, 0x02, 0x10, 0, 0, 0x03};
    msg4[1] = _addr;
    insertCRC(msg4, 1, 5);
    transmitData(msg4, 9);
    // receive 1 byte {06}
    recvLen = receiveData(_recvBuf, 1);
    if (!isReplyValid(_recvBuf, 1)) return 1;
    // receive 24 bytes, _reducedTemp = byte 16, _comfortTemp = byte 20
    recvLen = receiveData(_recvBuf, 24);
    if (!isMessageValid(_recvBuf, recvLen)) {
        return 1;
    };
    uint8_t replyHeader[]{0x82, 0x10, 0x20, 0x10, 0x02};
    if (memcmp(_recvBuf, replyHeader, 5)) return 1;

    ThermoReadings readings;
    readings.outsideTemp = (int8_t)_recvBuf[10] / 2.0f;  // assume this is a signed value as it can be negative
    readings.hotWaterTemp = _recvBuf[11] / 2.0f;
    readings.mixerTemp = _recvBuf[14] / 2.0f;
    readings.boilerTemp = _recvBuf[15] / 2.0f;
    _reducedTemp = _recvBuf[16];
    _comfortTemp = _recvBuf[20];
    readingsQueueSend(&readings);

    vTaskDelay(6);

    // send 1 byte {90}
    transmitData(msg_90, 1, PARITY_MARK);
    // receive 2 bytes {06 90}
    recvLen = receiveData(_recvBuf, 2);
    if (!isReplyValid(_recvBuf, 2)) return 1;
    vTaskDelay(1);
    // send 9 bytes {82 _addr 10 01 06 28 CRC 03}
    uint8_t msg6[]{0x82, 0, 0x10, 0x01, 0x06, 0x28, 0, 0, 0x03};
    msg6[1] = _addr;
    insertCRC(msg6, 1, 5);
    transmitData(msg6, 9);
    // receive 1 byte {06}
    recvLen = receiveData(_recvBuf, 1);
    if (!isReplyValid(_recvBuf, 1)) return 1;
    // receive 48 bytes, save in message buffer
    uint8_t msg7[48];
    recvLen = receiveData(msg7, 48);
    if (!isMessageValid(msg7, recvLen)) {
        return 1;
    };
    uint8_t replyHeader2[]{0x82, 0x10, 0x20, 0x28, 0x06};
    if (memcmp(msg7, replyHeader2, 5)) return 1;
    vTaskDelay(6);
    // send 1 byte {90}
    transmitData(msg_90, 1, PARITY_MARK);
    // receive 2 bytes {06 90}
    recvLen = receiveData(_recvBuf, 2);
    if (!isReplyValid(_recvBuf, 2)) return 1;
    vTaskDelay(1);
    // modify message buffer:
    // [1] = _addr
    // [5] = meas temp
    // [6] = knob setting
    // [10] = selector (00, 03 or 04)
    // [11] = DIP switch setting (80 or 81)
    // [23] [24] [25] (addr 21), [31] [32] [33] (addr 22) or [39] [40] [41] (addr 23) = {_comfortTemp _comfortTemp _reducedTemp}
    // [45] [46] = CRC
    msg7[1] = _addr;
    msg7[5] = _measTemp;
    msg7[6] = _knobSetting;
    msg7[10] = _selector;
    msg7[11] = _dipSwitch;
    int index = 23 + ((_addr & 0x0f) - 1) * 8;
    msg7[index] = _comfortTemp;
    msg7[index + 1] = _comfortTemp;
    msg7[index + 2] = _reducedTemp;
    insertCRC(msg7, 1, 44);
    // send message buffer
    transmitData(msg7, 48);
    // receive 1 byte {06}
    recvLen = receiveData(_recvBuf, 1);
    if (!isReplyValid(_recvBuf, 1)) return 1;
    ESP_LOGD("custom", "Address %02x completed data exchange", _addr);
    // Start polling
    return poll();
}

int RFF60Emulator::poll() {
    size_t recvLen = 0;
    _uart.setTimeout(POLLING_TIMEOUT);

    // send 5 times each: 21, 22, a3, 11, 12, 93, 14, 95, 96, 17, 18, 99, 9a, 1b, 9c, 1d, 1e, 9f, 0x90
    //    (starting from 0xa3 for _addr == 23, or 0x11 if _skipThermostatsFlag == true)
    // listen after each transmission for 2 bytes {06 address}, if received, return

    // static const uint8_t addresses[]{0x21, 0x22, 0xa3, 0x11, 0x12, 0x93, 0x14, 0x95, 0x96, 0x17, 0x18, 0x99, 0x9a, 0x1b, 0x9c, 0x1d, 0x1e, 0x9f, 0x90};
    // removed some unused addresses
    static const uint8_t addresses[]{0x21, 0x22, 0xa3, 0x9c, 0x1d, 0x1e, 0x9f, 0x90};
    static const int addressesLength = 8;

    int startIndex = 0;
    // if (_skipThermostatsFlag) {
    //     startIndex = 3;
    // } else {
    //     startIndex = _addr - 0x21;
    // }
    startIndex = _addr - 0x21;

    ESP_LOGD("custom", "Address %02x polling...", _addr);

    bool replyReceived = false;
    for (int i = startIndex; i < addressesLength; i++) {
        for (int j = 0; j < 5; j++) {
            transmitData(addresses + i, 1, PARITY_MARK);
            // if necessary, simulate the other thermostat replying to polling
            if (j == 1) {
                std::map<uint8_t, RFF60Emulator *>::iterator it = _instances.find(addresses[i]);
                if (it != _instances.end() && it->first != _addr7e) {
                    _skipThermostatsFlag = 0;
                    uint8_t msg[]{0x06, 0};
                    msg[1] = it->first;
                    transmitData(msg, 2);
                    uint8_t msg2[]{0x82, 0, 0xaa, 0x00, 0x00, 0, 0, 0, 0x03};
                    msg2[1] = _addr;
                    msg2[5] = _skipThermostatsFlag;
                    insertCRC(msg2, 1, 5);
                    transmitData(msg2, 9);
                    it->second->_skipThermostatsFlag = _skipThermostatsFlag;
                    ESP_LOGD("custom", "Address %02x finished polling", _addr);
                    return it->second->doDataExchange();
                }
            }
            // if a reply {06 90} was received from the regulator, send 9 bytes {82 _addr aa 01 00 _skipThermostatsFlag CRC 03}
            // check reply from regulator
            uint8_t reply[]{0x06, 0x90};
            recvLen = receiveData(_recvBuf, 2);
            if ((recvLen == 2) && !memcmp(_recvBuf, reply, 2)) {
                // prepare message
                uint8_t message[]{0x82, 0, 0xaa, 0x01, 0x00, 0, 0, 0, 0x03};
                static const int messageLength = 9;
                message[1] = _addr;
                message[5] = _skipThermostatsFlag;
                insertCRC(message, 1, 5);

                transmitData(message, messageLength);
                // receive 1 byte {06}
                recvLen = receiveData(_recvBuf, 1);
                if (!isReplyValid(_recvBuf, 1)) return 1;
                ESP_LOGD("custom", "Address %02x finished polling, got reply from regulator", _addr);
                return 0;
            }
        }
    }
    return 0;
}

void RFF60Emulator::transmitData(const uint8_t *buf, const size_t len, const Parity parity) {
    TickType_t xLastWakeTime;
    const TickType_t waitTicks = 3;
    BaseType_t xWasDelayed;
    digitalWrite(TX_ENABLE_PIN, HIGH);
    xLastWakeTime = xTaskGetTickCount();
    for (int i = 0; i < len; i++) {
        transmitByte(buf[i], parity);
        if (i < len - 1) {
            xWasDelayed = xTaskDelayUntil(&xLastWakeTime, waitTicks);
        }
    }
    digitalWrite(TX_ENABLE_PIN, LOW);
    if (len > 0) {
        printHex(buf, len, "2> ");
    }
}

void RFF60Emulator::transmitByte(const uint8_t byte, const Parity parity) {
    static portMUX_TYPE tx_spinlock = portMUX_INITIALIZER_UNLOCKED;
    taskENTER_CRITICAL(&tx_spinlock);
    _uart.write(byte, parity);
    taskEXIT_CRITICAL(&tx_spinlock);
}

size_t RFF60Emulator::receiveData(uint8_t *buf, size_t len) {
    size_t recvLen = _uart.readBytes(buf, len);
    if (recvLen > 0) {
        printHex(buf, recvLen, "1> ");
    }
    return recvLen;
}

void RFF60Emulator::printHex(const uint8_t *buf, size_t len, const char *prefix = "") {
    std::ostringstream stream("", std::ios_base::app);
    if (len > 0) {
        stream << prefix;
        for (int i = 0; i < len; i++) {
            stream << std::setfill('0') << std::setw(2) << std::hex << (int)buf[i];
            stream << " ";
        }
        if (_apiLogging) {
            ESP_LOGD("custom", stream.str().c_str());
        } else {
            vTaskDelay(LOGGING_SUBSTITUTE_DELAY_API);
        }
        if (_serialLogging) {
            Serial.println(stream.str().c_str());
        } else {
            vTaskDelay(LOGGING_SUBSTITUTE_DELAY_SERIAL);
        }
        // stream.str("");
    }
}

void RFF60Emulator::insertCRC(uint8_t *buffer, int start, int length) {
    _crc->restart();
    _crc->add(buffer + start, length);
    uint16_t crcResult = _crc->calc();
    int crcPosn = start + length;
    buffer[crcPosn] = crcResult & 0x00ff;
    buffer[crcPosn + 1] = (crcResult & 0xff00) >> 8;
}

// returns true if CRC is OK
bool RFF60Emulator::checkCRC(uint8_t *buffer, int start, int length) {
    _crc->restart();
    _crc->add(buffer + start, length);
    uint16_t crcResult = _crc->calc();
    int crcPosn = start + length;
    if (buffer[crcPosn] == (crcResult & 0x00ff) && (buffer[crcPosn + 1] == (crcResult & 0xff00) >> 8)) {
        return true;
    }
    return false;
}

// returns true if message is OK
bool RFF60Emulator::isMessageValid(uint8_t *buffer, int length) {
    if (buffer[0] != 0x82) return false;
    if (buffer[length - 1] != 0x03) return false;
    if (!checkCRC(buffer, 1, length - 4)) return false;
    return true;
}

// returns true if message is OK
bool RFF60Emulator::isReplyValid(uint8_t *buffer, int length) {
    static uint8_t reply[]{0x06, 0x90};
    if (memcmp(buffer, reply, length) == 0) {
        return true;
    }
    return false;
}

void RFF60Emulator::setKnobSetting(float offset) {
    _knobSetting = (uint8_t)(int8_t)(offset * 2);
}

void RFF60Emulator::setMeasTemp(float temp) {
    _measTemp = (uint8_t)(int8_t)(temp * 2 + 0.5);
}

void RFF60Emulator::setSelector(SELECTOR_POSN posn) {
    _selector = (uint8_t)posn;
}

void RFF60Emulator::setIgnoreMeasTemp(bool enable) {
    _dipSwitch = 0x80 + (enable ? 1 : 0);
}

float RFF60Emulator::getKnobSetting() {
    return ((int8_t)_knobSetting) / 2.0;
}

float RFF60Emulator::getMeasTemp() {
    return ((int8_t)_measTemp) / 2.0;
}

RFF60Emulator::SELECTOR_POSN RFF60Emulator::getSelector() {
    return (SELECTOR_POSN)_selector;
}

bool RFF60Emulator::getIgnoreMeasTemp() {
    return (_dipSwitch & 0x01 == 1);
}

void RFF60Emulator::settingsQueueSend(ThermoSettings *pSettings) {
    xQueueSend(_settingsQueue, pSettings, 0);
}

bool RFF60Emulator::settingsQueueReceive() {
    if (xQueueReceive(_settingsQueue, &_receivedSettings, 0) == pdTRUE) {
        ESP_LOGD("custom", "received settings:\n  selectorPosition: %d\n  temperatureOffset: %f\n  temperatureMeasurement: %f\n  verboseLogging: %d",
                 _receivedSettings.selectorPosition, _receivedSettings.temperatureOffset, _receivedSettings.temperatureMeasurement, _receivedSettings.verboseLogging);
        return true;
    }
    return false;
}

void RFF60Emulator::readingsQueueSend(ThermoReadings *pReadings) {
    xQueueSend(_readingsQueue, pReadings, 0);
}

bool RFF60Emulator::readingsQueueReceive(ThermoReadings *pReadings) {
    if (xQueueReceive(_readingsQueue, pReadings, 0) == pdTRUE) {
        ESP_LOGD("custom", "received readings:\n  outsideTemp: %f\n  hotWaterTemp: %f\n  mixerTemp: %f\n  boilerTemp: %f",
                 pReadings->outsideTemp, pReadings->hotWaterTemp, pReadings->mixerTemp, pReadings->boilerTemp);
        return true;
    }
    return false;
}

}  // namespace rea131b
}  // namespace esphome
