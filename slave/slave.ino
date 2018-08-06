/* 
 * Master must be turned on first 
 * Then turn slaves on, one by one
 * Master gives unique ids to slaves during the first connection
 * 
 * After the first connection slaves should send sensor values two times
 * because first is only used to confirm succesfull connection with the master
 * The second is the real sensor value transmission
 * 
 * At the moment code supports one master + 7 slaves
 */



// INCLUDES
#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <SoftwareSerial.h>
#include <Wire.h>

// i2c disp
#include <Arduino.h>
#include <U8x8lib.h>
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE); // constructor for the disp

 
// BASIC VARIABLES, defines
char display_messages [][8] = {"Conn...","Conn","Con Err", "Standby", "ID Err"};
char slave_addr = 0x2A; // sensor board uses this addr

#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];

#define dust_power_pin 5 // this is used to control dust sensor power
#define fan_power_pin 4 // this is used to control fan power
#define multi_sensor_power_pin 3 //this is used to control multisensor power 
// Notice: multi_sensor_power_pin needs to be on HIGH state when display is written to


// RADIO nRF24L01 VARIABLES


/* Arduino pins for the radio module:
 *  CSN = 6
 *  CE = 7
 *  MOSI = 11
 *  SCK = 13
 *  MISO = 12
 *  VCC = 3,3 V (demands a stable and good current supply)
 */

RF24 radio(7,6);//(CE pin) on the RF module, Chip Select (SCN pin)
bool check_connection(U8X8_SSD1306_128X32_UNIVISION_HW_I2C &u8x8,unsigned long loop_time, unsigned long loop_max, char message[]);
char data[16];// this is used to send data to master

char id_array[2];
const uint8_t id_len = 2;
const uint8_t ack_len = 7;
unsigned long curr_time = 0;
unsigned long interval = 52000; // 52s sleep time 
unsigned long dust_count = 0;
unsigned long loop_time = 0;
unsigned long loop_max = 3000; // waits master answer 3s
bool first_connection = true;
bool id_problem = true;
bool problem = false;
bool power_on = true;

// these are used as ack to double check correct slave send data, must match master's,
// any standard length strings could be used
char id_list[][7] = {"device","sensor","projec","common","argume","return","monito"};
short id ; // number of the sensor unit, master gives this
const uint8_t len = 16; // data package length


// Multipipe addresses
uint8_t addresses[][6] = {"Node1","Node2","Node3","Node4","Node5","Node6","Node7","Node8","Node9"};


// DUST SENSOR RELATED FUNCTIONS

SoftwareSerial PMSerial(8, 9); // RX, TX so connect dust sensor TX to 8 and RX to 9

char checkValue(unsigned char *thebuf, char leng);
int transmitPM0_3(unsigned char *thebuf);
int transmitPM0_5(unsigned char *thebuf);
int transmitPM1_0(unsigned char *thebuf);
int transmitPM2_5(unsigned char *thebuf);
int transmitPM5_0(unsigned char *thebuf);
int transmitPM10(unsigned char *thebuf);


// DUST SENSOR VARIABLES

int PM0_3Value=0;          // > 0_3 um particles / dl
int PM0_5Value=0;         // > 0_5 um particles / dl
int PM1_0Value=0;         // > 1_0 um particles / dl
int PM2_5Value=0;          // > 2_5 um particles / dl
int PM5_0Value=0;         // > 5_0 um particles / dl
int PM10Value=0;         // > 10 um particles / dl
uint8_t n = 0;
int P_tot = 0;



// SENSOR BOARD

// Pin 3, SCL, UNO A5
// Pin 4, SDA, UNO A4

// I2C bus demands good pin connection,
// bad connections cause error readings

// Temp sensor seems to work ok at least when temp < 70
// when temp > 70, humd may be < 0 %
// motion sensor value is not read -> motion led remains on


struct Sensor{
  int temp = 0;
  int humd = 0;
  int light = 0;
}sensor;

int base = 10;


// functions for sensor board

//String update_sensor_board(char slave_addr);
int read_temp(void);
int read_humd(void);
int read_light(void);
struct Sensor sensor_board(struct Sensor sensor,char slave_addr);
void manual_data_buffer(char *data, short id, int dust, struct Sensor sensor, int base);




