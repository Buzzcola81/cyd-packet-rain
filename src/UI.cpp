#include "UI.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Settings.h"
#include "config.h"

extern void suppressTouchFor(unsigned long ms);
extern TFT_eSPI tft;
extern void setBrightnessPct(uint8_t pct);
extern uint8_t getBrightnessPct();
extern void setLedMaxBrightnessPct(uint8_t pct);
extern uint8_t getLedMaxBrightnessPct();
extern void setLedBreathSpeedPct(uint8_t pct);
extern uint8_t getLedBreathSpeedPct();
extern void setScreensaverEnabled(bool enabled);
extern bool getScreensaverEnabled();
extern void setScreensaverTarget(uint8_t target);
extern uint8_t getScreensaverTarget();
extern void setScreensaverDelayMs(unsigned long delayMs);
extern unsigned long getScreensaverDelayMs();
extern void setClockTextSizeIdx(uint8_t idx);
extern uint8_t getClockTextSizeIdx();

UI::UI(OpenWrtClient *client, const char *vpnInterface, VpnStatus *status)
    : _client(client), _vpnInterface(vpnInterface), _status(status) {}

void UI::begin()
{
    buildHomeScreen();
    buildMainScreen();
    buildTestScreen();
    buildMatrixScreen();
    buildClockScreen();
    buildWeatherScreen();
    buildSysInfoScreen();
    buildOptionsScreen();
    lv_scr_load(_scrHome);
}

void UI::requestStatusRefresh() { _needsRefresh = true; }

// ============================================================
// HOME SCREEN (carousel)
// ============================================================

void UI::buildHomeScreen()
{
    _scrHome = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrHome, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(_scrHome, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(_scrHome);
    lv_label_set_text(title, "// CYD PACKET RAIN");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // Left arrow
    lv_obj_t *btnPrev = lv_btn_create(_scrHome);
    lv_obj_set_size(btnPrev, 50, 80);
    lv_obj_align(btnPrev, LV_ALIGN_LEFT_MID, 8, 10);
    lv_obj_set_style_bg_color(btnPrev, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(btnPrev, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(btnPrev, 1, 0);
    lv_obj_set_style_radius(btnPrev, 4, 0);
    lv_obj_add_event_cb(btnPrev, onHomePrevClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblPrev = lv_label_create(btnPrev);
    lv_label_set_text(lblPrev, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lblPrev, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblPrev, &lv_font_montserrat_24, 0);
    lv_obj_center(lblPrev);

    // Right arrow
    lv_obj_t *btnNext = lv_btn_create(_scrHome);
    lv_obj_set_size(btnNext, 50, 80);
    lv_obj_align(btnNext, LV_ALIGN_RIGHT_MID, -8, 10);
    lv_obj_set_style_bg_color(btnNext, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(btnNext, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(btnNext, 1, 0);
    lv_obj_set_style_radius(btnNext, 4, 0);
    lv_obj_add_event_cb(btnNext, onHomeNextClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblNext = lv_label_create(btnNext);
    lv_label_set_text(lblNext, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lblNext, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblNext, &lv_font_montserrat_24, 0);
    lv_obj_center(lblNext);

    // Center select button
    _homeSelectBtn = lv_btn_create(_scrHome);
    lv_obj_set_size(_homeSelectBtn, 170, 100);
    lv_obj_align(_homeSelectBtn, LV_ALIGN_CENTER, 0, 15);
    lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(_homeSelectBtn, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(_homeSelectBtn, 2, 0);
    lv_obj_set_style_radius(_homeSelectBtn, 4, 0);
    lv_obj_add_event_cb(_homeSelectBtn, onHomeSelectClicked, LV_EVENT_CLICKED, this);
    _homeSelectLbl = lv_label_create(_homeSelectBtn);
    lv_obj_set_style_text_font(_homeSelectLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_homeSelectLbl, lv_color_hex(0x80FFA0), 0);
    lv_obj_center(_homeSelectLbl);

    // Caption under the center button
    _homeLabel = lv_label_create(_scrHome);
    lv_obj_set_style_text_color(_homeLabel, lv_color_hex(0x40A050), 0);
    lv_obj_set_style_text_font(_homeLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(_homeLabel, LV_ALIGN_BOTTOM_MID, 0, -8);

    updateHomeSelection();
}

void UI::showScreensaverTarget(uint8_t target)
{
    switch (target)
    {
    case SCREENSAVER_CLOCK:
        lv_async_call(loadClockScreenAsync, this);
        break;
    case SCREENSAVER_WEATHER:
        lv_async_call(loadWeatherScreenAsync, this);
        break;
    case SCREENSAVER_MATRIX:
    default:
        lv_async_call(loadMatrixScreenAsync, this);
        break;
    }
}

bool UI::isScreensaverTargetActive(uint8_t target) const
{
    lv_obj_t *s = lv_scr_act();
    switch (target)
    {
    case SCREENSAVER_CLOCK:
        return s == _scrClock;
    case SCREENSAVER_WEATHER:
        return s == _scrWeather;
    case SCREENSAVER_MATRIX:
    default:
        return s == _scrMatrix;
    }
}

void UI::updateHomeSelection()
{
    switch (_homeIndex)
    {
    case 0:
        lv_label_set_text(_homeSelectLbl, "VPN");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x031608), 0);
        lv_label_set_text(_homeLabel, "1 / 6  -  VPN Control");
        break;
    case 1:
        lv_label_set_text(_homeSelectLbl, "Clock");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x062612), 0);
        lv_label_set_text(_homeLabel, "2 / 6  -  NTP Clock");
        break;
    case 2:
        lv_label_set_text(_homeSelectLbl, "Weather");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x0A3A1A), 0);
        lv_label_set_text(_homeLabel, "3 / 6  -  Warrandyte Weather");
        break;
    case 3:
        lv_label_set_text(_homeSelectLbl, "System");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x0F5024), 0);
        lv_label_set_text(_homeLabel, "4 / 6  -  System Info");
        break;
    case 4:
        lv_label_set_text(_homeSelectLbl, "Matrix");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x14702E), 0);
        lv_label_set_text(_homeLabel, "5 / 6  -  Screensaver");
        break;
    case 5:
        lv_label_set_text(_homeSelectLbl, "Options");
        lv_obj_set_style_bg_color(_homeSelectBtn, lv_color_hex(0x1A8038), 0);
        lv_label_set_text(_homeLabel, "6 / 6  -  Display Options");
        break;
    }
}

void UI::onHomePrevClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    self->_homeIndex = (self->_homeIndex - 1 + HOME_MENU_COUNT) % HOME_MENU_COUNT;
    self->updateHomeSelection();
}

void UI::onHomeNextClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    self->_homeIndex = (self->_homeIndex + 1) % HOME_MENU_COUNT;
    self->updateHomeSelection();
}

void UI::onHomeSelectClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    switch (self->_homeIndex)
    {
    case 0:
        self->requestStatusRefresh();
        lv_async_call(loadMainScreenAsync, self);
        break;
    case 1:
        lv_async_call(loadClockScreenAsync, self);
        break;
    case 2:
        lv_async_call(loadWeatherScreenAsync, self);
        break;
    case 3:
        lv_async_call(loadSysInfoScreenAsync, self);
        break;
    case 4:
        lv_async_call(loadMatrixScreenAsync, self);
        break;
    case 5:
        lv_async_call(loadOptionsScreenAsync, self);
        break;
    }
}

// ============================================================
// MAIN (VPN) SCREEN
// ============================================================

