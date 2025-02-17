/*********************************************************************
 * ESP32-CAM + Webinterface + WireGuard + TLS Example
 *
 * In this sketch:
 *  1) We provide a Web interface (Port 80) to configure:
 *     - WLAN (SSID, password)
 *     - API credentials (username, password, deviceName) for vpn23.com
 *  2) The sketch will try to connect to Wi-Fi.
 *  3) If Wi-Fi is successful, it performs an HTTPS POST to
 *     "https://vpn23.com/login" with your credentials to obtain a JWT.
 *  4) Then it performs an HTTPS GET to
 *     "https://vpn23.com/clients/name/<deviceName>/config" (using the JWT),
 *     parses the WireGuard settings, and starts WireGuard (via
 *     WireGuard-ESP32 library).
 *  5) The ESP32-CAM runs a local camera server on port 80:
 *     - "/" shows a basic page with a live preview (MJPEG stream at "/stream")
 *     - "/config" shows a form to edit Wi-Fi + API creds
 *     - "/stream" returns an MJPEG live stream
 *
 * Required libraries:
 *   - ArduinoJson
 *   - WireGuard-ESP32 (https://github.com/ciniml/WireGuard-ESP32-Arduino)
 *   - esp32-camera (included in Arduino ESP32 core or as separate)
 *   - WiFiClientSecure.h (for HTTPS)
 *   - Preferences.h (for storing settings in NVS)
 *
 * NOTE:
 *   - If you want strict TLS verification, you should set the correct system
 *     time via NTP (configTime(...)).
 *   - The Root CA below is an example (GTS Root R4). Replace it if your
 *     server uses a different CA chain.
 *   - For fully restricting camera access to the WireGuard interface only,
 *     consider firewall or interface-binding measures. By default, the web
 *     server listens on all interfaces.
 *********************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WireGuard-ESP32.h>
#include "esp_camera.h"

// -------------------- PIN DEFINITIONS FOR AI-THINKER ESP32-CAM --------------------
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// Hardcoded endpoints
static const char* LOGIN_URL    = "https://vpn23.com/login";
static const char* CONFIG_URL_1 = "https://vpn23.com/clients/name/";
static const char* CONFIG_URL_2 = "/config";

// Example Root-CA (GTS Root R4) - FULL certificate included here.
// Replace with the actual CA if needed.
static const char* rootCACert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDejCCAmKgAwIBAgIQf+UwvzMTQ77dghYQST2KGzANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIzMTEx
NTAzNDMyMVoXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFI0
MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE83Rzp2iLYK5DuDXFgTB7S0md+8Fhzube
Rr1r1WEYNa5A3XP3iZEwWus87oV8okB2O6nGuEfYKueSkWpz6bFyOZ8pn6KY019e
WIZlD6GEZQbR3IvJx3PIjGov5cSr0R2Ko4H+MIH8MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwDwYDVR0TAQH/BAUwAwEB/zAd
BgNVHQ4EFgQUgEzW63T/STaj1dj8tT7FavCUHYwwHwYDVR0jBBgwFoAUYHtmGkUN
l8qJUC99BM00qP/8/UswNgYIKwYBBQUHAQEEKjAoMCYGCCsGAQUFBzAChhpodHRw
Oi8vaS5wa2kuZ29vZy9nc3IxLmNydDAtBgNVHR8EJjAkMCKgIKAehhxodHRwOi8v
Yy5wa2kuZ29vZy9yL2dzcjEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqG
SIb3DQEBCwUAA4IBAQAYQrsPBtYDh5bjP2OBDwmkoWhIDDkic574y04tfzHpn+cJ
odI2D4SseesQ6bDrarZ7C30ddLibZatoKiws3UL9xnELz4ct92vID24FfVbiI1hY
+SW6FoVHkNeWIP0GCbaM4C6uVdF5dTUsMVs/ZbzNnIdCp5Gxmx5ejvEau8otR/Cs
kGN+hr/W5GvT1tMBjgWKZ1i4//emhA1JG1BbPzoLJQvyEotc03lXjTaCzv8mEbep
8RqZ7a2CPsgRbuvTPBwcOMBBmuFeU88+FSBX6+7iP0il8b4Z0QFqIwwMHfs/L6K1
vepuoxtGzi4CZ68zJpiq1UvSqTbFJjtbD4seiMHl
-----END CERTIFICATE-----
)EOF";

// Structure for storing config
struct Config {
  String wifiSSID;
  String wifiPass;
  String apiUser;
  String apiPass;
  String deviceName;

  // From WireGuard server
  String privateKey;
  String address;
  String dns;
  String endpoint;
  String publicKey;
  String presharedKey;
};

// Globals
Config configData;
Preferences preferences;
WebServer server(80);
WireGuard wg;

/************************************************
 * HTML-Escape Helper
 ***********************************************/
