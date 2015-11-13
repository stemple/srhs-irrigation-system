/*
 
 NEEDED: a test, maybe once a day, for whether the system is still synced with the NTP. *timeStatus()*

 */
#include <TimeAlarms.h>
#include <Time.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
//include lcd library
#include <LiquidCrystal.h>

long day_ = 86400000; // 86400000 milliseconds in a day
long hour_ = 3600000; // 3600000 milliseconds in an hour
long minute_ = 60000; // 60000 milliseconds in a minute
long second_ =  1000; // 1000 milliseconds in a second

int status = WL_IDLE_STATUS;
int valvePins[] = {49}; //these are the pins to which each valve is attached. Hopefully the numbers will correspond
const int numberOfPins = 1;
int currentValve = 0;
int valveTimeHours[] = {0};
int valveTimeMinutes[] = {1};
//***WATERING START TIME IS SET ON LINE 93***
char ssid[] = "SRCS_Wireless";//  your network SSID (name)
char pass[] = "";       // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)

unsigned int localPort = 123;      // local port to listen for UDP packets

IPAddress timeServer(162, 243, 55, 105); // ntp.pool NTP server

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

time_t prevDisplay = 0; // when the digital clock was displayed
//PST is 8 hours behind GMT
const  long timeZoneOffset = -28800L; // set this to the offset in seconds to your local time;

//initialie the lcd interface pins
//old pins for reference in setup lcd(13, 11, 5, 4, 3, 2);
LiquidCrystal lcd(34, 30, 28, 26, 24, 22); 

void setup()
{
  //initialize all valve pins as outputs.
    int i;
    for( i = 0; i < numberOfPins; i++) {
      pinMode(valvePins[i], OUTPUT);
      digitalWrite(valvePins[i], LOW);
    }
  // Open serial communications and wait for port to open:
  Serial.begin(9600);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if ( fv != "1.1.0" )
    Serial.println("Please upgrade the firmware");

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect open network
    status = WiFi.begin(ssid);

    // wait 10 seconds for connection:
    delay(10000);

    Serial.println("waiting for time sync");
    setSyncProvider(getNtpTime);
    setSyncInterval(day_/1000);
    while (timeStatus() == timeNotSet); 
  }

  Serial.print("Connected to wifi");
  printWifiStatus();

  Serial.println("Starting connection to server...");
  Udp.begin(localPort);

  // Set the alarms.
  int startTimeHours = 9;
  int startTimeMinutes = 58;
  Alarm.alarmRepeat(startTimeHours, startTimeMinutes, 0, turnWaterOn);
  Serial.print("alarm set for ");
  Serial.print(startTimeHours);
  Serial.print(":");
  Serial.println(startTimeMinutes);
  
  
  
  for( i = 0; i < numberOfPins - 1; i++) {
    startTimeHours += valveTimeHours[i];
    startTimeMinutes += valveTimeMinutes[i];
    if(startTimeMinutes > 59){
      startTimeMinutes = startTimeMinutes%60;
      startTimeHours++;
    } 
    Alarm.alarmRepeat(startTimeHours, startTimeMinutes, 0, turnWaterOn);
    Serial.print("alarm set for ");
    Serial.print(startTimeHours);
    Serial.print(":");
    Serial.println(startTimeMinutes);
  }
  //set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  //print a message to the LCD.
  //lcd.print("watering system");
}

void loop()
{
  Alarm.delay(0);
  if ( now() != prevDisplay) //update the display only if the time has changed
  {
    time_t t = now();
    prevDisplay = now();
    digitalClockDisplay();
    //Serial.println(timeStatus());

  }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();

}

void printDigits(int digits) {
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}


unsigned long getNtpTime()
{
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);
  //Serial.println( Udp.parsePacket() );
  if ( Udp.parsePacket() ) {
    Serial.println("time server packet received");
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // add time zone offset and then return
    epoch = epoch + timeZoneOffset;
    return epoch;
  }
  return 0; // return 0 if unable to get the time
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

/*void checkTimeSync() {
  //checks if the clock is still synced to the wifi, if it is not, displays on the lcd screen
  if(timeStatus() == timeSet){
    lcd.print(hour());
    lcd.print(printDigits(minute()));
    lcd.print(printDigits(second()));
  }
  else if(timeStatus() == timeNeedsSync){*/
    
    

void turnWaterOn(){
  //turn on the solenoid for the current valve
  digitalWrite(valvePins[currentValve], HIGH);

  Serial.print("The water is on for valve ");
  Serial.println(currentValve);
  Alarm.delay((valveTimeHours[currentValve]*hour_)+(valveTimeMinutes[currentValve]*minute_));

  //turn solenoid off and switch to the next valve
  digitalWrite(valvePins[currentValve], LOW);
  Serial.print("The water is off for valve ");
  Serial.println(currentValve);

  if(currentValve < numberOfPins - 1) {
    currentValve++;
    Serial.print("Current Valve is now ");
    Serial.println(currentValve);
  }
  else {
    currentValve = 0;
    Serial.println("Current valve is now 0");
    
  }

}
