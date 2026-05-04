#include "Settings.h"
#include <Preferences.h>
#include "config.h"

String Settings::wifiSsid;
String Settings::wifiPassword;
String Settings::openwrtHost;
String Settings::openwrtUser;
String Settings::openwrtPass;
String Settings::vpnInterface;
String Settings::owmApiKey;
String Settings::owmLat;
String Settings::owmLon;
String Settings::owmLabel;

static const char *NS = "cfg";

void Settings::load()
{
    Preferences p;
    p.begin(NS, true);
    wifiSsid = p.getString("wifi_ssid", WIFI_SSID);
    wifiPassword = p.getString("wifi_pass", WIFI_PASSWORD);
    openwrtHost = p.getString("ow_host", OPENWRT_HOST);
    openwrtUser = p.getString("ow_user", OPENWRT_USER);
    openwrtPass = p.getString("ow_pass", OPENWRT_PASS);
    vpnInterface = p.getString("ow_vpn", VPN_INTERFACE);
    owmApiKey = p.getString("owm_key", OWM_API_KEY);
    owmLat = p.getString("owm_lat", OWM_LAT);
    owmLon = p.getString("owm_lon", OWM_LON);
    owmLabel = p.getString("owm_label", OWM_LABEL);
    p.end();
}

void Settings::saveAll()
{
    Preferences p;
    p.begin(NS, false);
    p.putString("wifi_ssid", wifiSsid);
    p.putString("wifi_pass", wifiPassword);
    p.putString("ow_host", openwrtHost);
    p.putString("ow_user", openwrtUser);
    p.putString("ow_pass", openwrtPass);
    p.putString("ow_vpn", vpnInterface);
    p.putString("owm_key", owmApiKey);
    p.putString("owm_lat", owmLat);
    p.putString("owm_lon", owmLon);
    p.putString("owm_label", owmLabel);
    p.end();
}

String Settings::openwrtRpcUrl()
{
    return String("http://") + openwrtHost + "/ubus";
}
