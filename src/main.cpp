#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

/*
 Embedded LED facilities
 Continus = AccessPoint waitting a wifi host selection
 blinking = Wait Station connection is OK
 */
#define EspLedBlue 2
/*
 Button BOOT on ESP32 is pressed more than 3 seconds
 reset all wifi_host selection
 */
#define ButtonBoot 0

#define optAP2Host 0
#define optStaOK   1
#define optHostRST 2

// Debug facilities
#define DBX_EVT  false
#define DBX_MSG  false
#define DBX_WIFI true

#define MAX_STA_WAIT 40  // One loop take 0.5 sec

// Access point Initialize
const char* addressIpAP             = "192.168.4.1";
const char* ssid                    = "ESP32-Dudu";
const char* password                = "123456789";
const char* ntpServer               = "pool.ntp.org";
const long  gmtOffset_sec           = 3600;
const int   daylightOffset_sec      = 3600;
static volatile bool wifi_stationok = false;
static volatile bool wifi_accessPts = false;
IPAddress apIP;                // AccessPoint IP address
Preferences preferences;       // The NVM storage
String headerIn;               // Html Header Input
String htmlSsidList;           // Html selector Wifi list
String wifiSSID;               // The SSID use
String wifiPAWD;               // Accosiate password
int flashLed = 0;              // current loop into MAX_STA_WAIT
bool rebootASAP = false;       // reboot ESP32 at next cycle.
uint32_t previousMillis = 0;   // Last milli-second
uint8_t seconds = 0;           // Seconds [0-59]
uint8_t incBtBoot = 0;         // ButtonBoot pressed during time in seconds
WiFiServer server(80);         // Set web server port number to 80
struct tm timeinfo;            // time struct
const char * EVENT_ID[26] = {
  "SYSTEM_EVENT_WIFI_READY",
  "SYSTEM_EVENT_SCAN_DONE",
  "SYSTEM_EVENT_STA_START",
  "SYSTEM_EVENT_STA_STOP",
  "SYSTEM_EVENT_STA_CONNECTED",
  "SYSTEM_EVENT_STA_DISCONNECTED",
  "SYSTEM_EVENT_STA_AUTHMODE_CHANGE",
  "SYSTEM_EVENT_STA_GOT_IP",
  "SYSTEM_EVENT_STA_LOST_IP",
  "SYSTEM_EVENT_STA_WPS_ER_SUCCESS",
  "SYSTEM_EVENT_STA_WPS_ER_FAILED",
  "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT",
  "SYSTEM_EVENT_STA_WPS_ER_PIN",
  "SYSTEM_EVENT_AP_START",
  "SYSTEM_EVENT_AP_STOP",
  "SYSTEM_EVENT_AP_STACONNECTED",
  "SYSTEM_EVENT_AP_STADISCONNECTED",
  "SYSTEM_EVENT_AP_STAIPASSIGNED",
  "SYSTEM_EVENT_AP_PROBEREQRECVED",
  "SYSTEM_EVENT_GOT_IP6",
  "SYSTEM_EVENT_ETH_START",
  "SYSTEM_EVENT_ETH_STOP",
  "SYSTEM_EVENT_ETH_CONNECTED",
  "SYSTEM_EVENT_ETH_DISCONNECTED",
  "SYSTEM_EVENT_ETH_GOT_IP",
  "SYSTEM_EVENT_MAX"
};