String htmlEscape(const String &s) {
  String out;
  for (char c : s) {
    switch(c) {
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '&': out += "&amp;"; break;
      case '"': out += "&quot;"; break;
      default: out += c; break;
    }
  }
  return out;
}

/************************************************
 * Preferences: load & save
 ***********************************************/
void loadConfig() {
  preferences.begin("vpn23cam", true);
  configData.wifiSSID   = preferences.getString("wifiSSID", "");
  configData.wifiPass   = preferences.getString("wifiPass", "");
  configData.apiUser    = preferences.getString("apiUser", "");
  configData.apiPass    = preferences.getString("apiPass", "");
  configData.deviceName = preferences.getString("devName", "ESP32CAM");

  configData.privateKey   = preferences.getString("privKey", "");
  configData.address      = preferences.getString("address", "");
  configData.dns          = preferences.getString("dns", "");
  configData.endpoint     = preferences.getString("endpoint", "");
  configData.publicKey    = preferences.getString("pubKey", "");
  configData.presharedKey = preferences.getString("psk", "");
  preferences.end();
}

void saveConfig() {
  preferences.begin("vpn23cam", false);
  preferences.putString("wifiSSID",   configData.wifiSSID);
  preferences.putString("wifiPass",   configData.wifiPass);
  preferences.putString("apiUser",    configData.apiUser);
  preferences.putString("apiPass",    configData.apiPass);
  preferences.putString("devName",    configData.deviceName);

  preferences.putString("privKey",    configData.privateKey);
  preferences.putString("address",    configData.address);
  preferences.putString("dns",        configData.dns);
  preferences.putString("endpoint",   configData.endpoint);
  preferences.putString("pubKey",     configData.publicKey);
  preferences.putString("psk",        configData.presharedKey);
  preferences.end();
}

/************************************************
 * Wi-Fi Connection
 ***********************************************/
bool connectWiFi(const String &ssid, const String &pass) {
  if (ssid.isEmpty() || pass.isEmpty()) {
    Serial.println("[WIFI] SSID or password is empty");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.printf("[WIFI] Connecting to %s...\n", ssid.c_str());

  unsigned long start = millis();
  const unsigned long timeout = 15000;
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Connected. IP: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("\n[WIFI] Connection failed.");
  return false;
}

/************************************************
 * HTTPS POST to /login => JWT
 ***********************************************/
String getJwtToken() {
  if (WiFi.status() != WL_CONNECTED) return "";

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, LOGIN_URL)) {
    Serial.println("[API] http.begin() failed");
    return "";
  }

  StaticJsonDocument<256> doc;
  doc["username"]   = configData.apiUser;
  doc["password"]   = configData.apiPass;
  doc["deviceName"] = configData.deviceName;
  String body;
  serializeJson(doc, body);

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(body);
  if (code == 200 || code == 201) {
    String resp = http.getString();
    http.end();
    Serial.println("[API] Login response: " + resp);

    StaticJsonDocument<512> respDoc;
    DeserializationError err = deserializeJson(respDoc, resp);
    if (!err) {
      const char* token = respDoc["token"];
      if (token) {
        Serial.println("[API] JWT received!");
        return String(token);
      }
    }
  } else {
    Serial.printf("[API] Login failed. code=%d\n", code);
    http.end();
  }
  return "";
}

/************************************************
 * GET /clients/name/<deviceName>/config => WG config
 ***********************************************/
