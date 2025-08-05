#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Ultrasonic.h>

static String WEB_SERVER = "http://172.20.10.2:5000";

#define WIFI_SSID "iPhone de Gonçalo"
#define WIFI_PASSWORD "Naodoute"

// OLED DISPLAY ON LEFT SIDE
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
//#define OLED_PIN 34
#define OLED_RESET -1

// LEFT SPOT
#define LEFT_RED_LIGHT_PIN 5
#define LEFT_GREEN_LIGHT_PIN 18
#define LEFT_BLUE_LIGHT_PIN 19
#define LEFT_ULTRASOUND 25
#define LEFT_SPOT_ID 0 // for scalability purposes

// RIGHT SPOT
#define RIGHT_RED_LIGHT_PIN 13 
#define RIGHT_GREEN_LIGHT_PIN 12
#define RIGHT_BLUE_LIGHT_PIN 14
#define RIGHT_ULTRASOUND 26
#define RIGHT_SPOT_ID 1 // for scalability purposes

Adafruit_SSD1306 r_display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long updateUserListMillis = 0;

int duration = 0; //ms
int distance = 0; //cm
int spot = 0;
String l_reservedTo = ""; //license plates
String r_reservedTo = "";
bool l_occupied = false;
bool r_occupied = false;

String jsonPayload;
int httpResponseCode = 0;

WebServer server(5000);
Ultrasonic ultrasonic_left(LEFT_ULTRASOUND);
Ultrasonic ultrasonic_right(RIGHT_ULTRASOUND);
void setup() {
  Serial.begin(9600); 

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(300);
  }

  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  pinMode(LEFT_RED_LIGHT_PIN, OUTPUT);
  pinMode(LEFT_GREEN_LIGHT_PIN, OUTPUT);
  pinMode(LEFT_BLUE_LIGHT_PIN, OUTPUT);


  pinMode(RIGHT_RED_LIGHT_PIN, OUTPUT);
  pinMode(RIGHT_GREEN_LIGHT_PIN, OUTPUT);
  pinMode(RIGHT_BLUE_LIGHT_PIN, OUTPUT);


  //pinMode(OLED_PIN, OUTPUT);
  if(!r_display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(1);
  }
  r_display.clearDisplay();
  r_display.setTextSize(2);
  r_display.setTextColor(WHITE);
  r_display.setCursor(5, 10);
  r_display.println("Free space");
  r_display.display();

  server.on("/update", HTTP_POST, handleUpdate);
  server.begin();
  delay(1000);
}

void loop(){
  // REFRESH INFO EVERY 12 HOURS
  if (millis() - updateUserListMillis > 43200000 || updateUserListMillis == 0){
    updateUserListMillis = millis();
    
    jsonPayload = "{\"name\":\"parkingSpots/0\"}";
    httpGetReservesRequest(0, jsonPayload);

    jsonPayload = "{\"name\":\"parkingSpots/1\"}";
    httpGetReservesRequest(1, jsonPayload);
  }

  server.handleClient();
  
  SPOT(ultrasonic_left, LEFT_RED_LIGHT_PIN, LEFT_GREEN_LIGHT_PIN, LEFT_BLUE_LIGHT_PIN, LEFT_SPOT_ID, l_reservedTo, l_occupied);
  
  server.handleClient(); // To avoid too much time without checking
  delay(1000);
  
  SPOT(ultrasonic_right, RIGHT_RED_LIGHT_PIN, RIGHT_GREEN_LIGHT_PIN, RIGHT_BLUE_LIGHT_PIN, RIGHT_SPOT_ID, r_reservedTo, r_occupied);
    
  delay(1000);
}