const String httpHeaders[7] = {
  "HTTP/1.1 200 OK",
  "Content-type:text/html",
  "Connection: close",
  "",
  "<!DOCTYPE html><html>",
  "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
  "<link rel=\"icon\" href=\"data:,\">"
};
const String httpNoConnected_1[15] = {
  "<style>",
  "html{font-family:Helvetica;display:inline-block;margin:0 auto;text-align:center;background-color:#eee;}",
  "div{padding:.3em;}",
  "form{margin:0 auto;width:250px;padding:.5em;border:1px solid #fff;border-radius:1em;}",
  "select{width:150px;}",
  "label{display:inline-block;width:80px;text-align:right;}",
  "input{width:150px;box-sizing:border-box;border:1px solid #999;}",
  "button{width: 234;padding:.5em;font-family:Arial;font-size:15px;font-weight:bold;border-radius:1em;}",
  "</style>",
  "</head> <body><h1>ESP32 Web Server</h1>",
  "<strong>ESP32-Dudu Host Wi-Fi selector :</strong>",
  "<div> <div/>",
  "<form action='ap' method='post'>",
  "<div>",
  "<label for'wifi'>Wi-Fi :</label>"
};
const String httpNoConnected_2[8] = {
  "</div><div>",
  "<label>Password :</label>",
  "<input type='text' name='password'/>",
  "</div><div>",
  "<button type='submit'>Connect on selected Wi-Fi</button>",
  "</div>",
  "</form></body></html>",
  ""
};

// Decode specific char into url (Space become a +, ...)
String urlDecode(const String& text) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len)	{
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len))	{
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);
      decodedChar = strtol(temp, NULL, 16);
    }	else {
      if (encodedChar == '+')	 decodedChar = ' ';
      else  decodedChar = encodedChar;
    }
    decoded += decodedChar;
  }
  return decoded;
}

// Time HH:MM.ss
String getTime() {
  char temp[10];
  sprintf(temp, "%02d:%02d:%02d", timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec );
  return String(temp);
}

// Date as europeen format
String getDate(){
  char temp[20];
  sprintf(temp, "%02d/%02d/%04d %02d:%02d:%02d",
          timeinfo.tm_mday, (timeinfo.tm_mon+1), (1900+timeinfo.tm_year),  timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec );
  return String(temp);
}

// Just some event are used see
// https://github.com/espressif/esp-idf/blob/master/components/esp32/include/esp_event.h
// wifi.h enum
void WiFiEvent(WiFiEvent_t event) {
  if (DBX_EVT) {
    if (event < 26) {
      Serial.println(EVENT_ID[event]);
    } else  {
      Serial.print(">>WiFiEvent WiFiEvent_t:"); Serial.println(event, DEC);
    }
  }
  switch (event) {
    case SYSTEM_EVENT_AP_START:
      WiFi.softAPsetHostname(ssid);
      break;
    case SYSTEM_EVENT_STA_START:
    //  WiFi.softAPsetHostname(ssid);
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); //init and get the time
      break;
    default:
    break;
  }
}

// Scanning SSID available
void wifiList () {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks(); // WiFi.scanNetworks will return the number of networks found
  if (n == 0) {
    htmlSsidList = "no networks found";
  } else {
    htmlSsidList = "<select name=\"wifi\">";
    for (unsigned int i = 0; i < n; ++i) { // Store SSID for each network found
      htmlSsidList = htmlSsidList + "<option value=\""+WiFi.SSID(i)+"\">";
      htmlSsidList = htmlSsidList + (i+1) + ": " + WiFi.SSID(i) + "</option>";
    }
    htmlSsidList = htmlSsidList + "</select>";
  }
}

// Connect to Wi-Fi network with SSID and password
void  setAccessPoint() {
  if (DBX_WIFI) Serial.println();
  wifi_accessPts = true;
  wifi_stationok = false;
  digitalWrite(EspLedBlue, HIGH);
  wifiList();                  // Read list of SSID
  WiFi.softAP(ssid, password); // Remove the password parameter, if you want the AP (Access Point) to be open
  delay (100 );
  if (DBX_WIFI) {
    Serial.print("Select WiFi Access Point: '"); Serial.print(ssid);
    Serial.print("' and open your browser on: ");
    Serial.print("http://");  Serial.println(WiFi.softAPIP());
  }
  server.begin();
}

void htmlHeader(WiFiClient client) {
  unsigned int i;
  for (i=0; i<7; i++)
    client.println(httpHeaders[i]);
}

void htmlNotConnected(WiFiClient client) {
  htmlHeader(client);
  unsigned int i;
  for (i=0; i<15; i++)
    client.println(httpNoConnected_1[i]);
  client.println(htmlSsidList); // ADD Wi-Fi list selector
  for (i=0; i<8; i++)
    client.println(httpNoConnected_2[i]);
  if (DBX_MSG) Serial.println("End htmlNotConnected HTML");
}

