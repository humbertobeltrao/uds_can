#include <SPI.h>
#include <mcp2515.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>

struct can_frame canMsg;
MCP2515 mcp2515(5);

//const unsigned long expectedFileSize = 237568; // Expected 240KB file size
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

    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    Serial.println("MCP2515 Initialized Successfully!");
    binFile = SPIFFS.open("/test.bin", FILE_WRITE);
    if (!binFile) {
        Serial.println("Failed to open file for writing");
        return;
    }

}

void loop() {
    if (!dataTransferStarted) {
        receiveUDSRequest();
    } else {
        receiveData();
    }
}

void receiveData() {
    while (true) {
        if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
            if (!timingStarted) {
                startTime = micros();
                timingStarted = true;
            }

            // Check for EOF indicator
//            if (canMsg.can_dlc == 8 && memcmp(canMsg.data, "\xDE\xAD\xBE\xEF\xFE\xED\xBA\xAD", 8) == 0) {
//                eofReceived = true;
//                break;
//            }

            if (canMsg.can_dlc == 3 && memcmp(canMsg.data, "\x37\x02\x01", 3) == 0) {
                eofReceived = true;
                sendUDSTransferExitResponse();
                break;
            }

            binFile.write(canMsg.data, canMsg.can_dlc);
            bytesReceived += canMsg.can_dlc;

            Serial.print("Bytes received: ");
            Serial.println(bytesReceived);

            delay(10); // Adjust delay as needed
        }
    }

    // Finalize the file transfer and close the file
    binFile.close();
    endTime = micros();
    unsigned long elapsedTime = endTime - startTime;

    Serial.print("Time taken to receive file: ");
    Serial.print(elapsedTime / 1000000.0);
    Serial.println(" seconds");

    // Verify the actual file size
    File receivedFile = SPIFFS.open("/test.bin", FILE_READ);
    if (receivedFile) {
        unsigned long actualFileSize = receivedFile.size();
        Serial.print("Actual file size received: ");
        Serial.println(actualFileSize);
        receivedFile.close();

        // Start flashing if EOF was received
        if (eofReceived) {
            flashESP32();
        } else {
            Serial.println("EOF not received properly.");
        }
    } else {
        Serial.println("Failed to open received file for verification.");
    }
}

void receiveUDSRequest() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        if (canMsg.can_id == 0x7DF) {
            if (canMsg.data[0] == 0x03 && canMsg.data[1] == 0x10 && canMsg.data[2] == 0x01) {
                sendUDSExtendedSessionResponse();
                Serial.println("UDS Extended Diagnostic Session Session Request valid, sending response...");
            } else if (canMsg.data[0] == 0x85 && canMsg.data[1] == 0x02 && canMsg.data[2] == 0x01) {
                sendUDSControlDTCSettingResponse();
                Serial.println("UDS ControlDTCSetting Request valid, sending response...");
            } else if(canMsg.data[0] == 0x02 && canMsg.data[1] == 0x10 && canMsg.data[2] == 0x01) {
                sendUDSProgrammingSessionResponse();
                Serial.println("UDS Programming Session Request valid, sending response...");
            } else if (canMsg.data[0] == 0x02 && canMsg.data[1] == 0x27) { 
                sendUDSSecurityAccessSeedResponse();
                Serial.println("UDS Security Access Request received, sending seed...");
            } else if(canMsg.data[0] == 0x06 && canMsg.data[1] == 0x27 && canMsg.data[2] == 0x02) {
                if (validateSecurityKey(canMsg.data + 3)) {
                    sendUDSPositiveResponse(0x02); 
                    Serial.println("Security Access Key validated. Access granted.");
                    // Proceed to the next step (e.g., allow data transfer)
                } else {
                    sendUDSNegativeResponse(0x27, 0x35);  
                    Serial.println("Security Access Key validation failed. Access denied.");
                }
            } else if (canMsg.data[0] == 0x02 && canMsg.data[1] == 0x31 && canMsg.data[2] == 0x01) {
                //formatSPIFFS();
                dataTransferStarted = true;
                sendUDSEraseMemoryResponse();
                Serial.println("UDS Erase Memory Request valid, performing erase...");
            }
        }
    }
}

void sendUDSProgrammingSessionResponse() {
    unsigned char udsResponse[8] = {0x02, 0x50, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Response sent");
}

void sendUDSExtendedSessionResponse() {
    unsigned char udsResponse[8] = {0x03, 0x50, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Response sent");
}


void sendUDSSecurityAccessSeedResponse() {
    byte seed[4] = {0x01, 0x02, 0x03, 0x04};  // Example seed for security access
    byte udsResponse[6] = {0x67, 0x01, seed[0], seed[1], seed[2], seed[3]};
    
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 6;
    memcpy(canMsg.data, udsResponse, 6);
    
    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Security Access Seed Response sent");
}


void sendUDSControlDTCSettingResponse() {
    unsigned char udsResponse[8] = {0xC5, 0x02, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Control DTC Setting Response sent");
}

void sendUDSEraseMemoryResponse() {
    unsigned char udsResponse[8] = {0x71, 0x03, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Memory Erase Response sent");
}

void sendUDSTransferExitResponse() {
    unsigned char udsResponse[8] = {0x77, 0x02, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Transfer and Exit Response sent");
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
        sendUDSEraseMemoryResponse();
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
    canMsg.can_id = 0x7E8;  // Response ID
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);
    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Positive Response sent for Security Access granted.");
}

void sendUDSNegativeResponse(byte serviceId, byte errorCode) {
    byte udsResponse[3] = {0x7F, serviceId, errorCode};  // UDS Negative Response format
    canMsg.can_id = 0x7E8;  // Response ID
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);
    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Negative Response sent");
}