void UI::buildMainScreen()
{
    _scrMain = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrMain, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(_scrMain, LV_OBJ_FLAG_SCROLLABLE);

    // Home button (top left)
    lv_obj_t *btnHome = lv_btn_create(_scrMain);
    lv_obj_set_size(btnHome, 70, 28);
    lv_obj_align(btnHome, LV_ALIGN_TOP_LEFT, 6, 4);
    lv_obj_set_style_bg_color(btnHome, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(btnHome, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(btnHome, 1, 0);
    lv_obj_set_style_radius(btnHome, 4, 0);
    lv_obj_add_event_cb(btnHome, onHomeFromVpnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblHome = lv_label_create(btnHome);
    lv_label_set_text(lblHome, "< Home");
    lv_obj_set_style_text_color(lblHome, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblHome, &lv_font_montserrat_12, 0);
    lv_obj_center(lblHome);

    // Title (centered)
    lv_obj_t *title = lv_label_create(_scrMain);
    lv_label_set_text(title, "// VPN");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Status badge (top right) - framed pill matching theme
    _statusBadge = lv_label_create(_scrMain);
    lv_label_set_text(_statusBadge, "OFF");
    lv_obj_set_style_text_color(_statusBadge, lv_color_hex(0xFF6060), 0);
    lv_obj_set_style_text_font(_statusBadge, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(_statusBadge, lv_color_hex(0x1A0606), 0);
    lv_obj_set_style_bg_opa(_statusBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_statusBadge, lv_color_hex(0xC04040), 0);
    lv_obj_set_style_border_width(_statusBadge, 1, 0);
    lv_obj_set_style_radius(_statusBadge, 4, 0);
    lv_obj_set_style_pad_hor(_statusBadge, 8, 0);
    lv_obj_set_style_pad_ver(_statusBadge, 4, 0);
    lv_obj_align(_statusBadge, LV_ALIGN_TOP_RIGHT, -8, 10);

    // Stats panel (bordered, matches home/options theme)
    lv_obj_t *panel = lv_obj_create(_scrMain);
    lv_obj_set_size(panel, 300, 130);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 4, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);

    // Info rows
    int y = 0;
    auto makeRow = [&](const char *label) -> lv_obj_t *
    {
        lv_obj_t *l = lv_label_create(panel);
        lv_label_set_text(l, label);
        lv_obj_set_style_text_color(l, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, y);
        lv_obj_t *v = lv_label_create(panel);
        lv_obj_set_style_text_color(v, lv_color_hex(0x80FFA0), 0);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
        lv_obj_align(v, LV_ALIGN_TOP_LEFT, 104, y);
        y += 22;
        return v;
    };

    _vpnIp = makeRow("VPN IP:");
    _publicIp = makeRow("Public IP:");
    _location = makeRow("Location:");
    _traffic = makeRow("Traffic:");
    _uptime = makeRow("Uptime:");

    // Toggle button (stacked, above Test Connection)
    _btnToggle = lv_btn_create(_scrMain);
    lv_obj_set_size(_btnToggle, 300, 34);
    lv_obj_align(_btnToggle, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(_btnToggle, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(_btnToggle, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(_btnToggle, 2, 0);
    lv_obj_set_style_radius(_btnToggle, 4, 0);
    lv_obj_add_event_cb(_btnToggle, onToggleClicked, LV_EVENT_CLICKED, this);
    _lblBtnToggle = lv_label_create(_btnToggle);
    lv_label_set_text(_lblBtnToggle, "Enable VPN");
    lv_obj_set_style_text_color(_lblBtnToggle, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(_lblBtnToggle, &lv_font_montserrat_14, 0);
    lv_obj_center(_lblBtnToggle);

    // Test button
    lv_obj_t *btnTest = lv_btn_create(_scrMain);
    lv_obj_set_size(btnTest, 300, 34);
    lv_obj_align(btnTest, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btnTest, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(btnTest, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(btnTest, 1, 0);
    lv_obj_set_style_radius(btnTest, 4, 0);
    lv_obj_add_event_cb(btnTest, onTestClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblTest = lv_label_create(btnTest);
    lv_label_set_text(lblTest, "Test Connection");
    lv_obj_set_style_text_color(lblTest, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblTest, &lv_font_montserrat_14, 0);
    lv_obj_center(lblTest);
}

void UI::updateMainScreen()
{
    if (_status->up)
    {
        lv_label_set_text(_statusBadge, "ON");
        lv_obj_set_style_text_color(_statusBadge, lv_color_hex(0x80FFA0), 0);
        lv_obj_set_style_bg_color(_statusBadge, lv_color_hex(0x041808), 0);
        lv_obj_set_style_border_color(_statusBadge, lv_color_hex(0x00FF66), 0);
        lv_label_set_text(_lblBtnToggle, "Disable VPN");
        lv_obj_set_style_bg_color(_btnToggle, lv_color_hex(0x1A0606), 0);
        lv_obj_set_style_border_color(_btnToggle, lv_color_hex(0xC04040), 0);
        lv_obj_set_style_text_color(_lblBtnToggle, lv_color_hex(0xFF8080), 0);
    }
    else
    {
        lv_label_set_text(_statusBadge, "OFF");
        lv_obj_set_style_text_color(_statusBadge, lv_color_hex(0xFF6060), 0);
        lv_obj_set_style_bg_color(_statusBadge, lv_color_hex(0x1A0606), 0);
        lv_obj_set_style_border_color(_statusBadge, lv_color_hex(0xC04040), 0);
        lv_label_set_text(_lblBtnToggle, "Enable VPN");
        lv_obj_set_style_bg_color(_btnToggle, lv_color_hex(0x041808), 0);
        lv_obj_set_style_border_color(_btnToggle, lv_color_hex(0x00FF66), 0);
        lv_obj_set_style_text_color(_lblBtnToggle, lv_color_hex(0x80FFA0), 0);
    }

    lv_label_set_text(_vpnIp, _status->ipAddr.length() ? _status->ipAddr.c_str() : "-");
    lv_label_set_text(_publicIp, _status->publicIp.length() ? _status->publicIp.c_str() : "-");
    lv_label_set_text(_location, _status->location.length() ? _status->location.c_str() : "-");

    String traffic = formatBytes(_status->rxBytes) + " / " + formatBytes(_status->txBytes);
    lv_label_set_text(_traffic, traffic.c_str());

    lv_label_set_text(_uptime, _status->up ? formatUptime(_status->uptime).c_str() : "-");
}

// ============================================================
// VPN TOGGLE
// ============================================================

void UI::onToggleClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    if (self->_status->up)
        self->_client->vpnDown(self->_vpnInterface);
    else
        self->_client->vpnUp(self->_vpnInterface);
    self->requestStatusRefresh();
}

void UI::onHomeFromVpnClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    lv_async_call(loadHomeScreenAsync, self);
}

// ============================================================
// TEST SCREEN
// ============================================================

void UI::buildTestScreen()
{
    _scrTest = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrTest, lv_color_hex(0x101418), 0);

    lv_obj_t *title = lv_label_create(_scrTest);
    lv_label_set_text(title, "Connection Test");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 6);

    _lblTestResults = lv_label_create(_scrTest);
    lv_obj_set_style_text_color(_lblTestResults, lv_color_hex(0xC0E0FF), 0);
    lv_obj_set_style_text_font(_lblTestResults, &lv_font_montserrat_14, 0);
    lv_obj_set_width(_lblTestResults, 300);
    lv_label_set_long_mode(_lblTestResults, LV_LABEL_LONG_WRAP);
    lv_label_set_text(_lblTestResults, "Running...");
    lv_obj_align(_lblTestResults, LV_ALIGN_TOP_LEFT, 8, 36);

    lv_obj_t *btnBack = lv_btn_create(_scrTest);
    lv_obj_set_size(btnBack, 300, 34);
    lv_obj_align(btnBack, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_event_cb(btnBack, onBackToVpnClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "Back");
    lv_obj_center(lblBack);
}

void UI::onTestClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    self->_testResults = "Running tests...\n";
    lv_label_set_text(self->_lblTestResults, self->_testResults.c_str());
    self->_testStep = TEST_BOARD;
    if (self->_testTimer == nullptr)
        self->_testTimer = lv_timer_create(testStepTimerCb, 200, self);
    else
        lv_timer_resume(self->_testTimer);
    lv_async_call(loadTestScreenAsync, self);
}

void UI::onBackToVpnClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    if (self->_testTimer)
        lv_timer_pause(self->_testTimer);
    self->requestStatusRefresh();
    lv_async_call(loadMainScreenAsync, self);
}

void UI::testStepTimerCb(lv_timer_t *t)
{
    UI *self = static_cast<UI *>(t->user_data);
    self->runNextTestStep();
}

void UI::runNextTestStep()
{
    switch (_testStep)
    {
    case TEST_BOARD:
    {
        JsonDocument args;
        bool ok = _client->callOk("system", "board", args.to<JsonObject>());
        _testResults += ok ? "[OK] system.board\n" : "[FAIL] system.board\n";
        _testStep = TEST_VPN_STATUS;
        break;
    }
    case TEST_VPN_STATUS:
    {
        JsonDocument args;
        bool ok = _client->callOk((String("network.interface.") + _vpnInterface).c_str(), "status", args.to<JsonObject>());
        _testResults += ok ? "[OK] network.interface.status\n" : "[FAIL] network.interface.status\n";
        _testStep = TEST_VPN_UP;
        break;
    }
    case TEST_VPN_UP:
    {
        JsonDocument args;
        JsonObject obj = args.to<JsonObject>();
        obj["scope"] = "ubus";
        obj["object"] = (String("network.interface.") + _vpnInterface).c_str();
        obj["function"] = "up";
        bool ok = _client->callOk("session", "access", obj);
        _testResults += ok ? "[OK] write-perm check\n" : "[FAIL] write-perm check (need write ACL)\n";
        _testStep = TEST_NETWORK_RESTART;
        break;
    }
    case TEST_NETWORK_RESTART:
    {
        JsonDocument args;
        JsonObject obj = args.to<JsonObject>();
        obj["config"] = "network";
        bool ok = _client->callOk("uci", "get", obj);
        _testResults += ok ? "[OK] uci.get\n" : "[FAIL] uci.get\n";
        _testStep = TEST_DONE;
        break;
    }
    case TEST_DONE:
    {
        _testResults += "\nDone.";
        if (_testTimer)
            lv_timer_pause(_testTimer);
        _testStep = TEST_IDLE;
        break;
    }
    default:
        break;
    }
    lv_label_set_text(_lblTestResults, _testResults.c_str());
}

// ============================================================
// MATRIX SCREENSAVER SCREEN
// ============================================================

static char randMatrixChar()
{
    // Pick from a pool of "matrix-y" looking ASCII characters
    static const char pool[] = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ#$%&*+-/=?@<>";
    return pool[random(0, (int)sizeof(pool) - 1)];
}

void UI::buildMatrixScreen()
{
    _scrMatrix = lv_obj_create(NULL);
    // Strip default styles so LVGL doesn't try to invalidate/repaint the screen
    // while we're drawing directly with TFT_eSPI.
    lv_obj_remove_style_all(_scrMatrix);
    lv_obj_set_style_bg_opa(_scrMatrix, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(_scrMatrix, lv_color_black(), 0);
    lv_obj_clear_flag(_scrMatrix, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_scrMatrix, 0, 0);

    // Make the entire screen clickable so any tap returns to home
    lv_obj_add_flag(_scrMatrix, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_scrMatrix, onMatrixScreenTouched, LV_EVENT_CLICKED, this);

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        // Stagger initial head positions above the screen so columns don't all
        // start in lockstep.
        _matrixHead[c] = (int8_t)(-(int)random(0, MATRIX_ROWS * 2));
        _matrixTrailLen[c] = (int8_t)random(4, 13); // 4..12 inclusive
        for (int r = 0; r < MATRIX_ROWS; r++)
            _matrixChar[c][r] = randMatrixChar();
    }
}

void UI::matrixTimerCb(lv_timer_t *t)
{
    UI *self = static_cast<UI *>(t->user_data);
    self->updateMatrix();
}

void UI::updateMatrix()
{
    static const int CELL_W = 320 / MATRIX_COLS;
    static const int CELL_H = 240 / MATRIX_ROWS;
    // Use text size 1 (6x8 GLCD) so chars fit in narrow cells. The cell is
    // taller (~20 px) so the char sits with vertical breathing room.
    static const int TEXT_SIZE = 1;
    static const int CHAR_W = 6 * TEXT_SIZE;
    static const int CHAR_H = 8 * TEXT_SIZE;
    static const int X_OFFSET = (CELL_W - CHAR_W) / 2;
    static const int Y_OFFSET = (CELL_H - CHAR_H) / 2;

    if (_matrixNeedsClear)
    {
        tft.fillScreen(TFT_BLACK);
        _matrixNeedsClear = false;
    }

    // Brightness ramp from head down. Index 0 = head (white), 1..7 dim greens.
    static const uint16_t trail[8] = {
        TFT_WHITE,
        0x07E8, // bright green
        0x06A6,
        0x05A4,
        0x0463,
        0x0320,
        0x01C0,
        0x0080 // very dark green
    };

    for (int c = 0; c < MATRIX_COLS; c++)
    {
        int h = _matrixHead[c];
        int x = c * CELL_W + X_OFFSET;
        int trailLen = _matrixTrailLen[c]; // 4..8

        // Head (level 0, white) — generate a fresh random char
        if (h >= 0 && h < MATRIX_ROWS)
        {
            char ch = randMatrixChar();
            _matrixChar[c][h] = ch;
            tft.drawChar(x, h * CELL_H + Y_OFFSET, ch, trail[0], TFT_BLACK, TEXT_SIZE);
        }
        // Trail levels 1..trailLen-1, mapped across the dim color slots.
        for (int lvl = 1; lvl < trailLen; lvl++)
        {
            int r = h - lvl;
            if (r >= 0 && r < MATRIX_ROWS)
            {
                // Map this trail level into our color ramp (1..7).
                int colorIdx = 1 + ((lvl - 1) * 6) / (trailLen > 1 ? (trailLen - 1) : 1);
                if (colorIdx > 7)
                    colorIdx = 7;
                tft.drawChar(x, r * CELL_H + Y_OFFSET, _matrixChar[c][r], trail[colorIdx], TFT_BLACK, TEXT_SIZE);
            }
        }
        // Cell beyond the trail: clear back to black
        int clearR = h - trailLen;
        if (clearR >= 0 && clearR < MATRIX_ROWS)
            tft.fillRect(c * CELL_W, clearR * CELL_H, CELL_W, CELL_H, TFT_BLACK);

        _matrixHead[c]++;
        // When the entire trail has scrolled off the bottom, respawn above the
        // top with a random delay and a new random trail length.
        if (_matrixHead[c] >= MATRIX_ROWS + trailLen)
        {
            _matrixHead[c] = (int8_t)(-(int)random(2, 25));
            _matrixTrailLen[c] = (int8_t)random(4, 13);
        }
    }
}

void UI::onMatrixScreenTouched(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    if (self->_matrixTimer)
    {
        lv_timer_pause(self->_matrixTimer);
    }
    lv_async_call(loadHomeScreenAsync, self);
}

// ============================================================
// ASYNC SCREEN LOADERS (with touch suppression)
// ============================================================

void UI::loadHomeScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    lv_scr_load(self->_scrHome);
}

void UI::loadMainScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    self->updateMainScreen();
    lv_scr_load(self->_scrMain);
}

void UI::loadTestScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    lv_scr_load(self->_scrTest);
}

void UI::loadMatrixScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    lv_scr_load(self->_scrMatrix);
    self->_matrixNeedsClear = true;
    if (self->_matrixTimer == nullptr)
        self->_matrixTimer = lv_timer_create(matrixTimerCb, 80, self);
    else
        lv_timer_resume(self->_matrixTimer);
}

// ============================================================
// CLOCK / WEATHER / SYSINFO  (shared back handler)
// ============================================================

void UI::onPassiveScreenTouched(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    if (self->_clockTimer)
        lv_timer_pause(self->_clockTimer);
    if (self->_sysInfoTimer)
        lv_timer_pause(self->_sysInfoTimer);
    if (self->_weatherTimer)
        lv_timer_pause(self->_weatherTimer);
    lv_async_call(loadHomeScreenAsync, self);
}

// ============================================================
// CLOCK SCREEN
// ============================================================

void UI::buildClockScreen()
{
    _scrClock = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrClock, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(_scrClock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_scrClock, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_scrClock, onPassiveScreenTouched, LV_EVENT_CLICKED, this);

    // Clock font size mapping: 0=Small(20), 1=Medium(28), 2=Large(40), 3=ExtraLarge(48)
    const lv_font_t *clockFonts[] = {
        &lv_font_montserrat_20, // Small
        &lv_font_montserrat_28, // Medium
        &lv_font_montserrat_40, // Large
        &lv_font_montserrat_48  // ExtraLarge
    };
    const lv_font_t *dateFonts[] = {
        &lv_font_montserrat_12, // Small
        &lv_font_montserrat_14, // Medium
        &lv_font_montserrat_20, // Large
        &lv_font_montserrat_24  // ExtraLarge
    };
    uint8_t sizeIdx = getClockTextSizeIdx();
    if (sizeIdx > 3)
        sizeIdx = 2;
    const lv_font_t *selectedClockFont = clockFonts[sizeIdx];
    const lv_font_t *selectedDateFont = dateFonts[sizeIdx];

    _lblClockTime = lv_label_create(_scrClock);
    lv_obj_set_style_text_font(_lblClockTime, selectedClockFont, 0);
    lv_obj_set_style_text_color(_lblClockTime, lv_color_hex(0x00FF66), 0);
    lv_label_set_text(_lblClockTime, "--:--:--");
    lv_obj_align(_lblClockTime, LV_ALIGN_CENTER, 0, -30);

    _lblClockDate = lv_label_create(_scrClock);
    lv_obj_set_style_text_font(_lblClockDate, selectedDateFont, 0);
    lv_obj_set_style_text_color(_lblClockDate, lv_color_hex(0x40FF80), 0);
    lv_label_set_text(_lblClockDate, "Synchronizing...");
    lv_obj_align(_lblClockDate, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *hint = lv_label_create(_scrClock);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x208030), 0);
    lv_label_set_text(hint, "tap to return home");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void UI::clockTimerCb(lv_timer_t *t)
{
    UI *self = static_cast<UI *>(t->user_data);
    self->updateClockScreen();
}

void UI::updateClockScreen()
{
    time_t now = time(nullptr);
    struct tm tmInfo;
    localtime_r(&now, &tmInfo);

    char tbuf[12];
    char dbuf[40];
    if (tmInfo.tm_year < (2024 - 1900))
    {
        strcpy(tbuf, "--:--:--");
        strcpy(dbuf, "Waiting for NTP...");
    }
    else
    {
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tmInfo);
        strftime(dbuf, sizeof(dbuf), "%a %d %b %Y", &tmInfo);
    }
    lv_label_set_text(_lblClockTime, tbuf);
    lv_label_set_text(_lblClockDate, dbuf);
}

