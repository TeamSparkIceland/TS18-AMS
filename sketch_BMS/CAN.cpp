/* Name:        CAN.h
   Description: Implements CAN communication for BMS.
                Reads current measurment from power meter
                and sends measured voltages and temperatures
                and TSAL states over CAN.
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
   Try initializing CAN bus for two seconds
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
        //msg_buf.data = (uint16_t) Mesurments.mesurment;
        CAN.sendMsgBuf(BMS_CAN_ID + 7, 0, 2, (uint16_t) Mesurments.mesurment);
        return (float) Mesurments.mesurment / 1000.0;
      }
    }
    if ( millis() - t >= max_current_read_time) {
      return -1000;
    }
  }
  return -1000; // No current measurment revieved
}

/* void can_send(uint16_t error, uint16_t cell_voltage[][12], uint16_t cell_temperature[][12], uint16_t cell_discharge[], uint8_t TSAL)
   Send measurments over CAN. Format:
   BMS: 0  id 200 | voltage  | ???
*/
void can_send(bool pec_error_v, bool pec_error_t, uint16_t cell_temperature[][12], uint16_t cell_voltage[TOTAL_IC][12], uint8_t TSAL) {
  //CAN.sendMsgBuf(0x01, 0, sizeof(msg) + 4, msg);
  uint8_t tmp = TSAL << 4 + (uint8_t) pec_error_v << 1 + (uint8_t) pec_error_t;
  CAN.sendMsgBuf(BMS_CAN_ID, 0, 1, tmp);

  uint16_t id_ic = 0;
  uint16_t id_cell = 0;
  uint16_t max_T = 0;
  uint16_t mean_T = 0;
  uint16_t counter = 0;
  uint16_t min_T = 100;

  uint16_t max_V = 0;
  uint16_t min_V = 6800;
  uint16_t total_V = 0;
  for (uint8_t ic = 0; ic != TOTAL_IC; ic++) {
    for (uint8_t cell = 0; cell != TOTAL_SENSORS; cell++) {
      if ( cell_temperature[id_ic][id_cell] > max_T )
        max_T = cell_temperature[id_ic][id_cell];
      if ( cell_temperature[id_ic][id_cell] < min_T  && cell_temperature[id_ic][id_cell] > 0 )
        min_T = cell_temperature[id_ic][id_cell];
      counter++;
      mean_T = (mean_T * (counter - 1) + cell_temperature[id_ic][id_cell]) / counter;

      if ( cell_voltage[id_ic][id_cell] > max_V && cell_voltage[id_ic][id_cell] < 60000)
        max_V = cell_voltage[id_ic][id_cell];
      if ( cell_voltage[id_ic][id_cell] < min_V )
        min_V = cell_voltage[id_ic][id_cell];
      }
      total_V += cell_voltage[id_ic][id_cell];
  }
  static union Msg msg_buf;
  msg_buf.data = max_T;
  CAN.sendMsgBuf(BMS_CAN_ID + 1, 0, 2, msg_buf.buf);
  msg_buf.data = mean_T;
  CAN.sendMsgBuf(BMS_CAN_ID + 2, 0, 2, msg_buf.buf);
  msg_buf.data = min_T;
  CAN.sendMsgBuf(BMS_CAN_ID + 3, 0, 2, msg_buf.buf);
  msg_buf.data = max_V;
  CAN.sendMsgBuf(BMS_CAN_ID + 4, 0, 2, msg_buf.buf);
  msg_buf.data = min_V;
  CAN.sendMsgBuf(BMS_CAN_ID + 5, 0, 2, msg_buf.buf);
  msg_buf.data = total_V;
  CAN.sendMsgBuf(BMS_CAN_ID + 6, 0, 2, msg_buf.buf);

  // Send lowest voltage

}






