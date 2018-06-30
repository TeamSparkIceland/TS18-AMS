/*!
  Name:         BMS.h
  Date created: 2.5.18
  Version:      2.0 (16.06.18)
  Description:  Contains functions for reading and working with data from LTC6811 using library for LTC68041
*/

#ifndef BMS_H
#define	BMS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

// Define wether to print out and excecute debug commands
#define BMS_debug  true

#define TOTAL_SENSORS 12
#define TEMP_COUNT 5
#define VOLTAGE_COUNT 5

//#define TOTAL_IC  12  // number of ICs in the daisy chain


// Structure for containing values for each individual cell
typedef struct Cells {
//  float voltage;
//  float temperature;
//  bool voltage_pec_failure;
//  bool temperature_pec_failure;
//  uint8_t voltage_error_count;
//  uint8_t temperature_error_count;
  bool discharge_enabled;
} Cell;

//typedef struct dischargeState {
//  bool state;
//} DischargeState;


#define BMS_low_voltage      3.2   // V
#define BMS_high_voltage     4.18  // V
#define BMS_low_temperature  5.0   // Degrees C
#define BMS_high_temperature 58.0  // Degrees C



// Function declarations
void BMS_Initialize();
bool BMS_check();
bool BMS_is_error();

uint16_t BMS_get_error_code();

void BMS_set_thresholds(float cell_low, float cell_high, float sensor_low, float sensor_high);

//void BMS_test_stuff();

bool BMS_is_discharge_enabled();
float BMS_get_target_voltage();
void BMS_set_discharge(bool state);
void BMS_handle_discharge();
void BMS_clear_discharge();


#ifdef	__cplusplus
}
#endif

#endif	/* BMS_H */
