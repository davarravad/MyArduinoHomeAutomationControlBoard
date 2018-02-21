/******************************************************************************
Name:     : My Arduino Home Automation controller
Version   : 2.5
Authors   : David "DaVaR" Sargent
          : Alex Thompson
Hardware  : Arduino Mega 2560
          : Arduino Ethernet Shield
          : CD4021B Shift Register(s)
          : 74HC595 Shift Register(s)
          : MAH WDT (Watch Dog Timer)
          : Amazon Alexa
          : MyArduinoHome UserApplePie Website Extension
          : DS18B20 Digital temperature sensors
Notes     : Use AHACB v2.2 to control lights via light switch
          : buttons, website, or Alexa
Website   : https://www.MyArduinoHome.com
******************************************************************************/

/***** Include libraries *****/
#include <SPI.h>
#include <Ethernet.h> // Used for Ethernet
#include <OneWire.h> // Used for Multi Temp Sensors
#include <DallasTemperature.h> // Used for Temp Sensors
#include <Shifter.h>  // Used for shift registers

/***** Declare Constants *****/
// Debug Settings
#define DEBUG 0   // 1 For Debugging - 0 To Disable Debugging
                  // Debug takes up too much memory for UNO
// Setup on and off status for relays
#define RLON LOW  // Relay ON value
#define RLOFF HIGH // Relay OFF value
// Temp Sensor data port
#define ONE_WIRE_BUS 2
// Number of Registers per board
#define NUM_REGISTERS_PER_BOARD 4
// Number of Boards in Stack
#define NUM_BOARDS 1  // Edit this if you have more than one board
// Number of Channels Total Per Board
#define NUM_CHANNELS_BOARD 8*NUM_REGISTERS_PER_BOARD
// Total Chips for input or outputs
#define NUM_CHIPS NUM_BOARDS*NUM_REGISTERS_PER_BOARD

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
const int house_id = "***********";  // House ID from web site

// Token For Website - Needed to connect to web server
const String website_token = "***********"; // Token from web site

// Garage Door strings
bool garageEnable01 = true;  // Enable Garage 1 true/false
bool garageEnable02 = true;  // Enable Garage 2 true/false

// Let system know if internet is working or not
bool internetEnabled = true;

// Get info for settings and stuff
char inString[32]; // string for incoming serial data
int stringPos = 0; // string index counter
bool startRead = false; // is reading?
String dataFromWebsite = ""; // Setup dfw string

int total_buttons = NUM_CHANNELS_BOARD*NUM_BOARDS; // Get int ready default 15
String doorStatus1 = "OPEN";
String doorStatus2 = "OPEN";

// CD4021
int inDataPin = 12; //Pin connected to SERIN-Pin11 of CD4021
int inLatchPin = 7; //Pin connected to PSC-Pin9 of CD4021
int inClockPin = 13; //Pin connected to CLOCK-Pin10 of CD4021

// 74HC595
int outDataPin = 11; //Pin connected to DS of 74HC595
int outLatchPin = 8; //Pin connected to ST_CP of 74HC595
int outClockPin = 6; //Pin connected to SH_CP of 74HC595

//Define variables to hold the data for each shiftIn register.
byte switchVar[] = {
  16, 32, 48, 64,
  80, 96, 112, 128,
  144, 160, 176, 192,
  208, 224, 240, 250
};

// Define Strings for relay inputs
const int cdInputArraySize = (NUM_CHANNELS_BOARD*NUM_BOARDS);
String cdINw[cdInputArraySize] = "";
String cdIN = "";
String pageValueLight[] = "";

// Define Lights status
int lightOutputValue[cdInputArraySize];
int lita = 0;

// WDT Pin
int wdt = 3;

// Initialize shifter using the Shifter library
Shifter shifter(outDataPin, outLatchPin, outClockPin, NUM_CHIPS);

