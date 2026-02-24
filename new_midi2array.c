// TODO unsolve: 718, 732

#include "new_midi2array.h"
#include "structure_of_ws2812.h"
#include <stdio.h>
#include "stdint.h"
#include "string.h"

/*
	these arrays shouldn't be used by user through main.c; instead, they serve
	as waystations to connect midi files to self-define structure (we need to
	calculate lasting time for each special effects for evaluating the situation
	of lights at specific frame better)
	
	Assume partA ~ partD are single ws2812 and partE and F are ws2812 strips
*/

/*
	color, color_time, and SPX (if any) are wrapped together
	time and brightness are wrapped together
*/

static uint32_t partA_time[ARRAY_SIZE] = {0};			// time array
static uint32_t partA_color_time[ARRAY_SIZE] = {0};		// time to change color
static uint32_t partA_color[ARRAY_SIZE];				// color data
static uint8_t partA_brightness[ARRAY_SIZE] = {0};		// brightness of light
static uint16_t indexA_c = 1;							// index of color arrays (color and color_time)
static uint16_t indexA_t = 1;							// index of time array

static uint32_t partB_time[ARRAY_SIZE] = {0};
static uint8_t partB_color_time[ARRAY_SIZE] = {0};
static uint32_t partB_color[ARRAY_SIZE];				// there might be some default colors
static uint8_t partB_brightness[ARRAY_SIZE] = {0};
static uint16_t indexB_c = 1;
static uint16_t indexB_t = 1;							// index time also indicates the len of array in the end

static uint32_t partC_time[ARRAY_SIZE] = {0};
static uint8_t partC_color_time[ARRAY_SIZE] = {0};
static uint32_t partC_color[ARRAY_SIZE];
static uint8_t partC_brightness[ARRAY_SIZE] = {0};
static uint16_t indexC_c = 1;
static uint16_t indexC_t = 1;

static uint32_t partD_time[ARRAY_SIZE] = {0};
static uint8_t partD_color_time[ARRAY_SIZE] = {0};
static uint32_t partD_color[ARRAY_SIZE];
static uint8_t partD_brightness[ARRAY_SIZE] = {0};
static uint16_t indexD_c = 1;
static uint16_t indexD_t = 1;

static uint32_t partE_time[ARRAY_SIZE] = {0};
static uint8_t partE_color_time[ARRAY_SIZE] = {0};
static uint32_t partE_color[ARRAY_SIZE];
static uint8_t partE_brightness[ARRAY_SIZE] = {0};
static uint8_t partE_SPX[ARRAY_SIZE] = {0};
static uint16_t indexE_c = 1;
static uint16_t indexE_t = 1;

static uint32_t partF_time[ARRAY_SIZE] = {0};
static uint8_t partF_color_time[ARRAY_SIZE] = {0};
static uint32_t partF_color[ARRAY_SIZE];
static uint8_t partF_brightness[ARRAY_SIZE] = {0};
static uint8_t partF_SPX[ARRAY_SIZE] = {0};
static uint16_t indexF_c = 1;
static uint16_t indexF_t = 1;


// * finction definations

uint32_t readHeader(FILE **midi_input) {
	uint32_t buffer;
	uint32_t ticks_per_qnote;

	fread(&buffer, 4, 1, *midi_input);	// check MThd

	if (buffer != 0x6468544d) {
		printf("Input file error, was it even a midi file?\n");
		return 0;
	}

	fread(&buffer, 4, 1, *midi_input);	// skip header track length
	fread(&buffer, 2, 1, *midi_input);
	if ((buffer & 0x0000ffff) != 0x0100) {	// type 1
		printf("input file not supported because I'm stupid\n");
		printf("make sure the midi file is exported by mscore\n");
		printf("or contact with engineer\n");
		return 0;
	}

	fread(&buffer, 2, 1, *midi_input);
	if ((buffer & 0x0000ffff) != 0x0100) {	// only one track
		printf("input file not supported because I'm stupid\n");
		printf("when export midi files, export each staff individually\n");
		printf("or contact with engineer\n");
		return 0;
	}

	fread(&buffer, 2, 1, *midi_input);	// time info
	ticks_per_qnote = (((buffer & 0x0000ff00) >> 8) | ((buffer & 0x000000ff) << 8));

	fread(&buffer, 4, 1, *midi_input); // MTrk
	if (buffer != 0x6b72544d) {
		printf("track not found\n");
		return 0;
	}
	fread(&buffer, 4, 1, *midi_input);	// skip track length
	return ticks_per_qnote;
}

