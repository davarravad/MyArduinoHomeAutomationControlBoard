// **********************************************************************
// **********************************************************************
// **  Name    : My Arduino Home Automation
// **  Author  : David "DaVaR" Sargent
// **  Date    : 15 Jan, 2018
// **  Version : 2.1
// **  Hardware: AHACB v2.2
// **          : CD4021B Shift Register(s)
// **          : 74HC595 Shift Register(s)
// **          : Arduino Mega 2560
// **          : Arduino Ethernet Shield
// **          : Amazon Alexa
// **          : MyArduinoHome UserApplePie Website Extension
// **          : DS18B20 Digital temperature sensors
// **  Notes   : Use AHACB v2.2 to control lights via light switch
// **          : buttons, website, or Alexa
// ** Website  : https://www.MyArduinoHome.com
// **********************************************************************
// **********************************************************************
// ** Include libraries
// **********************************************************************
#include <SPI.h>
#include <Ethernet.h> // Used for Ethernet
#include <OneWire.h> // Used for Multi Temp Sensors
#include <DallasTemperature.h> // Used for Temp Sensors
#include <Shifter.h>  // Used for shift registers
// **********************************************************************
// ** Declare Constants
// **********************************************************************
// Debug Settings
#define DEBUG 0  // 1 For Debugging - 0 To Disable Debugging

// Setup on and off status for relays
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// Garage door pins
#define GarageDoor_1  10
#define Garage_Door_Sensor_1  15
#define GarageDoor_2  9
#define Garage_Door_Sensor_2  14

// Temp Sensor data port
#define ONE_WIRE_BUS 2

// Number of Registers per board
#define NUM_REGISTERS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
DeviceAddress temp_1 = { 0x28, 0x70, 0xF9, 0x7D, 0x06, 0x00, 0x00, 0x37 };
DeviceAddress temp_2 = { 0x28, 0x48, 0x2E, 0x7E, 0x06, 0x00, 0x00, 0xFC };
DeviceAddress temp_3 = { 0x28, 0x7D, 0xFE, 0x7D, 0x06, 0x00, 0x00, 0x2C };

// Ethernet Settings
// Ethernet MAC address - must be unique on your local network
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x25 };
EthernetClient client;

// IP Address or Domain Name to web server
char server[] = "***********";  // Web Server Address

// House ID - Needed to connect to web server
String house_id = "***********";  // House ID from web site

// Token For Website - Needed to connect to web server
String website_token = "***********"; // Token from web site

// Garage Door strings
boolean garage_1_enable = true;  // Enable Garage 1 true/false
boolean garage_2_enable = true;  // Enable Garage 2 true/false

// Let system know if internet is working or not
boolean internetEnabled = true;

// Get info for settings and stuff
char inString[32]; // string for incoming serial data
int stringPos = 0; // string index counter
boolean startRead = false; // is reading?
String dataFromWebsite = ""; // Setup dfw string

int total_buttons = 15; // Get int ready default 15
int garage_1 = "14"; // If garage 1 is enabled only use 15 inputs as buttons
int garage_2 = "13"; // If garage 2 is enabled only use 14 inputs as buttons
String door_button_nothing = "DO_NOTHING";
String door_button_push = "PUSH_BUTTON";
String door_1_status = "OPEN";
String door_2_status = "OPEN";

// Speaker Beep Beep
int speakerPin = A5;  // Notification speaker

// CD4021
int inDataPin = 12; //Pin connected to SERIN-Pin11 of CD4021
int inLatchPin = 7; //Pin connected to PSC-Pin9 of CD4021
int inClockPin = 13; //Pin connected to CLOCK-Pin10 of CD4021

// 74HC595
int outDataPin = 11; //Pin connected to DS of 74HC595
int outLatchPin = 8; //Pin connected to ST_CP of 74HC595
int outClockPin = 6; //Pin connected to SH_CP of 74HC595

