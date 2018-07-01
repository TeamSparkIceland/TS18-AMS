/*!
  Name:         BMS.cpp
  Date created: 2.5.18
  Version:      2.1 (24.06.18)
  Description:  Contains functions for reading and working with data from LTC6811 using library for LTC68041
*/
#include <Arduino.h>
#include <stdint.h>
#include "Linduino.h"
#include "LT_SPI.h"
#include "UserInterface.h"
#include "LTC68041.h"
#include "BMS.h"
#include "CAN.h"
#include "tmap.h"

float lowest_voltage;
float BMS_discharge_voltage;
bool BMS_discharge_enabled = false;
uint8_t discharge_part = 0;
uint8_t discharge_completed = 0;

static uint16_t cell_discharge[TOTAL_IC];
static uint16_t cell_temperature[TOTAL_IC][12];

// error counters too account for random noise
static volatile uint8_t error_reset_counter = 0;
static volatile uint8_t voltage_error_count = 0;
static volatile uint8_t temperature_error_count = 0;
static volatile uint8_t current_error_count = 0;

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
  quikeval_SPI_connect();
  spi_enable(SPI_CLOCK_DIV16); // This will set the Linduino to have a 1MHz Clock
  LTC6804_initialize();
  reset_configs();
  LTC6804_wrcfg( tx_cfg); // ---------------     Stuck    ------------------
  set_adc(MD_NORMAL, DCP_DISABLED, CELL_CH_ALL, AUX_CH_GPIO1);
}


/* bool check_cell_voltages()
   Checks the voltages of all cells on all BMSs.
   Returns true if error exceeds error count. If everything is okay false is returned
*/
static uint16_t cell_voltage[TOTAL_IC][12];
bool check_cell_voltages() {
  error_reset_counter++;
  bool pec_error = false;
  lowest_voltage = BMS_high_voltage;
  float voltage;
  BMS_debug && Serial.print("Voltages\r\n");
  wakeup_idle();
  LTC6804_adcv();  //---------------     Stuck    ------------------
  delayMicroseconds(10);

  // Read each cell voltage register
  if ( LTC6804_rdcv(CELL_CH_ALL,  cell_voltage) != 0 ) {
    pec_error = true;
  }

  for (uint8_t k = 0; k < TOTAL_IC; k++) {
    for (uint8_t l = 0; l < 12; l++) {
      voltage = (float)(cell_voltage[k][l] / 10000.0);
      BMS_debug && Serial.print(voltage);

      if (voltage < lowest_voltage) {
        lowest_voltage = voltage;
      }
      BMS_debug && Serial.print("\r\n");

      if ( voltage >= BMS_high_voltage )
        voltage_error_count++;
      if ( voltage <= BMS_low_voltage )
        voltage_error_count++;
    }
    BMS_debug && Serial.print("\r\n");
  }
  if ( pec_error == true )
    voltage_error_count++;

  if ( voltage_error_count >= VOLTAGE_COUNT) {
    voltage_error_count = 0;
    return true;
  }
  if ( error_reset_counter >= RESET_ERROR_COUNT) {
    voltage_error_count = 0;
    error_reset_counter = 0;
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


/* bool check_cell_temperatures()
   Checks the temperatures of all cells on all BMSs
   Return true if error counter exceed maximum else true.
*/
bool check_cell_temperatures() {
  error_reset_counter++;
  bool result = true;
  bool pec_error = false;
  int8_t error;
  uint16_t temp_code;
  float temp_code_div;
  float convVal;

  // To avoid a false first reading, do a dummy adc conversion
  LTC6804_adax();
  delay(3);

  BMS_debug && Serial.print("Temperatures: \r\n");

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
      Serial.print("RDAUX error\r\n");
      result = false;
      pec_error = true;
      //break;
    }
    for (uint8_t current_ic = 0; current_ic < TOTAL_IC; current_ic++)
    {
      temp_code = (uint16_t)((aux_codes[current_ic][1] << 8) + aux_codes[current_ic][0]);
      temp_code_div = (float)(temp_code / 10000.0);
      convVal = LookupTemperature(temp_code_div);
      cell_temperature[current_ic][sensor_id] = (uint16_t) (convVal);
      if (BMS_debug) {
        Serial.print("B");
        Serial.print(current_ic);
        Serial.print(" C");
        Serial.print(sensor_id);
        Serial.print(" ");
        Serial.print(temp_code);
        Serial.print(" ");
        Serial.print(convVal);
        Serial.print("\r\n");
      }
      if ( convVal >= BMS_high_temperature )
        temperature_error_count++;
      if ( convVal <= BMS_low_temperature )
        temperature_error_count++;
    }
  }
  if ( pec_error == true )
    temperature_error_count++;

  if ( temperature_error_count >= TEMP_COUNT) {
    temperature_error_count = 0;
    return true;
  }
  if ( error_reset_counter >= RESET_ERROR_COUNT) {
    temperature_error_count = 0;
    error_reset_counter = 0;
  }
  return false;
}


/* void send_data_packet(uint8_t error)
   Print report to serial
*/
void send_data_packet(uint8_t error) {
  if (!BMS_debug) {
    for (uint8_t ic = 0; ic != TOTAL_IC; ic++) {
      Serial.print("D");
      Serial.print(ic);
      Serial.print("|");

      for (uint8_t cell = 0; cell != TOTAL_SENSORS; cell++) {
        if (bitRead(error,4) == 1) {
          Serial.print("PEC|");
        } else {
          Serial.print(cell);
          Serial.print("|");
          Serial.print( cell_voltage[ic][cell] / 10 );
          Serial.print("|");
        }
        if (bitRead(error,3) == 1)  {
          Serial.print("PEC|");
        } else {
          Serial.print(cell_temperature[ic][cell]);
          Serial.print("|");
        }
        //if (cell_discharge[ic][cell] == true) {
        if (bitRead(cell_discharge[ic], cell) == true) {
          Serial.print("1|");
        } else {
          Serial.print("0|");
        }
      }
      Serial.print("\r\n");
    }
  }
}


/* bool BMS_check()
   Read and check all voltage levels and temperature and current. Print out report.
   If no tolerances are exceeded and no errors occur "true" is returned otherwise in the event of failure "false" is returned
*/
bool BMS_check() {
  // Gather
  uint8_t error = 0; // | bit 4: voltage | bit 3: temperature | bit 2: current | bit 1:  |

  wakeup_sleep();
  if ( check_cell_voltages() )
    bitSet(error, 4);
  if ( check_cell_temperatures())
    bitSet(error, 3);

  current_measurement = can_read_current();
  if (current_measurement == -3.4028235E+38 || current_measurement < PWR_min_current || current_measurement >= PWR_max_current)
    current_error_count++;
  else if( error_reset_counter >= RESET_ERROR_COUNT)
    current_error_count = 0;
  if (current_error_count >= CURRENT_COUNT)
    bitSet(error, 2);

  Serial.print("Current measured: ");
  Serial.println(current_measurement);
  send_data_packet(error);
  can_send(error);

  return error > 0;
}

float BMS_get_target_voltage() {
  return BMS_discharge_voltage;
}

/**
   @return true if there was an error during last BMS_check call.
*/
//bool BMS_is_error() {
//  return false;
//}


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

      if ( (float) cell_voltage[bms_id][cell_id] / 10000.0 <= BMS_discharge_voltage) {
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


