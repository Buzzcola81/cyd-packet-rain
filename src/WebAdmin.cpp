#include "WebAdmin.h"
#include <WebServer.h>
#include <WiFi.h>
#include "Settings.h"

static WebServer server(80);

static String htmlEscape(const String &s)
{
    String out;
    out.reserve(s.length());
    for (size_t i = 0; i < s.length(); ++i)
    {
        char c = s[i];
        switch (c)
        {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        case '\'':
            out += "&#39;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

static String inputRow(const char *label, const char *name,
                       const String &value, const char *type = "text")
{
    String s;
    s += "<label>";
    s += label;
    s += "<input type=\"";
    s += type;
    s += "\" name=\"";
    s += name;
    s += "\" value=\"";
    s += htmlEscape(value);
    s += "\"></label>";
    return s;
}

static void handleRoot()
{
    String body;
    body += "<!doctype html><html><head><meta charset=\"utf-8\">";
    body += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    body += "<title>CYD Packet Rain</title>";
    body += "<style>"
            "body{background:#000;color:#40ff80;font-family:Consolas,monospace;"
            "max-width:560px;margin:0 auto;padding:16px;}"
            "h1{color:#00ff66;border-bottom:1px solid #0a3a1a;padding-bottom:8px;}"
            "h2{color:#00ff66;margin-top:24px;font-size:1.05em;}"
            "label{display:block;margin:10px 0;font-size:.9em;}"
            "input{display:block;width:100%;box-sizing:border-box;margin-top:4px;"
            "padding:8px;background:#031f0b;color:#80ffa0;border:1px solid #00b040;"
            "border-radius:4px;font-family:inherit;}"
            "button{margin-top:18px;padding:10px 18px;background:#0a3a1a;"
            "color:#80ffa0;border:1px solid #00ff66;border-radius:4px;"
            "font-family:inherit;cursor:pointer;font-size:1em;}"
            "button:hover{background:#0f5024;}"
            ".note{color:#208030;font-size:.8em;margin-top:8px;}"
            "</style></head><body>";
    body += "<h1>// CYD PACKET RAIN</h1>";
    body += "<form method=\"POST\" action=\"/save\">";

    body += "<h2>WiFi</h2>";
    body += inputRow("SSID", "wifi_ssid", Settings::wifiSsid);
    body += inputRow("Password", "wifi_pass", Settings::wifiPassword, "password");

    body += "<h2>OpenWrt Router</h2>";
    body += inputRow("Host (IP or hostname)", "ow_host", Settings::openwrtHost);
    body += inputRow("Username", "ow_user", Settings::openwrtUser);
    body += inputRow("Password", "ow_pass", Settings::openwrtPass, "password");
    body += inputRow("VPN interface", "ow_vpn", Settings::vpnInterface);

    body += "<h2>Weather (OpenWeatherMap)</h2>";
    body += inputRow("API key", "owm_key", Settings::owmApiKey, "password");
    body += inputRow("Latitude", "owm_lat", Settings::owmLat);
    body += inputRow("Longitude", "owm_lon", Settings::owmLon);
    body += inputRow("Label", "owm_label", Settings::owmLabel);

    body += "<button type=\"submit\">Save &amp; Reboot</button>";
    body += "<p class=\"note\">Device will reboot after saving so the new settings take effect.</p>";
    body += "</form></body></html>";

    server.send(200, "text/html", body);
}

static void handleSave()
{
    if (server.hasArg("wifi_ssid"))
        Settings::wifiSsid = server.arg("wifi_ssid");
    if (server.hasArg("wifi_pass"))
        Settings::wifiPassword = server.arg("wifi_pass");
    if (server.hasArg("ow_host"))
        Settings::openwrtHost = server.arg("ow_host");
    if (server.hasArg("ow_user"))
        Settings::openwrtUser = server.arg("ow_user");
    if (server.hasArg("ow_pass"))
        Settings::openwrtPass = server.arg("ow_pass");
    if (server.hasArg("ow_vpn"))
        Settings::vpnInterface = server.arg("ow_vpn");
    if (server.hasArg("owm_key"))
        Settings::owmApiKey = server.arg("owm_key");
    if (server.hasArg("owm_lat"))
        Settings::owmLat = server.arg("owm_lat");
    if (server.hasArg("owm_lon"))
        Settings::owmLon = server.arg("owm_lon");
    if (server.hasArg("owm_label"))
        Settings::owmLabel = server.arg("owm_label");

    Settings::saveAll();

    String body;
    body += "<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta http-equiv=\"refresh\" content=\"6;url=/\">"
            "<title>Saved</title>"
            "<style>body{background:#000;color:#40ff80;font-family:Consolas,monospace;"
            "text-align:center;padding-top:80px;}h1{color:#00ff66;}</style></head><body>";
    body += "<h1>Settings saved</h1>";
    body += "<p>Rebooting in 2 seconds...</p>";
    body += "<p>This page will reload automatically.</p>";
    body += "</body></html>";
    server.send(200, "text/html", body);
    delay(500);
    server.client().flush();
    delay(1500);
    ESP.restart();
}

void WebAdmin::begin()
{
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound([]()
                      { server.send(404, "text/plain", "Not found"); });
    server.begin();
}

void WebAdmin::handle()
{
    server.handleClient();
}
