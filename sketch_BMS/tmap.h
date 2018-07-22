/*  Name: tmap.ccp
 *  Description: Changed from using a memory intensive and time consuming 
 *               lookup table to use a 6th degree taylor approximaiton found 
 *               with Python. Povides about a twofold increase in speed, 
 *               saves around 400 bytes and a max errror of around 0.6 degrees.
 *  Date:        30.06.18
 */


#ifndef TMAP_H
#define	TMAP_H


float LookupTemperature(float voltage);


#endif	/* TMAP_H */