void UI::applyClockTextSize()
{
    // Update clock time and date fonts based on current setting
    const lv_font_t *clockFonts[] = {
        &lv_font_montserrat_20, // Small
        &lv_font_montserrat_28, // Medium
        &lv_font_montserrat_40, // Large
        &lv_font_montserrat_48  // ExtraLarge
    };
    const lv_font_t *dateFonts[] = {
        &lv_font_montserrat_12, // Small
        &lv_font_montserrat_14, // Medium
        &lv_font_montserrat_20, // Large
        &lv_font_montserrat_24  // ExtraLarge
    };
    uint8_t sizeIdx = getClockTextSizeIdx();
    if (sizeIdx > 3)
        sizeIdx = 2;
    lv_obj_set_style_text_font(_lblClockTime, clockFonts[sizeIdx], 0);
    lv_obj_set_style_text_font(_lblClockDate, dateFonts[sizeIdx], 0);
}

void UI::loadClockScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    self->applyClockTextSize();
    self->updateClockScreen();
    lv_scr_load(self->_scrClock);
    if (self->_clockTimer == nullptr)
        self->_clockTimer = lv_timer_create(clockTimerCb, 500, self);
    else
        lv_timer_resume(self->_clockTimer);
}

// ============================================================
// WEATHER SCREEN
// ============================================================

void UI::buildWeatherScreen()
{
    _scrWeather = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrWeather, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(_scrWeather, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_scrWeather, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_scrWeather, onPassiveScreenTouched, LV_EVENT_CLICKED, this);

    _lblWeatherTitle = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblWeatherTitle, lv_color_hex(0x00FF66), 0);
    lv_label_set_text(_lblWeatherTitle, Settings::owmLabel.c_str());
    lv_obj_align(_lblWeatherTitle, LV_ALIGN_TOP_MID, 0, 4);

    _lblWeatherTemp = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherTemp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lblWeatherTemp, lv_color_hex(0x80FFA0), 0);
    lv_label_set_text(_lblWeatherTemp, "--\xC2\xB0");
    lv_obj_align(_lblWeatherTemp, LV_ALIGN_TOP_LEFT, 12, 22);

    _lblWeatherDesc = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherDesc, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblWeatherDesc, lv_color_hex(0x40FF80), 0);
    lv_label_set_text(_lblWeatherDesc, "Loading...");
    lv_obj_set_width(_lblWeatherDesc, 180);
    lv_label_set_long_mode(_lblWeatherDesc, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblWeatherDesc, LV_ALIGN_TOP_RIGHT, -12, 30);

    _lblWeatherDetails = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherDetails, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblWeatherDetails, lv_color_hex(0x40FF80), 0);
    lv_label_set_text(_lblWeatherDetails, "");
    lv_obj_set_width(_lblWeatherDetails, 220);
    lv_label_set_long_mode(_lblWeatherDetails, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblWeatherDetails, LV_ALIGN_TOP_RIGHT, -8, 58);

    _lblWeatherForecast = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherForecast, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblWeatherForecast, lv_color_hex(0x40FF80), 0);
    lv_label_set_text(_lblWeatherForecast, "");
    lv_obj_set_width(_lblWeatherForecast, 150);
    lv_label_set_long_mode(_lblWeatherForecast, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblWeatherForecast, LV_ALIGN_BOTTOM_LEFT, 8, -38);

    _lblWeatherForecast4d = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(_lblWeatherForecast4d, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblWeatherForecast4d, lv_color_hex(0x40FF80), 0);
    lv_label_set_text(_lblWeatherForecast4d, "");
    lv_obj_set_width(_lblWeatherForecast4d, 150);
    lv_label_set_long_mode(_lblWeatherForecast4d, LV_LABEL_LONG_WRAP);
    lv_obj_align(_lblWeatherForecast4d, LV_ALIGN_BOTTOM_RIGHT, -8, -38);

    lv_obj_t *hint = lv_label_create(_scrWeather);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x208030), 0);
    lv_label_set_text(hint, "tap to return home");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
}

