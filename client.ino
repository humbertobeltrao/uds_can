#include "SPIFFS.h"  // Change to SPIFFS
#include "FS.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <mcp_can.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "cert.h"


// CAN Bus (HSPI) Pin Definitions
#define CAN_SCK 14
#define CAN_MISO 12
#define CAN_MOSI 13
#define CAN_CS 5

#define CAN_INT_PIN 4  // Interrupt pin connected to MCP2515 INT

// SD Card (VSPI) Pin Definitions
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS 15              // VSPI CS pin for SD card

// Replace with your Wi-Fi credentials
const char* ssid = "Humberto_ETECH_ULTRA";
const char* password = "astronauta3005";

bool dataTransferStarted = false;

bool writtenToSD = false;

String fileName;

//const int SPI_CS_PIN = 5;
MCP_CAN CAN(CAN_CS);  // Create a CAN object

unsigned long bytesSent = 0;
const int bufferSize = 8;
char buffer[bufferSize];

const char* url;

SPIClass spiSD(HSPI);  // Create a new SPIClass instance for VSPI

File firmwareFile;

// Web server on port 80
WebServer server(80);

void handleUpdateFirmware() {
  if (server.hasArg("plain")) {
    String requestBody = server.arg("plain");
    StaticJsonDocument<200> doc;

    // Parse the received JSON
    DeserializationError error = deserializeJson(doc, requestBody);
    if (error) {
      Serial.println("Failed to parse JSON");
      server.send(400, "application/json", "{\"status\":\"error\"}");
      return;
    }

    // Extract the URL from the JSON
    url = doc["url"];
  
    // Attempt to download the firmware
    if (downloadFirmware(url, "/firmware.bin")) {
      server.send(200, "application/json", "{\"status\":\"success\"}");
    } else {
      server.send(500, "application/json", "{\"status\":\"error\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\"}");
  }
}

bool downloadFirmware(const char* url, String filename) {
  
    Serial.println(url);
    WiFiClientSecure * client = new WiFiClientSecure;

    if (client) {
        client->setCACert(rootCACertificate);  // Set root CA certificate for HTTPS

        HTTPClient https;
        if (https.begin( * client, url)) {
            Serial.print("[HTTPS] GET...\n");
            delay(100);
            int httpCode = https.GET();
            if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND || httpCode == HTTP_CODE_SEE_OTHER) {
              String redirectURL = https.getLocation();  // Get the new URL from the Location header
              Serial.print("Redirecting to: ");
              Serial.println(redirectURL);
              https.end();  // Close the initial connection
          
              // Begin a new connection with the redirect URL
              https.begin(* client, redirectURL);
              httpCode = https.GET();  // Make the new request
            }
            delay(100);
            if (httpCode == HTTP_CODE_OK) {
              
                Serial.println("Downloading file...");

                File file = SD.open(filename, FILE_WRITE);

                WiFiClient* stream = https.getStreamPtr();
                
                int totalDownloaded = 0;
                unsigned long startTime = millis();
                while (https.connected() && (https.getSize() > 0 || https.getSize() == -1)) {
                  static uint8_t buffer[16384];
                    size_t size = stream->available();
                    if (size) {
                        int bytesRead = stream->readBytes(buffer, ((size > sizeof(buffer)) ? sizeof(buffer) : size));
                        file.write(buffer, bytesRead);
                        totalDownloaded += bytesRead;
                        Serial.print("Downloaded: ");
                        Serial.print(totalDownloaded);
                        Serial.println(" bytes");
                    }
                     // Break the loop when all data has been downloaded
                    if (https.getSize() != -1 && totalDownloaded >= https.getSize()) {
                        break;
                    }
                    
                }

                file.close();
                Serial.print("Download completed in ");
                Serial.print(millis() - startTime);
                Serial.println(" ms");
                writtenToSD = true;
                Serial.println("Start to transfer firmware via UDS...");
                sendUDSExtendedDiagnosticSessionRequest();
                //delay(1000);
            } else {
                Serial.print("Error Occurred During Download: ");
                Serial.println(httpCode);
                
            }
            https.end();
        }
        delete client;
        return true;
    }
    return false;
}

void IRAM_ATTR CANInterruptHandler() {
  receiveUDSResponse();  // Handle the CAN message in the ISR
}

