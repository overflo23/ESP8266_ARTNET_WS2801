/*
This example will receive multiple universes via Artnet and control a strip of ws2811 leds via 
Adafruit's NeoPixel library: https://github.com/adafruit/Adafruit_NeoPixel
This example may be copied under the terms of the MIT license, see the LICENSE file for details
*/


#include <FS.h>     


#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ArtnetWifi.h>


//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager


#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson



#include "Adafruit_WS2801.h"
#include "SPI.h" // Comment out this line if using Trinket or Gemma








// WS801 settings
const int numLeds = 170; // change for your setup
const int numberOfChannels = numLeds * 3; // Total number of channels you want to receive (1 led = 3 channels)
Adafruit_WS2801 leds = Adafruit_WS2801(numLeds, 5, 4);
//Adafruit_WS2801 leds = Adafruit_WS2801(numLeds);




// onbaord 3mm red led
#define LEDPIN    16





// each controller should parse 5 bytes from the 512 bytes universe data
// the first controller reads 0-4, the second should have "5" as offset and read from 5-9 and so on..
// these are changeed from the webinterface
int idOffset = 0;
int artnetUniverse = 0;





//network stuff
//default custom static IP, changeable from the webinterface
char ip[16] = ""; // could be 1.3.3.7
char gw[16] = ""; // could be 1.3.3.1
char sn[16] = ""; // could be 255.255.255.0







/* ----------------- DON'T touch the defines below unless you now what you are up to.---------------------------- */



// Artnet settings
ArtnetWifi artnet;



// a flag for ip setup
boolean set_ip = 0;



// TOODO:  should check if i still need this ..
// Check if we got all universes  
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;





// temporary buffer for webinterface
char artnet_universe[3];
char id_offset[3];

//flag for saving data
bool shouldSaveConfig = false;




#define WL_MAC_ADDR_LENGTH 6


//creates the string that shows if the device goes into accces pint mode
String getUniqueSystemName()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);


  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) + String("-") + String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);

  macID.toUpperCase();
  String UniqueSystemName = String("ARTNET_") + macID;

  return UniqueSystemName;
}





// displays mac address on serial port
void printMac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);

  Serial.print("MAC: ");
  for (int i = 0; i < 5; i++)
  {
    Serial.print(mac[i], HEX);
    Serial.print(":");
  }
  Serial.println(mac[5],HEX);

}








//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}






// this does a lot.

boolean StartWifiManager(void)
{


  Serial.println("StartWifiManager() called");



 
  // add parameter for artnet setup in GUI
  WiFiManagerParameter custom_artnet_universe("artnet_universe", "ARTnet Universe (Default: 0)", artnet_universe, 3);
  WiFiManagerParameter custom_id_offset("id_offset", "ID offset  (Default: 0, +5)", id_offset, 3);


  // add parameters for IP setuyp in GUI
  WiFiManagerParameter custom_ip("ip", "Static IP (Blank for DHCP)", ip, 16);
  WiFiManagerParameter custom_gw("gw", "Static Gateway (Blank for DHCP)", gw, 16);
  WiFiManagerParameter custom_sn("sn", "Static Netmask (Blank for DHCP)", sn, 16);



  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;





  //reset settings - for testing
  //wifiManager.resetSettings();


  // this is what is called if the webinterface want to save data, callback is right above this function and just sets a flag.
  wifiManager.setSaveConfigCallback(saveConfigCallback);


  // this actually adds the parameters defined above to the GUI
  wifiManager.addParameter(&custom_artnet_universe);
  wifiManager.addParameter(&custom_id_offset);




  // if the flag is set we configure a STATIC IP!
  if (set_ip)
  {
    //set static ip
    IPAddress _ip, _gw, _sn;
    _ip.fromString(ip);
    _gw.fromString(gw);
    _sn.fromString(sn);
    
    // this adds 3 fields to the GIU for ip, gw and netmask, but IP needs to be defined for this fields to show up.
    wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn);



    Serial.println("Setting IP to:");
    Serial.print("IP: ");
    Serial.println(ip);
    Serial.print("GATEWAY: ");
    Serial.println(gw);
    Serial.print("NETMASK: ");
    Serial.println(sn);






  }
  else
  {
    
   // i dont want to fill these fields per default so i had to implement this workaround .. its ugly.. but hey. whatever.  
    wifiManager.addParameter(&custom_ip);
    wifiManager.addParameter(&custom_gw);
    wifiManager.addParameter(&custom_sn);
  }









  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep in seconds
  // also really annoying if you just connected and the damn thing resets in the middel of filling in the GUI..
  // 5 minuts seems reasonable
  wifiManager.setTimeout(300);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(getUniqueSystemName().c_str())) {
    Serial.println("failed to connect and hit timeout");


    Serial.println("Good bye kids! I am all outta here.");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    //   we never get here.
    
    
  }

  //everything below here is only executed once we are connected to a wifi.


  //if you get here you have connected to the WiFi
  Serial.println("CONNECTED");



  // connection worked so lets save all those parameters to the config file
  strcpy(ip, custom_ip.getValue());
  strcpy(gw, custom_gw.getValue());
  strcpy(sn, custom_sn.getValue());




  // if we defined something in the gui before that does not work we might to get rid of previous settings in the config file
  // so if the form is transmitted empty, delete the entries.
  if (strlen(ip) < 8)
  {
    strcpy(ip, "");
    strcpy(gw, "");
    strcpy(sn, "");
    Serial.println("RESETTING IP/GW/SUBNET EMPTY!");
  }

  strcpy(artnet_universe, custom_artnet_universe.getValue());
  strcpy(id_offset, custom_id_offset.getValue());


  // this is true if we come from the GUI and clicked "save"
  // the flag is created in the callback above..
  if (shouldSaveConfig) {






    Serial.println("saving config");
    
    // JSON magic
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();


    // things the JSON should save
    json["artnet_universe"] = artnet_universe;
    json["id_offset"] = id_offset;

    json["ip"] = ip;
    json["gw"] = gw;
    json["sn"] = sn;



    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    // dump config to Serial
    json.printTo(Serial);

    Serial.println("");

    // dump config to file in FS
    json.printTo(configFile);
    Serial.println("");

    configFile.close();
    //end save
  }







}




