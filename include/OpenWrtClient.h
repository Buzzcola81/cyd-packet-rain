#ifndef OPENWRT_CLIENT_H
#define OPENWRT_CLIENT_H

#include <Arduino.h>
#include <ArduinoJson.h>

struct VpnStatus
{
    bool up;
    String ipAddr;
    String protocol;
    unsigned long uptime;
    String publicIp;
    String location;
    String device;
    uint64_t rxBytes;
    uint64_t txBytes;
};

class OpenWrtClient
{
public:
    OpenWrtClient(const char *rpcUrl, const char *user, const char *pass);

    bool login();
    String call(const char *path, const char *method, JsonObject args);
    bool callOk(const char *path, const char *method, JsonObject args);

    void fetchVpnStatus(const char *vpnInterface, VpnStatus &out);
    void fetchPublicIpInfo(VpnStatus &out);

    bool vpnUp(const char *vpnInterface);
    bool vpnDown(const char *vpnInterface);

private:
    String _rpcUrl;
    String _user;
    String _pass;
    String _token;
};

// Format helpers
String formatUptime(unsigned long seconds);
String formatBytes(uint64_t bytes);

#endif