double read_dt(FILE **midi_input, double us_per_tick) {
	uint32_t dt = 0;
	uint8_t time_buffer = 0;

	fread(&time_buffer, 1, 1, *midi_input);
	while ((time_buffer >> 7)) {
		dt = (dt | (time_buffer & 0x7f));
		dt = (dt << 7);
		fread(&time_buffer, 1, 1, *midi_input);
	}
	dt = (dt | time_buffer);

	return (dt * us_per_tick);
}

/* 
	implemented messages:
		event : status_byte : meaning
		0     : 8*** 		: NOTE OFF (turn off light)
		1 	  : 9*** 		: NOTE ON (turn on light)
		30 	  : b*02  		: brightness (controll lights brightness, not strip)
		70 	  : ff51 		: tempo
		71 	  : ff58 		: time signature (we don't use this)
		72 	  : ff2f  		: end of track
		73 	  : ff05  		: lyrics (change color)
		39, 127,126,125 : other_status : not useful for our propose
*/
uint8_t readEvent(FILE **midi_input, uint64_t *data, uint8_t *event) {
	uint8_t event_buffer;   // store temp event
	uint8_t data_buffer[5] = {0};   // store temp data (pos, r, g, b, SPX)
	uint8_t data_length;
	uint8_t dump[8];		// dump uesless data
	uint8_t tmp[2] = {0};	// store temp value for some computation
	uint8_t is_running = 0;	// 1 if the event is same as previous onein
    int i;  // loop index

	fread(&event_buffer, 1, 1, *midi_input);

	// channel info is ignored and get the event
	switch ((event_buffer >> 4)) {
	case 0x8:   // turn off light
		*event = 0;
		break;

	case 0x9:   // turn on light
		*event = 1;
		break;

	case 0xb:   // controll brightness
		fread(&event_buffer, 1, 1, *midi_input);

		if (event_buffer != 0x02) {
			*event = 39;    // unused event
		} else {
			*event = 30;    // the only case we will use is 0xb002
		}
		break;

	case 0xf:   // meta event
		fread(&event_buffer, 1, 1, *midi_input); // meta event = 0xff**

		if (event_buffer == 0x51) { // temple
			*event = 70;
		} else if (event_buffer == 0x58) { // time signature
			*event = 71;
		} else if (event_buffer == 0x2f) { // end of track
			*event = 72;
			return 1;   // use to indicate the end
		} else if (event_buffer == 0x05) { // set the color
			*event = 73;
		} else { // unsupported
			*event = 125;
		}

		fread(&data_length, 1, 1, *midi_input);
		break;

	case 0xc: // fallthrough
	case 0xd:
		*event = 126; // unsupported
		break;

	case 0xe:
		*event = 127; // unsupported
        break;

	default: // running status (MSB is 0, event is same as previous one)
		if (*event == 30) { // check brightness
			if (event_buffer != 0x02) { // unsupported
				*event = 39;
			} else {
				*event = 30;
			}
			fread(&event_buffer, 1, 1, *midi_input);
		}
		data_buffer[0] = event_buffer;  // part info
		is_running = 1;  // running status, keep the last event
	}

	switch (*event) {
	case 0: // turn off light
		fread(&data_buffer[0], 1, 1 - is_running, *midi_input);

		// the second data byte does not matter
		fread(&dump, 1, 1, *midi_input);
		break;

	case 1: // turn on light
		fread(&data_buffer[0], 1 - is_running, 1, *midi_input);
		fread(&data_buffer[1], 1, 1, *midi_input);
		break;

	case 30: // brightness
		fread(&data_buffer[1], 1 - is_running, 1, *midi_input);
		break;

	case 70: // fallthrough
	case 71: // data for these two events are at most 4 bytes
		for ( ;data_length > 0; data_length--) {
			fread(&data_buffer[4 - data_length], 1, 1, *midi_input);
		}
		break;

	case 73: // lyrics, only used to change color
		if (data_length != 0x08) {  // unsupported
			for ( ;data_length > 0; data_length--) {
				fread(&dump, 1, 1, *midi_input);
			}
			*event = 125;
			break;
		} else {
			fread(&data_buffer[0], 1, 1, *midi_input);

			switch (data_buffer[0]) {
                // use to decide the part
			}
			if (!data_buffer[0]) { // if no part selected, dump the rest
				for ( ;data_length > 1; data_length--) {
					fread(&dump, 1, 1, *midi_input);
				}
				*event = 125;
				break;
			}
			fread(&dump, 1, 1, *midi_input); // dump "="

            // read rgb info
            for (i = 1; i < 4; i++) {
                fread(&tmp[0], 1, 1, *midi_input);
			    fread(&tmp[1], 1, 1, *midi_input);
			    data_buffer[i] = ascii_hex2value(tmp[0],tmp[1]);
            }

            /*
            if (data[0] == part which is strip) {
                fread(&tmp[0], 1, 1, *midi_input);
			    fread(&tmp[1], 1, 1, *midi_input);
			    data_buffer[4] = ascii_hex2value(tmp[0],tmp[1]);
            }
            */
		}
		break;

	case 72: // fallthrough
	case 125:
		for ( ;data_length > 0; data_length--) {
			fread(&dump, 1, 1, *midi_input);
		}

	default:
		printf("????\n");
	}

    /*
		Event		   : usage of data_buffer
        turn on light  : data_buffer[0] is part, data_buffer[1] is brightness
							of a single ws2812 (midi seems to set "NOTE OFF"
							by using "NOTE ON" and brightness is 0)
        turn off light : data_buffer[0] is part (maybe won't use)
        set color light: data_buffer[0] is part, data_buffer[1 ~ 3] are rgb
        set color strip: data_buffre[0] is part, data_buffer[1 ~ 3] are rgb and
							data_buffer[4] is SPX type
        set brightness : only data_buffer[1] is used (change all lights)

		change tempo   : at most 4 bytes are uesd
		time signature : no use
    */
    
    *data = ((uint64_t)data_buffer[4] << 32) | (data_buffer[3] << 24)
            | (data_buffer[2] << 16) | (data_buffer[1] << 8) | (data_buffer[0]);

	return 0;
}

