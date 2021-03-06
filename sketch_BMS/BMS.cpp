/*!
  Name:         BMS.cpp
  Date created: 2.5.18
  Description:  Contains functions for reading and working with data from LTC6811 using library for LTC68041
*/
#include <Arduino.h>
#include <stdint.h>
#include "Linduino.h"
#include "LT_SPI.h"
#include "UserInterface.h"
#include "LTC68041.h"
#include "tmap.h"
#include "BMS.h"
#include "CAN.h"

float lowest_voltage;
float BMS_discharge_voltage;
bool BMS_discharge_enabled = false;
uint8_t discharge_part = 0;
uint8_t discharge_completed = 0;

static uint16_t cell_discharge[TOTAL_IC];
static uint16_t cell_temperature[TOTAL_IC][12];
static float current_measurement;
static uint8_t TSAL;  // |bit: 0 TSAL 0| ... |bit: 3 TSAL 3| wa | sted | spa | ce |

// error counters too account for random noise
static volatile uint8_t t_erc = 0;
static volatile uint8_t c_erc = 0;
static volatile uint8_t voltage_erc[TOTAL_IC];
static volatile uint8_t voltage_error_count[TOTAL_IC];
static volatile uint8_t temperature_error_count;
static volatile uint8_t current_error_count = 0;
static uint8_t error = 0; // | bit 4: voltage | bit 3: temperature | bit 2: current | bit 1:  |

// variable for temperature measurement readings
uint16_t aux_codes[TOTAL_IC][6];
/* The GPIO codes will be stored in the aux_codes[][6] array in the following format:

  |  aux_codes[0][0]| aux_codes[0][1] |  aux_codes[0][2]|  aux_codes[0][3]|  aux_codes[0][4]|  aux_codes[0][5]| aux_codes[1][0] |aux_codes[1][1]|  .....    |
  |-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|-----------------|---------------|-----------|
  |IC1 GPIO1        |IC1 GPIO2        |IC1 GPIO3        |IC1 GPIO4        |IC1 GPIO5        |IC1 Vref2        |IC2 GPIO1        |IC2 GPIO2      |  .....    |
*/

// Configuration variables
uint8_t tx_cfg[TOTAL_IC][6];
/* The tx_cfg[][6] stores the LTC6804 configuration data that is going to be written to the LTC6804 ICs on the daisy chain.
    The LTC6804 configuration data that will be written should be stored in blocks of 6 bytes. The array should have the following format:

  |  tx_cfg[0][0]| tx_cfg[0][1] |  tx_cfg[0][2]|  tx_cfg[0][3]|  tx_cfg[0][4]|  tx_cfg[0][5]| tx_cfg[1][0] |  tx_cfg[1][1]|  tx_cfg[1][2]|  .....    |
  |--------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|--------------|-----------|
  |IC1 CFGR0     |IC1 CFGR1     |IC1 CFGR2     |IC1 CFGR3     |IC1 CFGR4     |IC1 CFGR5     |IC2 CFGR0     |IC2 CFGR1     | IC2 CFGR2    |  .....    |
*/


/* void reset_configs()
   Put the default configuration in the tx_cfg
*/
void reset_configs() {
  //                                     ??FE??
  const uint8_t BMS_default_config[6] = {0x0E, 0x00, 0x00, 0x00, 0x00, 0x00};
  //uint16_t vuv = (((BMS_low_voltage * 10000)/16)-1)
  //uint16_t vov = 0;
  for (uint8_t i = 0; i < TOTAL_IC; i++) {
    for (uint8_t j = 0; j < 6; j++) {
      tx_cfg[i][j] = BMS_default_config[j];
    }
  }
}


/* void BMS_Initialize()
   Initializes the AMS isoSPI module and the BMSs.
*/
void BMS_Initialize() {
  //quikeval_SPI_connect();
  spi_enable(); // This will set the Linduino to have a 1MHz Clock
  LTC6804_initialize();
  reset_configs();
  LTC6804_wrcfg( tx_cfg); // ---------------     Stuck    ------------------
  set_adc(MD_NORMAL, DCP_DISABLED, CELL_CH_ALL, AUX_CH_GPIO1);
}

static bool pec_error_v;

