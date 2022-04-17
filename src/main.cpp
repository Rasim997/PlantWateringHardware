#include <Arduino.h>
//wifi server libraries
// #include <WiFiClient.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Preferences.h>
//json parsing library
#include <ArduinoJson.h>
#include <nvs_flash.h>
//Sensor Library
#include <DHT.h>
#include <math.h>
#include <time.h>

#define dhtSensor 33
#define sSensor 32
#define mRelay 26
DHT dht(dhtSensor,DHT11);
Preferences preferences;
WebServer server(80);
HTTPClient httpClient;

//check the Wifi Mode
bool isAP(IPAddress ip){
  if(ip[0]==0){return true;}
  else{return false;}
}
//reset the server
void reset(){
  preferences.begin("credentials", false);
  preferences.clear();
  preferences.end();
  Serial.println("Device Reset Completed");
  server.send(200, F("text/json"),"Device Restarted, Data Wiped");
  delay(1000);
  ESP.restart();
}
//check status of the server
void checkPulse(){
  if(isAP(WiFi.localIP())==true){
    server.send(200, F("text/html"),F("Welcome to the Access Point"));
  }
  else{
    server.send(200, F("text/html"),F("Welcome to the REST Web Server"));
  }
  
}
//set wifi Credentials
void wifiInfo(){

  if(isAP(WiFi.localIP())==false)
  {
    server.send(403, F("text/json"),"Forbidden - Request can not be fulfilled");
  }
  else{
    String postBody = server.arg("plain");
    DynamicJsonDocument doc(512);
    DeserializationError parsingError = deserializeJson(doc, postBody);

    //if there is error in the recieved json object
    if (parsingError) {
        String msg = parsingError.c_str();
        server.send(400, F("text/json"),"Error in parsin json body!" + msg);
 
    } 
    //if the object is able to be parsed
    else {
      //create a json object
        JsonObject postObj = doc.as<JsonObject>();
        //if the method is post
        if (server.method() == HTTP_POST) {
          //if the post request has whats needed
            if (postObj.containsKey("SSID")&&postObj.containsKey("PASS")) {
 
                String SSID = doc["SSID"];
                String PASS = doc["PASS"];
                const char* ssid = SSID.c_str();
                const char* pass = PASS.c_str();

                preferences.begin("credentials", false);
                preferences.putString("ssid", ssid); 
                preferences.putString("password", pass);

                Serial.println("Network Credentials Saved");
                preferences.end();


                //creating data to send as a response to the main thing 
                DynamicJsonDocument doc(512);
                //populating the json Object
                doc["ssid"] = ssid;
                doc["password"] = pass;
                String buf;
                //serialising data
                serializeJson(doc, buf);
 
                server.send(201, F("application/json"), buf);
                delay(1000);
                ESP.restart();
            }else {
                DynamicJsonDocument doc(512);
                doc["status"] = "KO";
                doc["message"] = F("No data found, or incorrect!");
 
                Serial.print(F("Stream..."));
                String buf;
                serializeJson(doc, buf);
 
                server.send(400, F("application/json"), buf);
                Serial.print(F("done."));
            }
        }
    }
  }
}
//WiFi provisioning
void handleConnection(String ssid, String pass){
  if(ssid=="0"){
    WiFi.mode(WIFI_AP);
    WiFi.softAP("PlantWatering", "123456789");
    Serial.print(ssid);
    Serial.println(WiFi.localIP());
  }
  else{
    const char* s = ssid.c_str();
    const char* p = pass.c_str();
    WiFi.mode(WIFI_STA);
    WiFi.begin(s, p);
    Serial.println("");
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}
//deepSleep
void enDeepSleep(){
  preferences.begin("flags",false);
  preferences.putBool("sleep",true);
  preferences.end();
  server.send(201, F("text/json"),"Device Successfully In Deep Sleep");
  delay(1000);
  //set the time for this
  ESP.deepSleep(3e7);
}
//get sensor data
void sensorData(){

  int soilMoisture = map(analogRead(sSensor),3500,0,1,100);
  float humidity = dht.readHumidity();
  float temprature = round(dht.readTemperature());

  DynamicJsonDocument doc(512);
                //populating the json Object
                doc["Soil_Moisture"] = soilMoisture;
                doc["Humidity"] = humidity;
                doc["Temprature"] = temprature;
                String buf;
                //serialising data
                serializeJson(doc, buf);
 
                server.send(200, F("application/json"), buf);
}
//get Data From API
String apiDataGet(){
  String serverPath = "http://api.weatherapi.com/v1/current.json?key=e839e0772ccb45a4a96140209220404&q=m95pl&aqi=no";
        
  // Your Domain name with URL path or IP address with path
  httpClient.begin(serverPath);
  // Send HTTP GET request
  int httpResponseCode = httpClient.GET();
        
  if (httpResponseCode>0) {
    String payload = httpClient.getString();
    return payload;
  }
  else {
    Serial.print("Error getting data. Error code: ");
    Serial.println(httpResponseCode);
    return "error";
  }
  // Free resources
  httpClient.end();

}

//Send API data to app
void apiDataSend(){
  String buf = apiDataGet();
    server.send(200,F("application/json"),buf);
}
//get time
void getDevTime(){
  time_t now;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  // Serial.print(&timeinfo);
  // Serial.print(" ");
  // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  time(&now);
  Serial.println(now);
}
//TEST JSON DATA PARSE
void JSON(){
  DynamicJsonDocument doc(1536);
  DeserializationError error = deserializeJson(doc, apiDataGet());

if (error) {
  Serial.print("deserializeJson() failed: ");
  Serial.println(error.c_str());
  return;
}
JsonObject current_condition = doc["current"]["condition"];
const char* current_condition_text = current_condition["text"];
const char* current_condition_icon = current_condition["icon"];
String buf;
serializeJson(current_condition,buf);
server.send(200,F("application/json"),buf);
}
// Manage not found URL
void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void restServerRouting() {
    server.on("/", HTTP_GET,checkPulse);
    server.on(F("/reset"), HTTP_GET,reset);
    server.on(F("/connect"),HTTP_POST,wifiInfo);
    server.on(F("/sleep"),HTTP_GET,enDeepSleep);
    server.on(F("/sensors"),HTTP_GET,sensorData);
    server.on(F("/weather"),HTTP_GET,apiDataSend);
    server.on(F("/time"),HTTP_GET,getDevTime);
    server.on(F("/json"),HTTP_GET,JSON);

}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(mRelay,OUTPUT);
  preferences.begin("credentials", true);
  String ssid = preferences.getString("ssid","0");
  String pass = preferences.getString("password","0");
  preferences.end();
  handleConnection(ssid,pass);

  //assigning a hostname to the device
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  configTime(0, 0, "pool.ntp.org");
}

void loop() {
  server.handleClient();
}