void saveData(const uint64_t data, const uint8_t event, const double time_in_us,
			  double *us_per_tick, const int ticks_per_qnote) {
	switch (event) {
	case 0:		// note off (turn off light)
		switch (data & 0xff) {
			// case partA:
				partA_time[indexA_t] = (uint32_t)time_in_us;
				partA_brightness[indexA_t] = 0;
				indexA_t++;
				break;

			// case partB:
				partB_time[indexB_t] = (uint32_t)time_in_us;
				partA_brightness[indexB_t] = 0;
				indexB_t++;
				break;

			// case partC:
				partC_time[indexC_t] = (uint32_t)time_in_us;
				partC_brightness[indexC_t] = 0;
				indexC_t++;
				break;

			// case partD:
				partD_time[indexD_t] = (uint32_t)time_in_us;
				partD_brightness[indexD_t] = 0;
				indexD_t++;
				break;

			// case partE:
				partE_time[indexE_t] = (uint32_t)time_in_us;
				partE_brightness[indexE_t] = 0;
				indexE_t++;
				break;

			// case partF:
				partF_time[indexF_t] = (uint32_t)time_in_us;
				partF_brightness[indexF_t] = 0;
				indexF_t++;
				break;

			default:
				printf("something went wrong...\n");
		}
		break;
	
	case 1:		// note on (turn on light)
		switch (data & 0xff) {
			// case partA:
				partA_time[indexA_t] = (uint32_t)time_in_us;
				partA_brightness[indexA_t] = (data >> 8) & 0xff;
				indexA_t++;
				break;

			// case partB:
				partB_time[indexB_t] = (uint32_t)time_in_us;
				partA_brightness[indexB_t] = 0;
				indexB_t++;
				break;

			// case partC:
				partC_time[indexC_t] = (uint32_t)time_in_us;
				partC_brightness[indexC_t] = (data >> 8) & 0xff;
				indexC_t++;
				break;

			// case partD:
				partD_time[indexD_t] = (uint32_t)time_in_us;
				partD_brightness[indexD_t] = (data >> 8) & 0xff;
				indexD_t++;
				break;

			// case partE:
				partE_time[indexE_t] = (uint32_t)time_in_us;
				partE_brightness[indexE_t] = (data >> 8) & 0xff;
				indexE_t++;
				break;

			// case partF:
				partF_time[indexF_t] = (uint32_t)time_in_us;
				partF_brightness[indexF_t] = (data >> 8) & 0xff;
				indexF_t++;
				break;

			default:
				printf("something went wrong...\n");
		}
		break;
	
	case 30: 	// brightness(to all lights except strips)
		if (partA_time[indexA_t - 1] != (uint32_t)time_in_us
			&& partA_brightness[indexA_t - 1] != 0) {
				partA_time[indexA_t] = (uint32_t)time_in_us;
				partA_brightness[indexA_t] = (data >> 8) & 0xff;
				indexA_t++;
		}

		if (partB_time[indexB_t - 1] != (uint32_t)time_in_us
			&& partB_brightness[indexB_t - 1] != 0) {
				partB_time[indexB_t] = (uint32_t)time_in_us;
				partB_brightness[indexB_t] = (data >> 8) & 0xff;
				indexB_t++;
		}

		if (partC_time[indexC_t - 1] != (uint32_t)time_in_us
			&& partC_brightness[indexC_t - 1] != 0) {
				partC_time[indexC_t] = (uint32_t)time_in_us;
				partC_brightness[indexC_t] = (data >> 8) & 0xff;
				indexC_t++;
		}

		if (partD_time[indexD_t - 1] != (uint32_t)time_in_us
			&& partD_brightness[indexD_t - 1] != 0) {
				partD_time[indexB_t] = (uint32_t)time_in_us;
				partD_brightness[indexD_t] = (data >> 8) & 0xff;
				indexD_t++;
		}

		// if (partE_time[indexE_t - 1] != (uint32_t)time_in_us		// ! maybe we shouldn't adjust
		// 	&& partE_brightness[indexE_t - 1] != 0) {				// ! the brightness of strip (due to SPX)
		// 		partE_time[indexE_t] = (uint32_t)time_in_us;	
		// 		partE_brightness[indexE_t] = (data >> 8) & 0xff;
		// 		indexE_t++;
		// }

		// if (partF_time[indexF_t - 1] != (uint32_t)time_in_us
		// 	&& partF_brightness[indexF_t - 1] != 0) {
		// 		partF_time[indexF_t] = (uint32_t)time_in_us;
		// 		partF_brightness[indexF_t] = (data >> 8) & 0xff;
		// 		indexF_t++;
		// }

		break;

	case 70:	// temple (time controll)	
		*us_per_tick = (double)data / ticks_per_qnote;
		break;

	case 71:	// time signal (we don't use this)
		break;

	// case 72 should not happen cause it's the end of the track 

	case 73:	// lyrics (change color, also the initialize brightness of single light)
		switch (data & 0xff) {
			// case partA:
				partA_color_time[indexA_c] = (uint32_t)time_in_us;
				partA_color[indexA_c] = ((data >> 8) & 0xffffff);
				indexA_c++;
				break;
			
			// case partB:
				partB_color_time[indexB_c] = (uint32_t)time_in_us;
				partB_color[indexB_c] = ((data >> 8) & 0xffffff);
				indexB_c++;
				break;

			// case partC:
				partC_color_time[indexC_c] = (uint32_t)time_in_us;
				partC_color[indexC_c] = ((data >> 8) & 0xffffff);
				indexC_c++;
				break;
			
			// case partD:
				partD_color_time[indexD_c] = (uint32_t)time_in_us;
				partD_color[indexD_c] = ((data >> 8) & 0xffffff);
				indexD_c++;
				break;
			
			// case partE:
				partE_color_time[indexE_c] = (uint32_t)time_in_us;
				partE_color[indexE_c] = ((data >> 8) & 0xffffff);
				partE_SPX[indexE_c] = (data >> 32);
				indexE_c++;
				break;

			// case partF:
				partF_color_time[indexF_c] = (uint32_t)time_in_us;
				partF_color[indexF_c] = ((data >> 8) & 0xffffff);
				partF_SPX[indexF_c] = (data >> 32);
				indexF_c++;
				break;
		}
		break;

	case 39:	// fallthrough
	case 125:	// fallthrough
	case 126:	// fallthrough
	case 127:	// fallthrough
		break;
	
	default:
		printf("something went wrong\n");
		break;
	}
}

