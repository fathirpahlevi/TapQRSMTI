#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPClient.h>
#include <Wiegand.h>

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

bool gateOpen = false;
bool gateOpened = false;
bool noDataFound = false;
bool comError = false;
bool grantedBeep = false;

unsigned long currentTime = 0;
unsigned long timer1 = 0;
// It's good practice to define constants for SSIDs, passwords, and URLs
const char* ssid = "Otomasi Industri";
const char* password = "12otomasi3";
const char* serverUrl = "http://192.168.250.10:1881/data";

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
    if(hexString.length() > 0)sendPostRequest(hexString);
    oldHexString = hexString;
    hexString = "";
  }
  gateControl();
}

void gateControl(){
  if(gateOpen){
  digitalWrite(OUT,HIGH);
  digitalWrite(LED,LOW);
  gateOpened = true;
  }
  else{
    digitalWrite(OUT,LOW);
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

void sendPostRequest(String data) {
  // Always check connection right before starting the request
  if (!WiFi.isConnected()) {
    Serial.println("WiFi not connected. Cannot send POST request.");
    // Optional: Add reconnection logic here
    // WiFi.reconnect(); // Or your custom reconnect function
    return; // Exit if not connected
  }

  HTTPClient http;

  Serial.print("[HTTP] begin...\n");
  http.begin(serverUrl); // Use the constant URL

  Serial.print("[HTTP] set headers...\n");
  http.addHeader("Content-Type", "application/json"); // Correct header name

  String httpRequestData = "{\"data\":\""+ data +"\"}";
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
    if(payload == "Data Found"){
      gateOpen = true;
      timer1 = currentTime;
    }
    else{
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