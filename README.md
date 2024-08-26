## ESP32 OTA Update using UDS and CAN-based communication

### Overview
This project demonstrates a simple CAN-based communication application utilizing the UDS protocol. The goal also includes flashing the target hardware using the OTA approach. Also, a mobile application was developed to allow users to upload a .bin file and send it via Wifi. The UDS communication is triggered afterward. 

### Hardware Setup
- **2 ESP32s dev toolkit**
- **2 MCP2515 Transceivers**
- **ESP32 (Receiver)**

### Libraries Used
- mcp_can.h
- mcp2515.h
- SPI.h
