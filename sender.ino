#include <mcp_can.h>
#include <SPI.h>

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

const unsigned long totalBytes = 237568; // 240KB
unsigned long bytesSent = 0;
bool dataTransferStarted = false;
const int bufferSize = 8;
char buffer[bufferSize];

void setup() {
    Serial.begin(115200);

    if (CAN.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        Serial.println("CAN BUS Shield init ok!");
    } else {
        Serial.println("CAN BUS Shield init fail");
        while (1);
    }

    CAN.setMode(MCP_NORMAL);  // Set operation mode to normal so the MCP2515 sends acks

    sendUDSRequest();
}

void loop() {
    if (!dataTransferStarted) {
        // Wait for UDS response to start data transfer
        receiveUDSResponse();
    } else {
        // Data transfer loop
        if (Serial.available() >= bufferSize) {
            Serial.readBytes(buffer, bufferSize);
            CAN.sendMsgBuf(0x00, 0, bufferSize, (byte *)buffer);
            bytesSent += bufferSize;

            Serial.print("Bytes sent: ");
            Serial.println(bytesSent);
        } else if (bytesSent >= totalBytes) {
            sendEOFMessage();  // Send the EOF message
            bytesSent = 0;
            while (1); // Stop sending after file transfer is complete
        }

        delay(100); // Short delay to control the send rate
    }
}

void sendUDSRequest() {
    // Example UDS Start Diagnostic Session request
    unsigned char udsRequest[8] = {0x02, 0x10, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Diagnostic Control Session Request sent");
}

void sendUDSCommunicationControlRequest() {
    // Example UDS Start Diagnostic Session request
    unsigned char udsRequest[8] = {0x02, 0x31, 0x01}; // Communication Control (0x31), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS CommunicationControl Request sent");
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
                //dataTransferStarted = true;
                Serial.println("UDS Diagnostic Control Session Response valid, sending communication control request...");
                sendUDSCommunicationControlRequest();
            } else if (buf[0] == 0x03 && buf[1] == 0x71 && buf[2] == 0x01) {
              dataTransferStarted = true;
              Serial.println("UDS Communication Control Response valid, starting data transfer");
            }
        }
    }
}

void sendEOFMessage() {
    // Example EOF indicator message
    unsigned char eofMessage[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xBA, 0xAD}; // Special sequence of bytes
    CAN.sendMsgBuf(0x00, 0, 8, eofMessage);
    Serial.println("EOF message sent");
}
