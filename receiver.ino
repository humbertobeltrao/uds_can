#include <SPI.h>
#include <mcp2515.h>

struct can_frame canMsg;
MCP2515 mcp2515(5);

const unsigned long fileSize = 1048; // 10MB
unsigned long bytesReceived = 0;
unsigned long startTime = 0;
unsigned long endTime = 0;
bool timingStarted = false;
bool dataTransferStarted = false;

void setup() {
    Serial.begin(115200);

    mcp2515.reset();
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
    mcp2515.setNormalMode();

    Serial.println("MCP2515 Initialized Successfully!");
}

void loop() {
    if (!dataTransferStarted) {
        // Wait for UDS request to start data transfer
        receiveUDSRequest();
    } else {
        // Data transfer loop
        if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
            if (!timingStarted) {
                startTime = micros(); // Start timing on first message
                timingStarted = true;
            }
            bytesReceived += canMsg.can_dlc;
            Serial.println(bytesReceived);
            if (bytesReceived >= fileSize) { // Check if file size reached 10MB
                Serial.println("Done");
                endTime = micros(); // Stop timing after receiving the entire file
                unsigned long elapsedTime = endTime - startTime;

                Serial.print("Time taken to receive 1M file: ");
                Serial.print(elapsedTime/1000000.0);
                Serial.println(" seconds");

                while (1); // Stop processing after file transfer is complete
            }
            delay(100);
        }
    }
}

void receiveUDSRequest() {
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        if (canMsg.can_id == 0x7DF) { // Assuming 0x7DF is the request CAN ID
            Serial.println("UDS Request received");
            // Check UDS request and send UDS response
            if (canMsg.data[0] == 0x02 && canMsg.data[1] == 0x10 && canMsg.data[2] == 0x01) {
                sendUDSResponse();
                dataTransferStarted = true;
                Serial.println("UDS Request valid, sending response");
            }
        }
    }
}

void sendUDSResponse() {
    // Example UDS Start Diagnostic Session response
    unsigned char udsResponse[8] = {0x03, 0x50, 0x01}; // Positive response for Diagnostic Session Control
    canMsg.can_id = 0x7E8; // Assuming 0x7E8 is the response CAN ID
    canMsg.can_dlc = 3;
    memcpy(canMsg.data, udsResponse, 3);

    mcp2515.sendMessage(&canMsg);
    Serial.println("UDS Response sent");
}