/***** Start of Setup *****/
void setup(){
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
  Serial.println(" | Arduino Home Automation v2.5");
  Serial.println(" --------------------------------------------------- ");

  // Beep the controller to let user know it just (re)booted
  pinMode(A5, OUTPUT);
  beep(600);
  beep(400);
  beep(50);

  Serial.println(" | Designed by David Sargent");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");

  // Pet the dog
  wdt_heartbeat();

  Serial.println("");
  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Network Connection Information");
  Serial.println(" --------------------------------------------------- ");

  // Check if user has ethernet enabled
  if(internetEnabled == true){
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
  Serial.println(" | Setting up array size for lightOutputValue");
  Serial.println(" --------------------------------------------------- ");
  Serial.print(" | Array Size : ");
  Serial.println(cdInputArraySize);
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");
  Serial.println("");

  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | Controller Setup Now Finished.  Moving on to loop.");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("");
  Serial.println("");

}
/***** End of Setup *****/

/***** Start of Loop *****/
void loop(){

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
  delayMicroseconds(40);
  // set it to 0 to transmit data serially
  digitalWrite(inLatchPin,0);
  // while the shift register is in serial mode
  // collect each shift register into a byte
  // the register attached to the chip comes in first
  if( DEBUG ) Serial.println(" | ------------------------------------------- ");
  if( DEBUG ) Serial.println(" | ShiftIn Register(s) Status ");
  if( DEBUG ) Serial.println(" | ------------------------------------------- ");
  int inv_a = 0;
  int inv_b = 0;
  // Define Input variables
  int cdInput[cdInputArraySize];
  for (int sv_data=0; sv_data<=NUM_CHIPS-1; sv_data++)
  {
    switchVar[sv_data] = shiftIn(inDataPin, inClockPin);
    if( DEBUG ) Serial.println(" | -------------------------- ");
    if( DEBUG ) Serial.print(" | CD4021 - ");
    if( DEBUG ) Serial.println(sv_data);
    if( DEBUG ) Serial.print(" | switchVar[sv_data], BIN - ");
    if( DEBUG ) Serial.println(switchVar[sv_data], BIN);
    if(sv_data > 0){
      inv_a = ((sv_data+1)*8)-8;
      inv_b = inv_a+7;
    }else{
      inv_a = 0;
      inv_b = 7;
    }
    if( DEBUG ) Serial.println(" | -------------------------- ");
    if( DEBUG ) Serial.print(" | INV_A - ");
    if( DEBUG ) Serial.println(inv_a);
    if( DEBUG ) Serial.print(" | INV_B - ");
    if( DEBUG ) Serial.println(inv_b);


    for (int ina=0; ina<=7; ina++)
    {
      // Get current channel number for output array
      int inb = 0;
      if(sv_data > 0){
        inb = ina+inv_a; // Second+ chip 0-7 based on chip
      }else{
        inb = ina; // First Chip 0-7
      }
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.print(inb);
      // so, when n is 3, it compares the bits
      // in switchVar1 and the binary number 00001000
      // which will only return true if there is a
      // 1 in that bit (ie that pin) from the shift
      // register.
      if (switchVar[sv_data] & (1 << ina) ){
        // print the value of the array location
        if( DEBUG ) Serial.print(" (inCH:");
        if( DEBUG ) Serial.print(inb+1);
        if( DEBUG ) Serial.print("-");
        if( DEBUG ) Serial.print(ina);
        if( DEBUG ) Serial.print(") ");
        cdInput[inb] = 1;
      }else{
        cdInput[inb] = 0;
      }
      if( DEBUG ) Serial.print(" | ");
      if( DEBUG ) Serial.println(cdInput[inb]);
      delay(10);
    }

  }
  if( DEBUG ) Serial.println(" | -------------------------- ");
  if( DEBUG ) Serial.println();

  // Lights and Relays Updates
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Lights Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  if( DEBUG ) Serial.print(" | Total Buttons:  ");
  if( DEBUG ) Serial.print(total_buttons);
  if( DEBUG ) Serial.println(" | ");

  String cdINallData[] = "";
  int board_number = 0;

  for (int lit=0; lit<=total_buttons-1; lit++)
  {
    // Set current board number based on channel
    if(NUM_BOARDS > 0 && lit >= 0 && lit <= 15){
      // Skips inputs 0-15 for board 1
      board_number = 0;
    }
    // Check for second set of inputs.  Skip them for relay control
    if(NUM_BOARDS > 0 && lit >= 16 && lit <= 31){
      // Skips inputs 16-23 for board 1
      continue;
    }
    // Set current board number based on channel
    if(NUM_BOARDS > 1 && lit >= 32 && lit <= 47){
      // Skips inputs 32-47 for board 2
      board_number = 1;
    }
    // Check for second set of inputs.  Skip them for relay control
    if(NUM_BOARDS > 1 && lit >= 48 && lit <= 63){
      // Skips inputs 48-63 for board 2
      continue;
    }
    // Set current board number based on channel
    if(NUM_BOARDS > 2 && lit >= 64 && lit <= 79){
      // Skips inputs 64-79 for board 3
      board_number = 2;
    }
    // Check for second set of inputs.  Skip them for relay control
    if(NUM_BOARDS > 2 && lit >= 80 && lit <= 95){
      // Skips inputs 80-95 for board 3
      continue;
    }
    // Set current board number based on channel
    if(NUM_BOARDS > 3 && lit >= 96 && lit <= 111){
      // Skips inputs 96-111 for board 4
      board_number = 3;
    }
    // Check for second set of inputs.  Skip them for relay control
    if(NUM_BOARDS > 3 && lit >= 112 && lit <= 127){
      // Skips inputs 112-127 for board 4
      continue;
    }

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

    if( DEBUG ) Serial.print(" | cdInput : ");
    if( DEBUG ) Serial.println(cdInput[lit]);

    if( DEBUG ) Serial.print(" | Prev lightOutputValue : ");
    if( DEBUG ) Serial.println(lightOutputValue[lit]);

    // Check to see if button was pressed
    if(cdInput[lit] == 1){
      if(lightOutputValue[lit] == 1){
        lightOutputValue[lit] = 0;
        cdINw[lit] = "0";
      }else{
        lightOutputValue[lit] = 1;
        cdINw[lit] = "1";
      }
    }else{
      // No Button Press, Check Website
      if(pageValueLight[lit] == "1"){
        if(lightOutputValue[lit] == 0){
          lightOutputValue[lit] = 1;
          cdINw[lit] = "1";
        }
      }else if(pageValueLight[lit] == "0"){
        if(lightOutputValue[lit] == 1){
          lightOutputValue[lit] = 0;
          cdINw[lit] = "0";
        }
      }
    }

    // Check to see if garage doors are enabled
    if(garageEnable01 == true && lit == 15){
      if( DEBUG ) Serial.println(" | Garage 1 Door Skip  ");
      cdINw[lit] = "0";
    }else if(garageEnable02 == true && lit == 14){
      if( DEBUG ) Serial.println(" | Garage 2 Door Skip  ");
      cdINw[lit] = "0";
    }else{
      // Change relay state if button pressed
      if(lightOutputValue[lit] == true){
        if( DEBUG ) Serial.print(" | Light  ");
        if( DEBUG ) Serial.print(lit+1);
        if( DEBUG ) Serial.println(" On ");
        cdINw[lit] = "1";
        shifter.setPin(lit, RLON);
        shifter.setPin(lit + 16, RLON);
      }else{
        if( DEBUG ) Serial.print(" | Light  ");
        if( DEBUG ) Serial.print(lit+1);
        if( DEBUG ) Serial.println(" Off ");
        cdINw[lit] = "0";
        shifter.setPin(lit, RLOFF);
        shifter.setPin(lit + 16, RLOFF);
      }
    }

    if( DEBUG ) Serial.print(" | New lightOutputValue : ");
    if( DEBUG ) Serial.println(lightOutputValue[lit]);

    if( DEBUG ) Serial.print(" -- cdINw[");
    if( DEBUG ) Serial.print(lit);
    if( DEBUG ) Serial.print("] = ");
    if( DEBUG ) Serial.print(cdINw[lit]);
    if( DEBUG ) Serial.println(" -- ");

    cdINallData[board_number] += cdINw[lit];
    if( DEBUG ) Serial.println(" ------------------------ ");
  }


  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Sending Lights Data to Lights Relays ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Send data to outputs
  shifter.write(); //send changes to the chain and display them
  // Need some delay or light will flicker when button is pressed if connection to server is fast enough.
  delay(20);

  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | Setting up Lights Data for website ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Setup Lights Data for website
  // Add channels to string for website
  for (int bid=0; bid<=NUM_BOARDS-1; bid++)
  {
    if( DEBUG ) Serial.println();
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.print(" | cdINallData[");
    if( DEBUG ) Serial.print(bid);
    if( DEBUG ) Serial.print("] : ");
    if( DEBUG ) Serial.println(cdINallData[bid]);

    if( DEBUG ) Serial.println(" | Relay Loop - Update Server ");
    if(internetEnabled){
      connectAndUpdateRelays(cdINallData[bid], bid+1);
    }
  }

  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // Delay so all these print satements can keep up.
  if( DEBUG ) delay(10);

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
  if(garageEnable01){
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Website Command Received for Garage Door Button 1");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    String pageValue_door_button_1 = "DO_NOTHING";
    if(internetEnabled){
        // Connect to the server and read the output for door button
      pageValue_door_button_1 = connectAndRead("/home/garage.php?door_id=1&action=door_button");
      if( DEBUG ) Serial.print(" | - Data From Server :: ");
      if( DEBUG ) Serial.print(pageValue_door_button_1); //print out the findings.
      if( DEBUG ) Serial.println(" :: ");
    }
    // If Door Button pushed, open / close garage door
    if (pageValue_door_button_1 == "PUSH_BUTTON"){
      beep(600);
      beep(400);
      delay(10);
      // Pushing button
      digitalWrite(10, RLON);
      delay(500);
      digitalWrite(10, RLOFF);
      if( DEBUG ) Serial.println(" | -- PUSHED GARAGE DOOR 1 BUTTON --  ");
    }

    // If all lights nothing - do nothing with all lights
    if (pageValue_door_button_1 == "DO_NOTHING"){
      if( DEBUG ) Serial.println(" | -- GARAGE DOOR BUTTON 1 NO CHANGE --  ");
    }
  }
  // Check Garage Door 2 Button Status
  if(garageEnable02){
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    if( DEBUG ) Serial.println(" | Website Command Received for Garage Door Button 2 ");
    if( DEBUG ) Serial.println(" --------------------------------------------------- ");
    String pageValue_door_button_2 = "DO_NOTHING";
    if(internetEnabled){
      // Connect to the server and read the output for door button
      pageValue_door_button_2 = connectAndRead("/home/garage.php?door_id=2&action=door_button");
      if( DEBUG ) Serial.print(" | - Data From Server :: ");
      if( DEBUG ) Serial.print(pageValue_door_button_2); //print out the findings.
      if( DEBUG ) Serial.println(" :: ");
    }
    // If Door Button pushed, open / close garage door
    if (pageValue_door_button_2 == "PUSH_BUTTON"){
      beep(600);
      beep(400);
      delay(10);
      // Pushing button
      digitalWrite(9, RLON);
      delay(500);
      digitalWrite(9, RLOFF);
      if( DEBUG ) Serial.println(" | -- PUSHED GARAGE DOOR 2 BUTTON --  ");
    }

    // If all lights nothing - do nothing with all lights
    if (pageValue_door_button_2 == "DO_NOTHING"){
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
  if( DEBUG ) Serial.println(" | Checking if Garage Doors are Open or Closed ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  // If Door 1 OPEN
  if (cdInput[15] == 0 && doorStatus1 == "CLOSED" && garageEnable01 == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_1_OPEN --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=1&action=update_sensor&action_data=OPEN");
    }
    // Let Door Button know door is open
    doorStatus1 = "OPEN";
  }

  // If Door 1 CLOSED
  if (cdInput[15] == 1 && doorStatus1 == "OPEN" && garageEnable01 == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_1_CLOSED --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=1&action=update_sensor&action_data=CLOSED");
    }
    // Let Door Button know door is closed
    doorStatus1 = "CLOSED";
  }
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(cdInput[15]);
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(doorStatus1);
  if( DEBUG ) Serial.print(" | -- GD1 :  ");
  if( DEBUG ) Serial.println(garageEnable01);
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");

  // If Door 2 OPEN
  if (cdInput[14] == 0 && doorStatus2 == "CLOSED" && garageEnable02 == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_2_OPEN --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=2&action=update_sensor&action_data=OPEN");
    }
    // Let Door Button know door is open
    doorStatus2 = "OPEN";
  }
  // If Door 2 CLOSED
  if (cdInput[14] == 1 && doorStatus2 == "OPEN" && garageEnable02 == true){
    if( DEBUG ) Serial.println(" | -- GARAGE_DOOR_2_CLOSED --  ");
    if(internetEnabled){
      connectAndRead("/home/garage.php?door_id=2&action=update_sensor&action_data=CLOSED");
    }
    // Let Door Button know door is closed
    doorStatus2 = "CLOSED";
  }
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(cdInput[14]);
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(doorStatus2);
  if( DEBUG ) Serial.print(" | -- GD2 :  ");
  if( DEBUG ) Serial.println(garageEnable02);
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println(" | End Garage Door Button Check ");
  if( DEBUG ) Serial.println(" --------------------------------------------------- ");
  if( DEBUG ) Serial.println();

}
/***** End of Loop *****/

/***** Start of Functions *****/
/***** Function that checks the shiftIn registers for data *****/
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

/***** Function that updates current Temperature status to web site *****/
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

/***** Function that controls how the speaker beeps *****/
void beep(unsigned char delayms){
  digitalWrite(A5, HIGH);  // Almost any value can be used except 0 and 255
  delay(delayms);       // wait for a delayms ms
  digitalWrite(A5, LOW);   // 0 turns it off
  delay(delayms);       // wait for a delayms ms
}

/***** Function that gets and prints current temp from sensors *****/
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

/***** Function that gets current temp from sensors *****/
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

/***** Function to connect to web server and get data *****/
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

/***** Function to connect to web server and update relay status *****/
String connectAndUpdateRelays(String relay_data, int relay_set){
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
    // Pet The Dog
    wdt_heartbeat();
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

/*****
  readPage is used to check the website for commands.
  Website outputs in this format <ALL_ON>
  Arduino reads everything between '<' and '>' to get 'ALL_ON'
 *****/
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

/***** getBit gets data from shift register in a readable format *****/
bool getBit(byte myVarIn, byte whatBit) {
  bool bitState;
  bitState = myVarIn & (1 << whatBit);
  return bitState;
}

/***** WDT - HeartBeat *****/
void wdt_heartbeat() {
  // Sink current to drain charge from watchdog circuit
  pinMode(wdt, OUTPUT);
  digitalWrite(wdt, LOW);
  delay(100);
  // Return to high-Z
  pinMode(wdt, INPUT);
  Serial.println("  ");
  Serial.println(" --------------------------------------------------- ");
  Serial.println(" | WDT - Petting The Dog ");
  Serial.println(" --------------------------------------------------- ");
  Serial.println("  ");
}

/***** END of code *****/