void UI::fetchAndShowWeather()
{
    unsigned long cacheAgeMs = _weatherLastFetchMs == 0 ? ULONG_MAX : (millis() - _weatherLastFetchMs);
    bool hasWeatherCache = _weatherCurrentCache.length() > 0 && _weatherForecastCache.length() > 0;
    bool useCachedWeather = hasWeatherCache && cacheAgeMs < WEATHER_CACHE_MS;

    String body;
    String fbody;

    if (useCachedWeather)
    {
        body = _weatherCurrentCache;
        fbody = _weatherForecastCache;
    }

    if (!useCachedWeather && WiFi.status() != WL_CONNECTED)
    {
        lv_label_set_text(_lblWeatherDesc, "No WiFi");
        lv_label_set_text(_lblWeatherTemp, "--\xC2\xB0");
        lv_label_set_text(_lblWeatherDetails, "");
        lv_label_set_text(_lblWeatherForecast, "");
        lv_label_set_text(_lblWeatherForecast4d, "");
        return;
    }
    if (!useCachedWeather && (Settings::owmApiKey.length() == 0 ||
                              Settings::owmApiKey == "REPLACE_WITH_YOUR_API_KEY"))
    {
        lv_label_set_text(_lblWeatherDesc, "Set OWM API key");
        lv_label_set_text(_lblWeatherTemp, "--\xC2\xB0");
        lv_label_set_text(_lblWeatherDetails, "open the web admin");
        lv_label_set_text(_lblWeatherForecast, "");
        lv_label_set_text(_lblWeatherForecast4d, "");
        return;
    }

    if (!useCachedWeather)
    {
        String url = String("http://api.openweathermap.org/data/2.5/weather?lat=") +
                     Settings::owmLat + "&lon=" + Settings::owmLon +
                     "&units=metric&appid=" + Settings::owmApiKey;

        Serial.print("[WEATHER] GET ");
        Serial.println(url);

        HTTPClient http;
        http.setTimeout(8000);
        http.begin(url);
        int code = http.GET();
        body = http.getString();
        http.end();

        Serial.printf("[WEATHER] HTTP %d, body: %s\n", code, body.c_str());

        if (code != 200)
        {
            if (code == 401)
            {
                JsonDocument edoc;
                const char *serverMsg = "Invalid API key";
                if (deserializeJson(edoc, body) == DeserializationError::Ok)
                    serverMsg = edoc["message"] | serverMsg;
                lv_label_set_text(_lblWeatherTemp, "401");
                lv_label_set_text(_lblWeatherDesc, "API key rejected");
                String s(serverMsg);
                if (s.length() > 60)
                    s = s.substring(0, 57) + "...";
                lv_label_set_text(_lblWeatherDetails, s.c_str());
            }
            else if (code == 429)
            {
                lv_label_set_text(_lblWeatherTemp, "429");
                lv_label_set_text(_lblWeatherDesc, "Rate limited");
                lv_label_set_text(_lblWeatherDetails, "Free plan: 60 calls/min");
            }
            else if (code == 404)
            {
                lv_label_set_text(_lblWeatherTemp, "404");
                lv_label_set_text(_lblWeatherDesc, "Location not found");
                lv_label_set_text(_lblWeatherDetails, "Check OWM_LAT / OWM_LON");
            }
            else if (code < 0)
            {
                lv_label_set_text(_lblWeatherTemp, "ERR");
                lv_label_set_text(_lblWeatherDesc, "Network error");
                String s = String(code) + "  " + http.errorToString(code);
                lv_label_set_text(_lblWeatherDetails, s.c_str());
            }
            else
            {
                String hdr = "HTTP " + String(code);
                lv_label_set_text(_lblWeatherTemp, "--");
                lv_label_set_text(_lblWeatherDesc, hdr.c_str());
                String s = body;
                if (s.length() > 60)
                    s = s.substring(0, 57) + "...";
                lv_label_set_text(_lblWeatherDetails, s.c_str());
            }
            return;
        }

        String fUrl = String("http://api.openweathermap.org/data/2.5/forecast?lat=") +
                      Settings::owmLat + "&lon=" + Settings::owmLon +
                      "&units=metric&appid=" + Settings::owmApiKey;

        HTTPClient fhttp;
        fhttp.setTimeout(8000);
        fhttp.begin(fUrl);
        int fcode = fhttp.GET();
        fbody = fhttp.getString();
        fhttp.end();

        if (fcode != 200)
        {
            lv_label_set_text(_lblWeatherForecast, "Next 12 hour forecast:\nunavailable");
            lv_label_set_text(_lblWeatherForecast4d, "Next 4 day forecast:\nunavailable");
            return;
        }

        _weatherCurrentCache = body;
        _weatherForecastCache = fbody;
        _weatherLastFetchMs = millis();
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err)
    {
        lv_label_set_text(_lblWeatherDesc, "Parse error");
        return;
    }

    float temp = doc["main"]["temp"] | 0.0f;
    float feels = doc["main"]["feels_like"] | 0.0f;
    int humidity = doc["main"]["humidity"] | 0;
    float wind = doc["wind"]["speed"] | 0.0f;
    float gust = doc["wind"]["gust"] | wind;
    long tzOffset = doc["timezone"] | 0;
    time_t sunriseUtc = (time_t)(doc["sys"]["sunrise"] | 0);
    time_t sunsetUtc = (time_t)(doc["sys"]["sunset"] | 0);
    const char *desc = doc["weather"][0]["description"] | "n/a";

    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f°", temp);
    lv_label_set_text(_lblWeatherTemp, buf);

    String descStr(desc);
    if (descStr.length() > 0)
        descStr[0] = toupper(descStr[0]);
    lv_label_set_text(_lblWeatherDesc, descStr.c_str());

    auto hhmmAtOffset = [](time_t utc, long tzSec, char *out, size_t outSize)
    {
        if (utc <= 0 || outSize < 6)
        {
            snprintf(out, outSize, "--:--");
            return;
        }
        time_t localTs = utc + tzSec;
        struct tm tmLocal;
        gmtime_r(&localTs, &tmLocal);
        snprintf(out, outSize, "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
    };

    char sunriseBuf[8];
    char sunsetBuf[8];
    hhmmAtOffset(sunriseUtc, tzOffset, sunriseBuf, sizeof(sunriseBuf));
    hhmmAtOffset(sunsetUtc, tzOffset, sunsetBuf, sizeof(sunsetBuf));

    char details[160];
    snprintf(details, sizeof(details),
             "Feels %.0f°  -  Humidity %d%%\nWind %.1f m/s  -  Gust %.1f m/s\nSunrise %s | Sunset %s",
             feels, humidity, wind, gust, sunriseBuf, sunsetBuf);
    lv_label_set_text(_lblWeatherDetails, details);

    JsonDocument fdoc;
    DeserializationError ferr = deserializeJson(fdoc, fbody);
    if (ferr)
    {
        lv_label_set_text(_lblWeatherForecast, "Next 12 hour forecast:\nparse error");
        lv_label_set_text(_lblWeatherForecast4d, "Next 4 day forecast:\nparse error");
        return;
    }

    JsonArray flist = fdoc["list"];
    if (flist.isNull() || flist.size() == 0)
    {
        lv_label_set_text(_lblWeatherForecast, "Next 12 hour forecast:\nno data");
        lv_label_set_text(_lblWeatherForecast4d, "Next 4 day forecast:\nno data");
        return;
    }

    auto cleanForecastText = [](const char *raw, int maxLen) -> String
    {
        String out;
        if (raw == nullptr)
            return String("n/a");

        for (size_t i = 0; raw[i] != '\0'; i++)
        {
            uint8_t ch = static_cast<uint8_t>(raw[i]);
            if (ch >= 32 && ch <= 126)
                out += static_cast<char>(ch);
            else
                out += ' ';
        }

        out.trim();
        while (out.indexOf("  ") >= 0)
            out.replace("  ", " ");

        if (out.length() > maxLen)
        {
            int cut = maxLen;
            while (cut > 4 && out[cut - 1] != ' ')
                cut--;
            if (cut <= 4)
                cut = maxLen;
            out = out.substring(0, cut);
            out.trim();
        }

        if (out.length() == 0)
            out = "n/a";

        return out;
    };

    String text12h = "Next 12 hour forecast:";
    int slots = 0;
    for (size_t i = 0; i < flist.size() && slots < 3; i++)
    {
        JsonObject item = flist[i];
        time_t dt = (time_t)(item["dt"] | 0);
        float ftemp = item["main"]["temp"] | 0.0f;
        const char *d = item["weather"][0]["description"] | "n/a";
        String dstr = cleanForecastText(d, 12);

        struct tm tmLocal;
        localtime_r(&dt, &tmLocal);

        char row[80];
        snprintf(row, sizeof(row), "%02d:%02d %.0f\xC2\xB0 %s",
                 tmLocal.tm_hour, tmLocal.tm_min, ftemp, dstr.c_str());
        text12h += "\n";
        text12h += row;
        slots++;
    }
    if (slots == 0)
        text12h += "\nNo short-term slots";
    lv_label_set_text(_lblWeatherForecast, text12h.c_str());

    struct DayAgg
    {
        long dayKey;
        int day;
        int month;
        float minTemp;
        float maxTemp;
        String desc;
        bool used;
    };

    DayAgg days[4];
    for (int i = 0; i < 4; i++)
    {
        days[i].dayKey = 0;
        days[i].day = 0;
        days[i].month = 0;
        days[i].minTemp = 1000.0f;
        days[i].maxTemp = -1000.0f;
        days[i].desc = "";
        days[i].used = false;
    }

    long todayKey = (long)(time(nullptr) / 86400);
    int dayCount = 0;

    for (size_t i = 0; i < flist.size(); i++)
    {
        JsonObject item = flist[i];
        time_t dt = (time_t)(item["dt"] | 0);
        long key = (long)(dt / 86400);
        if (key <= todayKey)
            continue;

        int idx = -1;
        for (int j = 0; j < dayCount; j++)
        {
            if (days[j].used && days[j].dayKey == key)
            {
                idx = j;
                break;
            }
        }

        if (idx < 0)
        {
            if (dayCount >= 4)
                continue;
            idx = dayCount;
            dayCount++;

            struct tm tmDay;
            localtime_r(&dt, &tmDay);
            days[idx].dayKey = key;
            days[idx].day = tmDay.tm_mday;
            days[idx].month = tmDay.tm_mon + 1;
            days[idx].used = true;
        }

        float t = item["main"]["temp"] | 0.0f;
        if (t < days[idx].minTemp)
            days[idx].minTemp = t;
        if (t > days[idx].maxTemp)
            days[idx].maxTemp = t;

        if (days[idx].desc.length() == 0)
        {
            const char *d = item["weather"][0]["description"] | "n/a";
            days[idx].desc = cleanForecastText(d, 14);
        }
    }

    String text4d = "Next 4 day forecast:";
    for (int i = 0; i < dayCount; i++)
    {
        String dstr = cleanForecastText(days[i].desc.c_str(), 14);
        char row[72];
        snprintf(row, sizeof(row), "%02d/%02d %.0f\xC2\xB0/%.0f\xC2\xB0 %s",
                 days[i].day, days[i].month,
                 days[i].maxTemp, days[i].minTemp,
                 dstr.c_str());
        text4d += "\n";
        text4d += row;
    }

    if (dayCount == 0)
        text4d += "\nNo upcoming days";

    lv_label_set_text(_lblWeatherForecast4d, text4d.c_str());
}

void UI::loadWeatherScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    lv_label_set_text(self->_lblWeatherDesc, "Loading...");
    lv_label_set_text(self->_lblWeatherTemp, "--°");
    lv_label_set_text(self->_lblWeatherDetails, "");
    lv_label_set_text(self->_lblWeatherForecast, "");
    lv_label_set_text(self->_lblWeatherForecast4d, "");
    lv_scr_load(self->_scrWeather);
    lv_timer_handler(); // paint loading state before blocking on HTTP
    self->fetchAndShowWeather();
    unsigned long nextRefreshMs = WEATHER_CACHE_MS;
    if (self->_weatherLastFetchMs != 0)
    {
        unsigned long ageMs = millis() - self->_weatherLastFetchMs;
        if (ageMs < WEATHER_CACHE_MS)
            nextRefreshMs = WEATHER_CACHE_MS - ageMs;
    }

    // Refresh when the cached weather reaches the 15-minute TTL.
    if (self->_weatherTimer == nullptr)
        self->_weatherTimer = lv_timer_create(weatherTimerCb, nextRefreshMs, self);
    else
    {
        lv_timer_set_period(self->_weatherTimer, nextRefreshMs);
        lv_timer_resume(self->_weatherTimer);
    }
}

