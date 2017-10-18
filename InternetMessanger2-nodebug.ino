// JSON Library - we don't need it, as it is memory hungry
//#include <ArduinoJson.h>

#include <stdlib.h>;

// Ethernet Board libraries
#include <SPI.h>
//#include <Dhcp.h>
//#include <Dns.h>
#include <Ethernet.h>
//#include <EthernetClient.h>
//#include <EthernetServer.h>
//#include <EthernetUdp.h>

// MQTT library
// !!! Always increase packet size in PubSubClient.h
#include <PubSubClient.h>
// LCD library
#include <LiquidCrystal.h>

// initialize the library with the numbers of the interface pins
//pins 11 and 12 used by Ethernet, so first two are changed to 9/8
LiquidCrystal lcd(9, 8, 5, 4, 3, 2);

const int buttonPin = 7; //used for light switch, not relevant for demo

// MAC address for the board can be random, if the real chip MAC is not known
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// If serverName is string, DNS will be used. If it's IPAddress type, no DNS request.
char serverName[] = "api-dev.devicewise.com";
//IPAddress serverName(54,175,125,23);

// callback function is invoked, when MQTT message comes on any of the subscribed topics. 
// We define it here, so we can initialize client object.

void callback(char* topic, byte* payload, unsigned int length);

// Create objects for Ethernet port and connetion
EthernetClient ethClient;
// Create object for MQTT client
PubSubClient client(ethClient);

//actuall callback function definition
void callback(char* topic, byte* payload, unsigned int length) {

  String stringInput;
  char* command;

  // write received topic and message to the console for debugging  
  //Serial.print(topic);
  //Serial.print(":");
  //for (int i=0;i<length;i++) {
  //  Serial.print((char)payload[i]);
  //}
  //Serial.println();

  stringInput = String(topic);

  if(stringInput.equals("notify/mailbox_activity")) { // notification received
    // request the message from the mailbox
    command="{\"1\":{\"command\":\"mailbox.check\",\"params\":{\"autoComplete\":true,\"limit\": 0}}}";

    // print the prepared command to console for debug
    //Serial.print(command);
    //Serial.println();
    
    // send command on dedicated topic, so we can recognize the response
    // "api" topic is used for TR-50 calls, responses are received on "reply" topic
    // everything behind "api/" in the api call is copied to suffix of "reply/"
    // This helps to match requests with responses 
    client.publish("api/mbxchck",command);
    
  }

  if(stringInput.equals("reply/mbxchck")) {  // message received on mbxchck means, we received actual mail message as response to our earlier request
    int b=50;
    int i=0;

    // create command for acknoledgeng message received
    command = "{\"1\":{\"command\":\"mailbox.ack\",\"params\":{\"id\":\"xxxxxxxxxxxxxxxxxxxxxxxx\"}}}\0";

    // Fill in the message ID in the response command. Read it from the static position in the received message (characters 50 to 73).
    // For production this is not suitable, as it requires code change, if the message elements change even slightly
    for (int i=0;i<24;i++) {
       command[46+i]= (char)payload[i+50];
    }

    // print the prepared command to console for debug
    //Serial.print(command);
    //Serial.println();

    // send command to mbxack topic for later identification of response
    client.publish("api/mbxack",command);

  }

  if(stringInput.equals("reply/mbxack")) { // we have received success for ack message

    // prepare to execute the method reuested from deviceWise.
    // We know, it is method change-text, because we do not have any other defined.
    // For commercial deployment, we should check, what method needs to be executed.
    // In our case, we create command to read the current value of text attribute 
    command = "{\"1\":{\"command\":\"thing.attr.get\",\"params\":{\"key\":\"text\"}}}\0";

    // print the prepared command to console for debug
    //Serial.print(command);
    //Serial.println();
    
    // we publish to text topic to identify the response later
    client.publish("api/txt",command);

  }

  if(stringInput.equals("reply/txt")) { // topic is text, so it must be response to thing.attr.get
    
    lcd.setCursor(0, 1);

    // we determine the position of the value. Depending on the timestamp length, it is either at position 71 or 72, 
    // depending on where character " is.
    int p=72;
    if (payload[70] == 34) {
      p=71;
    }
    // read value until closing character " or read maximum of 16 characters
    int i=p;
    for (i; payload[i]!=34 && i<p+16;i++) {
      lcd.print((char)payload[i]);
    }
    // fill in blanks, if the message is less than 16, to clear the characters until end of line
    for (i; i<p+16; i++) {
      lcd.print(" "); 
    }
    
  } 
}

void reconnect() {
  // Loop until we're reconnected to MQTT
  while (!client.connected()) {
    //Serial.print("MQTT...");
    // Attempt to connect. As credentials, we use application name "Display Message", 
    // we connect as device "arduino-display1" with app token generated by deviceWise platform at the application creation.
    if (client.connect("Display Message","arduino-display1","TE3a9x1rtJME4ECK")) {
      //Serial.println("ok");
      // when connected, subscribe to all topics under "reply". This is used for responses to TR-50 requests.
      client.subscribe("reply");
    } else {
      //Serial.print("f,rc=");
      //Serial.print(client.state());
      //Serial.println("5s");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  // send initial request for the current text attribute. We will read it in callback when the response comes
  char* command = "{\"1\":{\"command\":\"thing.attr.get\",\"params\":{\"key\":\"text\"}}}\0";
  //Serial.print(command);
  //Serial.println();
  client.publish("api/txt",command);
}

void setup() {

  // open serial for debug outputs
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // pin for light. Not relevant for the demo
  pinMode(buttonPin, INPUT);

  // define the MQTT client's options. callback function is called everytime we receive message on subscribed topics
  client.setServer(serverName, 1883);
  client.setCallback(callback);

  // Start Ethernet with mac only parameters. This means, DHCP will be used.
  // For static IP assignment, Ethernet.begin(mac,ipa,dns,gw,mask) must be called. The paramters are arrays of 4 integers.
  Ethernet.begin(mac);
  while (Ethernet.begin(mac) == 0) {
    // wait 10 seconds for next DHCP request
    delay(10000);
    //Serial.print("timeout");
  }
  // debug print of IP
  printIPAddress();
  
  // set up the LCD. Needs number of columns and rows: 
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("");
  
}

void loop() {

  // always check, if MQTT is still connected. If not, reconnect
  if (!client.connected()) {
    reconnect();
  }
  // call loop function of teh MQTT client. Needs to be called regularily, so it can read the input from the MQTT connection.
  client.loop();

  xbeeloop();
  
}

void xbeeloop() {
  
  if(Serial.available()>=21) {
    if(Serial.read()==0x7E) {
      for (int i=0; i<18; i++) {
        byte discard = Serial.read();
      }
      int tempHigh = Serial.read();
      int tempLow = Serial.read();
      int tempVolts=tempLow+tempHigh*256;

      float temp=(float)tempVolts/1023*1200;
      int temp2=(int)temp;
      float temperature=(float)temp2/10+2;

      if(temperature<100 && temperature>0) {
      
        lcd.setCursor(0, 0);
        lcd.print(temperature);
        lcd.print("\'C       ");

        char buff[6];
        dtostrf(temperature,4,2,buff);
     
        client.publish("thing/self/property/temp",buff);
      }
    }
  }
}

// Debug function to print the assigned IP
void printIPAddress()
{
  //Serial.print("My IP:");
  //for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    //Serial.print(Ethernet.localIP()[thisByte], DEC);
    //Serial.print(".");
  //}

  //Serial.println();
}

// Debug function to troubleshoot memory problems
int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