void setup(void){
  Serial.begin(9600); //debugging only

  
  // Serial for dust sensor
  PMSerial.begin(9600);   
  PMSerial.setTimeout(1500);
  pinMode(dust_power_pin,OUTPUT); // dust sensor  power pin

  
  pinMode(fan_power_pin,OUTPUT); // fan board power pin
  pinMode(multi_sensor_power_pin,OUTPUT); // sensor board power pin
  digitalWrite(multi_sensor_power_pin,HIGH); // turn this on to write to display
  
  // init radio
  radio.begin();
  radio.setDataRate(RF24_250KBPS); // smaller trasnmit rate to better range
  radio.setChannel(108); //2.508 GHz

  delay(100);

 // init i2c display, oled library contains i2c bus init
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  
  u8x8.clear();
  u8x8.drawString(0,0,display_messages[0]);
  
  u8x8.refreshDisplay(); 
 }




 void loop(void){
 

  if (millis() > curr_time + interval || first_connection == true){


    // Turn on dust sensor to stabilize the measurements
    digitalWrite(dust_power_pin,HIGH); //dust sensor to normal operating mode
    delay(5000); // give time for the dust sensor to wake up correctly
   

    digitalWrite(multi_sensor_power_pin,HIGH); // sensor board to normal operating mode
    // sensor board also must be on when display is written to
    
    // update display
    u8x8.clear();
    u8x8.drawString(0,0,display_messages[0]);
    u8x8.refreshDisplay(); 
    
    
    
    digitalWrite(fan_power_pin,HIGH); // fan to normal operating mode

    // init global values
    n = 0;
    P_tot = 0;
    dust_count = millis();
        
    while(dust_count + 7000 > millis()){
      // give 7s measurement time for dust sensor and sensor board
      Serial.println(F("Measuring dust"));
      
       // update dust sensor values
      if(PMSerial.find(0x42)){    
        PMSerial.readBytes(buf,LENG);
    
    
        if(buf[0] == 0x4d){
          if(checkValue(buf,LENG)){
            // for now we just send one value to master, all particles scaled and mean taken
            PM0_3Value = transmitPM0_3(buf);
            n++;
            /*Serial.println(PM0_3Value);
            PM0_5Value = transmitPM0_5(buf);
            PM1_0Value = transmitPM1_0(buf);
            PM2_5Value = transmitPM2_5(buf);
            PM5_0Value = transmitPM5_0(buf);
            PM10Value = transmitPM10(buf);*/
            P_tot += PM0_3Value;
                        
          }  
        }
      }
      // update sensor board, sensor struct, this has to be done multiple times to get correct measurements
     sensor = sensor_board(sensor, slave_addr);
      
     }

     // scale (send as two ascii numbers) and take mean from dust measurenment
     P_tot = P_tot / (10 * n);
     Serial.println(P_tot);

    // Start radio again and turn off the dust sensor and fan
    digitalWrite(dust_power_pin,LOW);
    digitalWrite(fan_power_pin,LOW);
     
    power_on = true;
    
    while(power_on){
      
      radio.powerUp(); // power up radio, if radio already powered up -> does nothing
      
      if (first_connection)
      {
        // module needs to get an id from the master
        
        char request[8] = "request";
        radio.openReadingPipe(1,addresses[1]); // addresses[1] reserved for requests
        radio.openWritingPipe(addresses[0]);  // addresses[0] reserved for master
        radio.write(request,8);
        
        radio.startListening();
        loop_time = millis();
        
        problem = false; // tells if there is a connection problem to master
       
        /*while (! radio.available()){
          if (loop_time + loop_max < millis()){
              problem = true;
              Serial.println(F("Error connecting to the master"));
              
              //update display to inform user that there is a connection problem
              u8x8.clear();
              u8x8.drawString(0,0,display_messages[2]);
              u8x8.refreshDisplay();   
              break;        
           }
        }*/
        problem = check_connection(u8x8,loop_time,loop_max,display_messages[2]);
        
        if ( not problem){
         
          radio.read(id_array,id_len);
          delay(5);
          Serial.println(id_array);

          id = id_array[0] - '0'; // get the id as an integer
          // check id is ok
          if (id > 7 || id < 1)
          {
            // id error
             problem = true;
             Serial.println(F("ID Error, Noisy signal"));
              
             //update display to inform id error to user
             u8x8.clear();
             u8x8.drawString(0,0,display_messages[4]);
             u8x8.refreshDisplay();
          }
          else {
          first_connection = false;
          id_problem = false;        
      
          }
          
        }
      }
      
      // id problem means that slave hasn't gotten valid id from master during the first connection
      if ( not id_problem){
        // send sensor data to the master
      
        // create data buffer
        manual_data_buffer(data, id, P_tot, sensor, base);
      
        radio.openReadingPipe(1,addresses[id+1]); // 1st address reserved for master and 2nd for first connections
        
        radio.openWritingPipe(addresses[0]);
        radio.stopListening();
        radio.write(data,len); // send data to the master
        Serial.println(data);

        radio.flush_rx();
        radio.flush_tx();
        radio.startListening(); // listen the master

        loop_time = millis();
 
        problem = false;
        /*while(!radio.available()){

          if (millis() > (loop_time + loop_max)){
            // break the loop and report a problem
        
            Serial.println(F("Connection problem to the master"));
            problem = true;
            
            // update display to inform connection problem
            u8x8.clear();
            u8x8.drawString(0,0,display_messages[2]);
            u8x8.refreshDisplay(); 
            break;
          }
        }*/

        problem = check_connection(u8x8,loop_time,loop_max, display_messages[2]);
    
        if(not problem){
          // read master ack
          char ack[7];
    
          radio.read(ack, ack_len);
          Serial.println(ack); // debug use only

          if(strcmp(ack,id_list[id-1])== 0){
            // if strings are identical -> got correct ack
            // master got the message 
            //set radio and dust sensor to sleep mode
            radio.powerDown(); // reduce power usage
            power_on = false;
            curr_time = millis();
            
            // update display to inform activating standby mode
            u8x8.clear();
            u8x8.drawString(0,0,display_messages[3]);
            u8x8.drawString(0,2,"ID:");
            u8x8.drawString(3,2,id_array);
            
            u8x8.refreshDisplay();
            delay(10); //give time to update display
            digitalWrite(multi_sensor_power_pin,LOW); 
            }
        }
      }
    }
 
  }

 }

    
 // FUNCTIONS

