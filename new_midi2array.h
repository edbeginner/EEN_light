#ifndef MIDI2ARRAY_H
#define MIDI2ARRAY_H

#include <stdint.h>
#include <stdio.h>
#include "structure_of_ws2812.h"


// * macros

#define FILENAME_SIZE 20
#define ARRAY_SIZE 10000


// * function declarations

/*
	check if the given input file is supported
	and return ticks per quarter note
*/
uint32_t readHeader(FILE **midi_input);

// assume next data would be delta time, return dt in us
double read_dt(FILE **midi_input, double us_per_tick);

/*
	read midi events, return 1 if End of Track is read, otherwise return 0.
	Depended on the event byte, the data might have different meaning
*/
uint8_t readEvent(FILE **midi_input, uint64_t *data, uint8_t *event);

// store data to corresponding array
void saveData(const uint64_t data, const uint8_t event, const double time_in_us,
			  double *us_per_tick, const int ticks_per_qnote);

// change data to struct and return the len of array
int data2struct(const char name, ws2812 array[ARRAY_SIZE]);

// write the given structure array to an ouput header file
void write2file(FILE **output, char name, ws2812 *array);

// convert ascii code for 00-ff to its value
uint8_t ascii_hex2value(uint8_t hex1, uint8_t hex0);

#endif