void setup() {
  Serial.begin(115200);
  
   // Initialize CAN Bus on HSPI
  SPI.begin(CAN_SCK, CAN_MISO, CAN_MOSI, CAN_CS);  // Initialize HSPI
  if (CAN.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN Bus initialized on HSPI");
    CAN.setMode(MCP_NORMAL);  // Set CAN to normal mode
  } else {
    Serial.println("Error Initializing CAN Bus");
    while (1);
  }
  // Initialize SD Card on VSPI
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);  // Initialize VSPI for SD
  if (!SD.begin(SD_CS, spiSD)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("SD Card initialized on VSPI");

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up server routes
  server.on("/update-firmware", HTTP_POST, handleUpdateFirmware);
  server.begin();
  Serial.println("HTTP server started");

  //firmwareFile = SD.open("/firmware.bin", FILE_READ);

  
}


void loop() {
  if(!writtenToSD) {
    server.handleClient();
  } else {
    receiveUDSResponse();
    if(dataTransferStarted) {
      sendDataFromSDToCAN();
    }
  }
   
}

void sendUDSProgrammingSessionRequest() {
    byte udsRequest[3] = {0x02, 0x10, 0x02}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Programming Session Request sent");
}

void sendUDSExtendedDiagnosticSessionRequest() {
    byte udsRequest[3] = {0x02, 0x10, 0x03}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Extended Diagnostic Session Request sent");
}

void sendUDSControlDTCSettingRequest() {
    byte udsRequest[3] = {0x02, 0x85, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS ControlDTCSetting=Off Request sent");
}

void sendUDSCommunicationControlRequest() {
    byte udsRequest[3] = {0x02, 0x31, 0x01}; // Communication Control (0x31), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Communication Control Request sent");
}

void sendUDSSecurityAccessRequest() {
    byte udsRequest[2] = {0x02, 0x27}; // Request seed for Security Access
    CAN.sendMsgBuf(0x7DF, 0, 2, udsRequest);
    Serial.println("UDS Security Access Request sent");
}

void sendUDSSecurityAccessKeyResponse(byte* key) {
    byte udsRequest[8] = {0x06, 0x27, 0x02, key[0], key[1], key[2], key[3]}; // Send the generated key
    CAN.sendMsgBuf(0x7DF, 0, 8, udsRequest);
    Serial.println("UDS Security Access Key Request sent");
}

void receiveUDSResponse() {
    unsigned char len = 0;
    unsigned char buf[8];
    unsigned char ext = 0;
    unsigned long canId = 0;

    if (CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&canId, &ext, &len, buf);

        if (canId == 0x7E8) { // Assuming 0x7E8 is the response CAN ID
            if (buf[0] == 0x02 && buf[1] == 0x50 && buf[2] == 0x03) {
                Serial.println("UDS Extended Diagnostic Session Response valid, sending control DTC setting request...");
                sendUDSControlDTCSettingRequest();
            } else if (buf[0] == 0x02 && buf[1] == 0xC5 && buf[2] == 0x01) {
                Serial.println("UDS Control DTC Response valid, sending communication control request...");
                sendUDSCommunicationControlRequest();
            } else if (buf[0] == 0x02 && buf[1] == 0x71 && buf[2] == 0x01) {
                 Serial.println("UDS Communication Control Response valid, sending programming session request...");
                 sendUDSProgrammingSessionRequest();
            } else if (buf[0] == 0x02 && buf[1] == 0x50 && buf[2] == 0x02) {
                 Serial.println("UDS Programming Session Response valid, sending security access request...");
                 sendUDSSecurityAccessRequest();
            } else if (buf[0] == 0x02 && buf[1] == 0x67) {
                  Serial.println("UDS Security Access Seed received");
                  dataTransferStarted = true;
            }
        }
    }
}

void sendUDSTransferExitRequest() {
    //byte eofMessage[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xBA, 0xAD}; // Special sequence of bytes
    byte udsRequest[3] = {0x37, 0x02, 0x01};
    CAN.sendMsgBuf(0x00, 0, 3, udsRequest);
    //Serial.println("EOF message sent");
    Serial.println("UDS Transfer and Exit request sent");
}

void generateSecurityKey(byte* seed, byte* key) {
    // Implement your key generation logic based on the seed
    // This is a placeholder example; the actual algorithm will depend on your security requirements
    for (int i = 0; i < 4; i++) {
        key[i] = seed[i] + 1; // Simple example; replace with real algorithm
    }
}

void sendDataFromSDToCAN() {
  firmwareFile = SD.open("/firmware.bin", FILE_READ);
    while(firmwareFile.available()) {
      int len = firmwareFile.readBytes(buffer, bufferSize);
      CAN.sendMsgBuf(0x00, 0, len, (byte *)buffer);
      bytesSent += len;
      Serial.print("Bytes sent: ");
      Serial.println(bytesSent);
      delay(100);
    }
    firmwareFile.close();
    sendUDSTransferExitRequest(); // Send the EOF message
    writtenToSD = false;
    dataTransferStarted = false;
}