void UI::weatherTimerCb(lv_timer_t *t)
{
    UI *self = static_cast<UI *>(t->user_data);
    self->fetchAndShowWeather();
    lv_timer_set_period(t, WEATHER_CACHE_MS);
}

// ============================================================
// SYSTEM INFO SCREEN
// ============================================================

void UI::buildSysInfoScreen()
{
    _scrSysInfo = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrSysInfo, lv_color_hex(0x18120A), 0);
    lv_obj_clear_flag(_scrSysInfo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_scrSysInfo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_scrSysInfo, onPassiveScreenTouched, LV_EVENT_CLICKED, this);

    lv_obj_t *title = lv_label_create(_scrSysInfo);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(title, "System Info");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    _lblSysInfo = lv_label_create(_scrSysInfo);
    lv_obj_set_style_text_font(_lblSysInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblSysInfo, lv_color_hex(0xE0E0E0), 0);
    lv_label_set_text(_lblSysInfo, "");
    lv_obj_align(_lblSysInfo, LV_ALIGN_TOP_LEFT, 12, 40);

    lv_obj_t *hint = lv_label_create(_scrSysInfo);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x806040), 0);
    lv_label_set_text(hint, "tap to return home");
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);
}

void UI::sysInfoTimerCb(lv_timer_t *t)
{
    UI *self = static_cast<UI *>(t->user_data);
    self->updateSysInfoScreen();
}

void UI::updateSysInfoScreen()
{
    char buf[400];
    unsigned long secs = millis() / 1000UL;
    unsigned int days = secs / 86400UL;
    unsigned int hours = (secs % 86400UL) / 3600UL;
    unsigned int mins = (secs % 3600UL) / 60UL;
    unsigned int s = secs % 60UL;

    String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("--");
    int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;

    snprintf(buf, sizeof(buf),
             "Uptime:    %ud %02u:%02u:%02u\n"
             "Free heap: %u bytes\n"
             "Heap min:  %u bytes\n"
             "PSRAM free:%u bytes\n"
             "WiFi:      %s\n"
             "SSID:      %s\n"
             "IP:        %s\n"
             "RSSI:      %d dBm\n"
             "MAC:       %s\n"
             "CPU:       %u MHz\n"
             "Web admin: http://%s/",
             days, hours, mins, s,
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned)ESP.getFreePsram(),
             WiFi.status() == WL_CONNECTED ? "connected" : "disconnected",
             WiFi.SSID().c_str(),
             ip.c_str(),
             rssi,
             WiFi.macAddress().c_str(),
             (unsigned)ESP.getCpuFreqMHz(),
             ip.c_str());

    lv_label_set_text(_lblSysInfo, buf);
}

void UI::loadSysInfoScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    self->updateSysInfoScreen();
    lv_scr_load(self->_scrSysInfo);
    if (self->_sysInfoTimer == nullptr)
        self->_sysInfoTimer = lv_timer_create(sysInfoTimerCb, 1000, self);
    else
        lv_timer_resume(self->_sysInfoTimer);
}

// ============================================================
// OPTIONS SCREEN (display brightness, etc.)
// ============================================================

