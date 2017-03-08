// iSSR by emilio  
// D5 : Heizung
// D6 : Rührwerk
// D7 : Pumpe/Kühlung
// D8 : Alarm

#include <ESP8266WiFi.h>                             //https://github.com/esp8266/Arduino
#include <EEPROM.h>                                  
#include <WiFiManager.h>                             //https://github.com/kentaylor/WiFiManager
#include <DoubleResetDetector.h>                     //https://github.com/datacute/DoubleResetDetector

#define Version "1.0.0"

#define DRD_TIMEOUT 10                               // Number of seconds after reset during which a subseqent reset will be considered a double reset.
#define DRD_ADDRESS 0                                // RTC Memory Address for the DoubleResetDetector to use

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

unsigned int localPort = 5010;                        // Port auf dem gelesen wird
WiFiUDP Udp;

const int PIN_LED = 2;                                // Controls the onboard LED.

const int Heizung = D5;                               // Im folgenden sind die Pins der Sensoren und Aktoren festgelegt
const int Ruehrwerk = D6;
const int Pumpe = D7;
const int Summer = D8;

char charVal[5];
char packetBuffer[24];                                 // buffer to hold incoming packet,
char temprec[24] = "";
char relais[5] = "";
char state[3] = "";

bool HLowActive, RLowActive, PLowActive, ALowActive = true;

bool initialConfig = false;                             // Indicates whether ESP has WiFi credentials saved from previous session, or double reset detected
unsigned long jetztMillis = 0, letzteUDPMillis = 0, letzteOfflineMillis = 0;

void UDPRead()
{
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    for (int schleife = 0; schleife < 23; schleife++) { temprec[schleife] = ' '; }
    // read the packet into packetBufffer
    Udp.read(packetBuffer, packetSize);
    for (int schleife = 0; schleife < 23; schleife++) { temprec[schleife] = packetBuffer[schleife]; }
    letzteUDPMillis = millis();
    Serial.print("Udp Packet received"); 
    packetAuswertung();
  }
}    

void OfflineCheck()
{
  if (jetztMillis > letzteUDPMillis+10000) 
  {
    if (jetztMillis > letzteOfflineMillis+1000) {Serial.print("offline"); SerialOut(); letzteOfflineMillis=jetztMillis; } 
    relais[1]='h';      
    relais[2]='r';               
    relais[3]='p'; 
    relais[4]='a';       
    state[1]='o';
  }
}

void RelaisOut()
{
  if (relais[1] == 'H') { digitalWrite(Heizung,!HLowActive); } else { digitalWrite(Heizung,HLowActive); }
  if (relais[2] == 'R') { digitalWrite(Ruehrwerk,!RLowActive); } else { digitalWrite(Ruehrwerk,RLowActive); }
  if (relais[3] == 'P') { digitalWrite(Pumpe,!PLowActive); } else { digitalWrite(Pumpe,PLowActive); }
  if (relais[4] == 'A') { digitalWrite(Summer,!ALowActive); } else { digitalWrite(Summer,ALowActive); }
}

void SerialOut()
{
  Serial.print(" Reilaistatus: ");
  Serial.print(digitalRead(Heizung)); 
  Serial.print(digitalRead(Ruehrwerk));
  Serial.print(digitalRead(Pumpe));
  Serial.println(digitalRead(Summer));
}

void packetAuswertung()
{
  int temp = 0;
  int temp2 = 0;
  if ((temprec[0]=='C') && (temprec[18]=='c'))             // Begin der Decodierung des seriellen Strings  
  { 
    temp=(int)temprec[1];
    if ( temp < 0 ) { temp = 256 + temp; }
    if ( temp > 7) {relais[4]='A';temp=temp-8;} else {relais[4]='a';} 
    if ( temp > 3) {relais[3]='P';temp=temp-4;} else {relais[3]='p';} 
    if ( temp > 1) {relais[2]='R';temp=temp-2;} else {relais[2]='r';}
    if ( temp > 0) {relais[1]='H';temp=temp-1;} else {relais[1]='h';}   

    temp=(int)temprec[2];
    if ( temp < 0 ) { temp = 256 + temp; }
    if ( temp > 127) {temp=temp-128;}  
    if ( temp > 63) {temp=temp-64;}
    if ( temp > 31) {temp=temp-32;}    
    if ( temp > 15) {temp=temp-16;}  
    if ( temp > 7) {temp=temp-8;}  
    if ( temp > 3) {state[1]='x';temp=temp-4;} 
    else if ( temp > 1) {state[1]='z';temp=temp-2;}  
    else if ( temp > 0) {state[1]='y';temp=temp-1;}    
  }
  SerialOut();
}

void ReadSettings() {
  EEPROM.begin(512);
  EEPROM.get(0, localPort);
  EEPROM.get(20, HLowActive);
  EEPROM.get(30, RLowActive);
  EEPROM.get(40, PLowActive);
  EEPROM.get(50, ALowActive);
  EEPROM.commit();
  EEPROM.end();
}  

