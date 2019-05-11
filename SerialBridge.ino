#include <odroid_go.h>
#include <WiFi.h>
HardwareSerial Serial_2(1);

//how many clients should be able to telnet to this ESP32
#define MAX_SRV_CLIENTS 1
WiFiServer server(22);
WiFiClient serverClient;

#define SD_PIN_NUM_MISO 19
#define SD_PIN_NUM_MOSI 23
#define SD_PIN_NUM_CLK  18
#define SD_PIN_NUM_CS 22

void setup() {
  GO.begin();
  GO.Speaker.setVolume(0);
  pinMode(25, OUTPUT);
  digitalWrite(25, LOW);
  pinMode(26, OUTPUT);
  digitalWrite(26, LOW);
  GO.lcd.setTextWrap(false);
  GO.battery.setProtection(true);

  SPI.begin(SD_PIN_NUM_MISO, SD_PIN_NUM_MOSI, SD_PIN_NUM_CLK, SD_PIN_NUM_CS);
  if (!SD.begin(SD_PIN_NUM_CS)) {
    GO.lcd.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    GO.lcd.println("No SD card attached");
    return;
  }

  GO.lcd.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    GO.lcd.println("MMC");
  } else if (cardType == CARD_SD) {
    GO.lcd.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    GO.lcd.println("SDHC");
  } else {
    GO.lcd.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  GO.lcd.printf("SD Card Size: %lluMB\n", cardSize);

  String WifiSSID = "";
  String WifiPSK = "";

  String path = "/WIFI.TXT";
  GO.lcd.print("\nReading file: ");
  GO.lcd.println(path);
  File wifiFile = SD.open(path);
  if (!wifiFile) {
    GO.lcd.println("Failed to open file for reading");
    return;
  }
  while (wifiFile.available()) {
    WifiSSID = wifiFile.readStringUntil('\n');
    WifiSSID.replace("\r", "");
    WifiPSK = wifiFile.readStringUntil('\n');
    WifiPSK.replace("\r", "");
    break;
  }
  wifiFile.close();
  GO.lcd.print("SSID: '");
  GO.lcd.print(WifiSSID);
  GO.lcd.println("'");
  GO.lcd.print("PSK:  '");
  GO.lcd.print(WifiPSK);
  GO.lcd.println("'");

  String baud = "";
  path = "/SERIAL.TXT";
  GO.lcd.print("\nReading file: ");
  GO.lcd.println(path);
  File baudFile = SD.open(path);
  if (!baudFile) {
    GO.lcd.println("Failed to open file for reading");
    return;
  }
  while (baudFile.available()) {
    baud = baudFile.readStringUntil('\n');
    baud.replace("\r", "");
    break;
  }
  baudFile.close();
  GO.lcd.print("Serial baud rate: ");
  GO.lcd.println(baud);

  GO.lcd.println("\nSerial Connection");
  GO.lcd.println("GND|TX|RX|VCC");
  GO.lcd.println("GND|04|15|P3V3\n");

  GO.lcd.println("Battery:" + String(GO.battery.getPercentage()) + "%\n");  
  delay(3000);

  //delete old wifi Credentials
  WiFi.disconnect();
  WiFi.begin(WifiSSID.c_str(), WifiPSK.c_str());
  GO.lcd.print("Connecting Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    GO.lcd.print(".");
  }
  GO.lcd.println();
  GO.lcd.print("Connected to: ");
  GO.lcd.println(WifiSSID);

  server.begin();
  server.setNoDelay(true);

  GO.lcd.print("Ready! Use '");
  GO.lcd.print(WiFi.localIP());
  GO.lcd.println(":22' to connect\n");
  delay(3000);

  Serial_2.begin(baud.toInt(), SERIAL_8N1, 15, 4);
}

String displayBuffer[30];
bool dataDirection[30];

void updateDisplay() {
  GO.lcd.clearDisplay();
  GO.lcd.setCursor(0, 0);
  for (byte i = 0; i < 30; i++) {
    if (dataDirection[i])
      GO.lcd.setTextColor(MAGENTA);
    else
      GO.lcd.setTextColor(GREEN);
    GO.lcd.print(displayBuffer[i]);
  }
}

void pushLine(String newLine, bool dir) {
  for (byte i = 0; i < 29; i++) {
    displayBuffer[i] = displayBuffer[i + 1];
    dataDirection[i] = dataDirection[i + 1];
  }
  displayBuffer[29] = newLine;
  dataDirection[29] = dir;
  updateDisplay();
}

String inBuffer = "";
String outBuffer = "";

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    //check if there are any new clients
    if (server.hasClient()) {
      //find free/disconnected spot
      if (!serverClient || !serverClient.connected()) {
        GO.lcd.clearDisplay();
        GO.lcd.setCursor(0, 0);
        GO.lcd.setTextColor(YELLOW);
        if (serverClient) serverClient.stop();
        serverClient = server.available();
        if (!serverClient) {
          GO.lcd.println("available broken");
        }
        GO.lcd.print("New client: ");
        GO.lcd.println(serverClient.remoteIP());
        delay(1000);
      }
    }

    //check clients for data
    if (serverClient && serverClient.connected()) {
      if (serverClient.available()) {
        //get data from the telnet client and push it to the UART
        while (serverClient.available()) {
          char c = serverClient.read();
          Serial_2.write(c);

          outBuffer += c;
          if (c == '\n') {
            pushLine(outBuffer, 1);
            outBuffer = "";
          }
        }
      }
    }
    else {
      if (serverClient) {
        serverClient.stop();
      }
    }


    //check UART for data
    if (Serial_2.available()) {
      char c = Serial_2.read();
      //push UART data to all connected telnet clients
      if (serverClient && serverClient.connected()) {
        serverClient.write(c);
      }

      inBuffer += c;
      if (c == '\n') {
        pushLine(inBuffer, 0);
        inBuffer = "";
      }
    }

  }
  else {
    GO.lcd.println("WiFi not connected!");
    if (serverClient) serverClient.stop();
    delay(1000);
  }
}
