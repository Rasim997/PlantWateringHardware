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
#include <Timer.h>
//defining sensorpins
#define dhtSensor 33
#define sSensor 32
#define mRelay 18

const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

//init library objects
DHT dht(dhtSensor,DHT22);
Preferences preferences;
WebServer server(80);
HTTPClient httpClient;
Timer timer(MILLIS);
Timer Watering(MILLIS);
Timer WaitTimer(MILLIS);
//Flags
bool activateSystem = false;
bool wateringFlag = false;
//Preferences
int moisturePercentage= 0;
int timerInterval = 0;

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
  preferences.begin("flags", false);
  preferences.clear();
  preferences.end();
  preferences.begin("settings", false);
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

                WiFi.mode(WIFI_AP_STA);
                WiFi.begin(ssid, pass);
                Serial.println("");
                // Wait for connection
                int connecting = 0 ;
                while (WiFi.status() != WL_CONNECTED) {
                  delay(500);
                  Serial.print(".");
                  Serial.print(",");
                  connecting = connecting+1;
                  if(connecting == 30){
                    preferences.begin("credentials", false);
                    preferences.clear();
                    preferences.end();
                    server.send(404,"text/plain","notFound");
                    ESP.restart();
                  }
                }


                //creating data to send as a response to the main thing 
                DynamicJsonDocument doc(512);
                //populating the json Object
                doc["ssid"] = ssid;
                doc["password"] = pass;
                doc["hostname"] = String((uint32_t)ESP.getEfuseMac(), HEX);
                doc["ip"]=WiFi.localIP();
                String buf;
                //serialising data
                serializeJson(doc, buf);
 
                server.send(201, F("application/json"), buf);
                delay(1000);
                ESP.restart();
            }else {
                DynamicJsonDocument doc(64);
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
    WiFi.softAP("ESP32PWS");
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
    int connecting = 0 ;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      connecting = connecting+1;
      if(connecting == 15){
        preferences.begin("credentials", false);
        preferences.clear();
        preferences.end();
        ESP.restart();
      }
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

  if(isAP(WiFi.localIP())==false)
  {
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
            if (postObj.containsKey("interval_time")) {
              //set flags
              preferences.begin("flags",false);
              preferences.putBool("sleep",true);
              preferences.end();
              //calculate the recieved data for the time
              String timeString = doc["interval_time"];
              u32_t timeInDouble = timeString.toDouble();
              //send the reply to the server
              server.send(200, F("application/json"), "Going to Deep Sleep");
              delay(1000);
              //go to deep sleep
              ESP.deepSleep(timeInDouble);
               
            }else {
                DynamicJsonDocument doc(64);
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
  else{
    server.send(403, F("text/json"),"Forbidden - Request can not be fulfilled");
  }
}
//get sensor data
void sensorData(){

  int soilMoisture = map(analogRead(sSensor),3500,0,1,100);
  float humidity = dht.readHumidity();
  float temprature = round(dht.readTemperature());

  DynamicJsonDocument doc(64);
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
//controling the api
void algorithmControl(){
  if(isAP(WiFi.localIP())==true)
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
            if (postObj.containsKey("activateSystem")&&postObj.containsKey("timeInterval")&&postObj.containsKey("moisture")) {
                timerInterval= doc["timeInterval"];
                activateSystem= doc["activateSystem"];
                moisturePercentage = doc["moisture"];
                preferences.begin("settings");
                preferences.putInt("soilMoisture",moisturePercentage);
                preferences.putDouble("timerInterval",timerInterval);
                preferences.putBool("activateSystem", activateSystem);
                preferences.end();

                Serial.println("DID");
                timer.stop();
                Watering.stop();
                WaitTimer.stop();

                //creating data to send as a response to the main thing 
                DynamicJsonDocument doc(512);
                //populating the json Object
                doc["Time Interval"] = timerInterval;
                doc["System Activation"] = activateSystem; 
                doc["soil Moisture Content"] = moisturePercentage;
                
                String buf;
                //serialising data
                serializeJson(doc, buf);
 
                server.send(201, F("application/json"), buf);
                delay(1000);
            }else {
                DynamicJsonDocument doc(64);
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
//watering algorithm
void wateringAlgorithm(){
  if(activateSystem==true && moisturePercentage>0 && timerInterval>1000)
  {
    //if the timer is not running and the device is not currently watering the plant.
    if(timer.state()==STOPPED&&wateringFlag==false){
    Serial.println("Timer Started");
    timer.start();
    }
    //if the timer is running
    if (timer.state()==RUNNING){
      //check if the timer is elapsed
      if(timer.read()==timerInterval){
        Serial.println("Timer Time Reached");
        timer.stop();
        //if the soil moisture level is low
        if(map(analogRead(sSensor),3500,0,1,100)<=moisturePercentage){
          DynamicJsonDocument doc(64);
          deserializeJson(doc, apiDataGet());
          int weatherCode = doc["current"]["condition"]["code"];
          Serial.println(weatherCode);
          //if its not going to rain
          if (weatherCode !=1063||weatherCode !=1180||weatherCode !=1183||weatherCode !=1186||weatherCode !=1189||weatherCode !=1192||weatherCode !=1195||weatherCode !=1198||weatherCode !=1201||weatherCode !=1204||weatherCode !=1240||weatherCode !=1243||weatherCode !=1246||weatherCode !=1273||weatherCode !=1276){
            wateringFlag=true;
            Serial.println("watering flag set");
          }
        }
      }
    }
    //if the watering flag is set
    if(wateringFlag==true&&WaitTimer.state()==STOPPED)
    {
      //get the moisture reading and if its lower than whats needed 
      int currwaterLevel=map(analogRead(sSensor),3500,0,1,100);
      if(currwaterLevel < moisturePercentage){
        //check if the pump is not already running
        if(Watering.state()==STOPPED){
          Watering.start();
          //digitalWrite(mRelay,LOW);
          ledcWrite(ledChannel, 255);
          Serial.println("starting Watering");
        }
        //if the pump is running already
        else{
          if(Watering.read()>=5000)
          {
            Watering.stop();
            //digitalWrite(mRelay,HIGH);
            ledcWrite(ledChannel, 0);
            Serial.println("Stopping Watering");
            WaitTimer.start();
          }
        }
      }
      else{
        Watering.stop();
        //digitalWrite(mRelay,LOW);
        ledcWrite(ledChannel, 255);
        Serial.println("Stopping Watering");
        WaitTimer.start();
      }
    }
    //if the wait timer is running and the time is elapsed then turn it off
    if(WaitTimer.state()==RUNNING){
      if(WaitTimer.read()>=5000){
        Serial.println("Stopping wait timer");
        WaitTimer.stop();
        if(map(analogRead(sSensor),3500,0,1,100)>=moisturePercentage){
          wateringFlag=false;
          Serial.println("Watering Stopped");
          //digitalWrite(mRelay,HIGH);
          ledcWrite(ledChannel, 0);
        }
      }
    }
  }
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
    server.on(F("/sleep"),HTTP_POST,enDeepSleep);
    server.on(F("/sensors"),HTTP_GET,sensorData);
    server.on(F("/weather"),HTTP_GET,apiDataSend);
    server.on(F("/startSystem"),HTTP_POST,algorithmControl);

}

void setup() {
  Serial.begin(115200);
  dht.begin();
  //pinMode(mRelay,OUTPUT);
  //digitalWrite(mRelay,HIGH);
  preferences.begin("credentials", true);
  String ssid = preferences.getString("ssid","0");
  String pass = preferences.getString("password","0");
  preferences.end();
  handleConnection(ssid,pass);

  //assigning a hostname to the device
  String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
  if (MDNS.begin(chipId.c_str())) {
    Serial.println("MDNS responder started");
    Serial.println(chipId);
  }
  // Set server routing
  restServerRouting();
  // Set not found response
  server.onNotFound(handleNotFound);
  // Start server
  server.begin();
  Serial.println("HTTP server started");
  configTime(0, 0, "pool.ntp.org");
  preferences.begin("settings");
  moisturePercentage = preferences.getInt("soilMoisture",60);
  timerInterval = preferences.getDouble("timerInterval",10000);
  activateSystem = preferences.getBool("activateSystem",false);
  preferences.end();


  ledcSetup(ledChannel, freq, resolution);

  ledcAttachPin(mRelay, ledChannel);
  ledcWrite(ledChannel, 0);

  Serial.println(moisturePercentage);
  Serial.println(timerInterval);

}

void loop() {
  server.handleClient();
  wateringAlgorithm();
  //Serial.println(map(analogRead(sSensor),3500,0,1,100));
}