void debugDMX(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{

  
  

  //  DEBUG DMX FRAMES - left here for reference  if something does not work out for you, here is the raw data to peek around.

  boolean tail = false;

  Serial.print("DMX: Univ: ");
  Serial.print(universe, DEC);
  Serial.print(", Seq: ");
  Serial.print(sequence, DEC);
  Serial.print(", Data (");
  Serial.print(length, DEC);
  Serial.print("): ");

  if (length > 16) {
    length = 16;
    tail = true;
  }
  // send out the buffer
  for (int i = 0; i < length; i++)
  {
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  if (tail) {
    Serial.print("...");
  }
  Serial.println();
  
}


void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{

 // debugDMX(universe, length,sequence, data);
  
  
  
  
  
  
  
  
  
  
  
  
  // red led on
  digitalWrite(LEDPIN, LOW);


  // not our universe? good bye!
  if (universe != artnetUniverse)  return;


  int idx=0;

  for(int i=idOffset;i<(512-3);i+=3)
  {

/*
    Serial.print(i);  
  Serial.print(" <-leds.setPixelColor(");
  Serial.print(idx);
  Serial.print(", ");  
  Serial.print(data[i]);
  Serial.print(", ");  
  Serial.print(data[i+1]);
  Serial.print(", ");  
  Serial.print(data[i+2]);
  Serial.println(");");    
 */   

    leds.setPixelColor(idx,  data[i],  data[i+1],  data[i+2]);

    idx++;
  }
  
  
  Serial.println("leds.show()");
  leds.show();
  
  

  
  // red led on
  digitalWrite(LEDPIN, HIGH);


}

















// called when you turn the device on
// it opens files, but if it can not open files it creates the filesystem ..

void setupFS()
{


  Serial.println("SetupFs() called");


  //read configuration from FS json
  Serial.println("mounting FS...");




  if (SPIFFS.begin()) {
    Serial.println("mounted file system");


    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("file is there, reading config.json");
      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        
        // dump contents to serial..
        json.printTo(Serial);
        Serial.println("");
        
        if (json.success()) {
          Serial.println("\nparsed json");


        // are you looking for exploitable buffer overflows?
        // look no further!  but.. whatever :)
        // the IOT is fun fun fun!

        // make int from string
        strcpy(id_offset, json["id_offset"]);
        idOffset = atoi(id_offset);

        // make int from string
        strcpy(artnet_universe, json["artnet_universe"]);
        artnetUniverse = atoi(artnet_universe);


    
        // so there are IP settings in the config file.
        if (json["ip"]) {

            // lets use the IP settings from the config file for network config.
            set_ip = 1;


            Serial.println("setting custom ip from config");
           
            // tehse strings are parsed when the network is setup .. this is way up there, you already saw it.
            strcpy(ip, json["ip"]);
            strcpy(gw, json["gw"]);
            strcpy(sn, json["sn"]);




          } else {
            Serial.println("no custom ip in config");
          }




        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");


    Serial.println("UNABLE TO MOUNT FS! calling SPIFFS.format();");
    
    //filesystem not readable? You probably turned on the device for the first time. Let's create the filesystem.
    SPIFFS.format();



  }
  //end read

}


void setup()
{
  

  Serial.begin(115200);



  Serial.println("---");
  Serial.println("START");


  // onboard leds also outputs
  pinMode(LEDPIN, OUTPUT);





  // set default colors as long as there is no data over network coming in..


  Serial.println("Leds set to default states");

  // on boards led are wired for LOW to turn them on and HIGH turning them off..
  
  // red led ON
  digitalWrite(LEDPIN, HIGH);
  delay(200);
  // red led ON
  digitalWrite(LEDPIN, LOW);
  delay(500);
  digitalWrite(LEDPIN, HIGH);

  // display the MAC on Serial
  printMac();

  
  Serial.print("System Name: ");
  Serial.println(getUniqueSystemName());



  setupFS();
  Serial.println("SetupFS() done.");



  StartWifiManager();
  Serial.println("StartWifiManager() done.");



 delay(10000);

  Serial.println("leds_setup() called ");
   leds_setup();









  artnet.begin();
  Serial.println("artnet.begin() done.");






  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
  
  
  
  
  
  
  
  
  
  
  
  
}





// life in loops.
void loop()
{
  // we call the read function inside the loop
  artnet.read();
}






// ane they lived happily ever after
// the end.}
