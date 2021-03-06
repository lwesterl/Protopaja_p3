#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"


/* Master should be turned on first
 *  Then turn slaves on one by one
 *  During the first connection slaves receive an id from the master 
 *  Code supports at the moment one master + 7 slaves
 */


// DEFINE BASIC VARIABLES



const uint8_t len = 16;  // data package length
const uint8_t ack_len = 7;
char id_array[2];
const uint8_t id_len = 2; // id 2 - 6



unsigned long loop_time = 0;
unsigned long loop_max = 5000; // waits 5s slave to answer
bool problem = false;
bool request = false;


RF24 radio(9,10); //ce pin, csn pin

// these are used as ack to double check correct slave send data, must match slaves',
// any standard length strings could be used
char id_list[][7] = {"device","sensor","projec","common","argume","return","monito"};

//mutiple pipes use in communication, the first pipe reserved for first contacts by slaves
uint8_t addresses[][6] = {"Node1","Node2","Node3","Node4","Node5","Node6","Node7","Node8","Node9"};


short slaves = 1; //tells first free id, start value 1


void setup(void){
 Serial.begin(9600); //debugging only
 radio.begin();
 radio.setDataRate(RF24_250KBPS);
 radio.setChannel(108); //2.508 GHz
 radio.openReadingPipe (1,addresses[0]); //master address
 
 delay(100);
 
 }


 
 void loop(void){
 
    problem = false;
 
   // wait for a slave to write
   radio.startListening();
   
   
   while(!radio.available()){
     
       Serial.println("Waiting");
       delay(100);
        
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
        // inform slave that the message was succesfully received
        radio.openWritingPipe(addresses[(data[0]-'0'+1)]); // slaves' address,
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
           *    use timer to check enough time has elapsed since this slave has send data
           *    
           *    Also if problem = true
           *      server should be informed that there is a connection problem between
           *      slave and master
           */
       Serial.println("Send data to server");
       
        
      }
      
     }
     
     
   
 
 
 }

 




