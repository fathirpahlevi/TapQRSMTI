void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Declare a variable to store the result
  String result = "";

  for (int i = 0; i < length; i++) {
    result += (char)payload[i];
  }

  // Print the result to serial
  Serial.println(result);
  
  // Switch on the LED if an 1 was received as first character
  if (result == "Gate Open") {
    status = 10;
  }
  if (result == "Restart ESP") {
    ESP.restart();
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = hostname;
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("gateExit", "Connected");
      // ... and resubscribe
      client.subscribe("gateExit");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void wifiSetup(){
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

  server.on("/", []() {
    server.send(200, "text/plain", "SMTI Gate Exit");
  });
}

void inputOutputSetup(){
  pinMode(OUT,OUTPUT);
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