# Protopaja_p3
Contains Arduino codes from protopaja project, group 3
<br/>
The project was sponsored by Consair

<br/><br/>
These codes are intended to use with a server running contents of: <br/>
https://github.com/vikketii/protopaja-web

##		Content description



####	Master
	
 - This was used to test radio communication
 - The final implementation of master (hub unit) uses same kind of radio code
 
#### Master koodit 

 - NodeMcu codes 
 - Master unit codes (used in 'production' master unit)
 - Code 3 is the final implementation
 
 <b>NOTICE:</b><br/>
 Server domain needs to match your own server</br>
 Change the line which contains http.begin()

 
####	Slave
	
 - Used for sensor stations, DEBUG version (includes Serial prints)
 - Intended to run on Arduino Uno 
 - Combines code for radio, display, dust sensor, fan and multi sensor
 - This does NOT work with own pcb due to a different pin layout
 
 
####	Slave production

 - The final 'production' code
 - Intended to run on own pcb version 1 (prototype v2, black & white housing)
 
#### 	Slave production v2 and Slave production volts v2

 - The final codes that are intended to run on own pcb version 2 (prototype 3, yellow housing)
 - Pcb v2 has a bit different pin layout to version 1
 - Production volts has the same base code but it also includes battery voltage measurement

####	Old files
 - Contains old slave, slave production and slave production volts codes
 - Those codes were tested to reduce current consumption of the slaves
 - However, dust sensor didn't work reliably enough with those codes
 
 
##		Operating principle	

 - Master module gives id to slaves during the first connection of the slaves
 - After receiving data from a slave, master sends acknowledgment to the slave
 - Master, final implementation, transmits data to a server 
 
 ____
 
 - Slave sends data on constant intervals
 - Turns on the sensors and fan to update measurements
 - Takes mean from the dust measurement
 - Then turns on the radio module and transmits data to the hub
 - After receiving an acknowledgment continues to measure dust particles
 
 
 
##		Hardware requirements	

 - Test version: Arduino Unos for both master and slaves
 
 - The own pcbs had ATMega328p Mcus

 - The final master unit used NodeMcu
 
 - Dust sensor: DFROBOT SEN0177
 
 - Multi sensor: AmbiMate MS4 Sensor Module
 
 - Display: Generic 128x32 oled
 
 - Fan: Generic fan, a model with low power consumption recommended


