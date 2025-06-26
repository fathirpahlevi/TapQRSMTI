#include <WiFi.h>
#include <HTTPClient.h>
#include <Wiegand.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
// These are the pins connected to the Wiegand D0 and D1 signals.
#define PIN_D0 14
#define PIN_D1 13
#define BEEPER 33   
#define LED 25
#define OUT 26
#define espON1 12
#define espON2 27
#define yellowLED 32

// The object that handles the wiegand protocol
Wiegand wiegand;

String hexString = "";
String oldHexString = "";

String getResponse = "";

bool gateOpen = false;
bool gateOpened = false;
bool requesting = false;
bool noDataFound = false;
bool comError = false;
bool getError = false;
bool beep = false;
bool grantedBeep = false;
bool statusState = false;

uint8_t status = 0;
uint8_t retryCount = 0;
uint8_t failedCount = 0;
String waktu = "";
String postResponse = "";
String userID = "";

unsigned long currentTime = 0;
unsigned long timer1 = 0;
unsigned long timer2 = 0;
unsigned long timer3 = 0;
// It's good practice to define constants for SSIDs, passwords, and URLs
const char* ssid = "SMTI-PRO";
const char* password = "";
const char* waktuAPI = "http://192.168.1.199:1881/waktu";
const char* sendDataAPI = "http://192.168.1.199:1881/keluar";

const char* hostname = "SMTI Gate Exit";

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
  
  http.setConnectTimeout(2000);
  http.setTimeout(2000); 
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
      getError = false;
    } else {
      Serial.printf("[HTTP] Server responded with error code: %d\n", httpCode);
      getError = true;
      status = 25;
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    getError = true;
    status = 25;
  }

  http.end(); // Free the resources
  return payload;
}

void setup() {
  Serial.begin(115200);
  IPAddress local_IP(192, 168, 0, 141);
  IPAddress gateway(192, 168, 1, 254);
  IPAddress subnet(255, 255, 248, 0);
  IPAddress primaryDNS(192, 168, 1, 254); //optional
  
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password); 

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); // Print a dot to show activity
    digitalWrite(2, HIGH); // Turn LED on
    delay(500); // Short delay for blink effect
    digitalWrite(2, LOW); // Turn LED off
    delay(500); // Short delay for blink effect
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.setPasswordHash("83eaebf5769ba0437dae13a2c0d1a1ea");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();

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
  pinMode(2,OUTPUT);
  pinMode(LED,OUTPUT);
  pinMode(BEEPER,OUTPUT);
  pinMode(espON1,OUTPUT);
  pinMode(espON2,OUTPUT);
  pinMode(yellowLED,OUTPUT);
  digitalWrite(espON1,HIGH);
  digitalWrite(espON2,HIGH);
  digitalWrite(2,HIGH);
  digitalWrite(LED,LOW);
  digitalWrite(BEEPER,LOW);
  digitalWrite(yellowLED,HIGH);
}

void loop() {
  // Example of calling the function with a delay
  currentTime = millis();
  noInterrupts();
  wiegand.flush();
  interrupts();
  if(hexString != oldHexString && status == 0){
    Serial.println("New Data");
    if(hexString.length() > 0){
      digitalWrite(yellowLED,LOW);
      requesting = true;
      JsonDocument doc;
      getResponse = getRequest(waktuAPI);
      deserializeJson(doc,getResponse);
      waktu = doc["waktu"].as<String>();
      Serial.print("Waktu : ");
      Serial.println(waktu);
      if(!getError){
      sendPostRequest(waktu,hexString);
      }
      else {
        status = 30;
      }
    }
    oldHexString = hexString;
    hexString = "";
  }
  if(WiFi.status() == WL_CONNECTED){
    gateControl();
  }
  else{
    if(currentTime - timer2 >= 500){
      timer2 = currentTime;
      statusState = !statusState;
      digitalWrite(2, statusState);
    }
  }
}

