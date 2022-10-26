/*
 * A Code to monitor my minigreen house for seedlings.
 * This code was specifically written for, and tested on, an ESP32 Dev Module
 * Copyright (c) 2022, Erik C. Nykwest All rights reserved.
 * This source code is licensed under the GPL-style license 
 * found in the LICENSE file in the root directory of this source tree.
 *  
 *  It measures both the Temperature and Humidity and sends reports to both Blynk and Smartnest.
 *  Smartnest is connected to the Smart Life App via IFTTT to turn on and off 
 *  my wireless heating pad via a smart outlet.
 *  Blynk is used to log and plot Temp and humidity vs time on my smart phone 
 *  as well as send me warnings if things aren't working.
 *  The Servo contained in my kit physically CANNOT go to 0 degrees 
 *  due to a physical peg on the gear
 *  But thats OK because it's a cheap servo and electronic zero is not REAL zero
 *  The real 180 degree range is 22 to 94 to 177
 *  The functional range is 16 to 180
*/

// Include Packages
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <BlynkSimpleEsp32.h> // Control via the Blynk App
#include <SimpleDHT.h>
#include <ESP32Servo.h>

// Your WiFi credentials.
#define SSID_NAME "<CUSTOMIZE>"                   // Your Wifi Network name
#define SSID_PASSWORD "<CUSTOMIZE>"           // Your Wifi network password
// MQTT credentials
#define MQTT_BROKER "smartnest.cz"              // Broker host
#define MQTT_PORT 1883                          // Broker port
#define MQTT_USERNAME "<CUSTOMIZE>"                // Username from Smartnest
#define MQTT_PASSWORD "<CUSTOMIZE>"                // Password from Smartnest (or API key)
#define MQTT_CLIENT "<CUSTOMIZE>"                 // Device Id from smartnest

// Blynk Settings
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
#define BLYNK_AUTH "<CUSTOMIZE>" // Real Themostate Project

/* Debug
#define BLYNK_AUTH "<CUSTOMIZE>" // Fake DHT11 Sensor for Debuging
// Fake DHT11 
float temperature = 0;
int humidity = 0;
BLYNK_WRITE(V3) // V3 is the number of Virtual Pin  
{
  temperature = param.asInt();
}
*/

// for DHT11, 
//      VCC: 5V or 3V
//      GND: GND
//      DATA: 2
#define pinDHT11 15
SimpleDHT11 dht11;

// Define Variables
int lightPin=2;
int prevTemp = 0; // Used to detect changes in temperature
                  // so we aren't constantly sending mqtt messages

#define pinServo 13
int servoPos=91; // Start Closed
#define targetTemp 24 // Target temp for servo cooling
Servo myservo;  // create servo object to control a servo


WiFiClient espClient; // create a Wifi Client (called espClient)
                      // that can connect to a specific IP
PubSubClient client(espClient); // Tell PubSub which client to use


void setup() {
  Serial.begin(115200);
  pinMode(lightPin, OUTPUT);

  startWifi();
  startMqtt();
    
  // Connect to Blynk Server
  Blynk.begin(BLYNK_AUTH, SSID_NAME, SSID_PASSWORD);

  // Attach Servo
  myservo.attach(pinServo);

}


void loop() {
  // Run Blynk
  Blynk.run();

  // Check MQTT connection
  if(!client.connected()){
    startMqtt();
  }
  // Run MQTT
  client.loop();
  
  // Read Temp and Humidity from DHT11
  Serial.println("=================================");
  Serial.println("Sample DHT11...");


  // read DHT11 and save data to the variables
  // stored in comment block so I can toggle this off for debuging
  ///*  
  byte temperature = 0;
  byte humidity = 0;
  byte data[40] = {0};
  
  if (dht11.read(pinDHT11, &temperature, &humidity, data)) {
    Serial.println("Read DHT11 failed");
    delay(1000);
    //return; // seems to be the equivalent of "continue" in python
              // commented out bc errors shouldn't pass silently
  }
  
  // (Optional) Print DHT11 data stream (binary?)
  Serial.print("Sample RAW Bits: ");
  for (int i = 0; i < 40; i++) {
    Serial.print((int)data[i]);
    if (i > 0 && ((i + 1) % 4) == 0) {
      Serial.print(' ');
    }
  }
  Serial.println("");
  //*/
  
  
  Serial.print("Sample OK: ");
  Serial.print((float)temperature); Serial.print(" *C, ");
  Serial.print((int)humidity); Serial.println(" %");
  
  // Send Data to Blynk
  Serial.print("Sending Data to Blynk: ");
  Blynk.virtualWrite(V1, (float)temperature);
  Blynk.virtualWrite(V2, (int)humidity);

  // If temperature change detected, report it to Smartnest
  if ((int)temperature != prevTemp){
    Serial.print("Change in Temperature Detected... ");
    prevTemp = (int)temperature;
    
    // Convert the temperature from a number to a text value for mqtt
    char textTemp[4] = "124";
    sprintf(textTemp,"%i",(int)temperature);
  
    // Send New Temp to SmartHome (QMTT)
    char topic[100];
    sprintf(topic,"%s/report/temperature",MQTT_CLIENT);
    client.publish(topic, textTemp);
    Serial.println("Report sent to Smartnest");

  }
  

  // Adjust Servo to open or close roof
  if (temperature > 27) {
    servoPos=17;
  } else if (temperature < 20) {
    servoPos=91;
    
    // If we are NOT at maximum open or maximum close
  } else {
    int ds= targetTemp - (int)temperature;
    char message[40];
    sprintf(message,"Adjusting Servo by %d degrees",ds);
    Serial.println(message);
    servoPos= servoPos + ds;

    // Prevent bad servo positions
    if (servoPos < 17){
      servoPos=17;
    } else if (servoPos > 91){
      servoPos=91;
    }
  }
  
  // Update Servo Position
  myservo.write(servoPos);
  
  
  // DHT11 sampling rate is 1/6HZ. (Faster Sampling Is prone to errors)
  delay(6000);
}



