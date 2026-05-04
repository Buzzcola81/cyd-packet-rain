#ifndef RGBLED_H
#define RGBLED_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// CYD ESP32-2432S028 RGB LED pins (active LOW)
#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

enum LedState
{
    LED_OFF,
    LED_RED,    // VPN down
    LED_GREEN,  // VPN up
    LED_BLUE,   // Connecting / reconnecting
    LED_YELLOW, // Warning
};

class RgbLed
{
public:
    void begin();
    void set(LedState state);
    LedState current() const { return _state; }
    void stop();
    void setMaxBrightnessPct(uint8_t pct);
    uint8_t getMaxBrightnessPct() const { return _maxBrightnessPct; }
    void setBreathPeriodMs(uint16_t periodMs);
    uint16_t getBreathPeriodMs() const { return _breathPeriodMs; }
    bool isEnabled() const { return _enabled; }

private:
    LedState _state = LED_OFF;
    void writeRgb(bool r, bool g, bool b);

    // PWM channels for ESP32 LEDC
    const int _rChan = 1;
    const int _gChan = 2;
    const int _bChan = 3;

    uint8_t _maxBrightnessPct = 100;
    uint16_t _breathPeriodMs = 2000;
    bool _enabled = true;

    // breathing task handle
    TaskHandle_t _taskHandle = nullptr;
    volatile bool _taskRunning = false;
    static void breathingTask(void *pv);
};

#endif
