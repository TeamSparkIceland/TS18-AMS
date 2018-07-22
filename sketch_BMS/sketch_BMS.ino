/*!
  Team spark BMS
  Name:         sketch_BMS.ino
  Date created: 27.5.18
  Version:      2.0 (16.06.18)
  Description:

  @verbatim

  NOTES
  Setup:
   Set the terminal baud rate to 115200 and select the newline terminator.
   Ensure all jumpers on the demo board are installed in their default positions from the factory.
   Refer to Demo Manual D1894B.

  USER INPUT DATA FORMAT:
  decimal : 1024
  hex     : 0x400
  octal   : 02000  (leading 0)
  binary  : B10000000000
  float   : 1024.0
  @endverbatim

*/

#define IO_TSAL_ENABLE_GetValue()  digitalRead(0)


#define PERIOD 0x0001
#define ZERO 0x0000

#define DISCHARGE_CYCLE 60
#define REST_CYCLE 30

// Libraries
#include <Arduino.h>
#include <stdint.h>
#include "Linduino.h"
#include "LT_SPI.h"
#include "UserInterface.h"
#include "LTC68041.h"
#include <SPI.h>
#include "mcp_can.h"
// Other files
#include "BMS.h"
#include "CAN.h"

#define Shutdown_Pin 5


uint8_t TMR0IF;
uint8_t counter = 1;
bool reverse = false;
uint8_t timer_counter;
uint8_t inbound_data;

bool enable_discharge = false;

bool discharge_rest_period = false;


void setup()
{
  pinMode(Shutdown_Pin, OUTPUT);
  digitalWrite(Shutdown_Pin, HIGH);
  Serial.begin(115200);
  pinMode(IO_BMS_CS, OUTPUT);
  pinMode(TSAL_PIN_0, INPUT);
  pinMode(TSAL_PIN_1, INPUT);
  pinMode(TSAL_PIN_2, INPUT);
  pinMode(TSAL_PIN_3, INPUT);
  BMS_Initialize();
  init_can();
  TMR0IF = 0;
}

void loop()
{

  // Check data for tolerance levels
  if (BMS_check() == true) { // if true trigger shutdown
    Serial.print("E|BMS Shutdown triggered|\r\n");
    digitalWrite(Shutdown_Pin,LOW);
  } else {
    digitalWrite(Shutdown_Pin,HIGH);
  }
  
  if ((enable_discharge) && (!BMS_is_discharge_enabled())) {
    BMS_set_discharge(true);
    Serial.print("Discharge Voltage: ");
    Serial.println((uint16_t)(BMS_get_target_voltage() * 1000));
  } else if ((!enable_discharge) && (BMS_is_discharge_enabled())) {
    BMS_set_discharge(false);
  }

  if (TMR0IF >= 5) {
    timer_counter--;
    if (timer_counter == 0) {
      discharge_rest_period = !discharge_rest_period;
      if (discharge_rest_period) {
        timer_counter = REST_CYCLE;
      } else {
        timer_counter = DISCHARGE_CYCLE;
      }
    }

    TMR0IF = 0;

    // Command detection
    if (Serial.available() > 0) {
      inbound_data = Serial.read();
      switch (inbound_data) {
        case 'D':
          enable_discharge = false;
          break;
        case 'E':
          enable_discharge = true;
          break;
        case 'R':
          send_data_packet();
          break;
      }
    }
  }
  TMR0IF++;
  
  if (discharge_rest_period == false) {
    BMS_handle_discharge();
  } else {
    BMS_clear_discharge();
  }
}




