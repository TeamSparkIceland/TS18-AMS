/*  Name:        tmap.ccp
 *  Description: Changed from using a memory intensive and time consuming 
 *               lookup table to use a 6th degree taylor approximaiton found 
 *               with Python. Povides about a twofold increase in speed, 
 *               saves around 400 bytes and a max errror of around 0.6 degrees.
 *  Date:        30.06.18
 */


#include "tmap.h"
#include <stdint.h>
#include <stdio.h>


// 5th degree taylor
//static const float poly[6] = { -0.78897406,    9.3072085 ,  -43.9184899 ,  105.7000278 ,  -154.66094589,  150.62510468 };

// 6th degree taylor
static const float poly[7] = { 0.36451765,   -5.17246463,   29.94820667,  -92.05035697,  163.49325764, -188.03303634,  157.78043721 };


/* float LookupTemperature(float voltage)
 * Calculate tempurature from measured voltage
 * with a taylor approximation
 */
float LookupTemperature(float voltage) {
  if( voltage >= 3.64 ) 
    return 0;
  if( voltage <= 0.44 )
    return 100;
  // nested multiplication
  int i;
  float r = poly[0];
  for(i = 1; i < 7; i++) {  // 6th degree has 7 coefficients
    r = poly[i] + r*voltage;
  }
  return r;
}