// ----- Function Definitions-----

// What should happen when new message has been received
void callback(char* topic, byte* payload, unsigned int length) { //A new message has been received
  Serial.println("Receiving Message...");
  Serial.print("Topic:");
  Serial.println(topic);
  
  //Split the Topic into it's individual tokens
  int tokensNumber=10;
  char *tokens[tokensNumber];
  splitTopic(topic, tokens, tokensNumber);
  
  // Convert Incoming Message from bytes to letters
  char message[length+1];
  sprintf(message,"%c",(char)payload[0]);
  for (int i = 1; i < length; i++) {
    //Serial.println(message); // debug step to demonstrate next line
    sprintf(message,"%s%c",message,(char)payload[i]); // Assemble the Message 1 letter at a time
  }
  Serial.print("Message:");
  Serial.println(message);
 
  //------------------ACTIONS HERE---------------------------------
  // Now that we have the message and topic (see split topic)
  // We can act on the command
  char myTopic[100];

  Serial.println("Debug1");
  // If told to turn ON, Turn on LED
  if(strcmp(tokens[1],"directive")==0 && strcmp(tokens[2],"powerState")==0){
    Serial.println("Sending Message...");
    sprintf(myTopic,"%s/report/powerState",MQTT_CLIENT);

    Serial.println("Debug2");
    if(strcmp(message,"ON")==0){
      digitalWrite(lightPin, HIGH); 
      client.publish(myTopic, "ON") ;            

    } else if(strcmp(message,"OFF")==0){
      Serial.println("Debug3");
      digitalWrite(lightPin, LOW);                   
      client.publish(myTopic, "OFF"); 

    }
  }
}


void startWifi(){
  
  // Connect to Wifi
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(SSID_NAME);
  WiFi.begin(SSID_NAME, SSID_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());

}


void startMqtt(){
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(callback); // What should happen when new message has been received

  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect(MQTT_CLIENT, MQTT_USERNAME, MQTT_PASSWORD )) {
      Serial.println("Connected");

    }
    else {
      if (client.state()==5){

        Serial.println("Connection not allowed by broker, possible reasons:");
        Serial.println("- Device is already online. Wait some seconds until it appears offline for the broker");
        Serial.println("- Wrong Username or password. Check credentials");
        Serial.println("- Client Id does not belong to this username, verify ClientId");
      
      }
      else {

        Serial.println("Not possible to connect to Broker Error code:");
        Serial.print(client.state());
      }
      
      Serial.println("Retrying in 30 Seconds");
      delay(0x7530);
    }
  }

  // Subscribe to everything
  char topic[100];
  //sprintf(topic,"%s/#",MQTT_CLIENT);
  //client.subscribe(topic);

  // Subscribe to commands
  sprintf(topic,"%s/directive/#",MQTT_CLIENT);
  client.subscribe(topic);

  // Subscribe to Reports
  //sprintf(topic,"%s/report/#",MQTT_CLIENT); // format a string and save it to the variable "topic"
  //client.subscribe(topic);

  // Publish that the device is online
  sprintf(topic,"%s/report/online",MQTT_CLIENT);
  client.publish(topic, "true");

  // Publish that the device is using Celcius
  sprintf(topic,"%s/report/scale",MQTT_CLIENT);
  client.publish(topic, "C");
  
}


int splitTopic(char* topic, char* tokens[],int tokensNumber ){
  // Split the full topic into all of it's pieces (tokens) using the delimiter "/"
  // weirdly this returns the number of tokens, but not the tokens themselves
  // accodingly this PROBABLY writes to a global variable "tokens"
  
  const char s[2] = "/";
  int pos=0;
  
  // tokens is a list of every keyword in the topic, e.g.: home/garden/temp -> [home, garden, temp]
  // strtok parses the string "topic" based on the delimiter "/"
  // HOWEVER, strtok only returns 1 chunk (token) at a timeso you have to use a loop to pull all tokens
  tokens[0] = strtok(topic, s); // The C library function char *strtok(char *str, const char *delim) breaks string str into a series of tokens using the delimiter delim

  // The first call to strtok must pass the C string to tokenize, 
  // and subsequent calls must specify NULL as the first argument,
  // which tells the function to continue tokenizing the string you passed in first. 
  while(pos<tokensNumber-1 && tokens[pos] != NULL ) {
      pos++;
    tokens[pos] = strtok(NULL, s);
  }

  return pos;   
}
