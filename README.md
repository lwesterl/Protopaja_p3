# Protopaja_p3
Contains Arduino codes from protopaja project



-	-	-	Description	-	-	-


	Master
	
 - Used to test radio communication
 - Final implementation of master (hub unit) uses same kind of radio code

 
	Slave
	
 - Used for sensor modules
 - Combines code for radio, display, dust sensor, fan and multi sensor 
 
 
 
-	-	-	Operating principle	-	-	-

 - Master module gives id to slaves during the first connection of the slaves
 - After receiving data from a slave, master sends acknowledgment to the slave
 - Master, final implementation, transmits data to a server 
 
 
 - Slave sends data on constant intervals
 - Turns on the sensors and fan to update measurements
 - Then turns on the radio module and transmits data to the hub
 - After receiving an acknowledgment goes to 'sleep mode' (i.e. turns off sensors and radio)
 
 
 
-	-	-	Hardware requirements	-	-	-

 - Test version: Arduino Unos for both master and slaves
 
 - Dust sensor: DFROBOT SEN0177
 
 - Multi sensor: AmbiMate MS4 Sensor Module
 
 - Display: Generic 128x32 oled
 
 - Fan: Generic fan, a model with low power consumption recommended


