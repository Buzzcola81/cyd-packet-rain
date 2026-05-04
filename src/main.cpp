#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <lvgl.h>
#include <Preferences.h>
#include "config.h"
#include "Settings.h"
#include "WebAdmin.h"
#include "OpenWrtClient.h"
#include "UI.h"
#include "RgbLed.h"

// ===== Backlight PWM (LEDC) =====
#define BL_PWM_CHANNEL 0
#define BL_PWM_FREQ 20000
#define BL_PWM_RES 8
static Preferences prefs;
static uint8_t currentBrightnessPct = 100;
static uint8_t currentLedMaxBrightnessPct = 100;
static uint8_t currentLedBreathSpeedPct = 45;
static bool screensaverEnabled = false;
static uint8_t screensaverTarget = UI::SCREENSAVER_MATRIX;
static unsigned long screensaverDelayMs = 30000UL;
static bool screensaverRunning = false;
static RgbLed led;

static uint16_t ledSpeedPctToPeriodMs(uint8_t speedPct)
{
    if (speedPct >= 100)
        return 0;
    if (speedPct < 1)
        speedPct = 1;
    return (uint16_t)(3000 - ((uint32_t)(speedPct - 1) * 2500 / 98));
}

void setBrightnessPct(uint8_t pct)
{
    if (pct > 100)
        pct = 100;
    if (pct < 5)
        pct = 5; // never let the user black out the screen completely
    currentBrightnessPct = pct;
    uint32_t duty = (uint32_t)pct * 255 / 100;
    ledcWrite(BL_PWM_CHANNEL, duty);
    prefs.begin("ui", false);
    prefs.putUChar("bright", pct);
    prefs.end();
}

uint8_t getBrightnessPct() { return currentBrightnessPct; }

void setLedMaxBrightnessPct(uint8_t pct)
{
    if (pct > 100)
        pct = 100;
    currentLedMaxBrightnessPct = pct;
    led.setMaxBrightnessPct(pct);
    prefs.begin("ui", false);
    prefs.putUChar("led_max", pct);
    prefs.end();
}

uint8_t getLedMaxBrightnessPct() { return currentLedMaxBrightnessPct; }

void setLedBreathSpeedPct(uint8_t pct)
{
    if (pct > 100)
        pct = 100;
    currentLedBreathSpeedPct = pct;
    led.setBreathPeriodMs(ledSpeedPctToPeriodMs(pct));
    prefs.begin("ui", false);
    prefs.putUChar("led_spd", pct);
    prefs.end();
}

uint8_t getLedBreathSpeedPct() { return currentLedBreathSpeedPct; }

void setScreensaverEnabled(bool enabled)
{
    screensaverEnabled = enabled;
    prefs.begin("ui", false);
    prefs.putBool("ss_en", enabled);
    prefs.end();
    if (!enabled)
        screensaverRunning = false;
}

bool getScreensaverEnabled() { return screensaverEnabled; }

void setScreensaverTarget(uint8_t target)
{
    if (target > UI::SCREENSAVER_MATRIX)
        target = UI::SCREENSAVER_MATRIX;
    screensaverTarget = target;
    prefs.begin("ui", false);
    prefs.putUChar("ss_tgt", target);
    prefs.end();
}

uint8_t getScreensaverTarget() { return screensaverTarget; }

void setScreensaverDelayMs(unsigned long delayMs)
{
    if (delayMs != 30000UL && delayMs != 60000UL && delayMs != 120000UL && delayMs != 300000UL)
        delayMs = 30000UL;
    screensaverDelayMs = delayMs;
    prefs.begin("ui", false);
    prefs.putULong("ss_delay", screensaverDelayMs);
    prefs.end();
}

unsigned long getScreensaverDelayMs() { return screensaverDelayMs; }

// ===== Display & Touch hardware =====
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

static SPIClass touchSPI = SPIClass(HSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// ===== LVGL display buffers =====
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[SCREEN_WIDTH * 10];
static lv_color_t buf2[SCREEN_WIDTH * 10];

// ===== App state =====
static VpnStatus vpnStatus;
static OpenWrtClient *client = nullptr;
static UI *ui = nullptr;

static unsigned long lastVpnRefresh = 0;
#define VPN_REFRESH_INTERVAL 5000

static void updateLed()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        led.set(LED_BLUE);
        return;
    }
    led.set(vpnStatus.up ? LED_GREEN : LED_RED);
}