void SPOT(Ultrasonic ultrasonic, int redPin, int greenPin, int bluePin, int id, String reservedTo, bool &isOccupied){

  //Se tiver reservado, dar print a reservedTo 
  long rangeInCentimeters;
  rangeInCentimeters = ultrasonic.MeasureInCentimeters();
  Serial.println("Spot " + String(id) + ": " + String(rangeInCentimeters) + " centimeters");

  if (id == 1){ //For demo 
    if (reservedTo != ""){
      r_display.clearDisplay();
      r_display.setTextSize(2);
      r_display.setTextColor(WHITE);
      r_display.setCursor(10,10);
      r_display.println(reservedTo);
      r_display.display();
    }
  }

  if (rangeInCentimeters >= 0 && rangeInCentimeters <= 8){
    if (!isOccupied){
      isOccupied = true;
      
      jsonPayload = "{\"name\":\"parkingSpots/" + String(id) + "/state\", \"value\": \"occupied\"}";
      httpSetRequest(jsonPayload);

      if (id == 1 && reservedTo == ""){ //For demo purposes
        r_display.clearDisplay();
        r_display.setTextSize(2);
        r_display.setTextColor(WHITE);
        r_display.setCursor(5,10);
        r_display.println("Occupied");
        r_display.display();
      }
    
    }
    analogWrite(redPin, 255);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  }
  else{
    if (isOccupied){
      isOccupied = false;

      if (reservedTo == ""){
        jsonPayload = "{\"name\":\"parkingSpots/" + String(id) + "/state\", \"value\": \"free\"}";
        httpSetRequest(jsonPayload);

        //Done this way for demo purposes
        if (id == 1){
          r_display.clearDisplay();
          r_display.setTextSize(2);
          r_display.setTextColor(WHITE);
          r_display.setCursor(5,10);
          r_display.println("Free space");
          r_display.display();
        }
      }
      else{
        jsonPayload = "{\"name\":\"parkingSpots/" + String(id) + "/state\", \"value\": \"reserved\"}";
        httpSetRequest(jsonPayload);
      }
    }
    
    if (reservedTo != ""){
      analogWrite(redPin, 0);
      analogWrite(greenPin, 0);
      analogWrite(bluePin, 255);
    }
    else{
      analogWrite(redPin, 0);
      analogWrite(greenPin, 255);
      analogWrite(bluePin, 0);
    }
  }
}

void handleUpdate() {
  String requestBody = server.arg("plain");
  Serial.println(requestBody);

  DynamicJsonDocument jsonDocument(requestBody.length());
  DeserializationError error = deserializeJson(jsonDocument, requestBody);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    server.send(400); // Bad request
    return;
  }

  // Extract values from JSON data
  int spot = jsonDocument["spot"];
  String reservedTo = jsonDocument["reservedTo"];

  if (spot == LEFT_SPOT_ID){
    l_reservedTo = reservedTo;
  }
  else {
    r_reservedTo = reservedTo;
  }

  // Print the received values
  Serial.print("Received values - spot: ");
  Serial.print(spot);
  Serial.print(", reservedTo: ");
  Serial.println(reservedTo);

  server.send(200, "text/plain", "OK");
}

void httpSetRequest(String jsonPayload){
  if(WiFi.status()== WL_CONNECTED){
      HTTPClient http;

      String serverPath = WEB_SERVER + "/set";

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
      }
      // Free resources
      http.end();
  }
}

void httpGetReservesRequest(int spot, String jsonPayload){
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
        JsonObject response = doc["response"].as<JsonObject>();
        
        if (spot == 0){
          l_reservedTo = response["reservedTo"].as<String>();;
          Serial.println("Spot 0 reserved to: " + l_reservedTo);
        }
        else if (spot == 1){
          r_reservedTo = response["reservedTo"].as<String>();;

          r_display.clearDisplay();
          r_display.setTextSize(2);
          r_display.setTextColor(WHITE);
          r_display.setCursor(10,10);
          r_display.println(r_reservedTo);
          r_display.display();

          Serial.println("Spot 1 reserved to: " + r_reservedTo);
        }

      }
    }
    // Free resources
    http.end();
  }

}

