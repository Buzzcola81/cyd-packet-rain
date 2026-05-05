#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include "OpenWrtClient.h"

#define MATRIX_COLS 32
#define MATRIX_ROWS 24

class UI
{
public:
    enum ScreensaverTarget
    {
        SCREENSAVER_CLOCK = 0,
        SCREENSAVER_WEATHER = 1,
        SCREENSAVER_MATRIX = 2
    };

    UI(OpenWrtClient *client, const char *vpnInterface, VpnStatus *status);

    void begin();
    void updateMainScreen();
    void requestStatusRefresh();
    void showScreensaverTarget(uint8_t target);
    bool isScreensaverTargetActive(uint8_t target) const;
    bool needsStatusRefresh() const { return _needsRefresh; }
    void clearStatusRefreshFlag() { _needsRefresh = false; }
    bool isMatrixActive() const { return lv_scr_act() == _scrMatrix; }
    bool isPassiveScreen() const
    {
        lv_obj_t *s = lv_scr_act();
        return s == _scrMatrix || s == _scrClock || s == _scrSysInfo;
    }
    bool isAmbientScreen() const
    {
        lv_obj_t *s = lv_scr_act();
        return s == _scrMatrix || s == _scrClock || s == _scrSysInfo || s == _scrWeather;
    }

private:
    void buildHomeScreen();
    void buildMainScreen();
    void buildTestScreen();
    void buildMatrixScreen();
    void buildClockScreen();
    void buildWeatherScreen();
    void buildSysInfoScreen();
    void buildOptionsScreen();
    void buildWiFiSetupScreen();

    void updateClockScreen();
    void updateSysInfoScreen();
    void applyClockTextSize();
    void fetchAndShowWeather();

    static void onToggleClicked(lv_event_t *e);
    static void onTestClicked(lv_event_t *e);
    static void onBackToVpnClicked(lv_event_t *e);
    static void onHomePrevClicked(lv_event_t *e);
    static void onHomeNextClicked(lv_event_t *e);
    static void onHomeSelectClicked(lv_event_t *e);
    static void onHomeFromVpnClicked(lv_event_t *e);
    static void onMatrixScreenTouched(lv_event_t *e);
    static void onPassiveScreenTouched(lv_event_t *e);

    void updateHomeSelection();

    static void loadHomeScreenAsync(void *user_data);
    static void loadMainScreenAsync(void *user_data);
    static void loadTestScreenAsync(void *user_data);
    static void loadMatrixScreenAsync(void *user_data);
    static void loadClockScreenAsync(void *user_data);
    static void loadWeatherScreenAsync(void *user_data);
    static void loadSysInfoScreenAsync(void *user_data);
    static void loadOptionsScreenAsync(void *user_data);

    static void onBrightnessSliderChanged(lv_event_t *e);
    static void onLedBrightnessSliderChanged(lv_event_t *e);
    static void onLedSpeedSliderChanged(lv_event_t *e);
    static void onClockSizeSliderChanged(lv_event_t *e);
    static void onScreensaverSwitchChanged(lv_event_t *e);
    static void onScreensaverTargetChanged(lv_event_t *e);
    static void onScreensaverDelayChanged(lv_event_t *e);
    static void onWiFiWizardClicked(lv_event_t *e);
    static void onOptionsBackClicked(lv_event_t *e);
    static void onWiFiBackClicked(lv_event_t *e);
    static void onWiFiPassBackClicked(lv_event_t *e);

    static void clockTimerCb(lv_timer_t *t);
    static void sysInfoTimerCb(lv_timer_t *t);
    static void weatherTimerCb(lv_timer_t *t);

    static constexpr unsigned long WEATHER_CACHE_MS = 15UL * 60UL * 1000UL;

    enum TestStep
    {
        TEST_IDLE,
        TEST_BOARD,
        TEST_VPN_STATUS,
        TEST_VPN_UP,
        TEST_NETWORK_RESTART,
        TEST_DONE
    };
    static void testStepTimerCb(lv_timer_t *t);
    void runNextTestStep();

    static void matrixTimerCb(lv_timer_t *t);
    void updateMatrix();

    OpenWrtClient *_client;
    const char *_vpnInterface;
    VpnStatus *_status;

    // Home screen
    lv_obj_t *_scrHome = nullptr;
    lv_obj_t *_homeLabel = nullptr;
    lv_obj_t *_homeSelectBtn = nullptr;
    lv_obj_t *_homeSelectLbl = nullptr;
    int _homeIndex = 0;
    static const int HOME_MENU_COUNT = 6;