// ===== LVGL display flush callback =====
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, false);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// ===== Touch suppression after screen swaps =====
static unsigned long touchSuppressUntil = 0;
void suppressTouchFor(unsigned long ms) { touchSuppressUntil = millis() + ms; }

static void noteUserInteraction()
{
    lv_disp_trig_activity(NULL);
    screensaverRunning = false;
}

// ===== LVGL touch read callback =====
static void my_touch_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    if (millis() < touchSuppressUntil)
    {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    if (ts.tirqTouched() && ts.touched())
    {
        TS_Point p = ts.getPoint();
        int16_t x = map(p.x, 200, 3900, SCREEN_WIDTH, 0);
        int16_t y = map(p.y, 200, 3900, SCREEN_HEIGHT, 0);
        x = constrain(x, 0, SCREEN_WIDTH - 1);
        y = constrain(y, 0, SCREEN_HEIGHT - 1);

        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        noteUserInteraction();
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void connectWiFi()
{
    Serial.println("Connecting to WiFi...");
    led.set(LED_BLUE);
    WiFi.begin(Settings::wifiSsid.c_str(), Settings::wifiPassword.c_str());
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30)
    {
        delay(500);
        attempts++;
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
        Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    else
        Serial.println("\nWiFi failed");
}

// ===== Matrix-style boot splash (~1s) =====
static void bootSplash()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);

    const char *target = "CYD PACKET RAIN";
    const int targetLen = strlen(target);
    const int charW = 12; // text size 2 = 12 px wide
    const int charH = 16; // text size 2 = 16 px tall
    const int titleX = (SCREEN_WIDTH - targetLen * charW) / 2;
    const int titleY = SCREEN_HEIGHT / 2 - charH / 2;

    // Decoding scramble: each character settles after a per-char delay.
    const uint16_t bright = 0xFFE0 & 0; // unused
    const uint16_t green = 0x07E8;      // matrix green
    const uint16_t white = 0xFFFF;

    const int totalFrames = 22; // ~22 * 35ms = ~770ms
    const int settleStart = 6;  // first chars start settling at frame 6

    // Per-character settle frame (left-to-right reveal)
    int settleAt[64];
    for (int i = 0; i < targetLen; ++i)
        settleAt[i] = settleStart + i;

    randomSeed(esp_random());

    for (int frame = 0; frame < totalFrames; ++frame)
    {
        // Background rain streaks (a few random green chars across the screen)
        for (int i = 0; i < 12; ++i)
        {
            int rx = random(0, SCREEN_WIDTH / 6) * 6;
            int ry = random(0, SCREEN_HEIGHT / 8) * 8;
            char rc = 33 + random(0, 90);
            // skip drawing where the title sits to keep it readable
            if (ry + 8 > titleY - 4 && ry < titleY + charH + 4 &&
                rx + 6 > titleX - 6 && rx < titleX + targetLen * charW + 6)
                continue;
            tft.setTextSize(1);
            tft.setTextColor(0x0320, TFT_BLACK);
            tft.drawChar(rc, rx, ry, 1);
        }

        // Title scramble / settle
        tft.setTextSize(2);
        for (int i = 0; i < targetLen; ++i)
        {
            int x = titleX + i * charW;
            // Erase cell
            tft.fillRect(x, titleY, charW, charH, TFT_BLACK);
            char c;
            uint16_t color;
            if (frame >= settleAt[i])
            {
                c = target[i];
                color = (frame == settleAt[i]) ? white : green;
            }
            else
            {
                c = (target[i] == ' ') ? ' ' : (33 + random(0, 90));
                color = green;
            }
            tft.setTextColor(color, TFT_BLACK);
            tft.drawChar(c, x, titleY, 1);
        }

        delay(35);
    }

    // Brief hold on the finished title
    tft.setTextSize(2);
    tft.setTextColor(green, TFT_BLACK);
    for (int i = 0; i < targetLen; ++i)
        tft.drawChar(target[i], titleX + i * charW, titleY, 1);

    delay(1230);
    tft.fillScreen(TFT_BLACK);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("CYD Packet Rain (LVGL) starting...");

    // Load runtime settings from NVS (falls back to config.h defaults)
    Settings::load();

    // Display
    tft.begin();
    tft.setRotation(1);

    // Backlight via LEDC PWM (load saved brightness, default 100%)
    ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_RES);
    ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
    prefs.begin("ui", true);
    uint8_t savedPct = prefs.getUChar("bright", 100);
    prefs.end();
    setBrightnessPct(savedPct);

    // Matrix-style boot splash (~1s) before LVGL takes over
    bootSplash();

    // Touch
    touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSPI);
    ts.setRotation(3);

    // RGB LED
    led.begin();
    prefs.begin("ui", true);
    currentLedMaxBrightnessPct = prefs.getUChar("led_max", 100);
    currentLedBreathSpeedPct = prefs.getUChar("led_spd", 45);
    screensaverEnabled = prefs.getBool("ss_en", false);
    screensaverTarget = prefs.getUChar("ss_tgt", UI::SCREENSAVER_MATRIX);
    if (screensaverTarget > UI::SCREENSAVER_MATRIX)
        screensaverTarget = UI::SCREENSAVER_MATRIX;
    screensaverDelayMs = prefs.getULong("ss_delay", 30000UL);
    if (screensaverDelayMs != 30000UL && screensaverDelayMs != 60000UL &&
        screensaverDelayMs != 120000UL && screensaverDelayMs != 300000UL)
        screensaverDelayMs = 30000UL;
    prefs.end();
    led.setMaxBrightnessPct(currentLedMaxBrightnessPct);
    led.setBreathPeriodMs(ledSpeedPctToPeriodMs(currentLedBreathSpeedPct));
    led.set(LED_BLUE); // Booting / connecting

    // LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SCREEN_WIDTH * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touch_read;
    lv_indev_drv_register(&indev_drv);

    // UI
    client = new OpenWrtClient(Settings::openwrtRpcUrl().c_str(),
                               Settings::openwrtUser.c_str(),
                               Settings::openwrtPass.c_str());
    ui = new UI(client, Settings::vpnInterface.c_str(), &vpnStatus);
    ui->begin();

    // If WiFi is not configured, show the WiFi setup screen and wait
    bool needsWiFiSetup = (Settings::wifiSsid.length() == 0 ||
                           Settings::wifiSsid == "Update Me");
    if (needsWiFiSetup)
    {
        Serial.println("[WIFI] No credentials configured — launching WiFi setup screen");
        ui->showWiFiSetup();
        // Spin in the setup screen until the user connects successfully
        while (!ui->isWiFiSetupDone())
        {
            lv_timer_handler();
            delay(5);
        }
        // Reload settings after user saved new WiFi creds
        Settings::load();
        // Return to the home screen
        lv_scr_load(ui->getHomeScreen());
    }

    // Network
    if (WiFi.status() != WL_CONNECTED)
    {
        connectWiFi();
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        client->login();
        // Configure NTP for the clock screen
        configTime(0, 0, NTP_SERVER);
        setenv("TZ", TZ_POSIX, 1);
        tzset();
    }

    // Start the web admin server (port 80) — starts regardless of WiFi
    // state so it's ready when connection comes up or after reconnect.
    WebAdmin::begin();
    Serial.print("[WEB] Admin UI: http://");
    Serial.println(WiFi.localIP());

    client->fetchVpnStatus(Settings::vpnInterface.c_str(), vpnStatus);
    ui->updateMainScreen();
    updateLed();
    lastVpnRefresh = millis();
}

