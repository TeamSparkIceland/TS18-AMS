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
 */

/*!
LT_SPI: Routines to communicate with ATmega328P's hardware SPI port.

@verbatim

LT_SPI implements the low level master SPI bus routines using
the hardware SPI port.

SPI Frequency = (CPU Clock frequency)/(16+2(TWBR)*Prescaler)
SPCR = SPI Control Register (SPIE SPE DORD MSTR CPOL CPHA SPR1 SPR0)
SPSR = SPI Status Register (SPIF WCOL - - - - - SPI2X)

Data Modes:
CPOL  CPHA  Leading Edge    Trailing Edge
0      0    sample rising   setup falling
0      1    setup rising    sample falling
1      0    sample falling  setup rising
1      1    sample rising   setup rising

CPU Frequency = 16MHz on Arduino Uno
SCK Frequency
SPI2X  SPR1  SPR0  Frequency  Uno_Frequency
  0      0     0     fosc/4     4 MHz
  0      0     1     fosc/16    1 MHz
  0      1     0     fosc/64    250 kHz
  0      1     1     fosc/128   125 kHz
  0      0     0     fosc/2     8 MHz
  0      0     1     fosc/8     2 MHz
  0      1     0     fosc/32    500 kHz

@endverbatim

REVISION HISTORY
$Revision: 6237 $
$Date: 2016-12-20 15:09:16 -0800 (Tue, 20 Dec 2016) $
*/

//! @ingroup Linduino
//! @{
//! @defgroup LT_SPI LT_SPI: Routines to communicate with ATmega328P's hardware SPI port.
//! @}

/*! @file
    @ingroup LT_SPI
    Library for LT_SPI: Routines to communicate with ATmega328P's hardware SPI port.
*/

#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include "Linduino.h"
#include "LT_SPI.h"
#include "LTC68041.h"

#define SPI_BEGIN()        SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0))  //  1MHz SPI clock
#define SPI_END()          SPI.endTransaction()

// Setup the processor for hardware SPI communication.
// Must be called before using the other SPI routines.
// Alternatively, call quikeval_SPI_connect(), which automatically
// calls this function.
void spi_enable() // Configures SCK frequency. Use constant defined in header file.
{
  //pinMode(SCK, OUTPUT);             //! 1) Setup SCK as output
  //pinMode(MOSI, OUTPUT);            //! 2) Setup MOSI as output
  //pinMode(QUIKEVAL_CS, OUTPUT);     //! 3) Setup CS as output
  //SPI.begin();
  //SPI.setClockDivider(spi_clock_divider);
  SPI_BEGIN();
  IO_BMS_CS_SetLow();
  delay(1);
  IO_BMS_CS_SetHigh();
  SPI_END();
}


// Write a data byte using the SPI hardware
void spi_write(int8_t  data)  // Byte to be written to SPI port
{
  SPI_BEGIN();
  //IO_BMS_CS_SetLow();
  SPI.transfer(data);
  //IO_BMS_CS_SetHigh();
  SPI_END();

}

// Read and write a data byte using the SPI hardware
// Returns the data byte read
int8_t spi_read(int8_t  data) //!The data byte to be written
{
  SPI_BEGIN();
  //IO_BMS_CS_SetLow();
  int8_t tmp = SPI.transfer(data);
  //IO_BMS_CS_SetHigh();
  SPI_END();
  return tmp;
}

