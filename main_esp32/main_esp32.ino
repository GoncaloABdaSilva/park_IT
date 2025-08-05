#include <WiFi.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <time.h>


static String WEB_SERVER = "http://172.20.10.2:5000";

const char* websiteToGetTime = "pool.ntp.org";

#define WIFI_SSID "iPhone de Gonçalo"
#define WIFI_PASSWORD "Naodoute"

// TEMPERATURE AND HUMIDITY SENSOR
#define DHT_SENSOR_PIN  21 // ESP32 pin GPIO21 connected to DHT11 sensor
#define DHT_SENSOR_TYPE DHT11

DHT dht_sensor(DHT_SENSOR_PIN, DHT_SENSOR_TYPE);
float humi = 0.0;
float tempC = 0.0;

float humiThreshold = 70.0;
float tempThreshold = 40.0;

// MOTION SENSOR
#define MOTION_SENSOR_PIN 26
int motionPinStateCurrent = LOW;  // current state of pin
int motionPinStatePrevious = LOW;  // previous state of pin

// 4 LEDS
#define FULL_PARK_LED_PIN 32
#define ENTRY_GATE_PIN 12
#define EXIT_GATE_PIN 14
#define ALARM_LED_PIN 2

// GAS SENSOR
#define GAS_SENSOR_PIN 36 //SP aka VP
int gas = 0;
int gas_danger_lvl_1 = 2000;
int gas_danger_lvl_2 = 3000;

// BUZZER
#define BUZZER_PIN 15

// RFID_SCANNER
#define RFID_SS_PIN 5
#define RFID_RST_PIN 27
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

#define GET_USER_LIST_REQUEST 0
#define GET_USERS_WITH_RESERVED_SPOT_REQUEST 1
#define GET_THRESHOLD_VALUES 2
#define GET_SENSORS_STATE 3

unsigned int httpResponseCode = 0;

unsigned long sendDataPrevMillis = 0;
unsigned long rfidSequenceMillis = 0;
unsigned long updateUserListMillis = 0;
unsigned long openEntryGateTimeMillis = 0;
unsigned long openExitGateTimeMillis = 0;
unsigned long alarmTimerMillis = 0;

#define PARKING_SPOTS 2
unsigned int usersWithoutReseveParked = 0;
unsigned int usersWithReseveParked = 0;

bool dangerous_air_quality = false;

String* users_registered;
unsigned int numUsersRegistered = 0;

String* users_with_reserve;
unsigned int numUsersWithReserve = 0;

String users_in_park[PARKING_SPOTS];

String jsonPayload;

bool parkIsFull = false;

bool alarmActive = true;

bool parkIsOpen = true;
bool gatesOpenMode = false;
bool gasSensorActive = true;
bool tempHumSensorActive = true;

WebServer server(5000);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("."); 
    delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  configTime(0, 3600, websiteToGetTime);

  dht_sensor.begin(); 
  pinMode(MOTION_SENSOR_PIN, INPUT);
  
  pinMode(FULL_PARK_LED_PIN, OUTPUT);

  pinMode(ENTRY_GATE_PIN, OUTPUT);

  pinMode(EXIT_GATE_PIN, OUTPUT);
  
  pinMode(ALARM_LED_PIN, OUTPUT);
  
  pinMode(GAS_SENSOR_PIN, INPUT);
  
  pinMode(BUZZER_PIN, OUTPUT);

  SPI.begin(); 
  rfid.PCD_Init(); 

  for (int i=0; i< PARKING_SPOTS; i++){
    users_in_park[i] = "";
  }

  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();
  delay(1000);
}


void processGetUsersWithReserveListRequest(DynamicJsonDocument doc) {
  JsonObject response = doc["response"].as<JsonObject>();

  numUsersWithReserve = response.size();

  users_with_reserve = new String[numUsersWithReserve];

  int index = 0;

  Serial.println("USERS WITH RESERVE RFIDS: ");
  for (JsonPair kv : response) {
    users_with_reserve[index] = kv.value().as<String>();
    Serial.println(users_with_reserve[index]);
    index++;
  }
}