void htmlConnectedOk(WiFiClient client, int opt) {
  htmlHeader(client);
  client.println("<style>html{font-family:Helvetica;display:inline-block;margin: 0px auto;text-align:center;background-color:#eee;}");
  client.println("p {background-color:#4CAF50;border:none;color:white; padding: 16px 40px;} ");
  client.println(".button {background-color:#e4685d;border:none;color:blue;border-radius:4px;font-family:Arial;font-size:15px;font-weight:bold;padding: 6px 15px;");
  client.println("</style></head><body>");
  if (opt!=optHostRST) {
    client.println("<h1>ESP32 Web Server</h1><p><strong>ESP32 is connected at <b><br>");
    client.println(wifiSSID);
  }
  client.println("</b></strong></p>");
  if (opt==optStaOK) {
    client.println("<br>Station is running on IP: <b>");
    client.println(WiFi.localIP());
    client.println("</b> NTP Time:"); client.println(getDate());
  }
  client.println("<form action='sta' method='post'>");

  client.println("<br><hr><form action='sta' method='post'>");
  client.println("<button type='submit' name='reset' value='Reset'>Change WiFi</button>");
  client.println("</form>");

  client.println("<br><hr>Select WiFi Access Point: <b>");
  client.println( ssid);
  client.println("</b> and open your browser on IP:<b> ");
  client.println(addressIpAP);
  client.println("</body></html>");
  client.println();
  if (DBX_MSG)  Serial.println("End htmlConnectedOk HTML");
}

void putPreferences(String ssid, String pwd) {
  preferences.clear();
  preferences.begin("wifi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", pwd);
  preferences.end();
}

void connectToHost() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_stationok = false;
  if (DBX_WIFI)  { Serial.print("Try to connect WIFI: '"); Serial.print(wifiSSID);Serial.print("' PWD :"); Serial.println("wifiPAWD"); }
  flashLed=0;
  WiFi.begin(wifiSSID.c_str(), wifiPAWD.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(250),
    digitalWrite(EspLedBlue, HIGH);
    delay(250);
    digitalWrite(EspLedBlue, LOW);
    if (DBX_WIFI) Serial.print(".");
    if (flashLed++ > MAX_STA_WAIT) {
      if (DBX_WIFI) Serial.println(".");
      setAccessPoint(); // Start accesPoint because wifiSSID host is not availabale
      return;
    }
  }
  flashLed = 0;
  digitalWrite(EspLedBlue, LOW);
  if (DBX_WIFI) {
    Serial.println("");
    Serial.print("WiFi connected to [Host][IP]:[");Serial.print(wifiSSID);
    Serial.print("][]");Serial.print(WiFi.localIP());Serial.println("]");
  }
  server.begin();
  wifi_stationok = true;
}

// loopWifi in case of AP
void fromWiFiAP (WiFiClient client) {
  // wifiSSID is selected Try  connection
  if (headerIn.indexOf("POST /ap HTTP/") >= 0) {
    int sw = headerIn.indexOf("wifi=");
    int e = headerIn.indexOf("&password=");
    wifiSSID = urlDecode(headerIn.substring(sw+5, e));
    wifiPAWD = urlDecode(headerIn.substring(e+10));
    htmlConnectedOk(client, optAP2Host);      // Answer connection OK
    client.stop();
    putPreferences(wifiSSID, wifiPAWD);
    rebootASAP = true;
  } else {
    htmlNotConnected(client);
  }
}

// LoopWifi in mode STATION
void fromWiFiSTA(WiFiClient client) {
  int se = headerIn.indexOf("POST /sta HTTP/");
  int sw = headerIn.indexOf("reset=Reset");
  if (se >= 0 && sw >=0 ) {  // Back to AP for wifi selection
    putPreferences("none","none");
    htmlConnectedOk(client, optHostRST);
    client.stop();
    rebootASAP = true;
  } else {
    htmlConnectedOk(client, optStaOK);  // Normal page
  }
}

