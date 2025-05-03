#include <SPI.h>
#include <mcp_can.h>

MCP_CAN CAN(10); // CS pin

void setup() {
  Serial.begin(115200);
  if (CAN.begin(MCP_STDEXT, CAN_125KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN init ok!");
  } else {
    Serial.println("CAN init fail");
    while (1);
  }
  CAN.setMode(MCP_NORMAL);
}

void loop() {
  long unsigned int id = 0;
  unsigned char len = 0;
  unsigned char buf[8];
  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    CAN.readMsgBuf(&id, &len, buf);
    if (len > 0) {
      Serial.print("ID: 0x"); Serial.print(id, HEX);
      Serial.print(" Data: ");
      for (int i = 0; i < len; i++) {
        Serial.print(buf[i], HEX); Serial.print(" ");
      }
      Serial.println();
    }
  }
}
