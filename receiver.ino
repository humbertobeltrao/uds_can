#include <SPI.h>
#include <mcp_can.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>


#define CAN_SCK 14
#define CAN_MISO 12
#define CAN_MOSI 13
#define CAN_CS 5

MCP_CAN CAN(CAN_CS);         // MCP2515 instance

File firmwareFile;             // File object to write to SPIFFS
bool transferComplete = false;

unsigned long bytesReceived = 0;
unsigned long startTime = 0;
unsigned long endTime = 0;
bool timingStarted = false;
bool dataTransferStarted = false;
File binFile;
bool eofReceived = false;

void setup() {
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Initialize MCP2515 CAN on HSPI
    SPI.begin(CAN_SCK, CAN_MISO, CAN_MOSI, CAN_CS);  // Initialize HSPI
    if (CAN.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
      Serial.println("CAN Bus initialized on HSPI");
      CAN.setMode(MCP_NORMAL);  // Set CAN to normal mode
    } else {
      Serial.println("Error Initializing CAN Bus");
      while (1);
    }

    binFile = SPIFFS.open("/test.bin", FILE_WRITE);
    if (!binFile) {
        Serial.println("Failed to open file for writing");
        return;
    }
    Serial.println("Ready to receive firmware via CAN...");

}

void receiveCANData() {
  unsigned long canId;
  unsigned char len = 0;
  unsigned char buf[8];
  
  
    while(true){
      if (CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&canId, &len, buf);
        
       
       
        if(len == 3 && buf[0] == 0x37 && buf[1] == 0x02 && buf[2] == 0x01) {
          eofReceived = true;
          break;
        }

         // Write received data to the firmware file in SPIFFS
        binFile.write(buf, len);
        bytesReceived += len;
        // Log progress
        Serial.print("Bytes received: ");
        Serial.println(bytesReceived);
        delay(10);
      }
    }
    binFile.close();
    File receivedFile = SPIFFS.open("/test.bin", FILE_READ);
    if (receivedFile) {
        unsigned long actualFileSize = receivedFile.size();
        Serial.print("Actual file size received: ");
        Serial.println(actualFileSize);
        receivedFile.close();

        // Start flashing if EOF was received
        if (eofReceived) {
            flashESP32();
            dataTransferStarted = false;
        } else {
            Serial.println("EOF not received properly.");
        }
    } else {
        Serial.println("Failed to open received file for verification.");
    }
  
}
void loop() {

  if(!dataTransferStarted){
    receiveUDSRequest();
  } else {
    receiveCANData();
  }
    
}

void sendUDSExtendedSessionResponse() {
    byte udsRequest[3] = {0x02, 0x50, 0x03}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7E8, 0, 3, udsRequest);
    Serial.println("UDS Extended Diagnostic Session Response sent");
}

void sendUDSControlDTCSettingResponse() {
    byte udsRequest[3] = {0x02, 0xC5, 0x01}; // 
    CAN.sendMsgBuf(0x7E8, 0, 3, udsRequest);
    Serial.println("UDS Control DTC Diagnostic Session Response sent");
}

void sendUDSCommunicationControlResponse() {
    byte udsRequest[3] = {0x02, 0x71, 0x01}; // 
    CAN.sendMsgBuf(0x7E8, 0, 3, udsRequest);
    Serial.println("UDS Communication Control Response sent");
}

void sendUDSProgrammingSessionResponse() {
    byte udsRequest[3] = {0x02, 0x50, 0x02}; // 
    CAN.sendMsgBuf(0x7E8, 0, 3, udsRequest);
    Serial.println("UDS Programming Session Response sent");
}

void sendUDSSecurityAccessResponse() {
    byte udsRequest[2] = {0x02, 0x67}; // 
    CAN.sendMsgBuf(0x7E8, 0, 2, udsRequest);
    Serial.println("UDS Security Access Response sent");
}