/* bool check_cell_voltages()
   Checks the voltages of all cells on all BMSs.
   Returns true if error exceeds error count. If everything is okay false is returned
*/
static uint16_t cell_voltage[TOTAL_IC][12];
bool check_cell_voltages() {
  pec_error_v = false;
  bool result_error[TOTAL_IC];
  lowest_voltage = BMS_high_voltage;
  float voltage;
  //BMS_debug && Serial.print("Voltages\r\n");
  wakeup_idle();
  LTC6804_adcv();  //---------------     Stuck    ------------------
  delayMicroseconds(10);

  // Read each cell voltage register
  if ( LTC6804_rdcv(CELL_CH_ALL,  cell_voltage) != 0 ) {
    BMS_debug && Serial.print("PEC error\r\n");
    pec_error_v = true;
  }

  for (uint8_t k = 0; k < TOTAL_IC; k++) {
    voltage_erc[k]++;
    for (uint8_t l = 0; l < 12; l++) {
      voltage = (float)(cell_voltage[k][l] / 10000.0);
      //BMS_debug && Serial.print(voltage);

      if (voltage < lowest_voltage) {
        lowest_voltage = voltage;
      }
      //BMS_debug && Serial.print("\r\n");

      if ( voltage >= BMS_high_voltage) // Range to filter out noise and PEC errors
        result_error[k] = true;
      if ( voltage <= BMS_low_voltage)   // Range to filter out noise and PEC errors
        result_error[k] = true;
    }
    //BMS_debug && Serial.print("\r\n");
    if ( pec_error_v == true || result_error == true) {
      voltage_error_count[k]++;
      voltage_erc[k] = 0;
    }
    if ( voltage_error_count[k] >= VOLTAGE_COUNT) {
      //voltage_error_count = 0;
      return true;
    }
    if ( voltage_erc[k] >= RESET_ERROR_COUNT) {
      voltage_error_count[k] = 0;
      voltage_erc[k] = 0;
    }
  }

  return false;
}

/* void set_mux_address(uint8_t sensor_id)
   Modify the BMS configuration variable with the right MUX address to read
   from.
   @param sensor_id is the sensor (0-11) to read from next.
*/
void set_mux_address(uint8_t sensor_id) {
  uint8_t mux_address = sensor_id;
  uint8_t mod;
  for (uint8_t current_ic = 0; current_ic < TOTAL_IC; current_ic++) {
    mod = (uint8_t)((tx_cfg[current_ic][0] & 0x0F) | (mux_address << 4));
    tx_cfg[current_ic][0] = mod;
  }
}


static bool pec_error_t;

/* bool check_cell_temperatures()
   Checks the temperatures of all cells on all BMSs
   Return true if error counter exceed maximum else true.
*/
bool check_cell_temperatures() {
  bool result_error = false;
  pec_error_t = false;
  int8_t error;
  uint16_t temp_code;
  float temp_code_div;
  float convVal;

  // To avoid a false first reading, do a dummy adc conversion
  LTC6804_adax();
  delay(3);

  //BMS_debug && Serial.print("Temperatures: \r\n");

  for (uint8_t sensor_id = 0; sensor_id < TOTAL_SENSORS; sensor_id++)
  {
    // ----- BROKEN CODE -----
    set_mux_address(sensor_id);

    LTC6804_wrcfg( tx_cfg);
    delay(3);
    // -----------------------

    // ADAX
    LTC6804_clraux();
    delay(3);

    // ADAX
    LTC6804_adax();
    delay(3);

    error = LTC6804_rdaux(1,  aux_codes); // Set to read back aux register 1
    if (error == -1)
    {
      BMS_debug && Serial.print("RDAUX error\r\n");

      pec_error_t = true;
      //break;
    }
    for (uint8_t current_ic = 0; current_ic < TOTAL_IC; current_ic++)
    {
      //if ( !((current_ic == 2) || (current_ic == 3) || (current_ic == 8 && sensor_id == 4) || (current_ic == 9  && sensor_id == 10) || (current_ic == 6  && sensor_id == 3)) )  {
      if ( !((current_ic == 2) || (current_ic == 3) || (current_ic == 6) || (current_ic == 8) || (current_ic == 9) ) ) {
        temp_code = (uint16_t)((aux_codes[current_ic][1] << 8) + aux_codes[current_ic][0]);
        convVal = LookupTemperature((float)(temp_code / 10000.0));
        cell_temperature[current_ic][sensor_id] = (uint16_t) (convVal);
        //      if (BMS_debug) {
        //        Serial.print("B");
        //        Serial.print(current_ic);
        //        Serial.print(" C");
        //        Serial.print(sensor_id);
        //        Serial.print(" ");
        //        Serial.print(temp_code);
        //        Serial.print(" ");
        //        Serial.print(convVal);
        //        Serial.print("\r\n");
        //      }
        //Serial.print("Temp: ");
        //Serial.println(convVal);
        if ( convVal <= BMS_low_temperature || convVal >= BMS_high_temperature ) {
          Serial.print("T ic: ");
          Serial.println(current_ic);
          Serial.print("sensor id: ");
          Serial.println(sensor_id);
          result_error = true;
        }
      }
    }
  }
  //int k;
  //for (k = 0; k < TOTAL_IC; k++) {
  t_erc++;
  if ( result_error == true) {
    temperature_error_count++;
    t_erc = 0;
  }

  if ( temperature_error_count >= TEMP_COUNT) {
    temperature_error_count = 0;
    return true;
  }
  if ( t_erc >= RESET_ERROR_COUNT) {
    temperature_error_count = 0;
    t_erc = 0;
  }
  //}
  return false;
}