void processGetUserListRequest(DynamicJsonDocument doc) {
  JsonObject response = doc["response"].as<JsonObject>();

  numUsersRegistered = response.size();

  users_registered = new String[numUsersRegistered];

  int index = 0;

  Serial.println("USERS RFIDS: ");
  for (JsonPair kv : response) {
    users_registered[index] = kv.value().as<String>();
    Serial.println(users_registered[index]);
    index++;
  }
  
}

void processGetThresholdRequest(DynamicJsonDocument doc) {
  JsonObject response = doc["response"].as<JsonObject>();

  gas_danger_lvl_2 = response["gas_danger"];
  gas_danger_lvl_1 = gas_danger_lvl_2 * 2/3;
  tempThreshold = response["temperature_danger"];
  humiThreshold = response["humidity_danger"];
  Serial.println("Received inicial thresholds");
}

void processGetSensorsStateRequest(DynamicJsonDocument doc){
  JsonObject response = doc["response"].as<JsonObject>();
  
  parkIsOpen = response["access"];
  gatesOpenMode = response["open_gate"];
  gasSensorActive = response["air_quality"];
  tempHumSensorActive = response["humidity_and_temperature"];
  Serial.println("Received inicial sensor states");
}


void httpGetRequest(int type, String jsonPayload){
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

      String serverPath = WEB_SERVER + "/get";
      Serial.println(serverPath);
      
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());

      http.addHeader("Content-Type", "application/json");
      
      // Allways POST to be able to send jsonPayload
      httpResponseCode = http.POST(jsonPayload);     
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode < 300){
          
          String receivedString = http.getString();

          Serial.println(receivedString);
          DynamicJsonDocument doc( receivedString.length() );

          deserializeJson(doc, receivedString);

          switch (type){
            case GET_USER_LIST_REQUEST: 
              processGetUserListRequest(doc);
              break;

            case GET_USERS_WITH_RESERVED_SPOT_REQUEST: // SPOT : int, UID : string
              processGetUsersWithReserveListRequest(doc);
              break;

            case GET_THRESHOLD_VALUES:
              processGetThresholdRequest(doc);
              break;

            case GET_SENSORS_STATE:
              processGetSensorsStateRequest(doc);
              break;  

            default: 
              Serial.println("Error in get request");
              break;
          }
        }
        else {
          Serial.print("Error code: ");
          Serial.println(httpResponseCode);
        }

        httpResponseCode = 0;
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        httpResponseCode = 0;
      }
      // Free resources
      http.end();
  }
}

void httpSetRequest(String jsonPayload, String method){
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;
                                        
      String serverPath = WEB_SERVER + method;

      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());

      http.addHeader("Content-Type", "application/json");
      
      // Allways POST to be able to send jsonPayload
      httpResponseCode = http.POST(jsonPayload);     
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        httpResponseCode = 0;
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        httpResponseCode = 0;
        Serial.println(http.getString());
      }
      // Free resources
      http.end();
  }
}

void httpSetEntry(String jsonPayload){
    if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

      String serverPath = WEB_SERVER + String("/entry");

      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());

      http.addHeader("Content-Type", "application/json");
      
      // Allways POST to be able to send jsonPayload
      httpResponseCode = http.POST(jsonPayload);     
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        httpResponseCode = 0;
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        httpResponseCode = 0;
        Serial.println(http.getString());
      }
      // Free resources
      http.end();
  } 
}

