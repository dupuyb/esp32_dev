#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <Preferences.h>

#define EspLedBlue 2
#define ButtonBoot 0

#define optAP2Host 0
#define optStaOK  1
#define optHostRST 2

#define DBX_EVT false

// Access point Initialize
const char* ipAP     = "192.168.4.1";
const char* ssid     = "ESP32-Dudu";
const char* password = "123456789";
static volatile bool wifi_connected = false;
Preferences preferences;     // The NVM storage
String headerIn;             // Html Header Input
String htmlSsidList;         // Html selector Wifi list
String wifiSSID;             // The SSID use
String wifiPAWD;             // Accosiate password
int flashLed = 0;            // Led flashing 3 time = IP is correct
uint32_t previousMillis = 0; // Last milli-second
uint8_t seconds = 0;         // Seconds [0-59]
uint8_t minutes = 0;         // Minutes [0-59]
uint8_t heures  = 0;         // Hours
uint8_t closeAPinSec = 0;    // Close AP
uint8_t incBtBoot = 0;       // ButtonBoot pressed during time in seconds

// Set web server port number to 80
WiFiServer server(80);

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
			else  decodedChar = encodedChar; // normal ascii char
		}
		decoded += decodedChar;
	}
	return decoded;
}

String getMacAddress() {
	//static char buffer[25];
  //byte mac[6];
  //WiFi.macAddress(mac);
	//snprintf (buffer, 24, "%X:%X:%X:%X:%X:%X", mac[5],mac[4],mac[3],mac[2],mac[1],mac[0]);
	return String(WiFi.macAddress().c_str());
  //return buffer;
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
		case SYSTEM_EVENT_WIFI_READY:
	  	if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_WIFI_READY");
	  	break;
    case SYSTEM_EVENT_AP_START:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_START");
			WiFi.softAPsetHostname(ssid);
      break;
    case SYSTEM_EVENT_AP_STOP :
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_STOP");
      break;
    case  SYSTEM_EVENT_AP_STACONNECTED:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_STACONNECTED");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_STADISCONNECTED");
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_STAIPASSIGNED"); // Send html
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
        if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_AP_STA_GOT_I9P6");
        break;
    case SYSTEM_EVENT_STA_START:
      WiFi.softAPsetHostname(ssid);
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_STA_START");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_STA_CONNECTED");
      break;
	  case SYSTEM_EVENT_STA_GOT_IP:
	  	if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_STA_GOT_IP");
			flashLed = 3;
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_STA_DISCONNECTED");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      if (DBX_EVT) Serial.println(">> SYSTEM_EVENT_SCAN_DONE");
      break;
    default:
      if (DBX_EVT) Serial.print(">> default:"); Serial.println(event, DEC);
      break;
  }
}

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

void  setAccessPoint() {       // Connect to Wi-Fi network with SSID and password
	digitalWrite(EspLedBlue, HIGH);
	wifiList();                  // Read list of SSID
  WiFi.softAP(ssid, password); // Remove the password parameter, if you want the AP (Access Point) to be open
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Select WiFi Access Point: '"); Serial.print(ssid);
  Serial.print("' and open your browser on: ");
  Serial.print("http://");  Serial.println(IP);
  server.begin();
}

void htmlHeader(WiFiClient client){
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html><html>");
  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<link rel=\"icon\" href=\"data:,\">");
}