bool fetchWireGuardConfig(const String &jwt) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String(CONFIG_URL_1) + configData.deviceName + String(CONFIG_URL_2);
  Serial.println("[WG] Fetching from: " + url);

  WiFiClientSecure client;
  client.setCACert(rootCACert);

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[WG] http.begin() failed!");
    return false;
  }
  if (!jwt.isEmpty()) {
    String bearer = "Bearer " + jwt;
    http.addHeader("Authorization", bearer);
  }

  int code = http.GET();
  if (code == 200) {
    String resp = http.getString();
    http.end();
    Serial.println("[WG] Response: " + resp);

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (!err) {
      configData.privateKey   = doc["private_key"]   | "";
      configData.address      = doc["address"]       | "";
      configData.dns          = doc["dns"]           | "";
      configData.endpoint     = doc["endpoint"]      | "";
      configData.publicKey    = doc["public_key"]    | "";
      configData.presharedKey = doc["preshared_key"] | "";
      saveConfig();
      return true;
    } else {
      Serial.println("[WG] JSON parse error");
    }
  } else {
    Serial.printf("[WG] GET failed, code=%d\n", code);
    http.end();
  }
  return false;
}

/************************************************
 * Start WireGuard
 ***********************************************/
bool startWireGuard() {
  if (configData.privateKey.isEmpty() || configData.publicKey.isEmpty()) {
    Serial.println("[WG] Missing private/public key!");
    return false;
  }
  WGConfig c;
  c.private_key   = configData.privateKey;
  c.address       = configData.address;
  c.dns           = configData.dns;
  c.endpoint      = configData.endpoint;
  c.public_key    = configData.publicKey;
  c.preshared_key = configData.presharedKey;

  // e.g. c.allowedIPs.push_back("0.0.0.0/0");
  // c.keep_alive = 25;

  bool ok = wg.begin(c);
  if (ok) {
    Serial.println("[WG] WireGuard started!");
  } else {
    Serial.println("[WG] Failed to start WireGuard.");
  }
  return ok;
}

/************************************************
 * Initialize camera
 ***********************************************/
static void initCamera() {
  camera_config_t cam_cfg;
  cam_cfg.ledc_channel = LEDC_CHANNEL_0;
  cam_cfg.ledc_timer   = LEDC_TIMER_0;
  cam_cfg.pin_d0       = Y2_GPIO_NUM;
  cam_cfg.pin_d1       = Y3_GPIO_NUM;
  cam_cfg.pin_d2       = Y4_GPIO_NUM;
  cam_cfg.pin_d3       = Y5_GPIO_NUM;
  cam_cfg.pin_d4       = Y6_GPIO_NUM;
  cam_cfg.pin_d5       = Y7_GPIO_NUM;
  cam_cfg.pin_d6       = Y8_GPIO_NUM;
  cam_cfg.pin_d7       = Y9_GPIO_NUM;
  cam_cfg.pin_xclk     = XCLK_GPIO_NUM;
  cam_cfg.pin_pclk     = PCLK_GPIO_NUM;
  cam_cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cam_cfg.pin_href     = HREF_GPIO_NUM;
  cam_cfg.pin_sscb_sda = SIOD_GPIO_NUM;
  cam_cfg.pin_sscb_scl = SIOC_GPIO_NUM;
  cam_cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cam_cfg.pin_reset    = RESET_GPIO_NUM;
  cam_cfg.xclk_freq_hz = 20000000;
  cam_cfg.pixel_format = PIXFORMAT_JPEG;

  // For streaming, QVGA or lower might be smoother
  cam_cfg.frame_size   = FRAMESIZE_QVGA;
  cam_cfg.jpeg_quality = 12;
  cam_cfg.fb_count     = 2;

  esp_err_t err = esp_camera_init(&cam_cfg);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init failed 0x%x\n", err);
    delay(3000);
    ESP.restart();
  }
  Serial.println("[CAM] Initialized");
}

/************************************************
 * MJPEG Stream Handler
 ***********************************************/
static const char* BOUNDARY = "myboundary";
static const char* MJPEG_HEADER = "multipart/x-mixed-replace;boundary=myboundary";
static const char* FRAME_BOUNDARY = "--myboundary\r\n";
static const char* CONTENT_TYPE_JPEG = "Content-Type: image/jpeg\r\nContent-Length: ";