int data2struct(const char name, ws2812 array[ARRAY_SIZE]) {
	// we don't use index 0 (init to 0)
	memset(&array[0], 0, sizeof(ws2812));

	int i = 1, j = 1, count = 1;	// loop indices

	// merge time info
    switch (name) {
		// case partA:
			while (i < indexA_t && j < indexA_c) {
				if (partA_time[i] == partA_color_time[j]) {
					array[count].light.time = partA_time[i];
					array[count].light.red = (partA_color[j] & 0xff) * partA_brightness[i] / 255;
					array[count].light.green = ((partA_color[j] >> 8) & 0xff) * partA_brightness[i] / 255;
					array[count].light.blue = ((partA_color[j] >> 16) & 0xff) * partA_brightness[i] / 255;
					i++;
					j++;
					count++;
				} else if (partA_time[i] < partA_color_time[j]) {	// store brightness info (continue RGB info)
					array[count].light.time = partA_time[i];
					array[count].light.red = array[count - 1].light.red * partA_brightness[i] / 255;
					array[count].light.green = array[count - 1].light.green * partA_brightness[i] / 255;
					array[count].light.blue = array[count - 1].light.blue * partA_brightness[i] / 255;
					i++;
					count++;
				} else {	// store color info (continue brightness info)
					array[count].light.time = partA_color_time[j];
					array[count].light.red = (partA_color[j] & 0xff) * partA_brightness[i - 1] / 255;
					array[count].light.green = ((partA_color[j] >> 8) & 0xff) * partA_brightness[i - 1] / 255;
					array[count].light.blue = ((partA_color[j] >> 16) & 0xff) * partA_brightness[i - 1] / 255;
					j++;
					count++;
				}
			}

			while (i < indexA_t) {
				array[count].light.time = partA_time[i];
				array[count].light.red = array[count - 1].light.red * partA_brightness[i] / 255;
				array[count].light.green = array[count - 1].light.green * partA_brightness[i] / 255;
				array[count].light.blue = array[count - 1].light.blue * partA_brightness[i] / 255;
				i++;
				count++;
			}

			while (i < indexA_c) {
				array[count].light.time = partA_color_time[j];
				array[count].light.red = (partA_color[j] & 0xff) * partA_brightness[indexA_t - 1] / 255;
				array[count].light.green = ((partA_color[j] >> 8) & 0xff) * partA_brightness[indexA_t - 1] / 255;
				array[count].light.blue = ((partA_color[j] >> 16) & 0xff) * partA_brightness[indexA_t - 1] / 255;
				j++;
				count++;
			}
			indexA_t = count - 1;

			break;

		// case partB:
			while (i < indexB_t && j < indexB_c) {
				if (partB_time[i] == partB_color_time[j]) {
					array[count].light.time = partB_time[i];
					array[count].light.red = (partB_color[j] & 0xff) * partB_brightness[i] / 255;
					array[count].light.green = ((partB_color[j] >> 8) & 0xff) * partB_brightness[i] / 255;
					array[count].light.blue = ((partB_color[j] >> 16) & 0xff) * partB_brightness[i] / 255;
					i++;
					j++;
					count++;
				} else if (partB_time[i] < partB_color_time[j]) {
					array[count].light.time = partB_time[i];
					array[count].light.red = array[count - 1].light.red * partB_brightness[i] / 255;
					array[count].light.green = array[count - 1].light.green * partB_brightness[i] / 255;
					array[count].light.blue = array[count - 1].light.blue * partB_brightness[i] / 255;
					i++;
					count++;
				} else {
					array[count].light.time = partB_color_time[j];
					array[count].light.red = (partB_color[j] & 0xff) * partB_brightness[i - 1] / 255;
					array[count].light.green = ((partB_color[j] >> 8) & 0xff) * partB_brightness[i - 1] / 255;
					array[count].light.blue = ((partB_color[j] >> 16) & 0xff) * partB_brightness[i - 1] / 255;
					j++;
					count++;
				}
			}

			while (i < indexB_t) {
				array[count].light.time = partB_time[i];
				array[count].light.red = array[count - 1].light.red * partB_brightness[i] / 255;
				array[count].light.green = array[count - 1].light.green * partB_brightness[i] / 255;
				array[count].light.blue = array[count - 1].light.blue * partB_brightness[i] / 255;
				i++;
				count++;
			}

			while (i < indexB_c) {
				array[count].light.time = partB_color_time[j];
				array[count].light.red = (partB_color[j] & 0xff) * partB_brightness[indexB_t - 1] / 255;
				array[count].light.green = ((partB_color[j] >> 8) & 0xff) * partB_brightness[indexB_t - 1] / 255;
				array[count].light.blue = ((partB_color[j] >> 16) & 0xff) * partB_brightness[indexB_t - 1] / 255;
				j++;
				count++;
			}
			indexB_t = count - 1;

			break;

		// case partC:
			while (i < indexC_t && j < indexC_c) {
				if (partC_time[i] == partC_color_time[j]) {
					array[count].light.time = partC_time[i];
					array[count].light.red = (partC_color[j] & 0xff) * partC_brightness[i] / 255;
					array[count].light.green = ((partC_color[j] >> 8) & 0xff) * partC_brightness[i] / 255;
					array[count].light.blue = ((partC_color[j] >> 16) & 0xff) * partC_brightness[i] / 255;
					i++;
					j++;
					count++;
				} else if (partC_time[i] < partC_color_time[j]) {
					array[count].light.time = partC_time[i];
					array[count].light.red = array[count - 1].light.red * partC_brightness[i] / 255;
					array[count].light.green = array[count - 1].light.green * partC_brightness[i] / 255;
					array[count].light.blue = array[count - 1].light.blue * partC_brightness[i] / 255;
					i++;
					count++;
				} else {
					array[count].light.time = partC_color_time[j];
					array[count].light.red = (partC_color[j] & 0xff) * partC_brightness[i - 1] / 255;
					array[count].light.green = ((partC_color[j] >> 8) & 0xff) * partC_brightness[i - 1] / 255;
					array[count].light.blue = ((partC_color[j] >> 16) & 0xff) * partC_brightness[i - 1] / 255;
					j++;
					count++;
				}
			}

			while (i < indexC_t) {
				array[count].light.time = partC_time[i];
				array[count].light.red = array[count - 1].light.red * partC_brightness[i] / 255;
				array[count].light.green = array[count - 1].light.green * partC_brightness[i] / 255;
				array[count].light.blue = array[count - 1].light.blue * partC_brightness[i] / 255;
				i++;
				count++;
			}

			while (i < indexC_c) {
				array[count].light.time = partC_color_time[j];
				array[count].light.red = ((partC_color[j] & 0xff)) * partC_brightness[indexC_t - 1] / 255;
				array[count].light.green = ((partC_color[j] >> 8) & 0xff) * partD_brightness[indexC_t - 1] / 255;
				array[count].light.blue = ((partC_color[j] >> 16) & 0xff) * partD_brightness[indexC_t - 1] / 255;
				j++;
				count++;
			}
			indexC_t = count - 1;
			
			break;

		// case partD:
			while (i < indexD_t && j < indexD_c) {
				if (partD_time[i] == partD_color_time[j]) {
					array[count].light.time = partD_time[i];
					array[count].light.red = (partD_color[j] & 0xff) * partD_brightness[i] / 255;
					array[count].light.green = ((partD_color[j] >> 8) & 0xff) * partD_brightness[i] / 255;
					array[count].light.blue = ((partD_color[j] >> 16) & 0xff) * partD_brightness[i] / 255;
					i++;
					j++;
					count++;
				} else if (partD_time[i] < partD_color_time[j]) {
					array[count].light.time = partD_time[i];
					array[count].light.red = array[count - 1].light.red * partD_brightness[i] / 255;
					array[count].light.green = array[count - 1].light.green * partD_brightness[i] / 255;
					array[count].light.blue = array[count - 1].light.blue * partD_brightness[i] / 255;
					i++;
					count++;
				} else {
					array[count].light.time = partD_color_time[j];
					array[count].light.red = (partD_color[j] & 0xff) * partD_brightness[i - 1] / 255;
					array[count].light.green = ((partD_color[j] >> 8) & 0xff) * partD_brightness[i - 1] / 255;
					array[count].light.blue = ((partD_color[j] >> 16) & 0xff) * partD_brightness[i - 1] / 255;
					j++;
					count++;
				}
			}

			while (i < indexD_t) {
				array[count].light.time = partD_time[i];
				array[count].light.red = array[count - 1].light.red * partD_brightness[i] / 255;
				array[count].light.green = array[count - 1].light.green * partD_brightness[i] / 255;
				array[count].light.blue = array[count - 1].light.blue * partD_brightness[i] / 255;
				i++;
				count++;
			}

			while (i < indexD_c) {
				array[count].light.time = partD_color_time[j];
				array[count].light.red = (partD_color[j] & 0xff) * partD_brightness[indexD_t - 1] / 255;
				array[count].light.green = ((partD_color[j] >> 8) & 0xff) * partD_brightness[indexD_t - 1] / 255;
				array[count].light.blue = ((partD_color[j] >> 16) & 0xff) * partD_brightness[indexD_t - 1] / 255;
				j++;
				count++;
			}
			indexD_t = count - 1;

			break;

		// case partE:
			while (i < indexE_t && j < indexE_c) {
				if (partE_time[i] == partE_color_time[j]) {
					array[count].strip.time = partE_time[i];
					array[count].strip.red = (partE_color[j] & 0xff) * partE_brightness[i] / 255;
					array[count].strip.green = ((partE_color[j] >> 8) & 0xff) * partE_brightness[i] / 255;
					array[count].strip.blue = ((partE_color[j] >> 16) & 0xff) * partE_brightness[i] / 255;
					array[count].strip.SPX_type = partE_SPX[j];
					array[count].strip.SPX_duration = partE_color_time[j + 1] - partE_color_time[j];
					i++;
					j++;
					count++;
				} else if (partE_time[i] < partE_color_time[j]) {	// store brightness info (just turn on or off)
					if (partE_brightness[i] == 0) {		// turn off
						array[count].strip.time = partE_time[i];
						array[count].strip.red = 0;
						array[count].strip.green = 0;
						array[count].strip.blue = 0;
						array[count].strip.SPX_type = 0;
						i++;
						count++;
					} else if (partE_brightness[i - 1] == 0) {	// turn on (TBH I don't know it's allowed or not)
						// ! communicate with the other group!!!
					} else {
						printf("this shouldn't happen!!!\n");
					}
				} else {	// store color info first maybe this shouldn't happen...
					// array[count].strip.time = partE_color_time[j];
					// array[count].strip.red = (partE_color[j] & 0xff);
					// array[count].strip.green = (partE_color[j] >> 8) & 0xff;
					// array[count].strip.blue = (partE_color[j] >> 16) & 0xff;
					// j++;
					// count++;
				}
			}

			// ! discuss the writing convention of strip

			indexE_t = count - 1;

			break;
		
		// case partF:

			indexF_t = count - 1;

			break;

		default:
			printf("something went wrong...\n");
	}
}

