# Protopaja_p3
Contains Arduino codes from protopaja project, group 3
<br/>
The project was sponsored by Consair



##		Description	



####	Master
	
 - Used to test radio communication
 - Final implementation of master (hub unit) uses same kind of radio code

 
####	Slave
	
 - Used for sensor modules
 - Combines code for radio, display, dust sensor, fan and multi sensor
 - Debug code, includes serial prints etc.
 - Intended to run on Arduino Uno 
 
####	Slave production

 - The final 'production' code which is run on own pcb(Atmega328p mcu)version 1
 
#### 	Slave production v2 & Slave production volts v2

 - The final codes that are run on own pcb version 2, which has a bit different pin layout to version 1
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
 - Then turns on the radio module and transmits data to the hub
 - After receiving an acknowledgment goes to 'sleep mode' (i.e. turns off sensors and radio)
 
 
 
##		Hardware requirements	

 - Test version: Arduino Unos for both master and slaves
 
 - Dust sensor: DFROBOT SEN0177
 
 - Multi sensor: AmbiMate MS4 Sensor Module
 
 - Display: Generic 128x32 oled
 
 - Fan: Generic fan, a model with low power consumption recommended


