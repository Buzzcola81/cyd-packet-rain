#include "RgbLed.h"
#include <math.h>

// PWM frequency and resolution for LEDC (RGB channels)
static const int LEDC_FREQ = 5000;
static const int LEDC_RES = 8; // 8-bit

static inline void writePwmInv(int chan, uint8_t duty)
{
    // duty: 0..255 where 255 is fully ON for our logical duty.
    // LEDs are active-LOW so invert the duty when writing to LEDC.
    uint8_t inv = 255 - duty;
    ledcWrite(chan, inv);
}

static inline uint8_t pctToDuty(uint8_t pct)
{
    if (pct > 100)
        pct = 100;
    return (uint8_t)((uint32_t)pct * 255 / 100);
}

void RgbLed::begin()
{
    // Configure LEDC channels and attach pins
    ledcSetup(_rChan, LEDC_FREQ, LEDC_RES);
    ledcSetup(_gChan, LEDC_FREQ, LEDC_RES);
    ledcSetup(_bChan, LEDC_FREQ, LEDC_RES);
    ledcAttachPin(LED_R_PIN, _rChan);
    ledcAttachPin(LED_G_PIN, _gChan);
    ledcAttachPin(LED_B_PIN, _bChan);

    // Start with LED off
    writePwmInv(_rChan, 0);
    writePwmInv(_gChan, 0);
    writePwmInv(_bChan, 0);

    // Start breathing task
    _taskRunning = true;
    xTaskCreatePinnedToCore(breathingTask, "rgb_breath", 2048, this, 1, &_taskHandle, 1);
}

void RgbLed::stop()
{
    _taskRunning = false;
    if (_taskHandle)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
}

void RgbLed::setMaxBrightnessPct(uint8_t pct)
{
    if (pct > 100)
        pct = 100;
    _maxBrightnessPct = pct;
}

void RgbLed::setBreathPeriodMs(uint16_t periodMs)
{
    _breathPeriodMs = periodMs;
    _enabled = periodMs != 0;
    if (!_enabled)
        writeRgb(false, false, false);
}

void RgbLed::writeRgb(bool r, bool g, bool b)
{
    uint8_t onDuty = _enabled ? pctToDuty(_maxBrightnessPct) : 0;
    writePwmInv(_rChan, r ? onDuty : 0);
    writePwmInv(_gChan, g ? onDuty : 0);
    writePwmInv(_bChan, b ? onDuty : 0);
}

void RgbLed::set(LedState state)
{
    _state = state;
    // OFF is immediate; active colors are animated by the background task.
    switch (state)
    {
    case LED_OFF:
        writeRgb(false, false, false);
        break;
    case LED_RED:
    case LED_GREEN:
    case LED_BLUE:
    case LED_YELLOW:
    default:
        break;
    }
}

void RgbLed::breathingTask(void *pv)
{
    RgbLed *self = static_cast<RgbLed *>(pv);
    const uint32_t period = 2000; // ms per breath cycle
    const uint8_t minDuty = 20;   // don't go fully off
    const uint8_t maxDuty = 255;

    while (self->_taskRunning)
    {
        if (self->_enabled && self->_state != LED_OFF)
        {
            uint32_t now = millis();
            uint16_t period = self->_breathPeriodMs;
            if (period == 0)
            {
                writePwmInv(self->_rChan, 0);
                writePwmInv(self->_gChan, 0);
                writePwmInv(self->_bChan, 0);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                continue;
            }
            float phase = (now % period) / (float)period; // 0..1
            // smooth sine breathing
            const float kTwoPi = 6.2831855f;
            const float kHalfPi = 1.5707963f;
            float v = (sinf(phase * kTwoPi - kHalfPi) + 1.0f) / 2.0f; // 0..1
            uint8_t maxDuty = pctToDuty(self->_maxBrightnessPct);
            uint8_t minDuty = maxDuty / 10;
            uint8_t duty = (uint8_t)(minDuty + v * (maxDuty - minDuty));

            switch (self->_state)
            {
            case LED_RED:
                writePwmInv(self->_rChan, duty);
                writePwmInv(self->_gChan, 0);
                writePwmInv(self->_bChan, 0);
                break;
            case LED_GREEN:
                writePwmInv(self->_rChan, 0);
                writePwmInv(self->_gChan, duty);
                writePwmInv(self->_bChan, 0);
                break;
            case LED_BLUE:
                writePwmInv(self->_rChan, 0);
                writePwmInv(self->_gChan, 0);
                writePwmInv(self->_bChan, duty);
                break;
            case LED_YELLOW:
                writePwmInv(self->_rChan, duty);
                writePwmInv(self->_gChan, duty);
                writePwmInv(self->_bChan, 0);
                break;
            case LED_OFF:
            default:
                writePwmInv(self->_rChan, 0);
                writePwmInv(self->_gChan, 0);
                writePwmInv(self->_bChan, 0);
                break;
            }
            vTaskDelay(30 / portTICK_PERIOD_MS);
        }
        else
        {
            writePwmInv(self->_rChan, 0);
            writePwmInv(self->_gChan, 0);
            writePwmInv(self->_bChan, 0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
    }

    // Ensure LEDs are off when task exits
    writePwmInv(self->_rChan, 0);
    writePwmInv(self->_gChan, 0);
    writePwmInv(self->_bChan, 0);
    vTaskDelete(NULL);
}
