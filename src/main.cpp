#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#define ledInit 2
#define optAP2Host 0
#define optHostOK  1
#define optHostRST 2

#define DBX_EVT true

// Access point Initialize
const char* ssid     = "ESP32-Dudu";
const char* password = "123456789";
static volatile bool wifi_connected = false;
Preferences preferences; // The NVM storage
String headerIn;         // Html Header Input
String htmlSsidList;     // Html selector Wifi list
String wifiSSID;				 // The SSID use
String wifiPAWD;         // Accosiate password

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

void showMacAddress() {
	byte mac[6];
	WiFi.macAddress(mac);
	Serial.print("MAC: ");
	Serial.print(mac[5],HEX);
	Serial.print(":");
	Serial.print(mac[4],HEX);
	Serial.print(":");
	Serial.print(mac[3],HEX);
	Serial.print(":");
	Serial.print(mac[2],HEX);
	Serial.print(":");
	Serial.print(mac[1],HEX);
	Serial.print(":");
	Serial.println(mac[0],HEX);
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
        for (unsigned int i = 0; i < n; ++i) {
            // Store SSID for each network found
            htmlSsidList = htmlSsidList + "<option value=\""+WiFi.SSID(i)+"\">";
            htmlSsidList = htmlSsidList + (i+1) + ": " + WiFi.SSID(i) + "</option>";
        }
        htmlSsidList = htmlSsidList + "</select>";
    }
}

void  setAccessPoint() { // Connect to Wi-Fi network with SSID and password
	wifiList(); // Read list of SSID
  WiFi.softAP(ssid, password); // Remove the password parameter, if you want the AP (Access Point) to be open
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Select WiFi AP (Access Point): "); Serial.print(ssid);
  Serial.print(" and open your browser on: ");
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
  client.println("<form action=\"#\" method=\"get\">");
  client.println("<div>");
  client.println("<label for\"wifi\">Wi-Fi :</label>");
  client.println(htmlSsidList); // ADD Wi-Fi list selector
  client.println("</div><div>");
  client.println("<label>Password :</label>");
  client.println("<input type=\"text\" name=\"password\"/>");
  client.println("</div><div>");
  client.println("<button type=\"submit\">Connect on selected Wi-Fi</button>");
  //client.println("<input type=\"submit\" value=\"Connect on selected Wi-Fi\"/>");
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
	  client.println("<h1>ESP32 Web Server</h1><p><strong>ESP32 is connected at <b>");
    client.println(wifiSSID);
  }
  client.println("</b></strong></p>");
  client.println(); // The HTTP response ends with another blank line
  if (opt==optHostOK) {
    client.println("<br><hr><a href=\"/accpts\"><button class=\"button\">Change WiFi</button></a>");
  }
	if (opt==optHostRST || opt==optHostOK) {
		client.println("<br>Select WiFi Access Point: <b>");
		client.println( ssid);
		client.println("</b> and open your browser on IP:<b> 192.168.4.1");
	}
	client.println("</body></html>");
	client.stop();
}

void connectToHost() {
	WiFi.disconnect();
	wifi_connected = false;
	int mxl=0;
	WiFi.begin(wifiSSID.c_str(), wifiPAWD.c_str());
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
		if (mxl++ > 40) ESP.restart(); // Connection timeout back to AccessPoint
	}
	// Store new Wifi ssid and password
	preferences.clear();
	preferences.begin("wifi", false); // Note: Namespace name is limited to 15 chars
	preferences.putString("ssid", wifiSSID);
	preferences.putString("password", wifiPAWD);
	preferences.end();
	digitalWrite(ledInit, LOW);
	Serial.println("");
	Serial.print("WiFi connected to [Host][IP]:[");Serial.print(wifiSSID);
	Serial.print("][]");Serial.print(WiFi.localIP());Serial.println("]");
	server.begin();
	wifi_connected = true;
}

void disconnectedWifi (WiFiClient client) {
	if (headerIn.indexOf("GET /?wifi=") >= 0) { // wifiSSID is selected Try  connection
		int e = headerIn.indexOf("&password=");
		int s = headerIn.indexOf(" HTTP/");
		wifiSSID = urlDecode(headerIn.substring(11, e));
		wifiPAWD = urlDecode(headerIn.substring(e+10, s));
		Serial.print("WIFI:"); Serial.println(wifiSSID);
		Serial.print("PWD :"); Serial.println(wifiPAWD);
		htmlConnectedOk(client, optAP2Host);      // Answer connection OK
		connectToHost();                          // Store new Wifi ssid, password and connect
		return;
	}
	htmlNotConnected(client);
}

void connectedWifi(WiFiClient client) {       // Loop when Esp is connected to Wifi
	if (headerIn.indexOf("GET /accpts") >= 0) { // Back to AP for wifi selection
      preferences.clear();
			preferences.begin("wifi", false);
			preferences.putString("ssid", "none");
			preferences.putString("password", "none");
			preferences.end();
			htmlConnectedOk(client, optHostRST);
			Serial.println("REBOOT SENDING BY HTTP");
			client.stop();
			delay(500);
			ESP.restart();
	}
  htmlConnectedOk(client, optHostOK);
}

void loopWifi() {
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                             // if you get a client,
    Serial.println("New client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        headerIn += c;
        if (c == '\n') {                    // if the byte is a newline character
          if (currentLine.length() == 0) {  // that's the end of the client HTTP request, so send a response:
					  if (wifi_connected == false)
						  disconnectedWifi(client);     // AccessPoint html fom for wifi selection
					   else
						  connectedWifi(client);        // Wifi Host selection is OK
            break;                          // Break out of the while loop
          } else {                          // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {             // if you got anything else but a carriage return character,
          currentLine += c;                 // add it to the end of the currentLine
        }
      }                                     // end available
    }                                       // end client
    Serial.print("++headerIn:");
		Serial.println(headerIn);
    headerIn = "";                          // Clear the headerIn variable
    client.stop();                          // Close the connection
    Serial.println("Client disconnected.");
  }
}

void setup() {
  delay(500);
  Serial.begin(115200);
  WiFi.onEvent(WiFiEvent);
  pinMode(ledInit, OUTPUT);
  digitalWrite(ledInit, HIGH);
	// Get ssid pwd into flash ram
  preferences.begin("wifi", false);
  wifiSSID =  preferences.getString("ssid", "none");       // NVS key ssid
  wifiPAWD =  preferences.getString("password", "none");   // NVS key password
  preferences.end();
  if (wifiPAWD.indexOf("none")!=0)
	  connectToHost();  // Try to connect to wifiSSID in NVM stored
  if (wifi_connected==false)
    setAccessPoint(); // Start accesPoint because wifiSSID host is not availabale

		showMacAddress();
}

void loop() {
  loopWifi();
  delay(50);
}