void loop() {
  server.handleClient();

  // REFRESH INFO EVERY 12 HOURS
  if (millis() - updateUserListMillis > 43200000 || updateUserListMillis == 0){
    updateUserListMillis = millis();

    jsonPayload = "{\"name\":\"usersRFID\"}";
    httpGetRequest(GET_USER_LIST_REQUEST, jsonPayload);

    jsonPayload = "{\"name\":\"reserveUsersRFID\"}";
    httpGetRequest(GET_USERS_WITH_RESERVED_SPOT_REQUEST, jsonPayload);

    jsonPayload = "{\"name\":\"thresholdValues\"}";
    httpGetRequest(GET_THRESHOLD_VALUES, jsonPayload);

    jsonPayload = "{\"name\":\"SensorState\"}";
    httpGetRequest(GET_SENSORS_STATE, jsonPayload);  

    updateAvailableSpots();
  }

  if (parkIsOpen){
    if (gatesOpenMode){
      digitalWrite(ENTRY_GATE_PIN, HIGH);
      digitalWrite(EXIT_GATE_PIN, HIGH);
      digitalWrite(FULL_PARK_LED_PIN, LOW);
    }
    else{
      // ENTRY GATE
      if (millis() - openEntryGateTimeMillis < 3000){
        digitalWrite(ENTRY_GATE_PIN, HIGH);
      } else{ digitalWrite(ENTRY_GATE_PIN, LOW); }

      // EXIT GATE
      if (millis() - openExitGateTimeMillis < 3000){
        digitalWrite(EXIT_GATE_PIN, HIGH);
      } else{ digitalWrite(EXIT_GATE_PIN, LOW); }
      

      // FULL_PARK_LED
      if (usersWithoutReseveParked + usersWithReseveParked == PARKING_SPOTS){
        digitalWrite(FULL_PARK_LED_PIN, HIGH); //PARK IS FULL
        parkIsFull = true;
      }
      else if (usersWithoutReseveParked + numUsersWithReserve == PARKING_SPOTS){
        digitalWrite(FULL_PARK_LED_PIN, HIGH); //ONLY USERS WITH RESERVE CAN ENTER
        parkIsFull = false;
      }
      else{
        digitalWrite(FULL_PARK_LED_PIN, LOW);
        parkIsFull = false;
      }

      // MOTION SENSOR
      motionPinStatePrevious = motionPinStateCurrent; // store old state
      motionPinStateCurrent = digitalRead(MOTION_SENSOR_PIN);   // read new state

      if (motionPinStatePrevious == LOW && motionPinStateCurrent == HIGH){
        Serial.println("Motion detected");
        rfidSequenceMillis = millis();
        Serial.println(millis() - rfidSequenceMillis);
      }

      if (millis() - rfidSequenceMillis < 30000){
        // RFID_SCANNER
        if (rfid.PICC_IsNewCardPresent()) { // new tag is available
          if (rfid.PICC_ReadCardSerial()) { // NUID has been readed
            MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
            Serial.print("RFID/NFC Tag Type: ");
            Serial.println(rfid.PICC_GetTypeName(piccType));

            // print UID in Serial Monitor in the hex format
            Serial.print("UID:");
            String uidString = "";
            for (int i = 0; i < rfid.uid.size; i++) {
              uidString.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
              uidString.concat(String(rfid.uid.uidByte[i], HEX));
            }
            uidString.toUpperCase();
            Serial.println(uidString);

            rfid.PICC_HaltA(); // halt PICC
            rfid.PCD_StopCrypto1(); // stop encryption on PCD

            // CHECKING
            int parked = -1;
            for (int i=0; i < PARKING_SPOTS; i++){
              if (users_in_park[i] == uidString){
                parked = i;
                break;
              }
            }

            if (parked == -1){ // IF NOT PARKED
              if (!parkIsFull){
                bool reserved = false;
                
                for (int i=0; i < numUsersWithReserve; i++){
                  if (users_with_reserve[i] == uidString){ // IF IT HAS RESERVE
                    usersWithReseveParked++;
                    Serial.println("User entering with reservation");
                    openEntryGateTimeMillis = millis();
                    reserved = true;

                    for (int i=0; i < PARKING_SPOTS; i++){
                      if (users_in_park[i] == ""){
                        users_in_park[i] = uidString;
                        break;
                      }
                    }

                    //TODO
                    //HTTP REQUEST WARNING PARKING
                    //mandar para usersInPark
                    time_t now = time(nullptr);
                    char formattedDate[20]; // Allocate space for the formatted date
                    strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d %H:%M:%S", localtime(&now));
                    String formattedDateString = String(formattedDate);

                    Serial.println("Entry at " + formattedDateString);

                    jsonPayload = "{\"name\":\"" + uidString + "\", \"time\": \"" + formattedDateString + "\"}";
                    httpSetRequest(jsonPayload, String("/entry"));    

                    break;
                  }
                }

                if (!reserved){
                  bool registered = false;

                  for (int i=0; i < numUsersRegistered; i++){
                    if (users_registered[i] == uidString){
                      registered = true;
                      break;
                    }
                  }

                  if (!registered){
                    Serial.println("Attempt of invalid access to the park.");
                  }
                  else{
                    if (usersWithoutReseveParked + numUsersWithReserve < PARKING_SPOTS){
                      for (int i=0; i < PARKING_SPOTS; i++){
                        if (users_in_park[i] == ""){
                          users_in_park[i] = uidString;
                          usersWithoutReseveParked++;

                          Serial.println("User entering the park.");

                          //TODO
                          //HTTP REQUEST WARNING PARKING
                          time_t now = time(nullptr);
                          char formattedDate[20]; // Allocate space for the formatted date
                          strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d %H:%M:%S", localtime(&now));
                          String formattedDateString = String(formattedDate);

                          Serial.println("Entry at " + formattedDateString);

                          jsonPayload = "{\"name\":\"" + uidString + "\", \"time\": \"" + formattedDateString + "\"}";
                          httpSetRequest(jsonPayload, String("/entry"));    

                          updateAvailableSpots();

                          // ABRIR CANCELA DE ENTRADA
                          openEntryGateTimeMillis = millis();
                          break;
                        }
                      }
                    }
                    else{
                      Serial.println("All free spots are reserved");
                      Serial.println("Attempt of entry with full park");
                    }
                  }
                }
              }
              else{
                Serial.println("Attempt of entry with full park");
              }
            }
            else{ // IF PARKED
              users_in_park[parked] = "";
              
              bool hasLeft = false;
              for (int i=0; i < numUsersWithReserve; i++){
                if (users_with_reserve[i] == uidString){ 
                  usersWithReseveParked--;
                  hasLeft = true;
                  Serial.println("User with reservation leaving the park.");

                  break;
                }
              }

              if (!hasLeft){
                usersWithoutReseveParked--;
                Serial.println("User leaving the park.");

                updateAvailableSpots();
              }

              //TODO
              //HTTP REQUEST WARNING LEAVING PARK

              time_t now = time(nullptr);
              char formattedDate[20]; // Allocate space for the formatted date
              strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d %H:%M:%S", localtime(&now));
              String formattedDateString = String(formattedDate);

              Serial.println("Exit at " + formattedDateString);

              jsonPayload = "{\"name\":\"" + uidString + "\", \"time\": \"" + formattedDateString + "\"}";
              httpSetRequest(jsonPayload, String("/exit"));

              // ABRIR CANCELA DE SAIDA
              openExitGateTimeMillis = millis();
              
            }
          }
        }
      }
    }
  }
  else{
    digitalWrite(FULL_PARK_LED_PIN, HIGH);
  }
  // DATA SENSORS
  if (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0){
    sendDataPrevMillis = millis();

    if (tempHumSensorActive){
      humi = dht_sensor.readHumidity();
      tempC = dht_sensor.readTemperature();

      if ( isnan(tempC) || isnan(humi)){
        Serial.println("Failed to read from DHT sensor!");
      } 
      else {
        Serial.println("HUMIDITY : " + String(humi));
        jsonPayload = "{\"name\":\"Sensor/humidity_data\", \"value\": " + String(humi) +"}";
        httpSetRequest(jsonPayload, String("/set"));

        Serial.println("TEMPERATURE : " + String(tempC));
        jsonPayload = "{\"name\":\"Sensor/temperature_data\", \"value\": " + String(tempC) +"}";
        httpSetRequest(jsonPayload, String("/set"));
      }

      
    }

    if (gasSensorActive){
      gas = analogRead(GAS_SENSOR_PIN);

      if (isnan(gas)){
        Serial.println("Failed to read from gas sensor!");
      }
      else{
        Serial.println("GAS : " + String(gas));
        jsonPayload = "{\"name\":\"Sensor/air_quality_data\", \"value\": " + String(gas) +"}";
        httpSetRequest(jsonPayload, String("/set"));

        if (gas <= gas_danger_lvl_1){
          dangerous_air_quality = false;
          jsonPayload = "{\"name\":\"alarm\", \"value\": false }";
          httpSetRequest(jsonPayload, String("/set"));
        }
        
        if (gas > gas_danger_lvl_2){
          
            dangerous_air_quality = true;
            alarmTimerMillis = millis();
            digitalWrite(ALARM_LED_PIN, HIGH);

            jsonPayload = "{\"name\":\"alarm\", \"value\": true }";
            httpSetRequest(jsonPayload, String("/set"));
          
        }  
      }
    }
  }

  // ALARM
  if ( !( isnan(humi) || isnan(tempC) || isnan(gas) ) ){
    if (dangerous_air_quality){
      if (alarmActive){
        tone(BUZZER_PIN, 2000);
      }
      else{
        noTone(BUZZER_PIN);
      }
      unsigned long localTimer = millis() - alarmTimerMillis;
      digitalWrite(ALARM_LED_PIN, HIGH);
    }
    else if ( ((humi > humiThreshold || tempC > tempThreshold) && tempHumSensorActive)
          || (gas > gas_danger_lvl_1 &&  gasSensorActive)) {
      digitalWrite(ALARM_LED_PIN, HIGH);
    }
    else{
      digitalWrite(ALARM_LED_PIN, LOW);
    }
  }
}


