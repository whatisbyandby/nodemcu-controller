#line 1 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>

#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

const String UUID = "aabe63e0-576e-4993-8e57-e15a8528f46f";
const String chipId = String(ESP.getChipId(), HEX);
String topic;

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensors(&oneWire);

float setTemp = 60.0;
float tempRange = 1.0;

const char *ssid = "PerkyNet";
const char *password = "phobicstreet139";

ESP8266WebServer server(80);
WebSocketsClient webSocket;
ESP8266WiFiMulti WiFiMulti;

enum state
{
  HEATER,
  COOLER,
  CORRECT,
  ERR
};

state currentState = CORRECT;
bool running = false;

int heaterPin = 12;
int coolerPin = 13;

unsigned long startMillis;
unsigned long currentMillis;
unsigned long dataInterval = 1000;

//END OF GLOBAL VARIABLES

#line 56 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void initalizeWebsocket();
#line 63 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
#line 94 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void handleGetRequest();
#line 112 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void handlePostRequest();
#line 156 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void setup();
#line 198 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
state compareTemps(float currentTemperature);
#line 226 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void setNewState(state newState);
#line 245 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void turnHeaterOn();
#line 251 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void turnCoolerOn();
#line 257 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void turnAllOff();
#line 263 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void getNewData();
#line 311 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void loop();
#line 56 "/Users/scottperkins/Documents/BreweryControllers/nodemcu-controller/nodemcu-server.ino"
void initalizeWebsocket(){
  webSocket.begin("192.168.0.13", 8765, "/ws/topic/" + topic + "/asset/" + UUID);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[WSc] Disconnected!\n");
    break;
  case WStype_CONNECTED:
  {
    Serial.printf("[WSc] Connected to url: %s\n", payload);
  }
  break;
  case WStype_TEXT:
    Serial.printf("[WSc] get text: %s\n", payload);
    break;
  case WStype_BIN:
    Serial.printf("[WSc] get binary length: %u\n", length);
    hexdump(payload, length);
    break;
  case WStype_PING:
    // pong will be send automatically
    Serial.printf("[WSc] get ping\n");
    break;
  case WStype_PONG:
    // answer to a ping we send
    Serial.printf("[WSc] get pong\n");
    break;
  }
}

void handleGetRequest()
{
  DynamicJsonDocument doc(1024);
  JsonObject responseObj = doc.to<JsonObject>();

  responseObj["running"] = running;
  responseObj["topic"] = topic;
  responseObj["setTemp"] = setTemp;
  responseObj["tempRange"] = tempRange;
  responseObj["heaterPin"] = heaterPin;
  responseObj["coolerPin"] = coolerPin;
  responseObj["dataInterval"] = dataInterval;

  String newState;
  serializeJson(responseObj, newState);
  server.send(200, "application/json", newState);
}

void handlePostRequest()
{
  String body = server.arg("plain");
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, body);

  // Test if parsing succeeds.
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  bool newRunning = doc["running"];
  running = newRunning != NULL ? newRunning : running;
  float newSetTemp = doc["setTemp"];
  setTemp = newSetTemp > 1 ? newSetTemp : setTemp;
  float newTempRange = doc["tempRange"];
  tempRange = newTempRange > 0.1 ? newTempRange : tempRange;
  float newDataInterval = doc["dataInterval"];
  dataInterval = newDataInterval > 1 ? newDataInterval : dataInterval;
  String newTopic = doc["topic"];
  if (topic != newTopic && newTopic != NULL){
    topic = newTopic;
    initalizeWebsocket();
  }
  
  DynamicJsonDocument responseDoc(1024);
  JsonObject responseObj = responseDoc.to<JsonObject>();

  responseObj["running"] = running;
  responseObj["topic"] = topic;
  responseObj["setTemp"] = setTemp;
  responseObj["tempRange"] = tempRange;
  responseObj["heaterPin"] = heaterPin;
  responseObj["coolerPin"] = coolerPin;
  responseObj["dataInterval"] = dataInterval;

  String newState;
  serializeJson(responseObj, newState);
  server.send(200, "application/json", newState);
}

void setup()
{
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  pinMode(heaterPin, OUTPUT);
  pinMode(coolerPin, OUTPUT);
  startMillis = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/config", []() {
    HTTPMethod method = server.method();
    switch (method)
    {
    case HTTP_GET:
      handleGetRequest();
      break;
    case HTTP_POST:
      handlePostRequest();
      break;

    default:
      server.send(404, "text/plain", "Method not allowed");
      break;
    }
  });

  server.begin();

  sensors.begin();
}

state compareTemps(float currentTemperature)
{
  if (currentTemperature >= setTemp && currentState == COOLER)
  {
    currentState = COOLER;
  }
  else if (currentTemperature <= setTemp && currentState == HEATER)
  {
    currentState = HEATER;
  }
  else if (currentTemperature > setTemp + tempRange)
  {
    currentState = COOLER;
  }
  else if (currentTemperature < setTemp - tempRange)
  {
    currentState = HEATER;
  }
  else if (currentTemperature > 100 || currentTemperature < 0)
  {
    currentState = ERR;
  }
  else
  {
    currentState = CORRECT;
  }
}

void setNewState(state newState)
{
  switch (newState)
  {
  case COOLER:
    turnCoolerOn();
    break;
  case HEATER:
    turnHeaterOn();
    break;
  case CORRECT:
    turnAllOff();
    break;
  default:
    turnAllOff();
    break;
  }
}

void turnHeaterOn()
{
  digitalWrite(heaterPin, HIGH);
  digitalWrite(coolerPin, LOW);
}

void turnCoolerOn()
{
  digitalWrite(coolerPin, HIGH);
  digitalWrite(heaterPin, LOW);
}

void turnAllOff()
{
  digitalWrite(heaterPin, LOW);
  digitalWrite(coolerPin, LOW);
}

void getNewData()
{
  DynamicJsonDocument doc(1024);
  JsonObject obj = doc.to<JsonObject>();

  sensors.requestTemperatures();
  float currentTemp = sensors.getTempFByIndex(0);

  state newState = compareTemps(currentTemp);
  setNewState(newState);

  JsonArray fields = obj.createNestedArray("fields");

  DynamicJsonDocument stateDoc(256);
  JsonObject state = stateDoc.to<JsonObject>();
  state["value"] = int(newState);
  state["type"] = "int";
  state["key"] = "state";
  fields.add(state);

  DynamicJsonDocument currentTempDoc(256);
  JsonObject currentTempObj = currentTempDoc.to<JsonObject>();
  currentTempObj["value"] = currentTemp;
  currentTempObj["type"] = "float";
  currentTempObj["key"] = "currentTemp";
  fields.add(currentTempObj);

  DynamicJsonDocument setTempDoc(256);
  JsonObject setTempObj = setTempDoc.to<JsonObject>();
  setTempObj["value"] = setTemp;
  setTempObj["type"] = "float";
  setTempObj["key"] = "setTemp";
  fields.add(setTempObj);

  DynamicJsonDocument tempRangeDoc(256);
  JsonObject tempRangeObj = tempRangeDoc.to<JsonObject>();
  tempRangeObj["value"] = tempRange;
  tempRangeObj["type"] = "float";
  tempRangeObj["key"] = "tempRange";
  fields.add(tempRangeObj);

  String newJSON;
  serializeJson(obj, newJSON);

  webSocket.sendTXT(newJSON);
  startMillis = currentMillis;
}

void loop()
{
  currentMillis = millis();
  server.handleClient();
  webSocket.loop();
  if (currentMillis - startMillis >= dataInterval && running)
    {
      getNewData();
    }

}
