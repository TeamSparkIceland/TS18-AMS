/* Name:        CAN.h
 * Description: Implements CAN communication for BMS. 
 *              Reads current measurment from power meter
 *              and sends measured voltages and temperatures
 *              and TSAL states over CAN.
 */
#ifndef CAN_H
#define  CAN_H


#define CAN_CS_PIN             11
#define PWR_current_id         0x521
#define max_current_read_time  100 //ms
#define BMS_CAN_ID             0x200

union Data {    // Used to convert char to float
  char data_buf[4];
  long mesurment;
};


void init_can();
float can_read_current();
void can_send(uint16_t error, uint16_t cell_voltage[][12], uint16_t cell_temperature[][12], uint16_t cell_discharge[], uint8_t TSAL);


#endif  /* TMAP_H */