void loop()
{
    lv_timer_handler();
    WebAdmin::handle();

    if (screensaverEnabled && !ui->isWiFiSetupActive())
    {
        if (lv_disp_get_inactive_time(NULL) >= screensaverDelayMs)
        {
            if (!ui->isScreensaverTargetActive(screensaverTarget))
            {
                ui->showScreensaverTarget(screensaverTarget);
                screensaverRunning = true;
                delay(5);
                return;
            }
        }
        else
            screensaverRunning = false;
    }

    // Skip blocking network calls while passive screens (matrix, clock, system
    // info) are running so the UI stays smooth.
    if (ui->isPassiveScreen())
    {
        delay(5);
        return;
    }

    bool dueForRefresh = (millis() - lastVpnRefresh >= VPN_REFRESH_INTERVAL);
    if (dueForRefresh || ui->needsStatusRefresh())
    {
        ui->clearStatusRefreshFlag();
        client->fetchVpnStatus(Settings::vpnInterface.c_str(), vpnStatus);
        ui->updateMainScreen();
        updateLed();
        lastVpnRefresh = millis();
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        led.set(LED_BLUE);
        connectWiFi();
        if (WiFi.status() == WL_CONNECTED)
        {
            client->login();
            client->fetchVpnStatus(Settings::vpnInterface.c_str(), vpnStatus);
            ui->updateMainScreen();
            updateLed();
        }
    }

    delay(5);
}
