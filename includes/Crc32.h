#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

#include <boost/crc.hpp>

//! Рассчитывает CRC32.
//! @param buff - [in] буфер;
//! @param size - [in] размер буфера, в байтах.
//! @return CRC32.
inline uint32_t CalcCrc32(const uint8_t* buff, uint32_t size)
{
    boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, false> crcCalculator;

    for (size_t i = 0; i < size; ++i)
    {
        crcCalculator.process_byte(buff[i]);
    }

    return crcCalculator.checksum();
}

////! Рассчитывает CRC32.
////! @param buff - [in] буфер;
////! @param size - [in] размер буфера, в байтах.
////! @return CRC32.
//static uint64_t CalcCrc64(const uint8_t* buff, uint32_t size)
//{
//    boost::crc_optimal<64, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, false> crcCalculator;
//
//    for (size_t i = 0; i < size; ++i)
//    {
//        crcCalculator.process_byte(buff[i]);
//    }   
//
//    return crcCalculator.checksum();
//};

#endif // CRC32_H
