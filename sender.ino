#include <mcp_can.h>
#include <SPI.h>

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

const unsigned long totalBytes = 1048; // 10MB
unsigned long bytesSent = 0;
bool dataTransferStarted = false;

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
        if (bytesSent < totalBytes) {
            unsigned char stmp[8] = {'D', 'A', 'T', 'A', 'B', 'L', 'K', '1'}; // Simulated data block
            CAN.sendMsgBuf(0x00, 0, 8, stmp);
            bytesSent += 8;

            // Serial.print("Bytes sent: ");
            // Serial.println(bytesSent);
        } else {
            Serial.println("File transfer complete!");
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
    Serial.println("UDS Request sent");
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
                dataTransferStarted = true;
                Serial.println("UDS Response valid, starting data transfer");
            }
        }
    }
}
