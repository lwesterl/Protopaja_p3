#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <SoftwareSerial.h>
#include <Wire.h>
// i2c disp
#include <Arduino.h>
#include <U8x8lib.h>



// I2C Screen
//U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE); 
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(/* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE); 
 


//char display_messages [][17] = {"Connecting...","Connected","Connection Error", "Standby", " ID Error"};
char display_messages [][8] = {"Conn...","Conn","Con Err", "Standby", "ID Err"};
/* 
 * Master must be turned on first 
 * Then turn slaves on, one by one
 * Master gives unique ids to slaves during the first connection
 * 
 * After the first connection slaves should send sensor values two times
 * because first is only used to confirm succesfull connection with the master
 * The second is the real sensor value transmission
 * 
 * At the moment code supports one master + 5 slaves
 */

/* Arduino pins for the radio module:
 *  CSN = 6
 *  CE = 7
 *  MOSI = 11
 *  SCK = 13
 *  MISO = 12
 *  VCC = 3,3 V (demands a stable and good current supply)
 */

char slave_addr = 0x2A; // sensor board uses this addr


#define LENG 31   //0x42 + 31 bytes equal to 32 bytes
unsigned char buf[LENG];

#define dust_power_pin 5 // this is used to control dust sensor power
#define fan_power_pin 4 // this is used to control fan power
#define multi_sensor_power_pin 3 //this is used to control multisensor power 
// Notice: multi_sensor_power_pin needs to be on HIGH state when display is written to

// RADIO nRF24L01 VALUES

// this is used for master requests
char data[16];

struct Sensor{
  int temp = 0;
  int humd = 0;
  int light = 0;
}sensor;

int base = 10;
char id_array[2];
const uint8_t id_len = 2;
const uint8_t ack_len = 7;
unsigned long curr_time = 0;
unsigned long interval = 7000; // 53s sleep time 
unsigned long dust_count = 0;
unsigned long loop_time = 0;
unsigned long loop_max = 3000; // waits master answer 3s
bool first_connection = true;
bool id_problem = true;
bool problem = false;
bool power_on = true;

// these are used to determine correct slave
char id_list[][7] = {"device","sensor","projec","common","argume","return"};

int check_request(char *request,char *id);
bool check_ack(char *ack);

struct Sensor sensor_board(struct Sensor sensor,char slave_addr);
void manual_data_buffer(char *data, short id, int dust, struct Sensor sensor, int base);

// number of the sensor unit, master gives this
short id ; 

const uint8_t len = 16; // data package length


RF24 radio(7,6);//(CE pin) on the RF module, Chip Select (SCN pin)

// Communication addresses

//uint8_t address[][7] = {"Master","Slaves"};

// Multipipe addresses
uint8_t addresses[][6] = {"Node1","Node2","Node3","Node4","Node5","Node6","Node7"};


// DUST SENSOR RELATED FUNCTIONS


char checkValue(unsigned char *thebuf, char leng);

int transmitPM0_3(unsigned char *thebuf);

//transmit PM Value to PC
int transmitPM0_5(unsigned char *thebuf);

//transmit PM Value to PC
int transmitPM1_0(unsigned char *thebuf);

int transmitPM2_5(unsigned char *thebuf);

int transmitPM5_0(unsigned char *thebuf);

int transmitPM10(unsigned char *thebuf);


// DUST SENSOR VALUES


int PM0_3Value=0;          //PM0_3 hiukkasia desilitrassa
int PM0_5Value=0;         //PM0_5 hiukkasia desilitrassa
int PM1_0Value=0;         //PM1_0 hiukkasia desilitrassa
int PM2_5Value=0;          //PM2_5 hiukkasia desilitrassa
int PM5_0Value=0;         //PM5_0 hiukkasia desilitrassa
int PM10Value=0;         //PM10 hiukkasia desilitrassa

int PM0_3=0;          //PM0_3 hiukkasia litrassa
int PM0_5=0;         //PM0_5 hiukkasia litrassa
int PM1_0=0;         //PM1_0 hiukkasia litrassa
int PM2_5=0;          //PM2_5 hiukkasia litrassa
int PM5_0=0;         //PM5_0 hiukkasia litrassa
int PM10=0;         //PM10 hiukkasia litrassa
int P_tot = 0; // kaikki hiukkaset litrassa, lahetysta varten






// SENSOR BOARD

// Pin 3, SCL, UNO A5
// Pin 4, SDA, UNO A4

// I2C bus demands good pin connection,
// bad connections cause error readings

// Temp sensor seems to work ok at least when temp < 70
// when temp > 70, humd may be < 0 %
// motion sensor value is not read -> motion led remains on


String sensor_board_data;



// functions for sensor board
String update_sensor_board(char slave_addr);
int read_temp(void);
int read_humd(void);
int read_light(void);


SoftwareSerial PMSerial(8, 9); // RX, TX

void setup(void){
  Serial.begin(9600); //debugging only

  
  // Serial for dust sensor
  PMSerial.begin(9600);   
  PMSerial.setTimeout(1500);
  pinMode(dust_power_pin,OUTPUT); // dust sensor  power pin

  // init i2c bus
  //Wire.begin();
  //Wire.setClock(100000);
  pinMode(fan_power_pin,OUTPUT); // fan board power pin
  pinMode(multi_sensor_power_pin,OUTPUT); // sensor board power pin
  digitalWrite(multi_sensor_power_pin,HIGH); // turn this on to write to display
  // init radio
  radio.begin();

  delay(100);

 // init i2c display
  u8x8.begin();
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  
  u8x8.clear();
  u8x8.drawString(0,0,display_messages[0]);
  
  u8x8.refreshDisplay();    // only required for SSD1606/7 
 }




 void loop(void){
 

  if (millis() > curr_time + interval || first_connection == true){


    digitalWrite(multi_sensor_power_pin,HIGH); // sensor board to normal operating mode
    // sensor board also must be on when display is written to
    
    // update display
    u8x8.clear();
    u8x8.drawString(0,0,display_messages[0]);
    u8x8.refreshDisplay();    // only required for SSD1606/7 
    
    
    digitalWrite(dust_power_pin,HIGH); //dust sensor to normal operating mode
    digitalWrite(fan_power_pin,HIGH); // fan to normal operating mode
    
    
    
    dust_count = millis();
    while(dust_count + 7000 > millis()){
      // give 7s measurement time for dust sensor and sensor board
      Serial.println(F("Measuring dust"));
      
       // update dust sensor values
      if(PMSerial.find(0x42)){    
        PMSerial.readBytes(buf,LENG);
    
    
        if(buf[0] == 0x4d){
          if(checkValue(buf,LENG)){
            // tietyn kokoiset ja sita suuremmat hiukkaset desilitrassa
            PM0_3Value = transmitPM0_3(buf);
            //Serial.println(PM0_3Value);
            PM0_5Value = transmitPM0_5(buf);
            PM1_0Value = transmitPM1_0(buf);
            PM2_5Value = transmitPM2_5(buf);
            PM5_0Value = transmitPM5_0(buf);
            PM10Value = transmitPM10(buf);

            // vain kyseisen kokoiset hiukkaset litrassa
            PM0_3 = (PM0_3Value-PM0_5Value-PM1_0Value-PM2_5Value-PM5_0Value-PM10Value);
            PM0_5 = (PM0_5Value-PM1_0Value-PM2_5Value-PM5_0Value-PM10Value);
            PM1_0 = (PM1_0Value-PM2_5Value-PM5_0Value-PM10Value);
            PM2_5 = (PM2_5Value-PM5_0Value-PM10Value);
            PM5_0 = (PM5_0Value-PM10Value);
            //PM10Value = (PM10Value);

            P_tot = (PM0_3Value+PM0_5Value+PM1_0Value+PM2_5Value+PM5_0Value+PM10Value)/ 100; //scaling the value
            Serial.println(P_tot);

            
          }  
        }
      }
      // update sensor board, sensor struct
     sensor = sensor_board(sensor, slave_addr);
      
     }

    
    // Start radio again and turn off the sensors
    //digitalWrite(dust_power_pin,LOW);
    digitalWrite(fan_power_pin,LOW);
    //digitalWrite(multi_sensor_power_pin,LOW); 
    power_on = true;
    
    while(power_on){
      
      radio.powerUp(); // powerup radio, if radio already powered up -> does nothing
      
      /*radio.openReadingPipe(1, addresses[0]); //check that master is free
      
      while (radio.available()){
        Serial.println("Waiting turn");
      }*/
     //Serial.println("first_connection");
     if (first_connection)
      {
        // module needs to get an id from the master
        
        char request[8] = "request";
        radio.openReadingPipe(1,addresses[1]);
        radio.openWritingPipe(addresses[0]);
        radio.write(request,8);
        
        radio.startListening();
        loop_time = millis();
        
        problem = false;
        
      
        while (! radio.available()){
          if (loop_time + loop_max < millis()){
              problem = true;
              Serial.println(F("Error connecting to the master"));
              
              //update display
              u8x8.clear();
              u8x8.drawString(0,0,display_messages[2]);
              u8x8.refreshDisplay();    // only required for SSD1606/7 
              break;

            
           }
           
            
            radio.stopListening();
            //radio.write(request,8);
            // expect the master to answer
            radio.startListening();
            delay(100); // wait for an answer
        }
        
        if (!problem){
          
      
          radio.read(id_array,id_len);
          delay(5);
          Serial.println(id_array);

          id = id_array[0] - '0'; // get the id as an integer
          // check id is ok
          if (id > 9 || id < 0)
          {
            // id error
             problem = true;
             Serial.println(F("ID Error, Noisy signal"));
              
             //update display
             u8x8.clear();
             u8x8.drawString(0,0,display_messages[4]);
             u8x8.refreshDisplay();    // only required for SSD1606/7 
          }
          else {
          first_connection = false;
          id_problem = false;
          
          /* update display
          u8x8.clear();
          u8x8.drawString(0,0,display_messages[1]);
          u8x8.drawString(0,0,display_messages[1]);
          u8x8.refreshDisplay();    // only required for SSD1606/7 */
          
      
          }
          
        }
      }
      

      if (!id_problem){
          
        // send sensor data to the master
      
        // create data buffer
        manual_data_buffer(data, id, P_tot, sensor, base);
      
        radio.openReadingPipe(1,addresses[id]);
        
        
        radio.openWritingPipe(addresses[0]);
        radio.stopListening();
        radio.write(data,len); // send data to the master
        Serial.println(data);
        //delay(10);
        radio.flush_rx();
        radio.flush_tx();
        radio.startListening(); // listen the master

        loop_time = millis();
 
    
        problem = false;
        while(!radio.available()){

          if (millis() > (loop_time + loop_max)){
            // break the loop and report a problem
        
            Serial.println(F("Connection problem to the master"));
            problem = true;
            
            // update display
            u8x8.clear();
            u8x8.drawString(0,0,display_messages[2]);
            u8x8.refreshDisplay();    // only required for SSD1606/7 
            break;
          }
          //Serial.println(id_list[current_slave-1]); //debug use only
          radio.stopListening();

          //id list contains the correct request for the slave, note indexing
          //radio.write(data,len);
      
          radio.startListening();
          // periaatteessa loopista voisi poistua heti kun radio.available
          delay(100);

        }
    
        if(not problem){
          // read master ack
          char ack[7];
    
          radio.read(ack, ack_len);
          Serial.println(ack); // debug use only

         

          //if (String(ack) == String( id_list[id-1])){
          if(strcmp(ack,id_list[id-1])== 0){
            // if strings are identical -> got correct ack
            
           
            // master got the message 
            //set radio and dust sensor to sleep mode*/
            radio.powerDown(); // reduce power usage
            
            first_connection = false;
            power_on = false;
            curr_time = millis();
            // update display
            
            u8x8.clear();
            u8x8.drawString(0,0,display_messages[3]);
            u8x8.drawString(0,2,"ID:");
            u8x8.drawString(3,2,id_array);
            
            u8x8.refreshDisplay();    // only required for SSD1606/7 
            delay(10); //give time to update display
            digitalWrite(multi_sensor_power_pin,LOW); 
            

            }
            
            
        }
      }
    }
 
  }
  else {
    Serial.println(F("Sleeping"));
     
  }
 }

    

 


/* RADIO FUNCTIONS
 int check_request(char *request,char *id){
  // just check that correct slave is requested
  if (strcmp(request,id)==0){
    return 1;
  }
  return 0;

}
bool check_ack(char *ack){
 // just check that ack is "ok"

 if (strcmp(ack,"ok")==0)return true;

 return false;



}*/

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

    delay(1000); // time for update measurenments
 
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


/*short connecting_animation(short val, SSD1306AsciiWire oled){
  // animates connecting...
  // val tells how many . there are already, returns updated val
  if (val <3){
    oled.print(".");
    val ++;
    
  }
  else{
    oled.clear();
    oled.print("Connecting");
    val = 0;
    
  }
  return val;
}*/

