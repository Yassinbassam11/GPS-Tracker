#include <TinyGPS.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal.h>

// GPS Setup
#define GPS_RX 5  // Arduino pin 5 (RX)
#define GPS_TX 4  // Arduino pin 4 (TX)
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
TinyGPS gps;

// GSM Setup
#define GSM_RX 3  // Arduino pin 3 (RX)
#define GSM_TX 2  // Arduino pin 2 (TX)
SoftwareSerial mySerial(GSM_RX, GSM_TX);

// LCD Setup
LiquidCrystal lcd(7, 8, 9, 10, 11, 12); // RS, EN, D4, D5, D6, D7

// Global variables
// Initialize with default values
float lat = 30.90;
float lon = 33.70;
float altitude = 156.20;
int satellites = 8;
unsigned long age;
String lastSMS = "";
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 2000;
bool smsNotificationEnabled = true;

// Function declarations
void waitForResponse(unsigned long timeout);
void updateGPSData();
void displayGPSOnLCD();
void printGPSData();
void processInboxResponse(String response);
void processNewSMS(String response);
void displaySMSOnLCD(String sender, String message);
void checkIncomingSMS(bool manualCheck = false);
void printWelcomeMessage();
void processSMSCommand(String command);
void sendSMS(String number, String message);
void processTerminalCommand();
void sendLocationSMS(String number); // NEW FUNCTION

void setup() {
  // Initialize serial communications
  Serial.begin(9600);
  gpsSerial.begin(9600);
  mySerial.begin(9600);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.print("Initializing...");
  
  // Initialize GSM module
  delay(2000); // Wait for module to power on
  mySerial.println("AT"); // Check if module is responding
  waitForResponse(1000);
  mySerial.println("AT+CMGF=1"); // Set SMS text mode
  waitForResponse(1000);
  mySerial.println("AT+CNMI=2,2,0,0,0"); // Set new SMS notification
  waitForResponse(1000);
  
  lcd.clear();
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
  
  printWelcomeMessage();
}

void loop() {
  // GPS Processing
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (gps.encode(c)) {
      updateGPSData();
    }
  }

  // Update display
  if (millis() - lastDisplayUpdate > displayUpdateInterval) {
    displayGPSOnLCD();
    lastDisplayUpdate = millis();
  }

  // Check for incoming SMS
  checkIncomingSMS();

  // Process terminal commands
  if (Serial.available() > 0) {
    processTerminalCommand();
  }
}

void waitForResponse(unsigned long timeout) {
  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (mySerial.available()) {
      Serial.write(mySerial.read());
    }
  }
}

void updateGPSData() {
  gps.f_get_position(&lat, &lon, &age);
  satellites = gps.satellites();
  altitude = gps.f_altitude();
}

void displayGPSOnLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Lat:");
  lcd.print(lat, 4);
  
  lcd.setCursor(0, 1);
  lcd.print("Lon:");
  lcd.print(lon, 4);
}

void printGPSData() {
  Serial.println("\n=== GPS DATA ===");
  Serial.print("Latitude: ");
  Serial.println(lat, 6);
  Serial.print("Longitude: ");
  Serial.println(lon, 6);
  Serial.print("Satellites: ");
  Serial.println(satellites);
  Serial.print("Altitude: ");
  Serial.print(altitude);
  Serial.println(" m");
  Serial.println("================");
}

void checkIncomingSMS(bool manualCheck) {
  if (manualCheck) {
    Serial.println("Checking inbox...");
    lcd.clear();
    lcd.print("Checking Inbox");
    
    mySerial.println("AT+CMGF=1");
    delay(500);
    mySerial.println("AT+CMGL=\"ALL\"");
    
    // Wait for response
    unsigned long start = millis();
    String response = "";
    while (millis() - start < 5000) { // 5 second timeout
      while (mySerial.available()) {
        char c = mySerial.read();
        response += c;
        if (response.endsWith("OK\r\n")) {
          break;
        }
      }
    }
    
    if (response.length() > 0) {
      Serial.println(response);
      processInboxResponse(response);
    } else {
      Serial.println("No messages found");
      lcd.clear();
      lcd.print("No Messages");
      delay(2000);
    }
    return;
  }

  // Automatic SMS notification check
  while (mySerial.available()) {
    String response = mySerial.readString();
    if (response.indexOf("+CMT:") != -1) {
      processNewSMS(response);
    }
  }
}

