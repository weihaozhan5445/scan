#ifndef STORAGE_MAP_H
#define STORAGE_MAP_H

#include <stdint.h>

#define ADDR_FW_INIT_FLAG ADDR_EEPROM_INIT_FLAG

void W24C02_WriteByte(uint8_t byte_addr, uint8_t data);

#endif