void gateControl(){
  switch (status) {
    case 0: // If button is pressed
      gateOpen = false;
      gateOpened = false;
      requesting = false;
      noDataFound = false;
      comError = false;
      getError = false;
      beep = false;
      retryCount = 0;
      break;

    case 10: // Accepted
      gateOpen = true;
      timer1 = currentTime;
      if(gateOpened)status = 15;
      break;

    case 15:
      break;

    case 20: // Refused
      noDataFound = true;
      timer2 = currentTime;
      if(beep)status = 21;
      break;

    case 21:
      break;

    case 25: // Get Error
      status = 30;
      break;

    case 30: // Post Error
      comError = true;
      timer3 = currentTime;
      status = 31;
      break;
    case 31:
      if(failedCount > 2){
        ESP.restart();
      }
      if(retryCount > 4){
        failedCount++;
        status = 35;
      }
      else{
        retryCount++;
        status = 32;
      }
      break;
    case 32: // Communication Retry
      if(currentTime - timer3 >= 500){
        digitalWrite(yellowLED,LOW);
        requesting = true;
        JsonDocument doc;
        getResponse = getRequest(waktuAPI);
        deserializeJson(doc,getResponse);
        waktu = doc["waktu"].as<String>();
        Serial.print("Waktu : ");
        Serial.println(waktu);
        if(!getError){
        sendPostRequest(waktu,oldHexString);
        }
      }
      break;
    case 35:
      if(beep)status = 36;
      break;
    case 36:
      break;
      

    default: // Optional: Handle unexpected states
      Serial.println("Unknown state");
      break;
  }
  if(gateOpen){
    digitalWrite(OUT,HIGH);
    digitalWrite(LED,HIGH);
    gateOpened = true;
  }
  if(gateOpened){
    gateOpen = false;
    if(currentTime - timer1 >= 1000){
      Serial.println("Gate Opened");
      status = 0;
    }
  }
  else{
    digitalWrite(OUT,LOW);
    digitalWrite(LED,LOW);
  }
  if(noDataFound){
    digitalWrite(BEEPER,HIGH);
    beep = true;
  }
  if(comError){
    Serial.println("Communication Error");
    if(status == 31){
      digitalWrite(yellowLED,HIGH);
      requesting = false;
    }
    if(status == 35)beep = true;
  }
  if(!comError && !noDataFound)digitalWrite(BEEPER,LOW);
  if(beep && noDataFound){
    if(currentTime - timer2 >= 1000){
      status = 0;
    }
  }
  else if(beep && comError){
    if(currentTime - timer3 < 300){
      digitalWrite(BEEPER,HIGH);
    }
    if(currentTime - timer3 >= 300 && currentTime - timer3 < 400){
      digitalWrite(BEEPER,LOW);
    }
    if(currentTime - timer3 >= 400 && currentTime - timer3 < 700){
      digitalWrite(BEEPER,HIGH);
    }
    if(currentTime - timer3 >= 700){
      digitalWrite(BEEPER,LOW);
      status = 0;
    }
  }
  else digitalWrite(BEEPER,LOW);


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
  http.setConnectTimeout(2000);
  http.setTimeout(2500); 

  Serial.print("[HTTP] performing POST request...\n");
  int httpResponseCode = http.POST(httpRequestData);

  if (httpResponseCode > 0) {
    getError = false;
    Serial.printf("[HTTP] POST Response code: %d\n", httpResponseCode);
    String payload = http.getString();
    Serial.println("POST Payload:");
    Serial.println(payload);
    JsonDocument doc;
    deserializeJson(doc,payload);
    postResponse = doc["status"].as<String>();
    Serial.print("Status : ");
    Serial.println(postResponse);
    requesting = false;
    digitalWrite(yellowLED,HIGH);
    if(postResponse == "Accepted"){
      Serial.println("Accepted");
      status = 10;
    }
    else{
      Serial.println("Refused");
      status = 20;
    }
  } else {
    Serial.printf("[HTTP] POST Error code: %d\n", httpResponseCode);
    Serial.printf("[HTTP] Error message: %s\n", http.errorToString(httpResponseCode).c_str());
    status = 30;
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
