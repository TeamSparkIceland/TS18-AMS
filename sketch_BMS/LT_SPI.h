/*
 * Description: Changed this library to work with the CAN bus library
 * 
 * Format should preferbly be
 * SPI_BEGIN();
 * select ic by pulling chip select low
 * SPI.transfer();
 * deselect ic by setting chip select high
 * SPI_END();
 * 
 * is now because of TLC68041 library
 * select ic by pulling chip select low
 * SPI_BEGIN();
 * SPI.transfer();
 * SPI_END();deselect ic by setting chip select high
 * 
 * Probably doesnt matter at all or might be better the way it is
 * 
 */

/*! @file
    @ingroup LT_SPI
    Library Header File for LT_SPI: Routines to communicate with ATmega328P's hardware SPI port.
*/

#ifndef LT_SPI_H
#define LT_SPI_H

#include <stdint.h>
#include <SPI.h>

// Uncomment the following to use functions that implement LTC SPI routines
// //! @name SPI CLOCK DIVIDER CONSTANTS
// //! @{
// #define SPI_CLOCK_DIV4    0x00  // 4 Mhz
// #define SPI_CLOCK_DIV16   0x01  // 1 Mhz
// #define SPI_CLOCK_DIV64   0x02  // 250 khz
// #define SPI_CLOCK_DIV128  0x03  // 125 khz
// #define SPI_CLOCK_DIV2    0x04  // 8 Mhz
// #define SPI_CLOCK_DIV8    0x05  // 2 Mhz
// #define SPI_CLOCK_DIV32   0x06  // 500 khz
// //! @}
//
// //! @name SPI HARDWARE MODE CONSTANTS
// //! @{
// #define SPI_MODE0 0x00
// #define SPI_MODE1 0x04
// // #define SPI_MODE2 0x08
// #define SPI_MODE3 0x0C
// //! @}
//
// //! @name SPI SET MASKS
//! @{
// #define SPI_MODE_MASK      0x0C    // CPOL = bit 3, CPHA = bit 2 on SPCR
// #define SPI_CLOCK_MASK     0x03    // SPR1 = bit 1, SPR0 = bit 0 on SPCR
// #define SPI_2XCLOCK_MASK   0x01    // SPI2X = bit 0 on SPSR
// //! @}


void spi_enable();

//! Write a data byte using the SPI hardware
void spi_write(int8_t data  //!< Byte to be written to SPI port
              );

//! Read and write a data byte using the SPI hardware
//! @return the data byte read
int8_t spi_read(int8_t data //!< The data byte to be written
               );

#endif  // LT_SPI_H
