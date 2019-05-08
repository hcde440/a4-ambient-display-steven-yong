/////////////////////////////////////////////////////////////////////////
//Include all the necessary librarites for WiFi, MQTT, and parsing JSON.
/////////////////////////////////////////////////////////////////////////
#include "config.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


////////////////////////////////////////////////////////
//Set the topic to subscribe to as well as the LED pin.
////////////////////////////////////////////////////////
#define topic_name "steven/astro"
#define LED_PIN 5


//////////////////////////////////////////////////////////////////////
//Create instances of WiFiClient and PubSubClient to connect to MQTT.
//////////////////////////////////////////////////////////////////////
WiFiClient espClient;
PubSubClient client(espClient);


///////////////////////////////////////////////////////////
//Create float variables to store the JSON data from MQTT.
///////////////////////////////////////////////////////////
float cc = 0;
float mp = 0;


///////////////////////////////////////////////////////////////////////////////////////
//Initialize the LED pin (as an output), serial monitor, WiFi, and connection to MQTT.
///////////////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}


//////////////////////////////
//Function to start the WiFi.
//////////////////////////////
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//Function to process the MQTT response and turn on the LED when it's a good time to take photos of stars.
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  DynamicJsonBuffer  jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(payload);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  cc = root["Cloud Cover"];
  mp = root["Moon Phase"];
  Serial.println("Cloud Cover: " + String(cc) + "%");
  Serial.println("Portion of Moon Illuminated: " + String(mp));

  if(cc <= 10 && mp <= 0.1) {
    Serial.println("It is prime time for astrophotography.");
    digitalWrite(LED_PIN, HIGH);
  }
  if(cc >= 10 && mp >= 0.1) {
    Serial.println("It is cloudy and the moon is bright.");
    digitalWrite(LED_PIN, LOW);
  }
  if(cc >= 10 && mp <= 0.1) {
    Serial.println("It is cloudy.");
    digitalWrite(LED_PIN, LOW);
  }
  if (cc <= 10 && mp >= 0.1) {
    Serial.println("The moon is bright.");
    digitalWrite(LED_PIN, LOW);
  }
}


/////////////////////////////////////////////////////////
//Function to reconnect to MQTT if disconnection occurs.
/////////////////////////////////////////////////////////
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_server, mqtt_user, mqtt_password)) { //<-- a unique name, please.
      Serial.println("connected");
      client.subscribe(topic_name);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


////////////////////////////////////////////////////
//Continuously loop to maintain connection to MQTT.
////////////////////////////////////////////////////
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(5000);
}
