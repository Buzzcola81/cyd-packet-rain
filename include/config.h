#ifndef CONFIG_H
#define CONFIG_H

// WiFi credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// OpenWrt device settings
#define OPENWRT_HOST ""
#define OPENWRT_USER ""
#define OPENWRT_PASS ""

// OpenWrt ubus RPC endpoint
#define OPENWRT_RPC_URL "http://" OPENWRT_HOST "/ubus"

// VPN interface name as configured in OpenWrt (Network > Interfaces)
#define VPN_INTERFACE ""

// Weather (OpenWeatherMap). Get a free key from https://openweathermap.org/api
#define OWM_API_KEY ""
#define OWM_LAT ""
#define OWM_LON ""
#define OWM_LABEL "City Name, Country"

// Timezone for NTP clock (Melbourne, with DST)
#define TZ_POSIX "AEST-10AEDT,M10.1.0,M4.1.0/3"
#define NTP_SERVER "pool.ntp.org"

#endif // CONFIG_H
