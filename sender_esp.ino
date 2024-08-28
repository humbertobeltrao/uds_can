#include <SPI.h>
#include <mcp_can.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// CAN setup
const int SPI_CS_PIN = 5;
MCP_CAN CAN(SPI_CS_PIN); // Create a CAN object

unsigned long bytesSent = 0;
bool dataTransferStarted = false;
const int bufferSize = 8;
char buffer[bufferSize];

// Web server setup
AsyncWebServer server(80);
File fsUploadFile;
String fileName;

void setup() {
    Serial.begin(115200);
     Serial.begin(115200);

    // Attempt to mount LittleFS
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS. Formatting...");
        // Attempt to format the filesystem
        if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully. Mounting...");
            if (!LittleFS.begin()) {
                Serial.println("Failed to mount LittleFS after formatting");
                return;
            }
        } else {
            Serial.println("LittleFS format failed");
            return;
        }
    }

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());

    // Route for file upload
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
      request->send(200);
    }, handleUpload);
  
    server.begin();

    // Initialize the MCP2515 CAN controller
    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
      Serial.println("CAN BUS Shield init ok!");
    } else {
      Serial.println("CAN BUS Shield init fail");
      while (1);
    }
    CAN.setMode(MCP_NORMAL); // Set operation mode to normal
}

void loop() {
    if (!dataTransferStarted) {
        // Wait for UDS response to start data transfer
        receiveUDSResponse();
    } else {
        // Data transfer loop
        File file = LittleFS.open("/" + fileName, FILE_READ);
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        while (file.available()) {
            int len = file.readBytes(buffer, bufferSize);
            CAN.sendMsgBuf(0x00, 0, len, (byte *)buffer);
            bytesSent += len;
            delay(100); // Short delay to control the send rate
        }

        file.close();
        sendUDSTransferExitRequest(); // Send the EOF message
        dataTransferStarted = false;
    }
}

void sendUDSProgrammingSessionRequest() {
    byte udsRequest[3] = {0x02, 0x10, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Programming Session Request sent");
}

void sendUDSExtendedDiagnosticSessionRequest() {
    byte udsRequest[3] = {0x03, 0x10, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Extended Diagnostic Session Request sent");
}

void sendUDSControlDTCSettingRequest() {
    byte udsRequest[3] = {0x85, 0x02, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
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
    Serial.println("UDS Security Access Key Response sent");
}

void receiveUDSResponse() {
    unsigned char len = 0;
    unsigned char buf[8];
    unsigned char ext = 0;
    unsigned long canId = 0;

    if (CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&canId, &ext, &len, buf);

        if (canId == 0x7E8) { // Assuming 0x7E8 is the response CAN ID
            Serial.println("UDS Response received");
            // Check UDS response and set dataTransferStarted to true if valid
            if (buf[0] == 0x03 && buf[1] == 0x50 && buf[2] == 0x01) {
                Serial.println("UDS Extended Diagnostic Session Response valid, sending control DTC setting request...");
                sendUDSControlDTCSettingRequest();
            } else if (buf[0] == 0xC5 && buf[1] == 0x02 && buf[2] == 0x01) {
                Serial.println("UDS Control DTC Response valid, sending programming session control request...");
                sendUDSProgrammingSessionRequest();
            } else if (buf[0] == 0x02 && buf[1] == 0x50 && buf[2] == 0x01) {
                 sendUDSSecurityAccessRequest();
            } else if (buf[0] == 0x67 && buf[1] == 0x01) {
                  Serial.println("UDS Security Access Seed received");
                
                  byte key[4];
                  generateSecurityKey(buf + 2, key); // Generate key using the received seed
                  sendUDSSecurityAccessKeyResponse(key);
                  
            } else if (buf[0] == 0x03 && buf[1] == 0x67) {
                Serial.println("UDS Security Access Seed Response valid, sending communication control request...");
                sendUDSCommunicationControlRequest();
            } else if (buf[0] == 0x71 && buf[1] == 0x03 && buf[2] == 0x01) {
                dataTransferStarted = true;
                Serial.println("UDS Communication Control Response valid, starting file transfer...");
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

void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    fileName = filename;
    Serial.printf("UploadStart: %s\n", fileName.c_str());
    fsUploadFile = LittleFS.open("/" + fileName, "w");
  }
  if (fsUploadFile) {
    fsUploadFile.write(data, len);
  }
  if (final) {
    Serial.printf("UploadEnd: %s, %u B\n", fileName.c_str(), index + len);
    sendUDSExtendedDiagnosticSessionRequest();
    if (fsUploadFile) {
      fsUploadFile.close();
      fsUploadFile = LittleFS.open("/" + fileName, "r");
    }
  }
}

void generateSecurityKey(byte* seed, byte* key) {
    // Implement your key generation logic based on the seed
    // This is a placeholder example; the actual algorithm will depend on your security requirements
    for (int i = 0; i < 4; i++) {
        key[i] = seed[i] + 1; // Simple example; replace with real algorithm
    }
}
