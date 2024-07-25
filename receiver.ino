#include <SPI.h>
#include <mcp2515.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Update.h>

struct can_frame canMsg;
MCP2515 mcp2515(5);

const unsigned long expectedFileSize = 237568; // Expected 240KB file size
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

    binFile = SPIFFS.open("/test_blink_500.bin", FILE_WRITE);
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
            if (canMsg.can_dlc == 8 && memcmp(canMsg.data, "\xDE\xAD\xBE\xEF\xFE\xED\xBA\xAD", 8) == 0) {
                eofReceived = true;
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
    File receivedFile = SPIFFS.open("/test_blink.bin", FILE_READ);
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
            //Serial.println("UDS Request received");
            if (canMsg.data[0] == 0x02 && canMsg.data[1] == 0x10 && canMsg.data[2] == 0x01) {
                sendUDSResponse();
                //dataTransferStarted = true;
                Serial.println("UDS Diagnostic Control Session Request valid, sending response...");
            } else if (canMsg.data[0] == 0x02 && canMsg.data[1] == 0x31 && canMsg.data[2] == 0x01) {
                sendUDSCommunicationControlResponse();
                dataTransferStarted = true;
                Serial.println("UDS Communication Control Request valid, sending communication control response...");
            }
        }
    }
}

void sendUDSResponse() {
    unsigned char udsResponse[8] = {0x03, 0x50, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Response sent");
}

void sendUDSCommunicationControlResponse() {
    unsigned char udsResponse[8] = {0x03, 0x71, 0x01};
    canMsg.can_id = 0x7E8;
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Communication Control Response sent");
}

void flashESP32() {
    Serial.println("Starting ESP32 flash process...");

    File updateBin = SPIFFS.open("/test_blink.bin", FILE_READ);
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
   
