/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//This program is a tool for photographers looking to photograph stars in optimal conditions.
//There are a plethora of resources to access light pollution but not many to get cloud coverage and the phase of the moon, both of which can ruin astrophotographs.
//An issue that I struggled with was working with HTTPS (WiFiClientSecure) and JSON parsing.
//JSON parsing initially worked, but stopped working (despite the code being 100% the same and untouched).
//Luckily the API I was using to retrieve the forecast let me use HTTP (even though the documentation indicated to use HTTPS).
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////
//Integrate all the necessary libraries and files.
///////////////////////////////////////////////////
#include "config.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SPI.h>
#include "Adafruit_TSL2591.h"


/////////////////////////////////////////////////////////////
//Create an instance of the TSL2591 light sensor called tsl.
/////////////////////////////////////////////////////////////
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Create variables to store the value from light sensor as well as a flag so that values are published only once every time luminosity falls below 200.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int luminosity = 0;
boolean nightFlag = false;


//////////////////////////////////////////////////////////////////////////////////////////////
//Use the unique MAC address of the microcontroller as an identifier when connecting to MQTT.
//////////////////////////////////////////////////////////////////////////////////////////////
char mac[6];


//////////////////////////////////////////////////////////////////////
//Create a character array to store the JSON data to be sent to MQTT.
//////////////////////////////////////////////////////////////////////
char message[201];


///////////////////////////////////////////////////////////////
//Create instance of WiFiClient and use it to connect to MQTT.
///////////////////////////////////////////////////////////////
WiFiClient espClient;
PubSubClient mqtt(espClient);


/////////////////////////////////////////////////////////
//Create structs to store the information from the APIs.
/////////////////////////////////////////////////////////
typedef struct {
  String ip;
  String cc;
  String cn;
  String rc;
  String rn;
  String cy;
  String ln;
  String lt;
} GeoData;
GeoData location;
typedef struct {
  String cc;
  String mp;
} MetData;
MetData weather;


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Ensure that every the serial monitor, WiFi, light sensor, and MQTT are initialized at the start every time.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  Serial.print("connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("Starting Adafruit TSL2591 Test!"));
  if (tsl.begin()) 
  {
    Serial.println(F("Found a TSL2591 sensor"));
  } 
  else 
  {
    Serial.println(F("No sensor found ... check your wiring?"));
    while (1);
  }
  displaySensorDetails();
  configureSensor();

  Serial.println(F(""));
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  mqtt.setServer(mqtt_server, 1883);
  Serial.println(F(""));
}


///////////////////////////////////////////////
//Reconnect protocol in case MQTT disconnects.
///////////////////////////////////////////////
void reconnect() {
  while (!mqtt.connected()) {
    Serial.println(F(""));
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect(mac, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      mqtt.subscribe("steven/+");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Continuously read from the light sensor when it gets dark and maintain the state of the flag accordingly.
////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  simpleRead(); 
  //advancedRead();
  // unifiedSensorAPIRead();
  
  if(luminosity <= 200 && nightFlag == false) {  
    nightFlag = true;  
    if (!mqtt.connected()) {
    reconnect();
    }
    mqtt.loop();
    
    getMet();
    
    Serial.println("Cloud Cover: " + weather.cc + "%");
    Serial.println("Portion of Moon Illuminated: " + weather.mp);
    
    sprintf(message, "{\"Cloud Cover\":\"%s\", \"Moon Phase\":\"%s\"}", weather.cc.c_str(), weather.mp.c_str());
    mqtt.publish("steven/astro", message);
    Serial.println("Flag was set to true.");
    Serial.println(F(""));
  }
  
  if (luminosity >= 200 && nightFlag == true) {
    nightFlag = false;
    Serial.println("Flag was set to false.");
    Serial.println(F(""));
  }
  
  delay(5000);
}


//////////////////////////////////
//Function to get the IP address.
//////////////////////////////////
String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json");
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {

      DynamicJsonBuffer jsonBuffer;

      String payload = theClient.getString();
      JsonObject& root = jsonBuffer.parse(payload);
      ipAddress = root["ip"].as<String>();

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  Serial.println("End of getIP");
  return ipAddress;
}


////////////////////////////////////////////////////////////////
//Function to get the latitude and longitude of the IP address.
////////////////////////////////////////////////////////////////
void getGeo() {
  String ipAddress = getIP();
  HTTPClient theClient;
  Serial.println(F(""));
  Serial.println("Making HTTP request to get location.");
  theClient.begin("http://api.ipstack.com/" + ipAddress + "?access_key=" + geo_Key);
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload for location.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }

      location.ip = root["ip"].as<String>();
      location.cc = root["country_code"].as<String>();
      location.cn = root["country_name"].as<String>();
      location.rc = root["region_code"].as<String>();
      location.rn = root["region_name"].as<String>();
      location.cy = root["city"].as<String>();
      location.lt = root["latitude"].as<String>();
      location.ln = root["longitude"].as<String>();
      
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}


///////////////////////////////////////////////////////////////////////////////////
//Function to get the cloud cover and moon phase of the location of the IP address.
///////////////////////////////////////////////////////////////////////////////////
void getMet() {
  getGeo();
  
  String url = "/v2.0/forecast/daily?days=1&lat=" + location.lt + "&lon=" + location.ln + "&key=" + weather_Key;
  HTTPClient theClient;
  Serial.println(F(""));
  Serial.println("Making HTTP request to get weather.");
  theClient.begin("http://api.weatherbit.io" + url);
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload for weather.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Payload: " + payload);
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }

      weather.cc = root["data"][0]["clouds"].as<String>();
      weather.mp = root["data"][0]["moon_phase"].as<String>();
      
    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
    }
  }
}