/* void send_data_packet(uint8_t error)
   Print report to serial
*/
void send_data_packet() {
  for (uint8_t ic = 0; ic != TOTAL_IC; ic++) {
    Serial.print("D");
    Serial.print(ic);
    Serial.print("|");

    for (uint8_t cell = 0; cell != TOTAL_SENSORS; cell++) {
      //if (bitRead(error, 4) == 1) {
      //  Serial.print("PEC|");
      //} else {
      Serial.print(cell);
      Serial.print("|");
      Serial.print( cell_voltage[ic][cell] / 10 );
      Serial.print("|");
      //}
      //if (bitRead(error, 3) == 1)  {
      //  Serial.print("PEC|");
      //} else {
      Serial.print(cell_temperature[ic][cell]);
      Serial.print("|");
      //}
      //if (cell_discharge[ic][cell] == true) {
      if (bitRead(cell_discharge[ic], cell) == true) {
        Serial.print("1|");
      } else {
        Serial.print("0|");
      }
    }
    Serial.print("\r\n");
  }
  Serial.print("S|");
  Serial.print((int16_t) (lowest_voltage * 1000));
  Serial.print("|\r\n");
  Serial.print("C|");
  Serial.print((int16_t) current_measurement);
  Serial.print("|\r\n");
  Serial.print("T0|");
  Serial.print(bitRead(TSAL, 0));
  Serial.print("|\r\n");
  Serial.print("T1|");
  Serial.print(bitRead(TSAL, 1));
  Serial.print("|\r\n");
  Serial.print("T2|");
  Serial.print(bitRead(TSAL, 2));
  Serial.print("|\r\n");
  Serial.print("T3|");
  Serial.print(bitRead(TSAL, 3));
  Serial.print("|\r\n");
}

/* bool BMS_check()
   Read and check all voltage levels and temperature and current. Print out report.
   If no tolerances are exceeded and no errors occur "true" is returned otherwise in the event of failure "false" is returned
*/
bool BMS_check() {
  // Gather
  error = 0; // | bit 4: voltage | bit 3: temperature | bit 2: current | bit 1:  |

  wakeup_sleep();
  if ( check_cell_voltages() ) {
    Serial.println("Voltage error");
    bitSet(error, 4);
  }
  if ( check_cell_temperatures()) {
    Serial.println("Temperature error");
    bitSet(error, 3);
  }

  // Check current measurment from Power meter over CAN-BUS
  c_erc++;
  current_measurement = can_read_current();
  if (current_measurement < PWR_min_current || current_measurement >= PWR_max_current)
    current_error_count++;
  else if ( c_erc >= RESET_ERROR_COUNT) {
    current_error_count = 0;
    c_erc = 0;
  }
  if (current_error_count >= CURRENT_COUNT) {
    Serial.println("Current error");
    //bitSet(error, 2);
  }

  if (digitalRead(TSAL_PIN_0))
    bitSet(TSAL, 0);
  else
    bitClear(TSAL, 0);

  if (digitalRead(TSAL_PIN_1))
    bitSet(TSAL, 1);
  else
    bitClear(TSAL, 1);

  if (digitalRead(TSAL_PIN_2))
    bitSet(TSAL, 2);
  else
    bitClear(TSAL, 2);

  if (digitalRead(TSAL_PIN_3))
    bitSet(TSAL, 3);
  else
    bitClear(TSAL, 3);

  send_data_packet();
  can_send(pec_error_v, pec_error_t, cell_temperature, cell_voltage, TSAL);

  Serial.println("voltage error count: ");
  int i;
  for (i = 0; i < TOTAL_IC; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(voltage_error_count[i]);
  }
  Serial.println("Temp error count: ");
  //for (i = 0; i < TOTAL_IC; i++) {
  //  Serial.print(i);
  //  Serial.print(": ");
  Serial.println(temperature_error_count);

  Serial.print("Current error count: ");
  Serial.println(current_error_count);


  return error > 0; // Returns true if there is error and triggers shutdown
}

float BMS_get_target_voltage() {
  return BMS_discharge_voltage;
}


/**
   Gets the error code from the last failure.
   @return the error code (currently undefined behaviour).
*/
uint16_t BMS_get_error_code() {
  return 0;
}