void WriteSettings() {
  EEPROM.begin(512);
  EEPROM.put(0, localPort);
  EEPROM.put(20, HLowActive);
  EEPROM.put(30, RLowActive);
  EEPROM.put(40, PLowActive);
  EEPROM.put(50, ALowActive);
  EEPROM.end();    
}

void setup() {
  pinMode(PIN_LED, OUTPUT);       // Im folgenden werden die Pins als I/O definiert
  pinMode(Heizung, OUTPUT);
  pinMode(Summer, OUTPUT);
  pinMode(Ruehrwerk, OUTPUT);
  pinMode(Pumpe, OUTPUT);
 
  Serial.begin(19200);
  Serial.println("\n");
  Serial.println("Starting");
  Serial.print("iSSR V");  
  Serial.println(Version);

  WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
  WiFi.setOutputPower(20.5);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);

  ReadSettings();

  WiFi.printDiag(Serial);            //Remove this line if you do not want to see WiFi password printed

  if (WiFi.SSID()==""){
    Serial.println("We haven't got any access point credentials, so get them now");   
    initialConfig = true;
  }
 
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    initialConfig = true;
  }
  
  if (initialConfig) {
    Serial.println("Starting configuration portal.");
    digitalWrite(PIN_LED, LOW); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
    char convertedValue[5];
    
    sprintf(convertedValue, "%d", localPort);
    WiFiManagerParameter p_localPort("localPort", "receive relais state on UDP Port", convertedValue, 5);

    char hlowactive[24] = "type=\"checkbox\"";
    if (HLowActive) {
      strcat(hlowactive, " checked");
    }
    WiFiManagerParameter p_hlowactive("HLowActive", "Heizung (D5) Low Active", "T", 2, hlowactive, WFM_LABEL_AFTER);

    char rlowactive[24] = "type=\"checkbox\"";
    if (RLowActive) {
      strcat(rlowactive, " checked");
    }
    WiFiManagerParameter p_rlowactive("RLowAactive", "Ruehrwerk (D6) Low Active", "T", 2, rlowactive, WFM_LABEL_AFTER);

    char plowactive[24] = "type=\"checkbox\"";
    if (PLowActive) {
      strcat(plowactive, " checked");
    }
    WiFiManagerParameter p_plowactive("PLowActive", "Pumpe/Kuehlung (D7) Low Active", "T", 2, plowactive, WFM_LABEL_AFTER);

    char alowactive[24] = "type=\"checkbox\"";
    if (ALowActive) {
      strcat(alowactive, " checked");
    }
    WiFiManagerParameter p_alowactive("ALowActive", "Alarm (D8) Low Active", "T", 2, alowactive, WFM_LABEL_AFTER);

    WiFiManager wifiManager;
    wifiManager.setBreakAfterConfig(true);
    wifiManager.addParameter(&p_localPort);
    wifiManager.addParameter(&p_hlowactive);
    wifiManager.addParameter(&p_rlowactive);
    wifiManager.addParameter(&p_plowactive);
    wifiManager.addParameter(&p_alowactive);
    wifiManager.setConfigPortalTimeout(300);

    if (!wifiManager.startConfigPortal()) {
      Serial.println("Not connected to WiFi but continuing anyway.");
    } else {
      //if you get here you have connected to the WiFi
      Serial.println("connected...yeey :)");
    }
    
    localPort = atoi(p_localPort.getValue());
    HLowActive = (strncmp(p_hlowactive.getValue(), "T", 1) == 0);
    RLowActive = (strncmp(p_rlowactive.getValue(), "T", 1) == 0);
    PLowActive = (strncmp(p_plowactive.getValue(), "T", 1) == 0);
    ALowActive = (strncmp(p_alowactive.getValue(), "T", 1) == 0);

    WriteSettings();

    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

  }
   
  digitalWrite(PIN_LED, HIGH); // Turn led off as we are not in configuration mode.
  
  unsigned long startedAt = millis();
  Serial.print("After waiting ");
  int connRes = WiFi.waitForConnectResult();
  float waited = (millis()- startedAt);
  Serial.print(waited/1000);
  Serial.print(" secs in setup() connection result is ");
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("failed to connect, finishing setup anyway");
  } else{
    Serial.print("local ip: ");
    Serial.println(WiFi.localIP());
    Udp.begin(localPort);
    Serial.print("local port: ");
    Serial.println(localPort);
  }
}

void loop() {

  drd.loop();
    
  jetztMillis = millis();

  if (WiFi.status()!=WL_CONNECTED){
    WiFi.reconnect();
    Serial.println("lost connection");
    delay(5000);
    Udp.begin(localPort);
  } else{
    UDPRead();
    OfflineCheck();
    RelaisOut();
    if (jetztMillis < 100000000) {wdt_reset();}             // WatchDog Reset  
  }  
}

   
 
