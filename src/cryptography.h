#pragma once

#include <vector>

std::vector<uint8_t> ConvertHexStringToKey(char* hexString);
std::vector<uint8_t> DecryptHexString(char* source, size_t len, std::vector<uint8_t>* key);
uint16_t Crc16(uint8_t* pcBlock, size_t len);