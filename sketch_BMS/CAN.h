
#ifndef TMAP_H
#define  TMAP_H


#define SPI_CS_PIN             9
#define PWR_current_id         0x525
#define max_current_read_time  40 //ms
#define BMS_CAN_ID             0x600

union Data {    // Used to convert char to float
  char data_buf[4];
  long mesurment;
};



float can_read_current();



#endif  /* TMAP_H */