void write2file(FILE **output, char name, ws2812 *array) {
	int i;	// loop index

	switch (name) {
		// case partA:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexA_t);
			for (i = 0; i < indexA_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".light = {%d, %u, %u, %u}, ", array[i].light.time, array[i].light.red,
																array[i].light.green, array[i].light.blue);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;

		// case partB:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexB_t);
			for (i = 0; i < indexB_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".light = {%d, %u, %u, %u}, ", array[i].light.time, array[i].light.red,
																array[i].light.green, array[i].light.blue);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;

		// case partC:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexC_t);
			for (i = 0; i < indexC_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".light = {%d, %u, %u, %u}, ", array[i].light.time, array[i].light.red,
																array[i].light.green, array[i].light.blue);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;

		// case partD:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexD_t);
			for (i = 0; i < indexD_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".light = {%d, %u, %u, %u}, ", array[i].light.time, array[i].light.red,
																array[i].light.green, array[i].light.blue);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;

		// case partE:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexE_t);
			for (i = 0; i < indexE_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".strip = {%d, %u, %u, %u, %u, %u}, ", array[i].strip.time, array[i].strip.red,
																		array[i].strip.green, array[i].strip.blue,
																		array[i].strip.SPX_type, array[i].strip.SPX_duration);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;

		// case partF:
			fprintf(*output, "const ws2812 %c[%d] = {\n", name, indexF_t);
			for (i = 0; i < indexF_t - 1; i++) {
				if (i % 4 == 0) {
					fprintf(*output, "\t");
				}
				fprintf(*output, ".strip = {%d, %u, %u, %u, %u, %u}, ", array[i].strip.time, array[i].strip.red,
																		array[i].strip.green, array[i].strip.blue,
																		array[i].strip.SPX_type, array[i].strip.SPX_duration);
				if (i % 4 == 3) {
					fprintf(*output, "\n");
				}
			}
			fprintf(*output, "\n\t.light = {-1, 0, 0, 0}};");
			break;
	}
}

uint8_t ascii_hex2value(uint8_t hex1, uint8_t hex2) {
	hex1 = (hex1 <= '9') ? (hex1 - '0') :
		   (hex1 <= 'f') ? (hex1 - 'a' + 10) : (hex1 - 'A' + 10);
	
	hex2 = (hex2 <= '9') ? (hex2 - '0') :
		   (hex2 <= 'f') ? (hex2 - 'a' + 10) : (hex2 - 'A' + 10);

	return (hex1 << 4) | hex2;
}
