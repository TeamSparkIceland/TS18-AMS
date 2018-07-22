/* Name:        CAN.h
 * Description: Implements CAN communication for BMS. 
 *              Reads current measurment from power meter
 *              and sends measured voltages and temperatures
 *              and TSAL states over CAN.
 */
 
#include <Arduino.h>
#include <stdint.h>
#include <SPI.h>
#include "mcp_can.h"
#include "CAN.h"
#include "LTC68041.h"
#include "BMS.h"

MCP_CAN CAN(CAN_CS_PIN);       // Set CS pin


// Buffer for storing incoming data from Can bus
static unsigned char len = 0;
static unsigned char buf[8];


/* void init_can()
 * Try initializing CAN bus for two seconds
 */
void init_can() {
  uint32_t t = millis();
  while (CAN_OK != CAN.begin(CAN_500KBPS)) { // should be CAN.begin(CAN_1000KBPS)) {  but because of Power meter its 500
    if (millis() - t > 2000) {
      break;
    }
    delay(10);
  }
  Serial.println("CAN initialized");
}


/* float can_read_current()
   Try to read current measurment for max_current_read_time
   returns -1000 if nothing is read
*/
float can_read_current() {
  uint32_t t = millis();
  while (1) {
    if (CAN_MSGAVAIL == CAN.checkReceive()) {
      CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf
      int canId = CAN.getCanId();
      if (canId == PWR_current_id) {
        static union Data Mesurments;
        Mesurments.data_buf[0] = buf[5];
        Mesurments.data_buf[1] = buf[4];
        Mesurments.data_buf[2] = buf[3];
        Mesurments.data_buf[3] = buf[2];
        return (float) Mesurments.mesurment / 1000.0;
      }
    }
    if( millis() - t >= max_current_read_time) {
      return -1000;
    }
  }
  return -1000; // No current measurment revieved
}

/* void can_send(uint16_t error, uint16_t cell_voltage[][12], uint16_t cell_temperature[][12], uint16_t cell_discharge[], uint8_t TSAL)
 * Send measurments over CAN. Format:
 * BMS: 0  id 200 | voltage  | ???
 */
void can_send(uint16_t error, uint16_t cell_voltage[][12], uint16_t cell_temperature[][12], uint16_t cell_discharge[], uint8_t TSAL) {
  //CAN.sendMsgBuf(0x01, 0, sizeof(msg) + 4, msg);
  uint16_t id_ic;
  uint16_t id_cell;
  for (uint8_t ic = 0; ic != TOTAL_IC; ic++) {
    id_ic = BMS_CAN_ID + ic * TOTAL_IC;
    for (uint8_t cell = 0; cell != TOTAL_SENSORS; cell++) {
      id_cell = id_ic + cell * 4;
      CAN.sendMsgBuf(id_cell, 0, 1, cell);
      if (bitRead(error, 4) == 1) {
        CAN.sendMsgBuf(id_cell + 1, 0, 3, "PEC");
      } else {
        CAN.sendMsgBuf(id_cell + 1, 0, 2, cell_voltage[ic][cell] / 10 );
      }
      if (bitRead(error, 3) == 1) {
        CAN.sendMsgBuf(id_cell + 2, 0, 3, "PEC");
      } else {
        CAN.sendMsgBuf(id_cell + 2, 0, 2, cell_temperature[ic][cell] );
      }
      //CAN.sendMsgBuf(id_cell + 3, 0, 1, bitRead(cell_discharge[ic], cell) );
    }
  }
}






