#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPClient.h>
#include <Wiegand.h>
#include <ArduinoJson.h>
// These are the pins connected to the Wiegand D0 and D1 signals.
#define PIN_D0 22   
#define PIN_D1 23
#define BEEPER 14   
#define LED 12
#define OUT 2

// The object that handles the wiegand protocol
Wiegand wiegand;

String hexString = "";
String oldHexString = "";

String getResponse = "";

bool gateOpen = false;
bool gateOpened = false;
bool noDataFound = false;
bool comError = false;
bool grantedBeep = false;

String waktu = "";
String postResponse = "";
String userID = "";

unsigned long currentTime = 0;
unsigned long timer1 = 0;
// It's good practice to define constants for SSIDs, passwords, and URLs
const char* ssid = "SMTI-PRO";
const char* password = "";
const char* waktuAPI = "http://192.168.1.199:1881/waktu";
const char* sendDataAPI = "http://192.168.1.199:1881/masuk";

String getRequest(const char* serverUrl) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot perform HTTP GET.");
    return ""; // Return empty string if not connected
  }
  HTTPClient http;
  String payload = "";

  Serial.print("[HTTP] begin for URL: ");
  Serial.println(serverUrl);

  // Your Domain name with URL path or IP address with path
  http.begin(serverUrl);

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP GET request
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been sent and server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      payload = http.getString();
      Serial.println("Response Payload:");
      Serial.println(payload);
    } else {
      Serial.printf("[HTTP] Server responded with error code: %d\n", httpCode);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end(); // Free the resources
  return payload;
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(2000);
    ESP.restart();
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  wiegand.onReceive(receivedData, "Card readed: ");
  wiegand.onReceiveError(receivedDataError, "Card read error: ");
  wiegand.onStateChange(stateChanged, "State changed: ");
  wiegand.begin(Wiegand::LENGTH_ANY, false);

  //initialize pins as INPUT
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_D0), pinStateChanged, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_D1), pinStateChanged, CHANGE);
  pinStateChanged();

  pinMode(OUT,OUTPUT);
  pinMode(LED,OUTPUT);
  pinMode(BEEPER,OUTPUT);
  digitalWrite(LED,HIGH);
  digitalWrite(BEEPER,HIGH);
}

void loop() {
  // Example of calling the function with a delay
  currentTime = millis();
  noInterrupts();
  wiegand.flush();
  interrupts();
  if(hexString != oldHexString){
    Serial.println("New Data");
    if(hexString.length() > 0){
      
      JsonDocument doc;
      getResponse = getRequest(waktuAPI);
      deserializeJson(doc,getResponse);
      waktu = doc["waktu"].as<String>();
      Serial.print("Waktu : ");
      Serial.println(waktu);
      sendPostRequest(waktu,hexString);
    }
    oldHexString = hexString;
    hexString = "";
  }
  gateControl();
}

void gateControl(){
  if(gateOpen){
  digitalWrite(OUT,LOW);
  digitalWrite(LED,LOW);
  gateOpened = true;
  }
  else{
    digitalWrite(OUT,HIGH);
    digitalWrite(LED,HIGH);
    gateOpened = false;
  }
  if(gateOpened){
    if(currentTime - timer1 >= 1000)gateOpen = false;
  }
  if(noDataFound){
    digitalWrite(BEEPER,LOW);
    if(currentTime - timer1 >= 1000){
      digitalWrite(BEEPER,HIGH);
      noDataFound = false;
    }
  }
  if(comError){
    digitalWrite(BEEPER,LOW);
    if(currentTime - timer1 >= 500){
      digitalWrite(BEEPER,HIGH);
      comError = false;
    }

  }
}