bool discharge(uint8_t part) {

  uint16_t discharge_bits = 0; // Needs to have bits >= TOTAL_SENSORS
  bool completed = true;

  for (uint8_t bms_id = 0; bms_id != TOTAL_IC; bms_id++) {

    discharge_bits = 0; // Reset between bms modules

    for (uint8_t cell_id = part; cell_id < TOTAL_SENSORS; cell_id = cell_id + 3) {

      if (bitRead(cell_discharge[bms_id], cell_id) == false) {
        //if (cell_discharge[bms_id][cell_id] == false) {
        continue;
      }

      if ((cell_id != part) && (cell_id != part + 3) && (cell_id != part + 6) && cell_id != part + 9) {
        bitClear(cell_discharge[bms_id], cell_id);
        //cell_discharge[bms_id][cell_id] = false;
        continue;
      }

      if ( ((float) cell_voltage[bms_id][cell_id] / 10000.0) - BMS_discharge_voltage < 0.01) {
        bitClear(cell_discharge[bms_id], cell_id);
        //cell_discharge[bms_id][cell_id] = false;
      } else {
        completed = false;
        bitSet(cell_discharge[bms_id], cell_id);
        //cell_discharge[bms_id][cell_id] = true;
        discharge_bits |= 1u << cell_id;
      }
    }

    // For CFGR4 we have DCC8 DCC7 DCC6 DCC5 DCC4  DCC3  DCC2  DCC1
    // For CFGR5 we have x    x    x    x    DCC12 DCC11 DCC10 DCC9

    // Discharge_bits = xxxx1111 11111111 (12 to 1) or (11 to 0))

    /*
         00000010 01001001 (stage 0)
         00000100 10010010 (stage 1)
         00001001 00100100 (stage 2)

       Stage 1
         00000000 11111111 (0x00FF)
       & 00000010 01001001 (stage 1)
       -------------------
         00000000 01001001 (stage 1 saved)

         00000000 11111111 (0x00FF)
       & 00000100 10010010 (stage 2)
       -------------------
         00000000 10010010
       | 00000000 01001001 (stage 1 saved)
       -------------------
         00000000 11011011

       Better to take the top half of the config

         xxxxxxxx 00000000
       | 00000000 ssssssss

    */

    // These bit changes make the cells discharge
    tx_cfg[bms_id][4] = 0x00FF & discharge_bits;
    tx_cfg[bms_id][5] = (0x0F00 & discharge_bits) >> 8u;
  }
  return completed;
}

bool BMS_is_discharge_enabled() {
  return BMS_discharge_enabled;
}

void BMS_set_discharge(bool state) {
  if (state == true) {
    if (lowest_voltage > BMS_low_voltage) {
      BMS_discharge_voltage = lowest_voltage;
      BMS_discharge_enabled = true;
      discharge_part = 0;
      discharge_completed = 0;
      // Set enable discharge on all cells
      for (uint8_t i = 0; i < TOTAL_IC; i++) {
        for (uint8_t j = 0; j < TOTAL_SENSORS; j++) {
          bitSet(cell_discharge[i], j);
          //cell_discharge[i][j] = true;
        }
      }
    }
  } else {
    BMS_discharge_enabled = false;
    reset_configs();
    for (uint8_t i = 0; i < TOTAL_IC; i++) {
      for (uint8_t j = 0; j < TOTAL_SENSORS; j++) {
        bitClear(cell_discharge[i], j);
        //cell_discharge[i][j] = false;
      }
    }
  }
}

void BMS_clear_discharge() {
  reset_configs();
}

void BMS_handle_discharge() {
  if (BMS_discharge_enabled == false) {
    Serial.print("BMS_discharge_enabled is false, lest not do anything.\r\n");
    return;
  }

  if (BMS_discharge_voltage < BMS_low_voltage) {
    Serial.print("Discharge cancelled because threshold-voltage is below minimum voltage\r\n");
    BMS_set_discharge(false);
    return;
  }

  Serial.print("Discharging...\r\n");
  Serial.print("Target voltage ");
  Serial.print((uint16_t) (BMS_discharge_voltage * 1000));
  Serial.print(" V\r\n");
  if (discharge(discharge_part) == true) {
    discharge_completed |= 1u << discharge_part;
  }

  if (discharge_completed == 0x07) {
    BMS_set_discharge(false);
    return;
  }

  if (discharge_part == 2) {
    discharge_part = 0;
  } else {
    discharge_part++;
  }
}

/* On Discharge Request (CDEA) or (cdea)
   Go through all cells and pick the lowest voltage as threshold voltage
   While doing so, switch discharge on for all cells

   In BMS loop:
   if cell voltage is below or equal to threshold voltage, set discharge to
   false for that cell
   When discharge is decided for each cell it's added to a bitmask that gets
   applied to the bms after the cell-loop has finished

   Part argument stands for which of 3 possible discharge configurations toad
   use: 0: discharge 0, 3, 6, 9
        1: discharge 1, 4, 7, 10
        2: discharge 2, 5, 8, 11

   This is done to avoid thermal hot-spots when discharging (making sure that
   two discharge resistors are inactive between each active one)
*/