bool check_connection(U8X8_SSD1306_128X32_UNIVISION_HW_I2C &u8x8,unsigned long loop_time, unsigned long loop_max, char message[]){
  while(!radio.available()){

    if (millis() > (loop_time + loop_max)){
      // return true, stands for connection problem
  
      Serial.println(F("Connection problem to the master"));
           
      // update display to inform connection problem
      u8x8.clear();
      u8x8.drawString(0,0,message);
      u8x8.refreshDisplay(); 
      return true;
    }
  }
  return false;
}
 /*void create_data_buffer(char *data,const uint8_t data_length,short id,int dust, String sensor_data){
  // THIS IS ONLY PARTIALLY IMPLEMENTED
  // short id: 0-9 sensor node number
  // dust: int, all particles
  // value separator ;
 

   String id_s = String(id);
   String dust_s = String(dust);
   //Serial.println(dust);
   //Serial.println(dust_s);
   //String data_string = id_s + ";" + dust_s + ";" + sensor_data;
   //data_string.toCharArray(data,data_length);
   data[0] = (char)id;

   
   //Serial.println(data);
 }*/

void manual_data_buffer(char *data, short id, int dust, struct Sensor sensor, int base){
  
  // This function manually creates the string that is trasferred via radio 
  /*
   * ASSUMPTIONS
   *  id 0-9 (1 digit)
   *  dust 0-99 ( 2 digits)
   *  sensor.temp 0-99 ( 2 digits)
   *  sensor.humd 0-99 ( 2 digits)
   *  sensor.light 0-9 ( 1 digit)
   *  
   *  base = 10, normal DEC
   *  values separated with ;
   */

    // init data buffer
    for (int k=0; k<15;k++)data[k] = 0; //NULL
    data[15] = '\0';

    //  check values to avoid overflow, id check should be elsewhere
    if (sensor.light > 9)sensor.light = 9; 
    if (dust < 0 || dust > 99) dust = 99; 
    if (sensor.humd < 0) sensor.humd = 0;
    

    // Manually generate data string
  
    data[0] = (char) id + '0';
    data[1] = ';';
    data[2] = (char)(dust / base) + '0';
    data[3] = (char) (dust % base) + '0';
    data[4] = ';';
    data[5] = (char)(sensor.temp / base) + '0';
    data[6] = (char) (sensor.temp % base) + '0';
    data[7] = ';';
    data[8] = (char)(sensor.humd / base) + '0';
    data[9] = (char)(sensor.humd % base) + '0';
    data[10] = ';';
    data[11] = (char)sensor.light +'0';
      
   
  
 }


// DUST SENSOR FUNCTIONS

char checkValue(unsigned char *thebuf, char leng)
{  
  char receiveflag=0;
  int receiveSum=0;

  for(int i=0; i<(leng-2); i++){
  receiveSum=receiveSum+thebuf[i];
  }
  receiveSum=receiveSum + 0x42;
 
  if(receiveSum == ((thebuf[leng-2]<<8)+thebuf[leng-1]))  //check the serial data
  {
    receiveSum = 0;
    receiveflag = 1;
  }
  return receiveflag;
}