void handleStream() {
  WiFiClient client = server.client();
  if (!client) return;

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, MJPEG_HEADER);

  while (true) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("[CAM] fb_get failed");
      break;
    }
    // Write boundary
    client.write(FRAME_BOUNDARY, strlen(FRAME_BOUNDARY));

    // Header with size
    char sizeHeader[48];
    sprintf(sizeHeader, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(sizeHeader, strlen(sizeHeader));

    // Write image
    client.write((const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);

    if (!client.connected()) break;

    // small delay
    delay(25);
  }
}

// Root page
void handleRoot() {
  String html = "<html><head><title>ESP32-CAM WireGuard</title></head><body>";
  html += "<h1>ESP32-CAM + vpn23.com</h1>";

  // Wi-Fi status
  html += "<p><b>Wi-Fi:</b> ";
  if (WiFi.status() == WL_CONNECTED) {
    html += "Connected, IP=" + WiFi.localIP().toString();
  } else {
    html += "<span style='color:red'>Not connected</span>";
  }
  html += "</p>";

  // WG status
  html += "<p><b>WireGuard:</b> ";
  if (wg.isInitialized()) {
    html += "<span style='color:green'>Active</span>";
  } else {
    html += "<span style='color:red'>Inactive</span>";
  }
  html += "</p>";

  // MJPEG preview
  html += "<p><img src='/stream' style='max-width:320px;' /></p>";
  html += "<p><a href='/config'>Configure</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Config page
void handleConfigPage() {
  String html = "<html><head><title>Config</title></head><body>";
  html += "<h1>Configuration</h1>";
  html += "<form method='POST' action='/saveConfig'>";

  // Wi-Fi
  html += "<h3>Wi-Fi</h3>";
  html += "SSID: <input name='wifiSSID' value='" + htmlEscape(configData.wifiSSID) + "'/><br/>";
  html += "Password: <input type='password' name='wifiPass' value='" + htmlEscape(configData.wifiPass) + "'/><br/>";

  // API / device
  html += "<h3>API / Device</h3>";
  html += "Username: <input name='apiUser' value='" + htmlEscape(configData.apiUser) + "'/><br/>";
  html += "Password: <input type='password' name='apiPass' value='" + htmlEscape(configData.apiPass) + "'/><br/>";
  html += "DeviceName: <input name='devName' value='" + htmlEscape(configData.deviceName) + "'/><br/>";

  html += "<br/><input type='submit' value='Save'/>";
  html += "</form>";
  html += "<p><a href='/'>Back</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Save config
void handleSaveConfig() {
  if (server.hasArg("wifiSSID"))   configData.wifiSSID   = server.arg("wifiSSID");
  if (server.hasArg("wifiPass"))   configData.wifiPass   = server.arg("wifiPass");
  if (server.hasArg("apiUser"))    configData.apiUser    = server.arg("apiUser");
  if (server.hasArg("apiPass"))    configData.apiPass    = server.arg("apiPass");
  if (server.hasArg("devName"))    configData.deviceName = server.arg("devName");

  saveConfig();

  String msg = "<html><body><h1>Saved!</h1>";
  msg += "<p><a href='/'>Back</a></p></body></html>";
  server.send(200, "text/html", msg);
}

// SETUP
void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1) Load config
  loadConfig();

  // 2) Initialize camera
  initCamera();

  // 3) Try Wi-Fi
  bool wifiOk = connectWiFi(configData.wifiSSID, configData.wifiPass);
  if (!wifiOk) {
    // Start AP fallback
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config", "12345678");
    Serial.println("[WIFI] AP active. IP=" + WiFi.softAPIP().toString());
  } else {
    // Optionally get time from NTP for TLS
    // configTime(0,0,"pool.ntp.org");
    // delay(2000);

    // JWT & WG
    String token = getJwtToken();
    if (!token.isEmpty()) {
      bool gotWG = fetchWireGuardConfig(token);
      if (gotWG) {
        startWireGuard();
      }
    }
  }

  // 4) WebServer
  server.on("/", HTTP_GET, handleRoot);
  server.on("/config", HTTP_GET, handleConfigPage);
  server.on("/saveConfig", HTTP_POST, handleSaveConfig);
  server.on("/stream", HTTP_GET, handleStream);
  server.onNotFound([](){
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println("[WEB] Server started on port 80");
}

// LOOP
void loop() {
  server.handleClient();
  delay(10);
}