void UI::buildOptionsScreen()
{
    _scrOptions = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrOptions, lv_color_hex(0x000000), 0);
    lv_obj_set_scroll_dir(_scrOptions, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(_scrOptions, LV_SCROLLBAR_MODE_ACTIVE);

    // Back button
    lv_obj_t *btnBack = lv_btn_create(_scrOptions);
    lv_obj_set_size(btnBack, 70, 28);
    lv_obj_align(btnBack, LV_ALIGN_TOP_LEFT, 6, 4);
    lv_obj_set_style_bg_color(btnBack, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(btnBack, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(btnBack, 1, 0);
    lv_obj_set_style_radius(btnBack, 4, 0);
    lv_obj_add_event_cb(btnBack, onOptionsBackClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblBack = lv_label_create(btnBack);
    lv_label_set_text(lblBack, "< Home");
    lv_obj_set_style_text_color(lblBack, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblBack, &lv_font_montserrat_12, 0);
    lv_obj_center(lblBack);

    // Title
    lv_obj_t *title = lv_label_create(_scrOptions);
    lv_label_set_text(title, "// OPTIONS");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *cardDisplay = lv_obj_create(_scrOptions);
    lv_obj_set_size(cardDisplay, 304, 218);
    lv_obj_align(cardDisplay, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(cardDisplay, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(cardDisplay, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(cardDisplay, 1, 0);
    lv_obj_set_style_radius(cardDisplay, 6, 0);
    lv_obj_clear_flag(cardDisplay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *displayTitle = lv_label_create(cardDisplay);
    lv_label_set_text(displayTitle, "Display");
    lv_obj_set_style_text_color(displayTitle, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(displayTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(displayTitle, LV_ALIGN_TOP_LEFT, 10, 8);

    lv_obj_t *lblB = lv_label_create(cardDisplay);
    lv_label_set_text(lblB, "Brightness");
    lv_obj_set_style_text_color(lblB, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblB, &lv_font_montserrat_12, 0);
    lv_obj_align(lblB, LV_ALIGN_TOP_LEFT, 10, 30);

    _lblBrightnessVal = lv_label_create(cardDisplay);
    lv_obj_set_style_text_color(_lblBrightnessVal, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(_lblBrightnessVal, &lv_font_montserrat_14, 0);
    lv_label_set_text(_lblBrightnessVal, "100%");
    lv_obj_align(_lblBrightnessVal, LV_ALIGN_TOP_RIGHT, -10, 28);

    _brightnessSlider = lv_slider_create(cardDisplay);
    lv_obj_set_size(_brightnessSlider, 280, 14);
    lv_obj_align(_brightnessSlider, LV_ALIGN_TOP_MID, 0, 52);
    lv_slider_set_range(_brightnessSlider, 5, 100);
    lv_slider_set_value(_brightnessSlider, getBrightnessPct(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(0x062612), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(0x00FF66), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(0x80FFA0), LV_PART_KNOB);
    lv_obj_add_event_cb(_brightnessSlider, onBrightnessSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *lblLedB = lv_label_create(cardDisplay);
    lv_label_set_text(lblLedB, "LED Max Brightness");
    lv_obj_set_style_text_color(lblLedB, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblLedB, &lv_font_montserrat_12, 0);
    lv_obj_align(lblLedB, LV_ALIGN_TOP_LEFT, 10, 72);

    _lblLedBrightnessVal = lv_label_create(cardDisplay);
    lv_obj_set_style_text_color(_lblLedBrightnessVal, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(_lblLedBrightnessVal, &lv_font_montserrat_14, 0);
    lv_label_set_text(_lblLedBrightnessVal, "100%");
    lv_obj_align(_lblLedBrightnessVal, LV_ALIGN_TOP_RIGHT, -10, 70);

    _ledBrightnessSlider = lv_slider_create(cardDisplay);
    lv_obj_set_size(_ledBrightnessSlider, 280, 14);
    lv_obj_align(_ledBrightnessSlider, LV_ALIGN_TOP_MID, 0, 94);
    lv_slider_set_range(_ledBrightnessSlider, 0, 100);
    lv_slider_set_value(_ledBrightnessSlider, getLedMaxBrightnessPct(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_ledBrightnessSlider, lv_color_hex(0x062612), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_ledBrightnessSlider, lv_color_hex(0x00FF66), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_ledBrightnessSlider, lv_color_hex(0x80FFA0), LV_PART_KNOB);
    lv_obj_add_event_cb(_ledBrightnessSlider, onLedBrightnessSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *lblLedSpeed = lv_label_create(cardDisplay);
    lv_label_set_text(lblLedSpeed, "Breathing Speed");
    lv_obj_set_style_text_color(lblLedSpeed, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblLedSpeed, &lv_font_montserrat_12, 0);
    lv_obj_align(lblLedSpeed, LV_ALIGN_TOP_LEFT, 10, 114);

    _lblLedSpeedVal = lv_label_create(cardDisplay);
    lv_obj_set_style_text_color(_lblLedSpeedVal, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(_lblLedSpeedVal, &lv_font_montserrat_14, 0);
    lv_label_set_text(_lblLedSpeedVal, "45%");
    lv_obj_align(_lblLedSpeedVal, LV_ALIGN_TOP_RIGHT, -10, 112);

    _ledSpeedSlider = lv_slider_create(cardDisplay);
    lv_obj_set_size(_ledSpeedSlider, 280, 14);
    lv_obj_align(_ledSpeedSlider, LV_ALIGN_TOP_MID, 0, 136);
    lv_slider_set_range(_ledSpeedSlider, 0, 100);
    lv_slider_set_value(_ledSpeedSlider, getLedBreathSpeedPct(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_ledSpeedSlider, lv_color_hex(0x062612), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_ledSpeedSlider, lv_color_hex(0x00FF66), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_ledSpeedSlider, lv_color_hex(0x80FFA0), LV_PART_KNOB);
    lv_obj_add_event_cb(_ledSpeedSlider, onLedSpeedSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *lblClockSize = lv_label_create(cardDisplay);
    lv_label_set_text(lblClockSize, "Clock Text Size");
    lv_obj_set_style_text_color(lblClockSize, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblClockSize, &lv_font_montserrat_12, 0);
    lv_obj_align(lblClockSize, LV_ALIGN_TOP_LEFT, 10, 156);

    _lblClockSizeVal = lv_label_create(cardDisplay);
    lv_obj_set_style_text_color(_lblClockSizeVal, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(_lblClockSizeVal, &lv_font_montserrat_14, 0);
    lv_label_set_text(_lblClockSizeVal, "Large");
    lv_obj_align(_lblClockSizeVal, LV_ALIGN_TOP_RIGHT, -10, 154);

    _clockSizeSlider = lv_slider_create(cardDisplay);
    lv_obj_set_size(_clockSizeSlider, 280, 14);
    lv_obj_align(_clockSizeSlider, LV_ALIGN_TOP_MID, 0, 178);
    lv_slider_set_range(_clockSizeSlider, 0, 3);
    lv_slider_set_value(_clockSizeSlider, getClockTextSizeIdx(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_clockSizeSlider, lv_color_hex(0x062612), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_clockSizeSlider, lv_color_hex(0x00FF66), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_clockSizeSlider, lv_color_hex(0x80FFA0), LV_PART_KNOB);
    lv_obj_add_event_cb(_clockSizeSlider, onClockSizeSliderChanged, LV_EVENT_VALUE_CHANGED, this);

    lv_obj_t *cardScreensaver = lv_obj_create(_scrOptions);
    lv_obj_set_size(cardScreensaver, 304, 186);
    lv_obj_align(cardScreensaver, LV_ALIGN_TOP_MID, 0, 274);
    lv_obj_set_style_bg_color(cardScreensaver, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(cardScreensaver, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(cardScreensaver, 1, 0);
    lv_obj_set_style_radius(cardScreensaver, 6, 0);
    lv_obj_clear_flag(cardScreensaver, LV_OBJ_FLAG_SCROLLABLE);

    _lblScreensaver = lv_label_create(cardScreensaver);
    lv_label_set_text(_lblScreensaver, "Screensaver");
    lv_obj_set_style_text_color(_lblScreensaver, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(_lblScreensaver, &lv_font_montserrat_14, 0);
    lv_obj_align(_lblScreensaver, LV_ALIGN_TOP_LEFT, 10, 8);

    _screensaverSwitch = lv_switch_create(cardScreensaver);
    lv_obj_align(_screensaverSwitch, LV_ALIGN_TOP_RIGHT, -10, 4);
    lv_obj_add_event_cb(_screensaverSwitch, onScreensaverSwitchChanged, LV_EVENT_VALUE_CHANGED, this);

    _lblScreensaverTarget = lv_label_create(cardScreensaver);
    lv_label_set_text(_lblScreensaverTarget, "Saver Screen");
    lv_obj_set_style_text_color(_lblScreensaverTarget, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(_lblScreensaverTarget, &lv_font_montserrat_12, 0);
    lv_obj_align(_lblScreensaverTarget, LV_ALIGN_TOP_LEFT, 10, 48);

    auto styleDropdown = [](lv_obj_t *dd)
    {
        lv_obj_set_style_bg_color(dd, lv_color_hex(0x062612), LV_PART_MAIN);
        lv_obj_set_style_text_color(dd, lv_color_hex(0x80FFA0), LV_PART_MAIN);
        lv_obj_set_style_border_color(dd, lv_color_hex(0x00B040), LV_PART_MAIN);
        lv_obj_set_style_border_width(dd, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(dd, 4, LV_PART_MAIN);

        lv_obj_set_style_text_color(dd, lv_color_hex(0x40FF80), LV_PART_INDICATOR);

        lv_obj_set_style_bg_color(dd, lv_color_hex(0x0A3A1A), LV_PART_SELECTED);
        lv_obj_set_style_text_color(dd, lv_color_hex(0xC8FFD8), LV_PART_SELECTED);

        lv_obj_set_style_bg_color(dd, lv_color_hex(0x0A3A1A), LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(dd, LV_OPA_40, LV_PART_SCROLLBAR);

        lv_obj_set_style_bg_color(dd, lv_color_hex(0x031208), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(dd, lv_color_hex(0x3A5A46), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(dd, lv_color_hex(0x245235), LV_PART_MAIN | LV_STATE_DISABLED);
    };

    _screensaverTargetDropdown = lv_dropdown_create(cardScreensaver);
    lv_dropdown_set_options(_screensaverTargetDropdown, "Clock\nWeather\nMatrix");
    lv_obj_set_size(_screensaverTargetDropdown, 132, 28);
    lv_obj_align(_screensaverTargetDropdown, LV_ALIGN_TOP_RIGHT, -10, 42);
    styleDropdown(_screensaverTargetDropdown);
    lv_obj_add_event_cb(_screensaverTargetDropdown, onScreensaverTargetChanged, LV_EVENT_VALUE_CHANGED, this);

    _lblScreensaverDelay = lv_label_create(cardScreensaver);
    lv_label_set_text(_lblScreensaverDelay, "Delay");
    lv_obj_set_style_text_color(_lblScreensaverDelay, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(_lblScreensaverDelay, &lv_font_montserrat_12, 0);
    lv_obj_align(_lblScreensaverDelay, LV_ALIGN_TOP_LEFT, 10, 86);

    _screensaverDelayDropdown = lv_dropdown_create(cardScreensaver);
    lv_dropdown_set_options(_screensaverDelayDropdown, "30 Seconds\n1 Minute\n2 Minutes\n5 Minutes");
    lv_obj_set_size(_screensaverDelayDropdown, 132, 28);
    lv_obj_align(_screensaverDelayDropdown, LV_ALIGN_TOP_RIGHT, -10, 80);
    styleDropdown(_screensaverDelayDropdown);
    lv_obj_add_event_cb(_screensaverDelayDropdown, onScreensaverDelayChanged, LV_EVENT_VALUE_CHANGED, this);

    _lblScreensaverHint = lv_label_create(cardScreensaver);
    lv_obj_set_style_text_font(_lblScreensaverHint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblScreensaverHint, lv_color_hex(0x208030), 0);
    lv_obj_set_width(_lblScreensaverHint, 280);
    lv_label_set_long_mode(_lblScreensaverHint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_lblScreensaverHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_lblScreensaverHint, "Screensaver starts after selected delay.\nSaved automatically.");
    lv_obj_align(_lblScreensaverHint, LV_ALIGN_TOP_MID, 0, 124);

    lv_obj_t *cardWiFi = lv_obj_create(_scrOptions);
    lv_obj_set_size(cardWiFi, 304, 126);
    lv_obj_align(cardWiFi, LV_ALIGN_TOP_MID, 0, 426);
    lv_obj_set_style_bg_color(cardWiFi, lv_color_hex(0x041808), 0);
    lv_obj_set_style_border_color(cardWiFi, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(cardWiFi, 1, 0);
    lv_obj_set_style_radius(cardWiFi, 6, 0);
    lv_obj_clear_flag(cardWiFi, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lblWiFiTitle = lv_label_create(cardWiFi);
    lv_label_set_text(lblWiFiTitle, "WiFi Setup");
    lv_obj_set_style_text_color(lblWiFiTitle, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblWiFiTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(lblWiFiTitle, LV_ALIGN_TOP_LEFT, 10, 8);

    lv_obj_t *btnWiFiWizard = lv_btn_create(cardWiFi);
    lv_obj_set_size(btnWiFiWizard, 160, 34);
    lv_obj_align(btnWiFiWizard, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_bg_color(btnWiFiWizard, lv_color_hex(0x0A3A1A), 0);
    lv_obj_set_style_border_color(btnWiFiWizard, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(btnWiFiWizard, 1, 0);
    lv_obj_set_style_radius(btnWiFiWizard, 4, 0);
    lv_obj_add_event_cb(btnWiFiWizard, onWiFiWizardClicked, LV_EVENT_CLICKED, this);

    lv_obj_t *lblWiFiBtn = lv_label_create(btnWiFiWizard);
    lv_label_set_text(lblWiFiBtn, "Open WiFi Wizard");
    lv_obj_set_style_text_color(lblWiFiBtn, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_text_font(lblWiFiBtn, &lv_font_montserrat_12, 0);
    lv_obj_center(lblWiFiBtn);

    lv_obj_t *lblWiFiHint = lv_label_create(cardWiFi);
    lv_obj_set_style_text_font(lblWiFiHint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblWiFiHint, lv_color_hex(0x208030), 0);
    lv_obj_set_width(lblWiFiHint, 280);
    lv_label_set_long_mode(lblWiFiHint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(lblWiFiHint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblWiFiHint, "Run initial setup again to update WiFi credentials");
    lv_obj_align(lblWiFiHint, LV_ALIGN_TOP_MID, 0, 76);
}

void UI::onBrightnessSliderChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    int v = lv_slider_get_value(self->_brightnessSlider);
    setBrightnessPct((uint8_t)v);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", v);
    lv_label_set_text(self->_lblBrightnessVal, buf);
}

void UI::onLedBrightnessSliderChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    int v = lv_slider_get_value(self->_ledBrightnessSlider);
    setLedMaxBrightnessPct((uint8_t)v);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", v);
    lv_label_set_text(self->_lblLedBrightnessVal, buf);
}

void UI::onLedSpeedSliderChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    int v = lv_slider_get_value(self->_ledSpeedSlider);
    setLedBreathSpeedPct((uint8_t)v);
    if (v >= 100)
    {
        lv_label_set_text(self->_lblLedSpeedVal, "OFF");
        return;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", v);
    lv_label_set_text(self->_lblLedSpeedVal, buf);
}

void UI::onClockSizeSliderChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    int idx = lv_slider_get_value(self->_clockSizeSlider);
    setClockTextSizeIdx((uint8_t)idx);
    const char *labels[] = {"Small", "Medium", "Large", "ExtraLarge"};
    if (idx >= 0 && idx <= 3)
    {
        lv_label_set_text(self->_lblClockSizeVal, labels[idx]);
    }
}

void UI::onScreensaverSwitchChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    bool enabled = lv_obj_has_state(self->_screensaverSwitch, LV_STATE_CHECKED);
    setScreensaverEnabled(enabled);
    if (!enabled)
    {
        lv_obj_add_state(self->_screensaverTargetDropdown, LV_STATE_DISABLED);
        lv_obj_add_state(self->_screensaverDelayDropdown, LV_STATE_DISABLED);
        lv_obj_set_style_text_color(self->_lblScreensaver, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverTarget, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverDelay, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverHint, lv_color_hex(0x245235), 0);
    }
    else
    {
        lv_obj_clear_state(self->_screensaverTargetDropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(self->_screensaverDelayDropdown, LV_STATE_DISABLED);
        lv_obj_set_style_text_color(self->_lblScreensaver, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverTarget, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverDelay, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverHint, lv_color_hex(0x208030), 0);
    }
}

void UI::onScreensaverTargetChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    uint16_t selected = lv_dropdown_get_selected(self->_screensaverTargetDropdown);
    setScreensaverTarget((uint8_t)selected);
}

void UI::onScreensaverDelayChanged(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    uint16_t selected = lv_dropdown_get_selected(self->_screensaverDelayDropdown);
    unsigned long delayMs = 30000UL;
    if (selected == 1)
        delayMs = 60000UL;
    else if (selected == 2)
        delayMs = 120000UL;
    else if (selected == 3)
        delayMs = 300000UL;
    setScreensaverDelayMs(delayMs);
}

void UI::onWiFiWizardClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    suppressTouchFor(800);
    self->showWiFiSetup();
}

void UI::onOptionsBackClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    suppressTouchFor(800);
    lv_async_call(loadHomeScreenAsync, self);
}

void UI::loadOptionsScreenAsync(void *user_data)
{
    UI *self = static_cast<UI *>(user_data);
    suppressTouchFor(800);
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev)
    {
        lv_indev_reset(indev, NULL);
        lv_indev_wait_release(indev);
    }
    // Sync slider + label with current value in case it changed elsewhere
    lv_slider_set_value(self->_brightnessSlider, getBrightnessPct(), LV_ANIM_OFF);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", getBrightnessPct());
    lv_label_set_text(self->_lblBrightnessVal, buf);
    lv_slider_set_value(self->_ledBrightnessSlider, getLedMaxBrightnessPct(), LV_ANIM_OFF);
    snprintf(buf, sizeof(buf), "%d%%", getLedMaxBrightnessPct());
    lv_label_set_text(self->_lblLedBrightnessVal, buf);
    lv_slider_set_value(self->_ledSpeedSlider, getLedBreathSpeedPct(), LV_ANIM_OFF);
    if (getLedBreathSpeedPct() >= 100)
        lv_label_set_text(self->_lblLedSpeedVal, "OFF");
    else
    {
        snprintf(buf, sizeof(buf), "%d%%", getLedBreathSpeedPct());
        lv_label_set_text(self->_lblLedSpeedVal, buf);
    }

    lv_slider_set_value(self->_clockSizeSlider, getClockTextSizeIdx(), LV_ANIM_OFF);
    const char *sizeLabels[] = {"Small", "Medium", "Large", "ExtraLarge"};
    uint8_t sizeIdx = getClockTextSizeIdx();
    if (sizeIdx <= 3)
        lv_label_set_text(self->_lblClockSizeVal, sizeLabels[sizeIdx]);

    if (getScreensaverEnabled())
    {
        lv_obj_add_state(self->_screensaverSwitch, LV_STATE_CHECKED);
        lv_obj_clear_state(self->_screensaverTargetDropdown, LV_STATE_DISABLED);
        lv_obj_clear_state(self->_screensaverDelayDropdown, LV_STATE_DISABLED);
        lv_obj_set_style_text_color(self->_lblScreensaver, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverTarget, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverDelay, lv_color_hex(0x40FF80), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverHint, lv_color_hex(0x208030), 0);
    }
    else
    {
        lv_obj_clear_state(self->_screensaverSwitch, LV_STATE_CHECKED);
        lv_obj_add_state(self->_screensaverTargetDropdown, LV_STATE_DISABLED);
        lv_obj_add_state(self->_screensaverDelayDropdown, LV_STATE_DISABLED);
        lv_obj_set_style_text_color(self->_lblScreensaver, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverTarget, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverDelay, lv_color_hex(0x3A5A46), 0);
        lv_obj_set_style_text_color(self->_lblScreensaverHint, lv_color_hex(0x245235), 0);
    }
    lv_dropdown_set_selected(self->_screensaverTargetDropdown, getScreensaverTarget());

    unsigned long delayMs = getScreensaverDelayMs();
    uint16_t delaySel = 0;
    if (delayMs == 60000UL)
        delaySel = 1;
    else if (delayMs == 120000UL)
        delaySel = 2;
    else if (delayMs == 300000UL)
        delaySel = 3;
    lv_dropdown_set_selected(self->_screensaverDelayDropdown, delaySel);

    lv_obj_scroll_to_y(self->_scrOptions, 0, LV_ANIM_OFF);
    lv_scr_load(self->_scrOptions);
}

// ============================================================
// WIFI SETUP SCREEN (scan & connect)
// ============================================================

void UI::buildWiFiSetupScreen()
{
    _scrWiFiSetup = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scrWiFiSetup, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(_scrWiFiSetup, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t *title = lv_label_create(_scrWiFiSetup);
    lv_label_set_text(title, "// WiFi Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // Status label
    _wifiStatusLabel = lv_label_create(_scrWiFiSetup);
    lv_obj_set_style_text_color(_wifiStatusLabel, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(_wifiStatusLabel, &lv_font_montserrat_12, 0);
    lv_label_set_text(_wifiStatusLabel, "Scanning...");
    lv_obj_align(_wifiStatusLabel, LV_ALIGN_TOP_MID, 0, 28);

    // Scrollable list of SSIDs (top half of screen)
    _wifiList = lv_list_create(_scrWiFiSetup);
    lv_obj_set_size(_wifiList, 300, 130);
    lv_obj_align(_wifiList, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(_wifiList, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_border_color(_wifiList, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(_wifiList, 1, 0);
    lv_obj_set_style_radius(_wifiList, 4, 0);

    // Rescan button
    lv_obj_t *btnScan = lv_btn_create(_scrWiFiSetup);
    lv_obj_set_size(btnScan, 90, 28);
    lv_obj_align(btnScan, LV_ALIGN_BOTTOM_LEFT, 10, -6);
    lv_obj_set_style_bg_color(btnScan, lv_color_hex(0x0A3A1A), 0);
    lv_obj_set_style_border_color(btnScan, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(btnScan, 1, 0);
    lv_obj_set_style_radius(btnScan, 4, 0);
    lv_obj_add_event_cb(btnScan, onWiFiScanClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblScan = lv_label_create(btnScan);
    lv_label_set_text(lblScan, "Rescan");
    lv_obj_set_style_text_color(lblScan, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblScan, &lv_font_montserrat_12, 0);
    lv_obj_center(lblScan);

    _wifiBackBtn = lv_btn_create(_scrWiFiSetup);
    lv_obj_set_size(_wifiBackBtn, 90, 28);
    lv_obj_align(_wifiBackBtn, LV_ALIGN_BOTTOM_RIGHT, -10, -6);
    lv_obj_set_style_bg_color(_wifiBackBtn, lv_color_hex(0x0A3A1A), 0);
    lv_obj_set_style_border_color(_wifiBackBtn, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(_wifiBackBtn, 1, 0);
    lv_obj_set_style_radius(_wifiBackBtn, 4, 0);
    lv_obj_add_event_cb(_wifiBackBtn, onWiFiBackClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblBack = lv_label_create(_wifiBackBtn);
    lv_label_set_text(lblBack, "Back");
    lv_obj_set_style_text_color(lblBack, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblBack, &lv_font_montserrat_12, 0);
    lv_obj_center(lblBack);

    // Password entry panel (hidden until an SSID is selected)
    _wifiPassPanel = lv_obj_create(_scrWiFiSetup);
    lv_obj_set_size(_wifiPassPanel, 320, 240);
    lv_obj_align(_wifiPassPanel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_wifiPassPanel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all(_wifiPassPanel, 0, 0);
    lv_obj_clear_flag(_wifiPassPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_wifiPassPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *passTitle = lv_label_create(_wifiPassPanel);
    lv_label_set_text(passTitle, "Enter password:");
    lv_obj_set_style_text_color(passTitle, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_text_font(passTitle, &lv_font_montserrat_14, 0);
    lv_obj_align(passTitle, LV_ALIGN_TOP_LEFT, 10, 6);

    _wifiPassTA = lv_textarea_create(_wifiPassPanel);
    lv_obj_set_size(_wifiPassTA, 182, 36);
    lv_obj_align(_wifiPassTA, LV_ALIGN_TOP_LEFT, 10, 30);
    lv_textarea_set_one_line(_wifiPassTA, true);
    lv_textarea_set_password_mode(_wifiPassTA, true);
    lv_obj_set_style_bg_color(_wifiPassTA, lv_color_hex(0x031F0B), 0);
    lv_obj_set_style_text_color(_wifiPassTA, lv_color_hex(0x80FFA0), 0);
    lv_obj_set_style_border_color(_wifiPassTA, lv_color_hex(0x00B040), 0);
    lv_obj_set_style_border_width(_wifiPassTA, 1, 0);
    lv_obj_set_style_pad_left(_wifiPassTA, 8, 0);
    lv_obj_set_style_pad_right(_wifiPassTA, 8, 0);

    lv_obj_t *btnPassBack = lv_btn_create(_wifiPassPanel);
    lv_obj_set_size(btnPassBack, 92, 32);
    lv_obj_align(btnPassBack, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_set_style_bg_color(btnPassBack, lv_color_hex(0x0A3A1A), 0);
    lv_obj_set_style_border_color(btnPassBack, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(btnPassBack, 1, 0);
    lv_obj_set_style_radius(btnPassBack, 4, 0);
    lv_obj_add_event_cb(btnPassBack, onWiFiPassBackClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblPassBack = lv_label_create(btnPassBack);
    lv_label_set_text(lblPassBack, "Back");
    lv_obj_set_style_text_color(lblPassBack, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblPassBack, &lv_font_montserrat_12, 0);
    lv_obj_center(lblPassBack);

    // Connect button
    lv_obj_t *btnConnect = lv_btn_create(_wifiPassPanel);
    lv_obj_set_size(btnConnect, 92, 36);
    lv_obj_align(btnConnect, LV_ALIGN_TOP_RIGHT, -10, 44);
    lv_obj_set_style_bg_color(btnConnect, lv_color_hex(0x0A3A1A), 0);
    lv_obj_set_style_border_color(btnConnect, lv_color_hex(0x00FF66), 0);
    lv_obj_set_style_border_width(btnConnect, 1, 0);
    lv_obj_set_style_radius(btnConnect, 4, 0);
    lv_obj_add_event_cb(btnConnect, onWiFiConnectClicked, LV_EVENT_CLICKED, this);
    lv_obj_t *lblConn = lv_label_create(btnConnect);
    lv_label_set_text(lblConn, "Connect");
    lv_obj_set_style_text_color(lblConn, lv_color_hex(0x40FF80), 0);
    lv_obj_set_style_text_font(lblConn, &lv_font_montserrat_12, 0);
    lv_obj_center(lblConn);

    // On-screen keyboard
    _wifiKeyboard = lv_keyboard_create(_wifiPassPanel);
    lv_obj_set_size(_wifiKeyboard, 320, 154);
    lv_obj_align(_wifiKeyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(_wifiKeyboard, _wifiPassTA);
    lv_obj_set_style_bg_color(_wifiKeyboard, lv_color_hex(0x041808), 0);
    lv_obj_set_style_bg_color(_wifiKeyboard, lv_color_hex(0x0A3A1A), LV_PART_ITEMS);
    lv_obj_set_style_text_color(_wifiKeyboard, lv_color_hex(0x80FFA0), LV_PART_ITEMS);
    lv_obj_add_event_cb(_wifiKeyboard, onWiFiKbReady, LV_EVENT_READY, this);
}

void UI::showWiFiSetup()
{
    if (!_scrWiFiSetup)
        buildWiFiSetupScreen();

    bool wifiConfigured = (Settings::wifiSsid.length() > 0 &&
                           Settings::wifiSsid != "Update Me");
    if (_wifiBackBtn)
    {
        if (wifiConfigured)
            lv_obj_clear_flag(_wifiBackBtn, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(_wifiBackBtn, LV_OBJ_FLAG_HIDDEN);
    }

    _wifiSetupDone = false;
    lv_scr_load(_scrWiFiSetup);
    populateWiFiList();
}

void UI::populateWiFiList()
{
    lv_label_set_text(_wifiStatusLabel, "Scanning...");
    lv_timer_handler(); // flush UI so user sees "Scanning..."

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    if (n <= 0)
    {
        lv_label_set_text(_wifiStatusLabel, "No networks found. Tap Rescan.");
        return;
    }

    // Clear existing list items
    lv_obj_clean(_wifiList);

    char buf[64];
    for (int i = 0; i < n && i < 20; i++)
    {
        int rssi = WiFi.RSSI(i);
        const char *lock = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "" : LV_SYMBOL_EYE_CLOSE;
        snprintf(buf, sizeof(buf), "%s %s (%ddBm)", lock, WiFi.SSID(i).c_str(), rssi);

        lv_obj_t *btn = lv_list_add_btn(_wifiList, NULL, buf);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x031F0B), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x40FF80), LV_PART_MAIN);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, LV_PART_MAIN);
        // Store the SSID index in user_data
        lv_obj_add_event_cb(btn, onWiFiNetworkSelected, LV_EVENT_CLICKED, this);
        // Use the child index to recover the SSID later
        btn->user_data = (void *)(intptr_t)i;
    }

    snprintf(buf, sizeof(buf), "Found %d networks. Tap to select.", n);
    lv_label_set_text(_wifiStatusLabel, buf);
    WiFi.scanDelete();
}

void UI::onWiFiScanClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    self->populateWiFiList();
}

void UI::onWiFiBackClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    suppressTouchFor(800);
    lv_async_call(loadOptionsScreenAsync, self);
}

void UI::onWiFiPassBackClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    lv_obj_add_flag(self->_wifiPassPanel, LV_OBJ_FLAG_HIDDEN);
}

void UI::onWiFiNetworkSelected(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));

    // Get the label text from the button's child label
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (!label)
        return;
    const char *text = lv_label_get_text(label);

    // Parse SSID: skip lock symbol + space if present, extract between first space and last " ("
    String full(text);
    // The SSID was stored via scan index; re-scan results are deleted, so parse from label
    // Format: "[lock] SSID (RSSIdBm)"
    int parenPos = full.lastIndexOf(" (");
    if (parenPos < 0)
        parenPos = full.length();
    String ssid = full.substring(0, parenPos);
    // Trim leading lock symbol(s) and spaces
    if (ssid.startsWith(LV_SYMBOL_EYE_CLOSE))
        ssid = ssid.substring(String(LV_SYMBOL_EYE_CLOSE).length());
    ssid.trim();

    self->_selectedSSID = ssid;

    // Show password panel
    lv_textarea_set_text(self->_wifiPassTA, "");
    lv_obj_clear_flag(self->_wifiPassPanel, LV_OBJ_FLAG_HIDDEN);

    char buf[64];
    snprintf(buf, sizeof(buf), "Selected: %s", ssid.c_str());
    lv_label_set_text(self->_wifiStatusLabel, buf);
}

void UI::onWiFiKbReady(lv_event_t *e)
{
    // Keyboard "OK" pressed — same as Connect button
    onWiFiConnectClicked(e);
}

void UI::onWiFiConnectClicked(lv_event_t *e)
{
    UI *self = static_cast<UI *>(lv_event_get_user_data(e));
    const char *pass = lv_textarea_get_text(self->_wifiPassTA);

    lv_label_set_text(self->_wifiStatusLabel, "Connecting...");
    lv_obj_add_flag(self->_wifiPassPanel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler(); // paint

    WiFi.begin(self->_selectedSSID.c_str(), pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        attempts++;
        lv_timer_handler(); // keep UI responsive
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // Save to NVS so next boot uses these credentials
        Settings::wifiSsid = self->_selectedSSID;
        Settings::wifiPassword = String(pass);
        Settings::saveAll();

        char buf[80];
        snprintf(buf, sizeof(buf), "Connected! IP: %s", WiFi.localIP().toString().c_str());
        lv_label_set_text(self->_wifiStatusLabel, buf);
        lv_timer_handler();
        delay(1500);

        self->_wifiSetupDone = true;
    }
    else
    {
        lv_label_set_text(self->_wifiStatusLabel, "Failed. Tap network to retry.");
    }
}