//////////////////////////////////////////////////////////////////////////////////////////////
//The rest of the code pertains to the light sensor and is needed for its full functionality.
//////////////////////////////////////////////////////////////////////////////////////////////
void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Sensor:       ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:   ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:    ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:    ")); Serial.print(sensor.max_value); Serial.println(F(" lux"));
  Serial.print  (F("Min Value:    ")); Serial.print(sensor.min_value); Serial.println(F(" lux"));
  Serial.print  (F("Resolution:   ")); Serial.print(sensor.resolution, 4); Serial.println(F(" lux"));  
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
  delay(500);
}

void configureSensor(void)
{
  // You can change the gain on the fly, to adapt to brighter/dimmer light situations
  //tsl.setGain(TSL2591_GAIN_LOW);    // 1x gain (bright light)
  tsl.setGain(TSL2591_GAIN_MED);      // 25x gain
  //tsl.setGain(TSL2591_GAIN_HIGH);   // 428x gain
  
  // Changing the integration time gives you a longer time over which to sense light
  // longer timelines are slower, but are good in very low light situtations!
  //tsl.setTiming(TSL2591_INTEGRATIONTIME_100MS);  // shortest integration time (bright light)
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_200MS);
  tsl.setTiming(TSL2591_INTEGRATIONTIME_300MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_500MS);
  // tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);  // longest integration time (dim light)

  /* Display the gain and integration time for reference sake */  
  Serial.println(F("------------------------------------"));
  Serial.print  (F("Gain:         "));
  tsl2591Gain_t gain = tsl.getGain();
  switch(gain)
  {
    case TSL2591_GAIN_LOW:
      Serial.println(F("1x (Low)"));
      break;
    case TSL2591_GAIN_MED:
      Serial.println(F("25x (Medium)"));
      break;
    case TSL2591_GAIN_HIGH:
      Serial.println(F("428x (High)"));
      break;
    case TSL2591_GAIN_MAX:
      Serial.println(F("9876x (Max)"));
      break;
  }
  Serial.print  (F("Timing:       "));
  Serial.print((tsl.getTiming() + 1) * 100, DEC); 
  Serial.println(F(" ms"));
  Serial.println(F("------------------------------------"));
  Serial.println(F(""));
}

void simpleRead()
{
  // Simple data read example. Just read the infrared, fullspecrtrum diode 
  // or 'visible' (difference between the two) channels.
  // This can take 100-600 milliseconds! Uncomment whichever of the following you want to read
  uint16_t x = tsl.getLuminosity(TSL2591_VISIBLE);
  //uint16_t x = tsl.getLuminosity(TSL2591_FULLSPECTRUM);
  //uint16_t x = tsl.getLuminosity(TSL2591_INFRARED);

  Serial.print(F("[ ")); Serial.print(millis()); Serial.print(F(" ms ] "));
  Serial.print(F("Luminosity: "));
  Serial.println(x, DEC);

  luminosity = x;
}

void unifiedSensorAPIRead(void)
{
  /* Get a new sensor event */ 
  sensors_event_t event;
  tsl.getEvent(&event);
 
  /* Display the results (light is measured in lux) */
  Serial.print(F("[ ")); Serial.print(event.timestamp); Serial.print(F(" ms ] "));
  if ((event.light == 0) |
      (event.light > 4294966000.0) | 
      (event.light <-4294966000.0))
  {
    /* If event.light = 0 lux the sensor is probably saturated */
    /* and no reliable data could be generated! */
    /* if event.light is +/- 4294967040 there was a float over/underflow */
    Serial.println(F("Invalid data (adjust gain or timing)"));
  }
  else
  {
    Serial.print(event.light); Serial.println(F(" lux"));
  }
}
