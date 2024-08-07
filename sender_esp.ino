#include <SPI.h>
#include <mcp_can.h>

// Define the CS pin for MCP2515
const int SPI_CS_PIN = 5;
MCP_CAN CAN(SPI_CS_PIN); // Create a CAN object

const unsigned long totalBytes = 237568; // 240KB
unsigned long bytesSent = 0;
bool dataTransferStarted = false;
const int bufferSize = 8;
char buffer[bufferSize];

void setup() {
    Serial.begin(115200);

    // Initialize the MCP2515 CAN controller
    if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
        Serial.println("CAN BUS Shield init ok!");
    } else {
        Serial.println("CAN BUS Shield init fail");
        while (1);
    }

    CAN.setMode(MCP_NORMAL); // Set operation mode to normal

    sendUDSExtendedDiagnosticSessionRequest();
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
            sendEOFMessage(); // Send the EOF message
            bytesSent = 0;
            while (1); // Stop sending after file transfer is complete
        }

        delay(100); // Short delay to control the send rate
    }
}

void sendUDSProgrammingSessionRequest() {
    // Example UDS Start Diagnostic Session request
    byte udsRequest[3] = {0x02, 0x10, 0x01}; // Diagnostic Session Control (0x10), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Programming Session Request sent");
}

void sendUDSExtendedDiagnosticSessionRequest() {
    // Example UDS Start Diagnostic Session request
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
    // Example UDS Start Diagnostic Session request
    byte udsRequest[3] = {0x02, 0x31, 0x01}; // Communication Control (0x31), Default Session (0x01)
    CAN.sendMsgBuf(0x7DF, 0, 3, udsRequest);
    Serial.println("UDS Communication Control Request sent");
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
                Serial.println("UDS Programming Session Response valid, sending communication control request...");
                sendUDSCommunicationControlRequest();
            } else if (buf[0] == 0x03 && buf[1] == 0x71 && buf[2] == 0x01) {
                dataTransferStarted = true;
                Serial.println("UDS Communication Control Response valid, starting file transfer...");
            }
        }
    }
}

void sendEOFMessage() {
    // Example EOF indicator message
    byte eofMessage[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED, 0xBA, 0xAD}; // Special sequence of bytes
    CAN.sendMsgBuf(0x00, 0, 8, eofMessage);
    Serial.println("EOF message sent");
}