void processInboxResponse(String response) {
  int index = 0;
  while ((index = response.indexOf("+CMGL:", index)) != -1) {
    int msgStart = response.indexOf("\"", index);
    int msgEnd = response.indexOf("\"", msgStart + 1);
    String sender = response.substring(msgStart + 1, msgEnd);
    
    int contentStart = response.indexOf("\n", msgEnd) + 1;
    int contentEnd = response.indexOf("OK\r\n", contentStart);
    String content = response.substring(contentStart, contentEnd);
    content.trim();
    
    Serial.println("\n--- SMS Message ---");
    Serial.print("From: ");
    Serial.println(sender);
    Serial.print("Message: ");
    Serial.println(content);
    Serial.println("------------------");
    
    index = contentEnd;
  }
}

void processNewSMS(String response) {
  int index = response.indexOf('"');
  String sender = response.substring(index + 1, response.indexOf('"', index + 1));
  String message = response.substring(response.indexOf("\n") + 1);
  message.trim();
  
  lastSMS = message;
  
  Serial.println("\n--- NEW SMS ---");
  Serial.print("From: ");
  Serial.println(sender);
  Serial.print("Message: ");
  Serial.println(message);
  Serial.println("--------------");
  
  displaySMSOnLCD(sender, message);

  // 🔍 Check for location request
  String lowerMsg = message;
  lowerMsg.toLowerCase();
  if (lowerMsg.indexOf("get location") != -1 || lowerMsg.indexOf("location") != -1) {
    if (satellites > 0) {
      sendLocationSMS(sender);
    } else {
      sendSMS(sender, "No GPS fix yet. Try again later.");
    }
  }
}

void displaySMSOnLCD(String sender, String message) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("From: ");
  lcd.print(sender.substring(sender.length() - min(10, sender.length())));
  
  lcd.setCursor(0, 1);
  lcd.print(message.substring(0, min(16, message.length())));
  
  delay(5000);
  displayGPSOnLCD();
}

void processTerminalCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  
  if (command == "gps") {
    printGPSData();
  }
  else if (command.startsWith("sms ")) {
    processSMSCommand(command);
  }
  else if (command == "inbox") {
    checkIncomingSMS(true);
  }
  else if (command == "notify on") {
    smsNotificationEnabled = true;
    mySerial.println("AT+CNMI=2,2,0,0,0");
    Serial.println("SMS notifications enabled");
  }
  else if (command == "notify off") {
    smsNotificationEnabled = false;
    mySerial.println("AT+CNMI=0,0,0,0,0");
    Serial.println("SMS notifications disabled");
  }
  else if (command == "help") {
    printWelcomeMessage();
  }
  else {
    Serial.println("Unknown command. Type 'help' for commands.");
  }
}

void processSMSCommand(String command) {
  int firstSpace = command.indexOf(' ');
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  
  if (firstSpace == -1 || secondSpace == -1) {
    Serial.println("Usage: sms <number> <message>");
    return;
  }
  
  String number = command.substring(firstSpace + 1, secondSpace);
  String message = command.substring(secondSpace + 1);
  
  sendSMS(number, message);
}

void sendSMS(String number, String message) {
  mySerial.println("AT+CMGF=1");
  delay(500);
  mySerial.print("AT+CMGS=\"");
  mySerial.print(number);
  mySerial.println("\"");
  delay(500);
  mySerial.print(message);
  delay(100);
  mySerial.write(26); // CTRL+Z
  delay(1000);
  
  Serial.print("SMS sent to ");
  Serial.println(number);
  
  lcd.clear();
  lcd.print("SMS Sent to:");
  lcd.setCursor(0, 1);
  lcd.print(number.substring(0, 16));
  delay(3000);
  displayGPSOnLCD();
}

void sendLocationSMS(String number) {
  String mapLink = "https://maps.google.com/?q= ";
  mapLink += String(lat, 6);
  mapLink += ",";
  mapLink += String(lon, 6);
  mapLink += "&z=15";

  sendSMS(number, mapLink);
}

void printWelcomeMessage() {
  Serial.println("\n=== GPS-GSM SYSTEM ===");
  Serial.println("Available Commands:");
  Serial.println("gps - Show current GPS data");
  Serial.println("sms <number> <message> - Send SMS");
  Serial.println("inbox - Check all SMS messages");
  Serial.println("notify on/off - Toggle SMS notifications");
  Serial.println("help - Show this message");
  Serial.println("=====================");
}
