#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Runtime configuration backed by NVS (Preferences). On first boot the
// values fall back to the compile-time defaults from config.h. The web
// admin can update them at runtime; a reboot then picks them up.
class Settings
{
public:
    static void load();
    static void saveAll();

    // WiFi
    static String wifiSsid;
    static String wifiPassword;

    // OpenWrt
    static String openwrtHost;
    static String openwrtUser;
    static String openwrtPass;
    static String vpnInterface;

    // Weather (OpenWeatherMap)
    static String owmApiKey;
    static String owmLat;
    static String owmLon;
    static String owmLabel;

    // Derived
    static String openwrtRpcUrl();
};

#endif
