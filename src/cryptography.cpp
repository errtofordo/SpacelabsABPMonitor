#include <vector>
#include <bit>

using namespace std;

uint8_t DoubleAndWrap(uint8_t a1, uint8_t a2)
{
    if (a2 == 0) return a1;

    for (uint8_t i = 0; i < a2; i++)
    {
        a1 = (a1 << 1) | (a1 & 0x80 ? 1 : 0);
    }
    return a1;
}

vector<uint8_t> ConvertHexStringToKey(char* hexString)
{
    vector<uint8_t> key(2);

    unsigned long hexValue = strtoul(hexString, NULL, 16);

    uint8_t transformedKey = (rotr<uint8_t>(static_cast<uint8_t>(hexValue), 2) ^ 0xAA) + 37;
    uint8_t doubledValue = DoubleAndWrap((hexValue >> 16) & 0xFF, 2);
    uint8_t finalKeyPart = DoubleAndWrap(((hexValue >> 8) & 0xFF) ^ doubledValue, 1) - 37 + 0x55;

    key[0] = finalKeyPart;
    key[1] = transformedKey + 0x55;

    return key;
}

uint8_t BitwiseOperations(uint8_t a1, uint8_t a2, uint8_t a3)
{
    uint8_t val = (a1 & a2) ? (a3 | a1) : (a1 & ~a3);
    return (a1 & a3) ? (a2 | val) : (~a2 & val);
}

uint8_t TransformHexStringUsingKey(char* hexStr, vector<uint8_t> *key)
{
    unsigned long value = strtoul(hexStr, NULL, 16);
    uint8_t keyVal = 0;

    if (((*key)[0] & 7) != 0)
        keyVal = DoubleAndWrap(static_cast<uint8_t>(value), (*key)[0] & 7);
    else
        keyVal = (*key)[1] + 1 + static_cast<uint8_t>(value);

    if (((*key)[0] & 0x40) != 0 && ((*key)[0] & 0x80) != 0)
        keyVal = ~keyVal;
    if (((*key)[0] & 0xC0) == 0)
        keyVal = BitwiseOperations(keyVal, 8u, 0x10u);
    if (((*key)[0] & 8) != 0)
        keyVal = BitwiseOperations(keyVal, 4u, 0x20u);
    if (((*key)[0] & 0x10) != 0)
        keyVal = BitwiseOperations(keyVal, 2u, 0x40u);
    if (((*key)[0] & 0x20) != 0)
        keyVal = BitwiseOperations(keyVal, 1u, 0x80u);

    if (((*key)[0] & 0x38) == 0)
        keyVal = (*key)[0] ^ keyVal;
    if (((*key)[0] & 0x40) != 0)
        keyVal ^= (*key)[1];
    if (((*key)[0] & 0x80) != 0)
        keyVal -= (*key)[1];
    if ((*key)[0] > 0x1Fu)
        keyVal -= 37;

    (*key)[0] = (*key)[0] + (((*key)[0] + 37) ^ 0x15) + rotr<uint8_t>((*key)[1], 1) + rotr<uint8_t>(keyVal ^ 0x55, 2);
    (*key)[1] = ((*key)[1] ^ 0x15) + (rotr<uint8_t>((*key)[0], 2) & 0x3F) + 37;
    return keyVal ^ 0x55;
}

vector<uint8_t> DecryptHexString(char* source, size_t len, vector<uint8_t>* key)
{
    vector<uint8_t> result;

    size_t validLength = len - 7;
    if (validLength > 0)
    {
        for (size_t i = 0; i < validLength; i += 2)
        {
            char hexPair[3] = { source[i], source[i + 1], '\0' };
            result.push_back(TransformHexStringUsingKey(hexPair, key));
        }
        result.push_back(13);
    }
    return result;
}

uint16_t Crc16(uint8_t* pcBlock, size_t len)
{
    uint16_t crc = 0x0000;
    unsigned char i;

    while (len--)
    {
        crc ^= *pcBlock++ << 8;

        for (i = 0; i < 8; i++)
            crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
    }
    return crc;
}