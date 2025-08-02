#pragma once

// #define INCLUDE_xTaskDelayUntil 1

#include <CRC.h>
#include <CRC16.h>
#include <SoftwareSerial.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include <sys/time.h>

#include <bitset>
#include <exception>
#include <iomanip>
#include <ios>
#include <map>
#include <sstream>

namespace esphome {
namespace rea131b {

using namespace EspSoftwareSerial;

class RFF60Emulator {
   public:
    enum SELECTOR_POSN {
        TIMER = 0,
        COMFORT = 3,
        ECO = 4
    };

    enum VERBOSE_LOGGING {
        VERBOSE_OFF = 0,
        VERBOSE_API = 1,
        VERBOSE_SERIAL = 2,
        VERBOSE_BOTH = 3
    };

    static std::map<const std::string, RFF60Emulator::SELECTOR_POSN> selectorPosnMap;
    static std::map<const std::string, RFF60Emulator::VERBOSE_LOGGING> verboseLoggingMap;

    struct ThermoSettings {
        SELECTOR_POSN selectorPosition;
        float temperatureOffset;
        float temperatureMeasurement;
        bool ignoreMeasTemp;
        VERBOSE_LOGGING verboseLogging;
        bool remoteControl;
    };

    struct ThermoReadings {
        float outsideTemp;
        float hotWaterTemp;
        float mixerTemp;
        float boilerTemp;
    };

    RFF60Emulator(uint8_t, uint8_t, uint8_t);
    static void setup();
    static RFF60Emulator *addInstance(uint8_t, uint8_t, uint8_t);
    static RFF60Emulator *waitUntilPolled();
    int doDataExchangeWithHeader();
    int doDataExchange();
    int poll();

    void setKnobSetting(float);
    void setMeasTemp(float);
    void setSelector(SELECTOR_POSN);
    void setIgnoreMeasTemp(bool);
    float getKnobSetting();
    float getMeasTemp();
    SELECTOR_POSN getSelector();
    bool getIgnoreMeasTemp();
    void settingsQueueSend(ThermoSettings *);
    bool settingsQueueReceive();
    static void readingsQueueSend(ThermoReadings *);
    static bool readingsQueueReceive(ThermoReadings *);

   private:
    static void transmitData(const uint8_t *, const size_t, const Parity = PARITY_SPACE);
    static void transmitByte(const uint8_t, const Parity);
    static size_t receiveData(uint8_t *, const size_t);
    static void printHex(const uint8_t *, size_t, const char *);
    static void insertCRC(uint8_t *, int, int);
    static bool checkCRC(uint8_t *, int, int);
    static bool isMessageValid(uint8_t *, int);
    static bool isReplyValid(uint8_t *, int);

    uint8_t _addr;           // address of thermostat
    uint8_t _addr7e;         // address 7 bits with bit 8 = even parity e.g. addr = 0x23, addr7e = 0xa3
    uint8_t _regulatorAddr;  // address of regulator

    static std::map<uint8_t, RFF60Emulator *> _instances;
    QueueHandle_t _settingsQueue;
    static QueueHandle_t _readingsQueue;
    ThermoSettings _receivedSettings;
    
    uint8_t _skipThermostatsFlag;
    uint8_t _reducedTemp = 0x20;
    uint8_t _comfortTemp = 0x28;
    uint8_t _measTemp = 0x28;
    uint8_t _knobSetting = 0xfe;
    uint8_t _selector = TIMER;
    uint8_t _dipSwitch = 0x81;

    static SoftwareSerial _uart;
    static const int TX_ENABLE_PIN = 22;
    static uint8_t _recvBuf[];

    static CRC16 *_crc;
    static const int LONG_TIMEOUT = 2000;
    static const int POLLING_TIMEOUT = 70;
    static const int READ_TIMEOUT = 10;

    static const int LOGGING_SUBSTITUTE_DELAY_API = 3;
    static const int LOGGING_SUBSTITUTE_DELAY_SERIAL = 1;

    static bool _apiLogging;
    static bool _serialLogging;
    static bool _remoteControl;
};

}  // namespace rea131b
}  // namespace esphome
