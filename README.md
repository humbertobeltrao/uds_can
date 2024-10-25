
## ESP32 OTA Update using UDS and CAN-based communication

### Overview
This project demonstrates a simple CAN-based communication application utilizing the UDS protocol. The goal also includes flashing the target hardware using the OTA approach. Also, a mobile application was developed to allow users to upload a .bin file and send it via Wifi. When the first ESP32 receives the .bin, an SD Card is used for storage. Then, the application layer of the process which uses the UDS protocol communication, is triggered immediately, sending the .bin file from the SD Card to the second ESP32 via CAN, and flashing it at the end.

### Hardware Setup
- **2 ESP32s dev toolkit for establishing UDS-based communication (server_esp and receiver files, respectively)**
- **2 MCP2515 Transceivers**
- **ESP32 (Receiver)**

### Libraries Used
- mcp_can.h
- mcp2515.h
- SPI.h