void sendPostRequest(String waktu, String data) {
  // Always check connection right before starting the request
  if (!WiFi.isConnected()) {
    Serial.println("WiFi not connected. Cannot send POST request.");
    // Optional: Add reconnection logic here
    // WiFi.reconnect(); // Or your custom reconnect function
    return; // Exit if not connected
  }

  HTTPClient http;

  Serial.print("[HTTP] begin...\n");
  http.begin(sendDataAPI); // Use the constant URL

  Serial.print("[HTTP] set headers...\n");
  http.addHeader("Content-Type", "application/json"); // Correct header name

  String httpRequestData = "{\"waktu\":\""+waktu+"\",\"userID\":\""+ data +"\"}";
  Serial.print("[HTTP] POST data: ");
  Serial.println(httpRequestData);

  // Set timeouts to prevent hanging and Watchdog Timer resets
  http.setConnectTimeout(5000); // 5 seconds
  http.setTimeout(8000); // 8 seconds

  Serial.print("[HTTP] performing POST request...\n");
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST Response code: %d\n", httpResponseCode);
    String payload = http.getString();
    Serial.println("POST Payload:");
    Serial.println(payload);
    JsonDocument doc;
    deserializeJson(doc,payload);
    postResponse = doc["status"].as<String>();
    Serial.print("Status : ");
    Serial.println(postResponse);
    if(postResponse == "Accepted"){
      Serial.println("Accepted");
      gateOpen = true;
      timer1 = currentTime;
    }
    else{
      Serial.println("Refused");
      noDataFound = true;
      timer1 = currentTime;
    }
  } else {
    Serial.printf("[HTTP] POST Error code: %d\n", httpResponseCode);
    Serial.printf("[HTTP] Error message: %s\n", http.errorToString(httpResponseCode).c_str());
    comError = true;
    timer1 = currentTime;
  }

  Serial.print("[HTTP] end...\n");
  http.end(); // Ensure resources are freed
}

void pinStateChanged() {
  wiegand.setPin0State(digitalRead(PIN_D0));
  wiegand.setPin1State(digitalRead(PIN_D1));
}
// Notifies when a reader has been connected or disconnected.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onStateChange()`
void stateChanged(bool plugged, const char* message) {
    Serial.print(message);
    Serial.println(plugged ? "CONNECTED" : "DISCONNECTED");
}

// Notifies when a card was read.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onReceive()`
void receivedData(uint8_t* data, uint8_t bits, const char* message) {
    Serial.print(message);
    Serial.print(bits);
    Serial.print("bits / ");
    //Print value in HEX
    uint8_t bytes = (bits+7)/8;
    for (int i=0; i<bytes; i++) {
        Serial.print(data[i] >> 4, 16);
        Serial.print(data[i] & 0xF, 16);
    }
    Serial.println();

     // Declare an empty String variable to store the hexadecimal representation

    for (int i = 0; i < bytes; i++) {
    // Get the higher nibble and convert to hexadecimal character
    byte highNibble = (data[i] >> 4) & 0x0F;
      if (highNibble < 10) {
    hexString += (char)('0' + highNibble);
    } else {
        hexString += (char)('A' + highNibble - 10);
    }

    // Get the lower nibble and convert to hexadecimal character
    byte lowNibble = data[i] & 0x0F;
        if (lowNibble < 10) {
            hexString += (char)('0' + lowNibble);
        } else     {
            hexString += (char)('A' + lowNibble - 10);
        }   
    }
    
    Serial.print("converted: ");
    Serial.println(hexString);
}

// Notifies when an invalid transmission is detected
void receivedDataError(Wiegand::DataError error, uint8_t* rawData, uint8_t rawBits, const char* message) {
    Serial.print(message);
    Serial.print(Wiegand::DataErrorStr(error));
    Serial.print(" - Raw data: ");
    Serial.print(rawBits);
    Serial.print("bits / ");

    //Print value in HEX
    uint8_t bytes = (rawBits+7)/8;
    for (int i=0; i<bytes; i++) {
        Serial.print(rawData[i] >> 4, 16);
        Serial.print(rawData[i] & 0xF, 16);
    }
    Serial.println();

}