//Define variables to hold the data for each shiftIn register.
byte switchVar1 = 72;  //01001000
byte switchVar2 = 159; //10011111
byte switchVar3 = 246; //10011111
byte switchVar4 = 331; //10011111

// Define Lights status
boolean light_[] = {false, false, false, false, false, false, false, false, false, false,
                    false, false, false, false, false, false};
int lita = 0;

//define an array that corresponds to values for each
//of the first shift register's pins
String CD4021[] = {
    "IN1", "IN2", "IN3", "IN4", "IN5", "IN6", "IN7", "IN8",
    "IN9", "IN10", "IN11", "IN12", "IN13", "IN14", "IN15", "IN16",
    "IN1B", "IN2B", "IN3B", "IN4B", "IN5B", "IN6B", "IN7B", "IN8B",
    "IN9B", "IN10B", "IN11B", "IN12B", "IN13B", "IN14B", "IN15B", "IN16B"};

// Define Strings for relay inputs
String relayIN_[] = "";
String relayINw_[] = "";
String relayIN = "";
String relayIN_ALL = "";
String relayIN_ALL_Loop = "";
String pageValueLight[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"};

// Timer setup
unsigned long currentMillis;
unsigned long previousMillis;
unsigned long hourTimer = (60*60*1000); // One Hour Timer

// Initialize shifter using the Shifter library
Shifter shifter(outDataPin, outLatchPin, outClockPin, NUM_REGISTERS);

// Function to reset the controller
void(* resetFunc) (void) = 0; //declare reset function @ address 0


// **********************************************************************
// ** Start of Setup
// **********************************************************************
void setup() {

  //Define pin modes for 74HC595 Chips
  pinMode(outLatchPin, OUTPUT);
  pinMode(outClockPin, OUTPUT);
  pinMode(outDataPin, OUTPUT);

  // Set all pins to HIGH, for relays HIGH = OFF and LOW = ON
  shifter.setAll(HIGH); //set all pins on the shift register chain to HIGH
  shifter.write(); //send changes to the chain and display them

  //Define pin modes for CD4021 Chips
  pinMode(inLatchPin, OUTPUT);
  pinMode(inClockPin, OUTPUT);
  pinMode(inDataPin, INPUT);

  //start serial
  Serial.begin(9600);
  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Arduino Home Automation v2.1");
  Serial.println(" --------------------------------------------------- ");

  // Beep the controller to let user know it just (re)booted

  pinMode(speakerPin, OUTPUT);
  beep(100);
  beep(75);
  beep(50);

  Serial.println(" | Designed by David Sargent");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");
  Serial.println("");
  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Network Connection Information");
  Serial.println(" --------------------------------------------------- ");

  // Connect to the local network
  if (Ethernet.begin(mac) == 0){
    Serial.println(" | Failed to connect to local network.  Running in   ");
    Serial.println(" | No Internet Mode.  Website and other internet     ");
    Serial.println(" | devices will NOT work.  Please check your network ");
    Serial.println(" | and reset the Arduino controller.                 ");
    Serial.println(" --------------------------------------------------- ");
    Serial.println("");
    Serial.println("");
    internetEnabled = false;
  }else{
    Serial.print(" | IP Address        : ");
    Serial.println(Ethernet.localIP());
    Serial.print(" | Subnet Mask       : ");
    Serial.println(Ethernet.subnetMask());
    Serial.print(" | Default Gateway IP: ");
    Serial.println(Ethernet.gatewayIP());
    Serial.print(" | DNS Server IP     : ");
    Serial.println(Ethernet.dnsServerIP());
    Serial.println(" --------------------------------------------------- ");
    Serial.println("");
    Serial.println("");
    internetEnabled = true;
  }


  // Start up the temp library
  sensors.begin();
  // set the resolution to 10 bit (good enough?)
  sensors.setResolution(temp_1, 10);
  sensors.setResolution(temp_2, 10);
  sensors.setResolution(temp_3, 10);

  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Setting up Shift registers");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");
  Serial.println("");
  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Controller Setup Now Finished.  Moving on to loop.");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");
  Serial.println("");

  // Timer setup
  previousMillis = millis();

}
// **********************************************************************
// ** End of Setup
// **********************************************************************
// **********************************************************************
// ** Start of Loop
// **********************************************************************
void loop() {
  // Setup timer for stuff that needs timed
  unsigned long currentMillis = millis();
  // Read Website for Relay Updates
  if( DEBUG ) Serial.println("  ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Lights Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Check if internet is enabled
  if(internetEnabled){
    // Connect to the server and read the output for relays
    dataFromWebsite = connectAndRead("/home/relays.php?relay=LIST");
  }
  if( DEBUG ) Serial.print(" | - Data From Server :: ");
  if( DEBUG ) Serial.print(dataFromWebsite); //print out the findings.
  if( DEBUG ) Serial.println(" :: ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Website Command Received for Lights ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  // Shift In Data Read - Check CD4021 Chips for Inputs
  // Pulse the latch pin:
  // set it to 1 to collect parallel data
  digitalWrite(inLatchPin,1);
  // set it to 1 to collect parallel data, wait
  delayMicroseconds(20);
  // set it to 0 to transmit data serially
  digitalWrite(inLatchPin,0);

  // while the shift register is in serial mode
  // collect each shift register into a byte
  // the register attached to the chip comes in first
  switchVar1 = shiftIn(inDataPin, inClockPin);
  switchVar2 = shiftIn(inDataPin, inClockPin);
  switchVar3 = shiftIn(inDataPin, inClockPin);
  switchVar4 = shiftIn(inDataPin, inClockPin);

  if( DEBUG ) Serial.println("  ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | ShiftIn Register(s) Status ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  // Print out the results.
  // leading 0's at the top of the byte
  // (7, 6, 5, etc) will be dropped before
  // the first pin that has a high input
  // reading
  if( DEBUG ) Serial.print(" | CD4021 - 1 -  ");
  if( DEBUG ) Serial.print(switchVar1, BIN);
  if( DEBUG ) Serial.println("");
  if( DEBUG ) Serial.print(" | CD4021 - 2 -  ");
  if( DEBUG ) Serial.print(switchVar2, BIN);
  if( DEBUG ) Serial.println("");
  if( DEBUG ) Serial.print(" | CD4021 - 3 -  ");
  if( DEBUG ) Serial.print(switchVar3, BIN);
  if( DEBUG ) Serial.println("");
  if( DEBUG ) Serial.print(" | CD4021 - 4 -  ");
  if( DEBUG ) Serial.print(switchVar4, BIN);
  if( DEBUG ) Serial.println("");

  // First Chip
  // This for-loop steps through the byte
  // bit by bit which holds the shift register data
  // and if it was high (1) then it prints
  // the corresponding location in the array
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | CD4021 - 1 ");
  for (int ina=0; ina<=7; ina++)
  {
    // so, when n is 3, it compares the bits
    // in switchVar1 and the binary number 00001000
    // which will only return true if there is a
    // 1 in that bit (ie that pin) from the shift
    // register.
    if (switchVar1 & (1 << ina) ){
      // print the value of the array location
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(CD4021[ina]);
      relayIN_[ina] = "1";
    }else{
      relayIN_[ina] = "0";
    }
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(relayIN_[ina]);
  }

  int inb_8;
  // Second Chip
  // This for-loop steps through the byte
  // bit by bit which holds the shift register data
  // and if it was high (1) then it prints
  // the corresponding location in the array
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | CD4021 - 2  ");
  for (int inb=0; inb<=7; inb++)
  {
    inb_8 = inb + 8;
    // so, when n is 3, it compares the bits
    // in switchVar1 and the binary number 00001000
    // which will only return true if there is a
    // 1 in that bit (ie that pin) from the shift
    // register.
    if (switchVar2 & (1 << inb) ){
      // print the value of the array location
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(CD4021[inb_8]);
      relayIN_[inb_8] = "1";
    }else{
      relayIN_[inb_8] = "0";
    }
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(relayIN_[inb_8]);
  }

  int inc_16;
  // Third Chip
  // This for-loop steps through the byte
  // bit by bit which holds the shift register data
  // and if it was high (1) then it prints
  // the corresponding location in the array
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | CD4021 - 3  ");
  for (int inc=0; inc<=7; inc++)
  {
    inc_16 = inc + 16;
    // so, when n is 3, it compares the bits
    // in switchVar1 and the binary number 00001000
    // which will only return true if there is a
    // 1 in that bit (ie that pin) from the shift
    // register.
    if (switchVar3 & (1 << inc) ){
      // print the value of the array location
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(CD4021[inc_16]);
      relayIN_[inc_16] = "1";
    }else{
      relayIN_[inc_16] = "0";
    }
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(relayIN_[inc_16]);
  }

  int ind_24;
  // Forth Chip
  // This for-loop steps through the byte
  // bit by bit which holds the shift register data
  // and if it was high (1) then it prints
  // the corresponding location in the array
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | CD4021 - 4  ");
  for (int ind=0; ind<=7; ind++)
  {
    ind_24 = ind + 24;
    // so, when n is 3, it compares the bits
    // in switchVar1 and the binary number 00001000
    // which will only return true if there is a
    // 1 in that bit (ie that pin) from the shift
    // register.
    if (switchVar4 & (1 << ind) ){
      // print the value of the array location
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(CD4021[ind_24]);
      relayIN_[ind_24] = "1";
    }else{
      relayIN_[ind_24] = "0";
    }
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(relayIN_[ind_24]);
  }

  // Lights and Relays Updates
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Lights Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  // Check to see if garage doors are enabled
  if(garage_1_enable == true && garage_2_enable == false){
      total_buttons = 14;
  }else if(garage_1_enable == true && garage_2_enable == true){
      total_buttons = 13;
  }else{
      total_buttons = 15;
  }

  if( DEBUG ) Serial.print(" | Total Buttons:  ");
  if( DEBUG ) Serial.print(total_buttons);
  if( DEBUG ) Serial.println(" | ");

  for (int lit=0; lit<=total_buttons; lit++)
  {
      lita = lit + 1;

      // Check if internet is enabled
      if(internetEnabled){
        pageValueLight[lit] = dataFromWebsite.substring(lit,lita);
      }else{
        pageValueLight[lit] = "";
      }

      if( DEBUG ) Serial.print(" | -- Value for Output ");
      if( DEBUG ) Serial.print(lit);
      if( DEBUG ) Serial.print("  From Server  : ");
      if( DEBUG ) Serial.print(pageValueLight[lit]);
      if( DEBUG ) Serial.println(" --  ");

      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(relayIN_[lit]);

      // Check to see if button was pressed
      if(relayIN_[lit] == "1"){
        if(light_[lit] == true){
          light_[lit] = false;
          relayINw_[lit] = "0";
        }else{
          light_[lit] = true;
          relayINw_[lit] = "1";
        }
      }else{
        // No Button Press, Check Website
        if(pageValueLight[lit] == "1"){
          if(light_[lit] == false){
            light_[lit] = true;
            relayINw_[lit] = "1";
          }
        }else if(pageValueLight[lit] == "0"){
          if(light_[lit] == true){
            light_[lit] = false;
            relayINw_[lit] = "0";
          }
        }

      }


      // Change relay state if button pressed
      if(light_[lit] == true){
        if( DEBUG ) Serial.print(" | Light  ");
        if( DEBUG ) Serial.print(lit+1);
        if( DEBUG ) Serial.println(" On ");
        relayINw_[lit] = "1";
        shifter.setPin(lit, RELAY_ON);
        shifter.setPin(lit + 16, RELAY_ON);
      }else{
        if( DEBUG ) Serial.print(" | Light  ");
        if( DEBUG ) Serial.print(lit+1);
        if( DEBUG ) Serial.println(" Off ");
        relayINw_[lit] = "0";
        shifter.setPin(lit, RELAY_OFF);
        shifter.setPin(lit + 16, RELAY_OFF);
      }

      if( DEBUG ) Serial.println(" -- ");
  }

  if(garage_1_enable == true && garage_2_enable == false){
      relayIN_ALL = relayINw_[0] + relayINw_[1] + relayINw_[2] + relayINw_[3] + relayINw_[4] + relayINw_[5] + relayINw_[6] + relayINw_[7] +
              relayINw_[8] + relayINw_[9] + relayINw_[10] + relayINw_[11] + relayINw_[12] + relayINw_[13] + relayINw_[14];
  }else if(garage_1_enable == true && garage_2_enable == true){
      relayIN_ALL = relayINw_[0] + relayINw_[1] + relayINw_[2] + relayINw_[3] + relayINw_[4] + relayINw_[5] + relayINw_[6] + relayINw_[7] +
              relayINw_[8] + relayINw_[9] + relayINw_[10] + relayINw_[11] + relayINw_[12] + relayINw_[13];
  }else{
      relayIN_ALL = relayINw_[0] + relayINw_[1] + relayINw_[2] + relayINw_[3] + relayINw_[4] + relayINw_[5] + relayINw_[6] + relayINw_[7] +
              relayINw_[8] + relayINw_[9] + relayINw_[10] + relayINw_[11] + relayINw_[12] + relayINw_[13] + relayINw_[14] + relayINw_[15];
  }

  // Send data to outputs
  shifter.write(); //send changes to the chain and display them
  // Need some delay or light will flicker when button is pressed if connection to server is fast enough.
  delay(200);

  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  if( DEBUG ) Serial.print(" | ");
  if( DEBUG ) Serial.println(relayIN_ALL);

  if(relayIN_ALL_Loop != relayIN_ALL){
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(" Changes In Relay Loop - Update Server ");
    if(internetEnabled){
      connectAndUpdateRelays(relayIN_ALL, "1");
    }
  }else{
    if( DEBUG ) Serial.print(" | ");
    if( DEBUG ) Serial.println(" No Changes In Relay Loop ");
  }

  // Update Relay Loop
  relayIN_ALL_Loop = relayIN_ALL;

  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Delay so all these print satements can keep up.
  if( DEBUG ) delay(1000);

  // Update Temperature Status to Database
  // Check if internet is enabled
  if(internetEnabled){
    //Update Current Status of fish tanks to database
    if( DEBUG ) Serial.println();
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Updating Current Temps ");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");

      // Update temp status in database
        update_temp_status("Temp_Status", 1);  //sending data to server - temp 1
        update_temp_status("Temp_Status", 2);  //sending data to server - temp 2
        update_temp_status("Temp_Status", 3);  //sending data to server - temp 3

    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println();
  }

  // Garage Door Button Database Check
  if( DEBUG ) Serial.println();
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Checking if Garage Door Button Pushed ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Check Garage Door 1 Button Status
  if(garage_1_enable){
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Website Command Received for Garage Door Button 1");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    // Connect to the server and read the output for door button
    String pageValue_door_button_1 = connectAndRead("/home/garage.php?door_id=1&action=door_button");
    if( DEBUG ) Serial.print(" | - Data From Server :: ");
    if( DEBUG ) Serial.print(pageValue_door_button_1); //print out the findings.
    if( DEBUG ) Serial.println(" :: ");
    // If Door Button pushed, open / close garage door
    if (pageValue_door_button_1 == door_button_push){
      beep(600);
      beep(600);
      delay(10);
      // Pushing button
      digitalWrite(GarageDoor_1, RELAY_ON);
      delay(1000);
      digitalWrite(GarageDoor_1, RELAY_OFF);
      if( DEBUG ) Serial.println(" | -- PUSHED GARAGE DOOR 1 BUTTON --  ");
    }

    // If all lights nothing - do nothing with all lights
    if (pageValue_door_button_1 == door_button_nothing){
      if( DEBUG ) Serial.println(" | -- GARAGE DOOR BUTTON 1 NO CHANGE --  ");
    }
  }
  // Check Garage Door 2 Button Status
  if(garage_2_enable){
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Website Command Received for Garage Door Button 2 ");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    // Connect to the server and read the output for door button
    String pageValue_door_button_2 = connectAndRead("/home/garage.php?door_id=2&action=door_button");
    if( DEBUG ) Serial.print(" | - Data From Server :: ");
    if( DEBUG ) Serial.print(pageValue_door_button_2); //print out the findings.
    if( DEBUG ) Serial.println(" :: ");
    // If Door Button pushed, open / close garage door
    if (pageValue_door_button_2 == door_button_push){
      beep(600);
      beep(600);
      delay(10);
      // Pushing button
      digitalWrite(GarageDoor_2, RELAY_ON);
      delay(1000);
      digitalWrite(GarageDoor_2, RELAY_OFF);
      if( DEBUG ) Serial.println(" | -- PUSHED GARAGE DOOR 2 BUTTON --  ");
    }

    // If all lights nothing - do nothing with all lights
    if (pageValue_door_button_2 == door_button_nothing){
      if( DEBUG ) Serial.println(" | -- GARAGE DOOR BUTTON 2 NO CHANGE --  ");
    }
  }
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | End Garage Door Button Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println();


  // *** Garage Door Sensor Database Update *** //
  if( DEBUG ) Serial.println();
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Checking if Garage Door is Open or Closed ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(relayINw_[15]);
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(door_1_status);
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(garage_1_enable);
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // If Door 1 OPEN
  if (relayINw_[15] == "0" && door_1_status == "CLOSED" && garage_1_enable == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_1_OPEN --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=1&action=update_sensor&action_data=OPEN");
    }
    // Let Door Button know door is open
    door_1_status = "OPEN";
  }

  // If Door 1 CLOSED
  if (relayINw_[15] == "1" && door_1_status == "OPEN" && garage_1_enable == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_1_CLOSED --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=1&action=update_sensor&action_data=CLOSED");
    }
    // Let Door Button know door is closed
    door_1_status = "CLOSED";
  }
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(relayINw_[14]);
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(door_2_status);
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(garage_2_enable);
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // If Door 2 OPEN
  if (relayINw_[14] == "0" && door_2_status == "CLOSED" && garage_2_enable == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_2_OPEN --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=2&action=update_sensor&action_data=OPEN");
    }
    // Let Door Button know door is open
    door_2_status = "OPEN";
  }

  // If Door 2 CLOSED
  if (relayINw_[14] == "1" && door_2_status == "OPEN" && garage_2_enable == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_2_CLOSED --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=2&action=update_sensor&action_data=CLOSED");
    }
    // Let Door Button know door is closed
    door_2_status = "CLOSED";
  }

  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | End Garage Door Button Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println();

  // Check for DHCP Lease Renewal Every Hour
  if(currentMillis - previousMillis > hourTimer)
  {
    previousMillis = currentMillis; // get ready for next hour

    if(internetEnabled){
      Ethernet.maintain(); // Renew DHCP Lease
    }else{
      resetFunc();  // Reset the controller
    }

  }

}
// **********************************************************************
// ** End of Loop
// **********************************************************************
// ** Start of Functions
// ***************************************************************************
// ** Function that checks the shiftIn registers for data
// **********************************************************************
byte shiftIn(int myDataPin, int myClockPin) {
  int i;
  int temp = 0;
  int pinState;
  byte myDataIn = 0;
  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, INPUT);
  for (i=7; i>=0; i--)
  {
    digitalWrite(myClockPin, 0);
    delayMicroseconds(2);
    temp = digitalRead(myDataPin);
    if (temp) {
      pinState = 1;
      myDataIn = myDataIn | (1 << i);
    }
    else {
      pinState = 0;
    }
    digitalWrite(myClockPin, 1);
  }
  return myDataIn;
}
// **********************************************************************
// ** Function that updates current Temperature status to web site
// **********************************************************************
void update_temp_status(char* temp_status, int temp_id){
    // Setup temperature data before sending to server
    if( DEBUG ) Serial.println("");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Getting Temp Sensors Data  ");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.print(" | Getting temperature...\n\r");
    // Get Temperature Data
    sensors.requestTemperatures();
    if (temp_id == 01){
      if( DEBUG ) Serial.print(" | Temp 1 temperature is: ");
      if( DEBUG ) printTemperature(temp_1);
      if( DEBUG ) Serial.print("\n\r");
    }
    if (temp_id == 2){
      if( DEBUG ) Serial.print(" | Temp 2 temperature is: ");
      if( DEBUG ) printTemperature(temp_2);
      if( DEBUG ) Serial.print("\n\r");
    }
    if (temp_id == 3){
      if( DEBUG ) Serial.print(" | Temp 3 temperature is: ");
      if( DEBUG ) printTemperature(temp_3);
      if( DEBUG ) Serial.print("\n\r");
    }
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    // If incoming data from the net connection, print it out
    if( DEBUG ) {
      if (client.available()) {
        char c = client.read();
        Serial.print(c);
      }
    }
    // Check for web server connection before sending data
    if (client.connect(server, 80))
    {
      if( DEBUG ) Serial.print(" | Temp ID: ");
      if( DEBUG ) Serial.print(temp_id);
      if( DEBUG ) Serial.println("");
      if( DEBUG ) Serial.println(" --------------------------------------------------- ");
      if( DEBUG ) Serial.println(" | Connected to Server ");
      if( DEBUG ) Serial.println(" | Updating Data ");
      // Send Temp Data to URL for Web server
      client.print( "GET /home/temps.php?");
      client.print("house_id");
      client.print("=");
      client.print(house_id);
      client.print("&temp_server_name");
      client.print("=");
      client.print(temp_id);          // Temp ID 1/2/3/4 etc.
      client.print("&temp_data");
      client.print("=");
      if (temp_id == 01){ getTemperature(temp_1); } // Get Temp 1
      if (temp_id == 2) { getTemperature(temp_2); } // Get Temp 2
      if (temp_id == 3) { getTemperature(temp_3); } // Get Temp 3
      client.print("&tkn");
      client.print("=");
      client.print(website_token);  // Account token for extra security
      client.println();
      client.println( " HTTP/1.1");
      client.println( "Host: 192.168.1.31" );
      client.print(" Host: ");
      client.println(server);
      client.println( "Connection: close" );
      client.println();
      client.println();
      // If the server's disconnected, stop the client
      if (client.connected()) {
        if( DEBUG ) Serial.println(" | Disconnecting from server...  ");
        client.stop();
      }
      if( DEBUG ) Serial.println(" --------------------------------------------------- ");
      if( DEBUG ) Serial.println();
    }
    else
    {
      return " | Connection Failed - update_temp_status function";
    }
}
// **********************************************************************
// ** Function that controls how the speaker beeps
// **********************************************************************
void beep(unsigned char delayms){
  digitalWrite(A5, 20);  // Almost any value can be used except 0 and 255
  delay(delayms);       // wait for a delayms ms
  digitalWrite(A5, 0);   // 0 turns it off
  delay(delayms);       // wait for a delayms ms
}
// **********************************************************************
// ** Function that gets and prints current temp from sensors
// **********************************************************************
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == -127.00) {
      if( DEBUG ) Serial.print(" No Temp ");
  } else {
      if( DEBUG ) Serial.print("C: ");
      if( DEBUG ) Serial.print(tempC);
      if( DEBUG ) Serial.print(" F: ");
      if( DEBUG ) Serial.print(DallasTemperature::toFahrenheit(tempC));
  }
}
// **********************************************************************
// ** Function that gets current temp from sensors
// **********************************************************************
void getTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == -127.00) {
    client.print("00");
  } else {
    // Get temperature in Fahrenheit format
    client.print(DallasTemperature::toFahrenheit(tempC));
  }
}
// **********************************************************************
// ** Function to connect to web server and get data
// **********************************************************************
String connectAndRead(char* read_data_page_url){
  // Connect to web server
  if( DEBUG ) Serial.println(" | Connecting... ");
  // Check for network connection
  if (client.connect(server, 80)) {
    if( DEBUG ) Serial.println(" | Connected ");
    client.print("GET ");
    client.print(read_data_page_url);
    client.print("&house_id=");
    client.print(house_id);
    client.print("&tkn=");
    client.println(website_token);
    client.println("HTTP/1.0");
    client.println();
    // Connected - Read the page
    return readPage(); // Read the output
    // If the server's disconnected, stop the client
    if (!client.connected()) {
      if( DEBUG ) Serial.println();
      if( DEBUG ) Serial.println(" | Disconnecting from server...  ");
      if( DEBUG ) Serial.println();
      client.stop();
    }
  }else{
    return " | Connection Failed - connectAndRead function";
  }
}
// **********************************************************************
// ** Function to connect to web server and update relay status
// **********************************************************************
String connectAndUpdateRelays(String relay_data, String relay_set){
  // Connect to web server
  if( DEBUG ) Serial.println(" | Connecting... ");
  // Check for network connection
  if (client.connect(server, 80)) {
    if( DEBUG ) Serial.println(" | Connected ");
    client.print("GET ");
    client.print("/home/lightswitch.php?relayset=");
    client.print(relay_set);
    client.print("&action=update_relay&action_data=");
    client.print(relay_data);
    client.print("&house_id=");
    client.print(house_id);
    client.print("&tkn=");
    client.println(website_token);
    client.println("HTTP/1.0");
    client.println();
    // Connected - Read the page data
    return readPage(); // Read the output
    // If the server's disconnected, stop the client
    if (!client.connected()) {
      if( DEBUG ) Serial.println();
      if( DEBUG ) Serial.println(" | Disconnecting from server...  ");
      if( DEBUG ) Serial.println();
      client.stop();
    }
  }else{
    return " | Connection Failed - connectAndUpdateRelays function";
  }
}
// **********************************************************************
// ** readPage is used to check the website for commands.
// ** Website outputs in this format <ALL_ON>
// ** Arduino reads everything between '<' and '>' to get 'ALL_ON'
// **********************************************************************
String readPage(){
  stringPos = 0;
  memset( &inString, 0, 32 ); //clear inString memory
  while(true){
    if (client.available()) {
      char c = client.read();
      if (c == '<' ) { //'<' is our begining character
        startRead = true; //Ready to start reading the part
      }else if(startRead){
        if(c != '>'){ //'>' is our ending character
          inString[stringPos] = c;
          stringPos ++;
        }else{
          //got what we need here! We can disconnect now
          startRead = false;
          client.stop();
          client.flush();
            if( DEBUG ) Serial.println(" | Disconnecting... ");
          return inString;
        }
      }
    }
  }
}
// **********************************************************************
// ** getBit gets data from shift register in a readable format
// **********************************************************************
boolean getBit(byte myVarIn, byte whatBit) {
  boolean bitState;
  bitState = myVarIn & (1 << whatBit);
  return bitState;
}

// ** END of code