void htmlNotConnected(WiFiClient client){
  // Display the HTML web page
  htmlHeader(client);
  client.println("<style>");
  client.println("html{font-family:Helvetica;display:inline-block;margin:0 auto;text-align:center;background-color:#eee;}");
  client.println("div{padding:.3em;}");
  client.println("form{margin:0 auto;width:250px;padding:.5em;border:1px solid #fff;border-radius:1em;}");
  client.println("select{width:150px;}");
  client.println("label{display:inline-block;width:80px;text-align:right;}");
  client.println("input{width:150px;box-sizing:border-box;border:1px solid #999;}");
  client.println("button{width: 234;padding:.5em;font-family:Arial;font-size:15px;font-weight:bold;border-radius:1em;}");
  client.println("</style>");
  client.println("</head> <body><h1>ESP32 Web Server</h1>");
  client.println("<strong>ESP32-Dudu Host Wi-Fi selector :</strong>");
  client.println("<div> <div/>");
  client.println("<form action='ap' method='get'>");
  client.println("<div>");
  client.println("<label for'wifi'>Wi-Fi :</label>");
  client.println(htmlSsidList); // ADD Wi-Fi list selector
  client.println("</div><div>");
  client.println("<label>Password :</label>");
  client.println("<input type='text' name='password'/>");
  client.println("</div><div>");
  client.println("<button type='submit'>Connect on selected Wi-Fi</button>");
  client.println("</div>");
  client.println("</form></body></html>");
  client.println(); // The HTTP response ends with another blank line
  Serial.println("End htmlNotConnected HTML");
}

void htmlConnectedOk(WiFiClient client, int opt){
  htmlHeader(client);
  client.println("<style>html{font-family:Helvetica;display:inline-block;margin: 0px auto;text-align:center;background-color:#eee;}");
  client.println("p {background-color:#4CAF50;border:none;color:white; padding: 16px 40px;} ");
	client.println(".button {background-color:#e4685d;border:none;color:blue;border-radius:4px;font-family:Arial;font-size:15px;font-weight:bold;padding: 6px 15px;");
  client.println("</style></head><body>");
	if (opt!=optHostRST) {
	  client.println("<h1>ESP32 Web Server</h1><p><strong>ESP32 is connected at <b>><br>");
    client.println(wifiSSID);
  }
  client.println("</b></strong></p>");
	if (opt==optStaOK) {
		client.println("<br>Station is running on IP: <b>");
		client.println(WiFi.localIP());
    client.println("</b> to be modified ...");
	}
//  client.println(); // The HTTP response ends with another blank line
	//if (opt==optStaOK) {
  client.println("<br><hr><a href=\"/reset\"><button class=\"button\">Change WiFi</button></a>");
  //}
	//if (opt==optHostRST || opt==optStaOK) {
	client.println("<br>Select WiFi Access Point: <b>");
	client.println( ssid);
	client.println("</b> and open your browser on IP:<b> ");
	client.println(ipAP);
	//}
	client.println("</body></html>");
//	IPAddress IP = WiFi.localIP();
//	Serial.println(">>>> Local IP: "); Serial.print(IP);Serial.println(" ");
//	client.stop();
}

void putPreferences(String ssid, String pwd) {
	preferences.clear();
	preferences.begin("wifi", false);
	preferences.putString("ssid", ssid);
	preferences.putString("password", pwd);
	preferences.end();
}

void connectToHost() {
	WiFi.disconnect();
	wifi_connected = false;
	Serial.print("Try to connect WIFI: '"); Serial.print(wifiSSID);Serial.print("' PWD :"); Serial.println(wifiPAWD);
	int mxl=0;
	WiFi.begin(wifiSSID.c_str(), wifiPAWD.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (mxl++ > 40) {
			Serial.println(".");
			setAccessPoint(); // Start accesPoint because wifiSSID host is not availabale
		  return;
		}
	}
	// Store new Wifi ssid and password
	putPreferences(wifiSSID, wifiPAWD);
	digitalWrite(EspLedBlue, LOW);
	Serial.println("");
	Serial.print("WiFi connected to [Host][IP]:[");Serial.print(wifiSSID);
	Serial.print("][]");Serial.print(WiFi.localIP());Serial.println("]");
	server.begin();
	closeAPinSec = 3; // 1 == AP stop()
	wifi_connected = true;
}

void fromWiFiAP (WiFiClient client) {
	if (headerIn.indexOf("GET /ap?wifi=") >= 0) { // wifiSSID is selected Try  connection
		int e = headerIn.indexOf("&password=");
		int s = headerIn.indexOf(" HTTP/");
		wifiSSID = urlDecode(headerIn.substring(13, e));
		wifiPAWD = urlDecode(headerIn.substring(e+10, s));
		htmlConnectedOk(client, optAP2Host);      // Answer connection OK
		delay(500);
		connectToHost();                          // Store new Wifi ssid, password and connect
	} else {
		htmlNotConnected(client);
	}
}

