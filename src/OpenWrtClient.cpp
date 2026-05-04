#include "OpenWrtClient.h"
#include <HTTPClient.h>

OpenWrtClient::OpenWrtClient(const char *rpcUrl, const char *user, const char *pass)
    : _rpcUrl(rpcUrl), _user(user), _pass(pass),
      _token("00000000000000000000000000000000")
{
}

bool OpenWrtClient::login()
{
    HTTPClient http;
    http.begin(_rpcUrl);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = 1;
    doc["method"] = "call";
    JsonArray params = doc["params"].to<JsonArray>();
    params.add("00000000000000000000000000000000");
    params.add("session");
    params.add("login");
    JsonObject loginArgs = params.add<JsonObject>();
    loginArgs["username"] = _user;
    loginArgs["password"] = _pass;

    String payload;
    serializeJson(doc, payload);

    int code = http.POST(payload);
    if (code == 200)
    {
        String response = http.getString();
        JsonDocument respDoc;
        deserializeJson(respDoc, response);
        const char *token = respDoc["result"][1]["ubus_rpc_session"];
        if (token)
        {
            _token = String(token);
            Serial.println("ubus login OK: " + _token);
            http.end();
            return true;
        }
    }
    Serial.println("ubus login failed HTTP: " + String(code));
    http.end();
    return false;
}

String OpenWrtClient::call(const char *path, const char *method, JsonObject args)
{
    HTTPClient http;
    http.begin(_rpcUrl);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = 1;
    doc["method"] = "call";
    JsonArray params = doc["params"].to<JsonArray>();
    params.add(_token);
    params.add(path);
    params.add(method);
    if (args.isNull())
        params.add<JsonObject>();
    else
        params.add(args);

    String payload;
    serializeJson(doc, payload);

    String result = "";
    int code = http.POST(payload);
    if (code == 200)
        result = http.getString();
    else
        Serial.println("ubus call HTTP fail: " + String(code));

    http.end();
    return result;
}

bool OpenWrtClient::callOk(const char *path, const char *method, JsonObject args)
{
    String r = call(path, method, args);
    return r.length() > 0 && r.indexOf("\"error\"") == -1;
}

void OpenWrtClient::fetchPublicIpInfo(VpnStatus &out)
{
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=query,country,city");
    int code = http.GET();
    if (code == 200)
    {
        String response = http.getString();
        JsonDocument doc;
        deserializeJson(doc, response);
        const char *ip = doc["query"];
        const char *country = doc["country"];
        const char *city = doc["city"];
        out.publicIp = ip ? String(ip) : "N/A";
        if (country)
            out.location = String(city ? city : "") + (city ? ", " : "") + String(country);
        else
            out.location = "Unknown";
    }
    else
    {
        out.publicIp = "N/A";
        out.location = "N/A";
    }
    http.end();
}

void OpenWrtClient::fetchVpnStatus(const char *vpnInterface, VpnStatus &out)
{
    String ifacePath = String("network.interface.") + vpnInterface;
    JsonDocument argsDoc;
    JsonObject args = argsDoc.to<JsonObject>();
    String response = call(ifacePath.c_str(), "status", args);

    if (response.length() == 0)
    {
        out.up = false;
        out.ipAddr = "N/A";
        out.protocol = "N/A";
        out.uptime = 0;
        out.publicIp = "N/A";
        out.location = "--";
        out.device = "";
        out.rxBytes = 0;
        out.txBytes = 0;
        return;
    }

    JsonDocument respDoc;
    deserializeJson(respDoc, response);
    JsonObject data = respDoc["result"][1];
    out.up = data["up"] | false;
    out.uptime = data["uptime"] | 0;
    out.protocol = data["proto"] | "unknown";
    out.device = data["l3_device"] | data["device"] | "";

    JsonArray addrs = data["ipv4-address"];
    if (addrs.size() > 0)
    {
        const char *addr = addrs[0]["address"];
        out.ipAddr = addr ? String(addr) : "N/A";
    }
    else
        out.ipAddr = "N/A";

    if (out.up && out.device.length() > 0)
    {
        JsonDocument devArgsDoc;
        JsonObject devArgs = devArgsDoc.to<JsonObject>();
        devArgs["name"] = out.device;
        String devResp = call("network.device", "status", devArgs);
        if (devResp.length() > 0)
        {
            JsonDocument devDoc;
            deserializeJson(devDoc, devResp);
            JsonObject stats = devDoc["result"][1]["statistics"];
            out.rxBytes = stats["rx_bytes"] | 0;
            out.txBytes = stats["tx_bytes"] | 0;
        }
    }
    else
    {
        out.rxBytes = 0;
        out.txBytes = 0;
    }

    fetchPublicIpInfo(out);

    Serial.printf("VPN: up=%d ip=%s rx=%llu tx=%llu\n",
                  out.up, out.ipAddr.c_str(), out.rxBytes, out.txBytes);
}

bool OpenWrtClient::vpnUp(const char *vpnInterface)
{
    String ifacePath = String("network.interface.") + vpnInterface;
    JsonDocument argsDoc;
    return callOk(ifacePath.c_str(), "up", argsDoc.to<JsonObject>());
}

bool OpenWrtClient::vpnDown(const char *vpnInterface)
{
    String ifacePath = String("network.interface.") + vpnInterface;
    JsonDocument argsDoc;
    return callOk(ifacePath.c_str(), "down", argsDoc.to<JsonObject>());
}

String formatUptime(unsigned long s)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%luh %lum %lus", s / 3600, (s % 3600) / 60, s % 60);
    return String(buf);
}

String formatBytes(uint64_t bytes)
{
    char buf[16];
    if (bytes < 1024)
        snprintf(buf, sizeof(buf), "%llu B", bytes);
    else if (bytes < 1024ULL * 1024)
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
    else if (bytes < 1024ULL * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else
        snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    return String(buf);
}
