#include <Arduino.h>
#include "devices.hpp"

void generatePacket(const AppleDevice& device, uint8_t* buffer, size_t& outLength) {
  memset(buffer, 0, 31); // Clear buffer

  if (device.type == APPLE_AUDIO) {
      outLength = 31;
      uint8_t header[] = {0x1e, 0xff, 0x4c, 0x00, 0x07, 0x19, 0x07};
      uint8_t body[]   = {0x20, 0x75, 0xaa, 0x30, 0x01, 0x00, 0x00, 0x45, 0x12, 0x12, 0x12};
      
      memcpy(buffer, header, 7);
      buffer[7] = device.modelId;
      memcpy(buffer + 8, body, 11);
  } 
  else if (device.type == APPLE_SETUP) {
      outLength = 23;
      // The common 23-byte setup prefix
      uint8_t prefix[] = {0x16, 0xff, 0x4c, 0x00, 0x04, 0x04, 0x2a, 0x00, 0x00, 0x00, 0x0f, 0x05, 0xc1};
      // The common 23-byte setup suffix (starting after index 13)
      uint8_t suffix[] = {0x60, 0x4c, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00};
      
      memcpy(buffer, prefix, 13);
      buffer[13] = device.modelId; // In "Short" packets, the ID is at index 13
      memcpy(buffer + 14, suffix, 9);
  }
  else if (device.type == SAMSUNG_BUDS) {
      outLength = 28;
      uint8_t prefix[] = {0x1b, 0xff, 0x75, 0x00};
      uint8_t payload[] = {0x42, 0x09, 0x81, 0x02, 0x14, 0x15, 0x03, 0x21, 0x01, 0x09, 0xab, 0x0c, 0x01, 0x46, 0x06, 0x3c, 0xdd, 0x0a, 0x00, 0x00, 0x00, 0x00, 0xa7, 0x00};
      memcpy(buffer, prefix, 4);
      memcpy(buffer + 4, payload, 24);
  }
  else if (device.type == SAMSUNG_WATCH) {
      outLength = 14;
      uint8_t prefix[] = {0x0d, 0xff, 0x75, 0x00};
      uint8_t payload[] = {0x01, 0x00, 0x02, 0x00, 0x01, 0x01, 0xff, 0x00, 0x00, 0x43};
      memcpy(buffer, prefix, 4);
      memcpy(buffer + 4, payload, 10);
  }
}