void fromWiFiSTA(WiFiClient client) {         // Loop when Esp is connected to Wifi
	if (headerIn.indexOf("GET /reset") >= 0) {  // Back to AP for wifi selection
      putPreferences("none","none");
			htmlConnectedOk(client, optHostRST);
			Serial.println("REBOOT SENDING BY HTTP");
			client.stop();
			delay(500);
			ESP.restart();
	} else {
		htmlConnectedOk(client, optStaOK);
	}
}

void loopWifi() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
		Serial.println("New client");
		if (client.connected()) {
	    unsigned long timeout = millis();
      while ( client.available() == 0 ) {  // wait a correct message
				 if (millis() - timeout > 5000) {
					 Serial.println("Timeout client");
					 client.stop();
					 return;
				 }
			}
			while ( client.available() > 0 ) {    // if there's bytes to read from the client,
				char c = client.read();
				headerIn += c;
			}
		  if (headerIn.length() > 0 ) {
			  Serial.println(headerIn);
		    if (wifi_connected == false)
			    fromWiFiAP(client);           // AccessPoint html fom for wifi selection
		    else
			    fromWiFiSTA(client);          // Wifi Host selection is OK
			  headerIn = "";                  // Clear the headerIn variable
		  }
	  }
    Serial.println("End client");
  }
}

void setup() {
  delay(5000);
  Serial.begin(115200);
	byte new_mac[8] = {0x30,0xAE,0xA4,0x90,0xFD,0xC8}; // Pif
//  byte new_mac[8] = {0x8C,0x29,0x37,0x43,0xCD,0x46}; // Dudu
	esp_base_mac_addr_set(new_mac);
	previousMillis = millis();
  // event WiFi
  WiFi.onEvent(WiFiEvent);
	// Led
  pinMode(EspLedBlue, OUTPUT);
	// Get ssid pwd into flash ram
  preferences.begin("wifi", false);
  wifiSSID =  preferences.getString("ssid", "none");       // NVS key ssid
  wifiPAWD =  preferences.getString("password", "none");   // NVS key password
  preferences.end();
  // Start AP and / or  STA
  if (wifiSSID.indexOf("none")!=0) {
	  connectToHost();  // Try to connect to wifiSSID in NVM stored
	} else {
    if (wifi_connected==false)
      setAccessPoint(); // Start accesPoint because wifiSSID host is not availabale
  }
	// Mac address
	Serial.print("Addr macAP : "); Serial.println( WiFi.softAPmacAddress().c_str() ); // Show mac address
	Serial.print("Addr macSTA: "); Serial.println( WiFi.macAddress().c_str() ); // Show mac address
}

void loop() {
	// Main loop from http
  loopWifi();
	// Main loop every new second elapes
	if ( millis() - previousMillis > 1000L) {
		int btBoot = digitalRead(ButtonBoot);
		if (btBoot==LOW) {
			incBtBoot++;
			Serial.println("btBoot pressed.");
			if (incBtBoot>3) {
				putPreferences("none","none");
				setAccessPoint();
			}
		} else {
			incBtBoot = 0;
		}

		previousMillis = millis();
    // Close AccessPoint
    if ( closeAPinSec > 0)
		  closeAPinSec --;
		if (closeAPinSec==1) WiFi.softAPdisconnect(true);
		// Flash LED process
		if (flashLed > 0) {
			if ( (seconds & 0x1) == 0 ) {
				digitalWrite(EspLedBlue, HIGH);
			} 	else {
				digitalWrite(EspLedBlue, LOW);
				flashLed--;
			}
		}
		// Timer incrementation
    if (seconds++ == 59) {
      seconds = 0;
      if (minutes++ == 59) {
        minutes = 0;
        heures++;
      }
    }
	}
}
