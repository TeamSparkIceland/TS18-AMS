/*!
  Name:         BMS.h
  Date created: 2.5.18
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
#define BMS_debug  false

#define TOTAL_SENSORS 12
#define TEMP_COUNT 5
#define VOLTAGE_COUNT 5
#define CURRENT_COUNT 2
#define RESET_ERROR_COUNT 10

#define TSAL_PIN_0  0
#define TSAL_PIN_1  1
#define TSAL_PIN_2  2
#define TSAL_PIN_3  3


#define BMS_low_voltage       3.2   // V
#define BMS_high_voltage      4.18  // V
#define BMS_low_temperature   5.0   // Degrees C
#define BMS_high_temperature  58.0  // Degrees C
#define PWR_max_current       150.0 //A
#define PWR_min_current      -12.0 //A


// Function declarations
void BMS_Initialize();
void send_data_packet();
bool BMS_check();

uint16_t BMS_get_error_code();
void BMS_set_thresholds(float cell_low, float cell_high, float sensor_low, float sensor_high);
bool BMS_is_discharge_enabled();
float BMS_get_target_voltage();
void BMS_set_discharge(bool state);
void BMS_handle_discharge();
void BMS_clear_discharge();


#ifdef	__cplusplus
}
#endif

#endif	/* BMS_H */