void handleUpdate(){
  String requestBody = server.arg("plain");
  
  DynamicJsonDocument jsonDocument(requestBody.length());
  DeserializationError error = deserializeJson(jsonDocument, requestBody);

  Serial.println(requestBody);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    server.send(400); // Bad request
    return;
  }
  
  String name = jsonDocument["name"];

  if (name.equals("gasThreshold")){
    gas_danger_lvl_2 = jsonDocument["value"];
    gas_danger_lvl_1 = gas_danger_lvl_2 * 2/3;
    Serial.println("Received value " + name + ": " + gas_danger_lvl_2);
  }
  else if (name.equals("tempThreshold")){
    tempThreshold = jsonDocument["value"];
    Serial.println("Received value " + name + ": " + tempThreshold);
  }
  else if (name.equals("humiThreshold")){
    humiThreshold = jsonDocument["value"];
    Serial.println("Received value " + name + ": " + humiThreshold);
  }  
  else if (name.equals("reserveRFIDs")){
    JsonObject response = jsonDocument["value"].as<JsonObject>();

    numUsersWithReserve = response.size();

    users_with_reserve = new String[numUsersWithReserve];

    int index = 0;

    Serial.println("USERS WITH RESERVE RFIDS: ");
    for (JsonPair kv : response) {
      users_with_reserve[index] = kv.value().as<String>();
      Serial.println(users_with_reserve[index]);
      index++;
    }  
    updateAvailableSpots();
  }
  else if (name.equals("usersRFIDs")){
    JsonObject response = jsonDocument["value"].as<JsonObject>();

    numUsersRegistered = response.size();

    users_registered = new String[numUsersRegistered];

    int index = 0;

    Serial.println("USERS RFIDS: ");
    for (JsonPair kv : response) {
      users_registered[index] = kv.value().as<String>();
      Serial.println(users_registered[index]);
      index++;
    }
  }
  else if (name.equals("constants")){
    parkIsOpen = jsonDocument["value"]["access"];
    gatesOpenMode = jsonDocument["value"]["gates"];
    gasSensorActive = jsonDocument["value"]["gas"];
    tempHumSensorActive = jsonDocument["value"]["temp_hum"];
    Serial.println("Received constants");
  }
  else if (name.equals("alarmState")){
    alarmActive = jsonDocument["value"];
    Serial.println("Received value " + name + ": " + alarmActive);
 }
  
  server.send(200, "text/plain", "Update received");
}

void updateAvailableSpots(){
  int availableSpots = PARKING_SPOTS - usersWithoutReseveParked - numUsersWithReserve;
  jsonPayload = "{\"name\":\"Sensor/counter\", \"value\": " + String(availableSpots) +"}";
  httpSetRequest(jsonPayload, "/set");
}