int transmitPM0_3(unsigned char *thebuf)
{
  int PM0_3Val;
  PM0_3Val=((thebuf[15]<<8) + thebuf[16]);
  return PM0_3Val;
}

//transmit PM Value to PC
int transmitPM0_5(unsigned char *thebuf)
{
  int PM0_5Val;
  PM0_5Val=((thebuf[17]<<8) + thebuf[18]);
  return PM0_5Val;
  }

//transmit PM Value to PC
int transmitPM1_0(unsigned char *thebuf)
{
  int PM1_0Val;
  PM1_0Val=((thebuf[19]<<8) + thebuf[20]);  
  return PM1_0Val;
}

int transmitPM2_5(unsigned char *thebuf)
{
  int PM2_5Val;
  PM2_5Val=((thebuf[21]<<8) + thebuf[22]);  
  return PM2_5Val;
}

int transmitPM5_0(unsigned char *thebuf)
{
  int PM5_0Val;
  PM5_0Val=((thebuf[23]<<8) + thebuf[24]);  
  return PM5_0Val;
}

int transmitPM10(unsigned char *thebuf)
{
  int PM10Val;
  PM10Val=((thebuf[25]<<8) + thebuf[26]);
  return PM10Val;
}



// SENSOR BOARD FUNCTIONS
/*
String update_sensor_board(char slave_addr){

  // basic varibles for sensor board

  int temp = 0; // board measures temp in C
  int humd = 0; // relative,5 - 95  %
  int light = 0; // unit?

  //for (char i=0; i<6; i++){
    
 
    // init all measurenments
    Wire.beginTransmission((uint8_t)slave_addr);
    Wire.write((uint8_t)0xC0); //start register
    Wire.write((uint8_t)0xFF);
    Wire.endTransmission();

    delay(1000); // time for update measurenments
 
    // read updated values
    Wire.beginTransmission(slave_addr);

    // only get temp,humd and light values
    Wire.write(0x01);
    Wire.endTransmission(false);//restart i2c, extremely important
 
    Wire.requestFrom((uint8_t)slave_addr,(uint8_t)6,(uint8_t)true); //6 bytes: temp, humd, light

    temp = read_temp();
    Serial.print("TEMP  ");
    Serial.println(temp);

    humd = read_humd();
    Serial.print("HUMIDITY  ");
    Serial.println(humd);

    light = read_light();
    Serial.print("LIGHT  ");
    Serial.println(light);
 
    Wire.endTransmission();
   //}

  // build a string which can be transmitted
  String S = String(temp) + ";" + String (humd) + ";" +String(light/100);
  return S;
  }*/


  int read_temp(void){
  // testing has shown that sensor reads a bit too high temp
  // -> -1 from the reading
  byte temp1 = Wire.read();
  double temp = ((double) (temp1*256 + Wire.read()))/ 10.0;
  return ((int) temp -1);
}

int read_humd(void){
  byte humd1 = Wire.read();
  double humd = ((double) (humd1*256 + Wire.read())) / 10.0;
  return (int) humd;
}

int read_light(void){
  byte light1 = Wire.read();
  int light = (256*light1 + Wire.read());
  return light;
}



struct Sensor sensor_board(struct Sensor sensor,char slave_addr){
     // This function updates sensor board and returns a struct containing new values
     
     // board measures temp in C
     // Humdity relative,5 - 95  %
     // Light unit?
 
    // init all measurenments
    Wire.beginTransmission((uint8_t)slave_addr);
    Wire.write((uint8_t)0xC0); //start register
    Wire.write((uint8_t)0xFF);
    Wire.endTransmission();

    delay(1000); // time for update measurenments (specified in datasheet)
 
    // read updated values
    Wire.beginTransmission(slave_addr);

    // only get temp,humd and light values
    Wire.write(0x01);
    Wire.endTransmission(false);//restart i2c, extremely important
 
    Wire.requestFrom((uint8_t)slave_addr,(uint8_t)6,(uint8_t)true); //6 bytes: temp, humd, light

    sensor.temp = read_temp();
    Serial.print("TEMP  ");
    Serial.println(sensor.temp);

    sensor.humd = read_humd();
    Serial.print("HUMIDITY  ");
    Serial.println(sensor.humd);

    sensor.light = read_light()/100; //scale properly [0-9]
    Serial.print("LIGHT  ");
    Serial.println(sensor.light);
 
    Wire.endTransmission();

    return sensor;
   
}
