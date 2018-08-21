// Koodi, jolla master vastaanottaa radiolla dataa slaveilta ja lähettää
// sen eteenpäin serverille wifi managerin määrittämän wifin kautta
// Tukee jännitteen mittausta

// rf-moduuli NodeMCU:n pinneihin:
// V+ -> 3.3V
// GND -> GND
// CSN -> D8 (pin 15)
// CE -> D4 (pin 2)
// MOSI -> D7 (HMOSI)
// SCK -> D5 (HSCLK)
// RQ ei kytketty
// MISO -> D6 (HMISO)

#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager
/*
 * Tama mahdollistaa siis sen, etta voidaan kannykan/lapparin yms. avulla maarittaa nodemcu:n laheisyydessa sille wifi
 * kayttaen esp8266 muodostamaa tukiasemaa
 * Kun Master moduuli kaynnistetaan uudelleen, moduuli yrittaa ensin yhdistaa kayttaen tata wifia,
 * jos yhdistaminen ei onnistu, voidaan taas maarittaa uusi wifi
 */

// DEFINE BASIC VARIABLES
const uint8_t len = 16;
const uint8_t ack_len = 7;
char id_array[2];
const uint8_t id_len = 2;

unsigned long loop_time = 0;
unsigned long loop_max = 5000;
bool problem = false;
bool request = false;

float minVoltMeasurement = 3.0;
float maxVoltMeasurement = 4.4;


RF24 radio(2,15); // ce, csn

//multicommunication addresses
uint8_t addresses[][6] = {"Node1","Node2","Node3","Node4","Node5","Node6","Node7", "Node8", "Node9"};
//uint8_t address[][7] = {"Master","Slaves"};

// these are used to determine correct slave, can be altered
char id_list[][7] = {"device","sensor","projec","common","argume","return", "monito"};

// THESE must match system settings
short slaves = 1; //tells first free id, starts at 1

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setup(void){
 Serial.begin(115200); //debugging only
 
 radio.begin();
 radio.setDataRate(RF24_250KBPS);
 radio.setChannel(108); //2.508 GHz
 radio.openReadingPipe (1,addresses[0]); //master address
 delay(100);

 WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
    
  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass from eeprom and tries to connect those
  //if it does not connect it starts an access point with the specified name and passwd
   
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("Master_wifi","Consair-proto"); // wifi ssid and passwd
     
  //if you get here you have connected to the WiFi
  Serial.println("wifi connection established!)");
 
 }
 
 void loop(void){
  
  problem = false;

   // wait for a slave to write
   radio.startListening();

   while(!radio.available()){
  
    Serial.println("Waiting for radio connection");
    delay(500);
     
    }
    
  // read sensor data
  if (radio.available()){
    char data[16];
    radio.read(data, len);   
    delay(10);
    Serial.println(data); //debug use only
    
    radio.flush_rx();
    radio.flush_tx();

    // write correct info to the slave
    radio.stopListening();
    //String data_string = String(data);

    // check if data contains id request
    if (String(data) == "request"){
      Serial.println("ID REQUEST"); // debug only
      // a new slave requested id
      String id_string = String(slaves);
      id_string.toCharArray(id_array,id_len);
      Serial.println(id_array);
      radio.openWritingPipe(addresses[1]);
      radio.write(id_array,id_len);
      // expect the slave to answer
      radio.startListening();
      delay(10);
      loop_time = millis(); //used to stop loop in error situation
      problem = false;
     
      while(!radio.available()){
        if (loop_time + loop_max < millis()){
          problem = true;
          Serial.println("Error connecting to a new slave");
          break;
        }
       
      }

      if (!problem)
      {
       
        radio.read(data, len);
        if (data[0] - '0' == slaves)slaves++; // a new slave added succesfully
        else problem = true; // wrong slave connected
        // slave should be disconnected and then reconnected
        // problem should be transmitted to the server
       
      }
   
   
   

    }

    else{

      // a slave sent information
      // Serial.println(data[0]);
      // inform slave that the message was succesfully received
      radio.openWritingPipe(addresses[(data[0]-'0'+1)]); // slaves' address
      // 1st reserved for master and 2nd for the first connections. slaves' ids start from 1
      Serial.println(id_list[data[0]-'1']);
    // Serial.println((int)data[0]);
      for (int i = 0; i<3;i++){
        radio.write(id_list[data[0]-'1'], ack_len); // write standard ack which matches slave id
        radio.flush_rx();
        radio.flush_tx();
        // writing ack 3 times seems to help slave to get the ack
        
        delay(40);
      }
      /*
        *  This section should contain the code which sends data to the server
        *  It's good to check first that duplicates are not trasferred to the server
        * use timer to check enough time has elapsed since this slave has send data
        *    
        * Also if problem = true
        *   server should be informed that there is a connection problem between
        *   slave and master
        */
        
    // Separate the incoming data to sender id and sensor measurement data
    String id = getValue(data,';',0);
    String dust = getValue(data,';',1);
    String temp = getValue(data,';',2);
    String humd = getValue(data,';',3);
    String light = getValue(data,';',4);
    String voltsString = getValue(data,';',5);
    //String voltsString = String(38); // testaus
    
    float volts = voltsString.toFloat()/10.0;

    // Prepare the string that is sent to the server
    String login_s = "username : testi, password : prototesti";
    String device_id_s = ", device_id :";
    String dust_s = ", dust :";
    String temp_s = ", temp :";
    String humd_s = ", humd :";
    String light_s = ", light :";
    String volts_s = ", volts :";
    
    if(volts > minVoltMeasurement || volts < maxVoltMeasurement){
      String message = login_s + device_id_s + id + dust_s + dust + temp_s + temp + humd_s + humd + light_s + light + volts_s + String(volts);//String(volts.toFloat()/10.0);
    }
    else{
      String message = login_s + device_id_s + id + dust_s + dust + temp_s + temp + humd_s + humd + light_s + light;
    }

    // Send the data to the server
    if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
 
      HTTPClient http;    //Declare object of class HTTPClient
 
      http.begin("http://vikke.me/send_string/"); //"http://jsonplaceholder.typicode.com/users");      //Specify request destination
      http.addHeader("Content-Type", "application/json"); //"text/plain");  //Specify content-type header

      int httpCode = http.POST(message); //"Message from ESP8266");   //Send the request
      String payload = http.getString();                  //Get the response payload

      Serial.println("httpCode:");
      Serial.println(httpCode);   //Print HTTP return code
      Serial.println("payload:");
      Serial.println(payload);    //Print request response payload
      Serial.println(" ");
 
      http.end();  //Close connection
 
    }else{
      Serial.println("Error in WiFi connection");   
    }
  }
 }
}