void sendUDSSecurityAccessSeedResponse() {
    byte seed[4] = {0x01, 0x02, 0x03, 0x04};  // Example seed for security access
    byte udsResponse[6] = {0x67, 0x01, seed[0], seed[1], seed[2], seed[3]};
    
    CAN.sendMsgBuf(0x7E8, 0, 6, udsResponse);
    Serial.println("UDS Security Access Seed Response sent");
}

void receiveUDSRequest() {
  unsigned long canId;
  unsigned char len = 0;
  unsigned char buf[8];
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
       CAN.readMsgBuf(&canId, &len, buf);
            if (canId == 0x7DF && buf[0] == 0x02 && buf[1] == 0x10 && buf[2] == 0x03) {
                Serial.println("UDS Extended Diagnostic Session Session Request valid, sending response...");
                sendUDSExtendedSessionResponse();
                //dataTransferStarted = true;
            } else if (canId == 0x7DF && buf[0] == 0x02 && buf[1] == 0x85 && buf[2] == 0x01) {
                Serial.println("UDS Control DTC Session Session Request valid, sending response...");
                sendUDSControlDTCSettingResponse();
            } else if (canId == 0x7DF && buf[0] == 0x02 && buf[1] == 0x31 && buf[2] == 0x01) {
                Serial.println("UDS Communication Control Request valid, sending response...");
                sendUDSCommunicationControlResponse();
            } else if (canId == 0x7DF && buf[0] == 0x02 && buf[1] == 0x10 && buf[2] == 0x02) {
                Serial.println("UDS Programming Session Request valid, sending response...");
                sendUDSProgrammingSessionResponse();
            } else if (canId == 0x7DF && buf[0] == 0x02 && buf[1] == 0x27) {
                Serial.println("UDS Security Access Request valid, sending seed...");
                sendUDSSecurityAccessResponse();
                dataTransferStarted = true;
            } 
        }
    
}


void formatSPIFFS() {
    if (!SPIFFS.begin(true)) { // true parameter will format the filesystem if it's corrupted
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully");

    if (SPIFFS.format()) {
        delay(15000);
        Serial.println("SPIFFS formatted successfully");
        //sendUDSEraseMemoryResponse();
    } else {
        Serial.println("SPIFFS formatting failed");
    }
}

void flashESP32() {
    Serial.println("Starting ESP32 flash process...");

    File updateBin = SPIFFS.open("/test.bin", FILE_READ);
    if (!updateBin) {
        Serial.println("Failed to open bin file for reading");
        return;
    }

    if (!Update.begin(updateBin.size())) {
        Serial.println("Not enough space to begin OTA");
        updateBin.close();
        return;
    }

    size_t written = Update.writeStream(updateBin);
    if (written == updateBin.size()) {
        Serial.println("Written : " + String(written) + " successfully");
    } else {
        Serial.println("Written only : " + String(written) + "/" + String(updateBin.size()) + ". Retry?");
    }
    if (Update.end()) {
        Serial.println("OTA done!");
        if (Update.isFinished()) {
            Serial.println("Update successfully completed. Rebooting.");
            ESP.restart();
        } else {
            Serial.println("Update not finished. Something went wrong!");
        }
    } else {
        Serial.println("Error Occurred. Error #: " + String(Update.getError()));
    }
    updateBin.close();
}

bool validateSecurityKey(byte* receivedKey) {
    byte expectedKey[4] = {0x02, 0x03, 0x04, 0x05}; // Replace with the expected key based on the seed
    for (int i = 0; i < 4; i++) {
        if (receivedKey[i] != expectedKey[i]) {
            return false;
        }
    }
    return true;
}


void sendUDSPositiveResponse(byte responseCode) {
    byte udsResponse[3] = {0x03, 0x67, responseCode};  // 0x67 for Security Access response
    CAN.sendMsgBuf(0x7E8, 0, 3, udsResponse);
    Serial.println("UDS Positive Response sent for Security Access granted.");
}

void sendUDSNegativeResponse(byte serviceId, byte errorCode) {
    byte udsResponse[3] = {0x7F, serviceId, errorCode};  // UDS Negative Response format
    CAN.sendMsgBuf(0x7E8, 0, 3, udsResponse);
    Serial.println("UDS Negative Response sent");
}