// For each client get correct HTTP message
void loopWifi() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    if (DBX_MSG) Serial.println("New client");
    if (client.connected()) {
      unsigned long timeout = millis();
      while ( client.available() == 0 ) {  // wait a correct message
        if (millis() - timeout > 5000) {
          if (DBX_MSG) Serial.println("Timeout client");
          client.stop();
          return;
        }
      }
      while ( client.available() > 0 ) {
        char c = client.read();
        headerIn += c;      // Concate char into headerIn
      }
      if (headerIn.length() > 0 ) {
        if (DBX_MSG) Serial.println(headerIn);
        if (wifi_stationok == false)
          fromWiFiAP(client);           // AccessPoint html fom for wifi selection
        else
          fromWiFiSTA(client);          // Wifi Host selection is OK
        headerIn = "";                  // Clear the headerIn variable
      }
    }
    if (DBX_MSG) Serial.println("End client");
  }
}

void setup() {
  Serial.begin(115200);

  // Wait a little bit for serial connection
  delay(5000);

  // Set MAC addess & AP IP
  byte new_mac[8] = {0x30,0xAE,0xA4,0x90,0xFD,0xC8}; // Pif
  //  byte new_mac[8] = {0x8C,0x29,0x37,0x43,0xCD,0x46}; // Dudu
  esp_base_mac_addr_set(new_mac); // Wifi_STA=mac  wifi_AP=mac+1  BT=mac+2
  apIP.fromString(addressIpAP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   // subnet FF FF FF 00

  // Ethernet.begin(new_mac);
  previousMillis = millis();

  // event WiFi
  WiFi.onEvent(WiFiEvent);

  // Set pin mode
  pinMode(EspLedBlue, OUTPUT);

  // Get ssid pwd into flash ram
  preferences.begin("wifi", false);
  wifiSSID =  preferences.getString("ssid", "none");       // NVS key ssid
  wifiPAWD =  preferences.getString("password", "none");   // NVS key password
  preferences.end();

  // Start AP and / or  STA
  if (DBX_MSG) {Serial.print("NVM ssid=");Serial.println(wifiSSID);}
  if (wifiSSID.indexOf("none")!=0) {
    connectToHost();  // Try to connect to wifiSSID in NVM stored
  } else {
    if (wifi_stationok==false)
      setAccessPoint(); // Start accesPoint because wifiSSID host is not availabale
  }

  // Mac address
  if (DBX_MSG)  {
    Serial.print("Addr macAP : "); Serial.println( WiFi.softAPmacAddress().c_str() ); // Show mac address
    Serial.print("Addr macSTA: "); Serial.println( WiFi.macAddress().c_str() ); // Show mac address
  }
}

void loop() {

  // Main loop from http decoded function
  loopWifi();

  // Main loop every new second elapes
  if ( millis() - previousMillis > 999L) {

    // rebootASAP detected
    if (rebootASAP)
      ESP.restart();

    // Get time
    previousMillis = millis();

    // STATION is OK
    if (wifi_stationok) {

      // Close Access Point.
      if ( wifi_accessPts ) {
        if (DBX_MSG) Serial.println("Disconnect Access Point.");
        WiFi.softAPdisconnect(true);
        wifi_accessPts = false;
      }

      // Debug message for test
      if (DBX_MSG) {
        if (!getLocalTime(&timeinfo))
          Serial.println("Failed to obtain time");
        else
          Serial.print("\r");Serial.print(getDate());
      }
    }

    // Button BOOT is pressed more than 3 sec.
    int btBoot = digitalRead(ButtonBoot);
    if (btBoot==LOW) {
      incBtBoot++;
      if (DBX_MSG) Serial.println("btBoot pressed.");
      if (incBtBoot>3) {
        putPreferences("none","none");
        // setAccessPoint();
        rebootASAP = true;
        //delay(50);
        //ESP.restart();
      }
    } else {
      incBtBoot = 0;
    }

    // Timer
    if (seconds++ == 59)
      seconds = 0;
  }
}