    // Main (VPN) screen
    lv_obj_t *_scrMain = nullptr;
    lv_obj_t *_statusBadge = nullptr;
    lv_obj_t *_vpnIp = nullptr;
    lv_obj_t *_publicIp = nullptr;
    lv_obj_t *_location = nullptr;
    lv_obj_t *_traffic = nullptr;
    lv_obj_t *_uptime = nullptr;
    lv_obj_t *_btnToggle = nullptr;
    lv_obj_t *_lblBtnToggle = nullptr;

    // Test screen
    lv_obj_t *_scrTest = nullptr;
    lv_obj_t *_lblTestResults = nullptr;
    TestStep _testStep = TEST_IDLE;
    String _testResults;
    lv_timer_t *_testTimer = nullptr;

    // Matrix screensaver screen (direct TFT drawing, not LVGL widgets)
    lv_obj_t *_scrMatrix = nullptr;
    int8_t _matrixHead[MATRIX_COLS];
    int8_t _matrixTrailLen[MATRIX_COLS];
    char _matrixChar[MATRIX_COLS][MATRIX_ROWS];
    lv_timer_t *_matrixTimer = nullptr;
    bool _matrixNeedsClear = false;

    // Clock screen
    lv_obj_t *_scrClock = nullptr;
    lv_obj_t *_lblClockTime = nullptr;
    lv_obj_t *_lblClockDate = nullptr;
    lv_timer_t *_clockTimer = nullptr;

    // Weather screen
    lv_obj_t *_scrWeather = nullptr;
    lv_obj_t *_lblWeatherTitle = nullptr;
    lv_obj_t *_lblWeatherTemp = nullptr;
    lv_obj_t *_lblWeatherDesc = nullptr;
    lv_obj_t *_lblWeatherDetails = nullptr;
    lv_obj_t *_lblWeatherForecast = nullptr;
    lv_obj_t *_lblWeatherForecast4d = nullptr;
    lv_timer_t *_weatherTimer = nullptr;
    String _weatherCurrentCache;
    String _weatherForecastCache;
    unsigned long _weatherLastFetchMs = 0;

    // System info screen
    lv_obj_t *_scrSysInfo = nullptr;
    lv_obj_t *_lblSysInfo = nullptr;
    lv_timer_t *_sysInfoTimer = nullptr;

    // Options screen
    lv_obj_t *_scrOptions = nullptr;
    lv_obj_t *_brightnessSlider = nullptr;
    lv_obj_t *_lblBrightnessVal = nullptr;
    lv_obj_t *_ledBrightnessSlider = nullptr;
    lv_obj_t *_lblLedBrightnessVal = nullptr;
    lv_obj_t *_ledSpeedSlider = nullptr;
    lv_obj_t *_lblLedSpeedVal = nullptr;
    lv_obj_t *_clockSizeSlider = nullptr;
    lv_obj_t *_lblClockSizeVal = nullptr;
    lv_obj_t *_screensaverSwitch = nullptr;
    lv_obj_t *_screensaverTargetDropdown = nullptr;
    lv_obj_t *_screensaverDelayDropdown = nullptr;
    lv_obj_t *_lblScreensaver = nullptr;
    lv_obj_t *_lblScreensaverTarget = nullptr;
    lv_obj_t *_lblScreensaverDelay = nullptr;
    lv_obj_t *_lblScreensaverHint = nullptr;

    bool _needsRefresh = false;

    // WiFi setup screen
    lv_obj_t *_scrWiFiSetup = nullptr;
    lv_obj_t *_wifiList = nullptr;
    lv_obj_t *_wifiStatusLabel = nullptr;
    lv_obj_t *_wifiKeyboard = nullptr;
    lv_obj_t *_wifiPassTA = nullptr;
    lv_obj_t *_wifiPassPanel = nullptr;
    lv_obj_t *_wifiBackBtn = nullptr;
    String _selectedSSID;
    bool _wifiSetupDone = false;

    void populateWiFiList();
    static void onWiFiNetworkSelected(lv_event_t *e);
    static void onWiFiConnectClicked(lv_event_t *e);
    static void onWiFiScanClicked(lv_event_t *e);
    static void onWiFiKbReady(lv_event_t *e);

public:
    void showWiFiSetup();
    bool isWiFiSetupDone() const { return _wifiSetupDone; }
    bool isWiFiSetupActive() const { return lv_scr_act() == _scrWiFiSetup; }
    lv_obj_t *getHomeScreen() const { return _scrHome; }
};

#endif
