/*	This piece of spaghetti takes midi input from the first argument
 *	and turn it into arrays of times and brightness
 *  only type 1 with only one track is supported
*/

// ? question: line-539 433
// ? ask about time merging logic of my code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/* macros */
#define FILENAME_SIZE 20
#define ARRAY_SIZE 10000

/* function declarations */

/* check if the given input file is supported
 * return ticks per quarter note
*/
unsigned int read_header(FILE **midi_input);

/* assuming next data would be delta time, return dt in us */
double read_dt(FILE **midi_input , double us_per_tick);

/* read midi events, return 1 if End of Track is read, ow, return 0
 * depended on the status byte, the data might have different meaning
 */
char read_event(FILE **midi_input, int *data, unsigned char *event); 

/* write to the output header file */
char write2arr(FILE **output , int* t, char* s,
			 int ind_t, int ind_c, char name, int* color_t, int* color);

/* convert ascii code for 00-ff to its value*/
unsigned char ascii_hex_to_value(unsigned char hexin1, unsigned char hexin0);


/* finction definations */

unsigned int read_header(FILE **midi_input)
{
	unsigned int buffer;
	unsigned int ticks_per_qnote;

	fread(&buffer,4,1,*midi_input);

	if (buffer != 0x6468544d) { /*check MThd*/
		printf("input file error, was it even a midi file?\n");
		return 0;
	}
	fread(&buffer,4,1,*midi_input); /*skip header track length*/
	fread(&buffer,2,1,*midi_input);
	if ((buffer & 0x0000ffff) != 0x0100) { /*type 1*/
		printf("input file not supported because I'm stupid\n");
		printf("make sure the midi file is exported by mscore\n");
		printf("or contact with engineer\n");
		return 0;
	}

	fread(&buffer,2,1,*midi_input);
	if ((buffer & 0x0000ffff) != 0x0100) {	/*only one track*/
		printf("input file not supported because I'm stupid\n");
		printf("when export midi files, export each staff individually\n");
		printf("or contact with engineer\n");
		return 0;
	}
	fread(&buffer,2,1,*midi_input);
	ticks_per_qnote = (((buffer & 0x0000ff00) >> 8) | ((buffer & 0x000000ff) << 8));
	fread(&buffer,4,1,*midi_input); /*MTrk*/
	if (buffer != 0x6b72544d) {
		printf("track not found\n");
		return 0;
	}
	fread(&buffer,4,1,*midi_input); /*skip track length*/
	return ticks_per_qnote;
}

double read_dt(FILE **midi_input, double us_per_tick)
{
	unsigned int dt = 0;
	unsigned char time_buffer = 0;

	fread(&time_buffer,1,1,*midi_input);
	while ((time_buffer >> 7)) {
		dt = (dt | (time_buffer & 0x7f));
		dt = (dt << 7);
		fread(&time_buffer,1,1,*midi_input);
	}
	dt = (dt | time_buffer);

	return (dt * us_per_tick);
}

/* implemented messages:
 * event : status_byte : meaning
 * 0 : 8*** : NOTE OFF
 * 1 : 9*** : NOTE ON
 ! 30 : b*02 : brightness
 * 70 : ff51 : tempo
 * 71 : ff58 : time signature
 * 72 : ff2f : end of track
 * 73 : ff05 : lyrics(change color)
 * 127,126,125,39 : other_status : not useful for our propose
*/
char read_event(FILE **midi_input, int *data, unsigned char *event)
{
	unsigned char event_buffer;
	unsigned char data_buffer[4] = {0};
	unsigned char data_length;
	char dump[8];  /*dump uesless data*/
	char is_running = 0; /*1 if running status*/
	char tmp[8] = {0}; /*store temporary value for some computation*/

	fread(&event_buffer, 1, 1, *midi_input);

	/* channel info is ignored and get the event */
	switch ((event_buffer >> 4)) {
	case 0x8:
		*event = 0;
		break;

	case 0x9:
		*event = 1;
		break;

	case 0xb:
		fread(&event_buffer, 1, 1, *midi_input);

		if (event_buffer != 0x02) {
			*event = 39;
		} else {
			*event = 30;
		}
		break;

	case 0xf:
		fread(&event_buffer, 1, 1, *midi_input);

		if (event_buffer == 0x51) { /*temple*/
			*event = 70;
		} else if (event_buffer == 0x58) { /*time sig*/
			*event = 71;
		} else if (event_buffer == 0x2f) { /*end of track*/
			*event = 72;
			return 1;
		} else if (event_buffer == 0x05) { /*lyrics*/
			*event = 73;
		} else { /*unsupported, discard <data_length> bytes*/
			*event = 125;
		}

		fread(&data_length, 1, 1, *midi_input);
		break;

	case 0xc: /*FALLTHROUGH*/

	case 0xd:
		*event = 126; /*only one data byte*/
		break;

	case 0xa: /*FALLTHROUGH*/

	case 0xe:
		*event = 127;
		break;

	default: /*running status*/
		if ((*event) / 10 == 3) { /*check brightness*/
			if (event_buffer != 0x02) {
				*event = 39;
			} else {
				*event = 30;
			}
			fread(&event_buffer,1,1,*midi_input);
		}	
		data_buffer[3] = event_buffer;
		is_running = 1;
		break; /*running status, keep the last event*/
	}

	switch (*event) {
	case 0: /* NOTE OFF */
		fread(&data_buffer[3], 1, 1 - is_running, *midi_input);
		/* the second data byte does not matter */
		fread(&dump, 1, 1, *midi_input);
		break;

	case 1: /* NOTE ON */
		fread(&data_buffer[3], 1 - is_running, 1, *midi_input);
		fread(&data_buffer[2], 1, 1, *midi_input);
		break;

	case 30: /*brightness*/
		fread(&data_buffer[3], 1 - is_running, 1, *midi_input);
		break;
	
	case 70: /*FALLTHROUGH*/

	case 71: /*data for these two events are at most 4 bytes*/
		for ( ; data_length > 0; data_length--) {
			fread(&data_buffer[data_length - 1], 1, 1, *midi_input);
		}
		break;

	case 73: /*lyrics, only used to change color*/
		if (data_length != 0x08) {
			for ( ;data_length > 0; data_length--) {
				fread(&dump,1,1,*midi_input);
			}
			*event = 125;
			break;
		} else {
			fread(&data_buffer[3], 1, 1, *midi_input);

			switch (data_buffer[3]) {
			case 0x48:		/*H*/
				data_buffer[3] = 1;
				break;
			case 0x49:		/*I*/
				data_buffer[3] = 2;
				break;
			case 0x4F:		/*O*/
				data_buffer[3] = 3;
				break;
			case 0x4C:		/*L*/
				data_buffer[3] = 4;
				break;
			default:
				data_buffer[3] = 0;
				break;
			}
			if (!(data_buffer[3])) { /*dump the rest and move on*/
				for ( ;data_length > 1; data_length--) {
					fread(&dump,1,1,*midi_input);
				}
				*event = 125;
				break;
			}
			fread(&dump, 1, 1, *midi_input); /*dump "="*/

			/*R*/
			fread(&tmp[1], 1, 1, *midi_input);
			fread(&tmp[0], 1, 1, *midi_input);
			data_buffer[2] = ascii_hex_to_value(tmp[1],tmp[0]);

			/*G*/
			fread(&tmp[1], 1, 1, *midi_input);
			fread(&tmp[0], 1, 1, *midi_input);
			data_buffer[1] = ascii_hex_to_value(tmp[1],tmp[0]);

			/*B*/
			fread(&tmp[1], 1, 1, *midi_input);
			fread(&tmp[0], 1, 1, *midi_input);
			data_buffer[0] = ascii_hex_to_value(tmp[1],tmp[0]);
		}
		break;

	case 72: /*FALLTHROUGH*/
	case 125:
		for ( ;data_length > 0; data_length --) {
			fread(&dump, 1, 1, *midi_input);
		}
		break;

	case 39: /*FALLTHROUGH*/
	case 126: /* unsupported, discard 1 byte */
		fread(&dump, 1 - is_running, 1, *midi_input);
		break;

	case 127: /* unsupported, discard 2 bytes */
		fread(&dump, 2 - is_running, 1, *midi_input);
		break;

	default:
	printf("??\n");
		break;
	}

	*data = ((data_buffer[3] << 24) | (data_buffer[2] << 16)
			 | (data_buffer[1] << 8) | (data_buffer[0]));

	return 0;
}

char write2arr(FILE **output , int* t, char* s, int ind_t, 
			   int ind_c, char name, int* color_t, int* color)
{
	int R,G,B;			/*color, range from 0-127*/
	int j = 0;		   	/*looping index for color*/
	
	fprintf(*output, "const int %c_t[%u] = {\n", name, ind_t + 1);
	for (int i = 0; i < ind_t; i++) {
		if ((i % 8) == 0) {
			fprintf(*output, "\t");
		}
		fprintf(*output, "%u, ", t[i]);
		if ((i % 8) == 7) {
			fprintf(*output, "\n");
		}
	}
	fprintf(*output, "\n\t-1 };\n");

	j = 0;
	fprintf(*output, "const char %c_r[%u] = {\n", name, ind_t + 1);
	for (int i = 0; i < ind_t; i++) {
		while ((j < ind_c - 1) && (t[i] >= color_t[j + 1])) {		
			j++;
		}
		R = (color[j] & 0x00ff0000) >> 16;
		if (i % 8 == 0) {
			fprintf(*output, "\t");
		}
		fprintf(*output,"%u, ",(s[i] * R) / 255);
		if (i % 8 == 7 ) {
			fprintf(*output, "\n");
		}
	}
	fprintf(*output, "\n\t-1 };\n");
	
	j = 0;
	fprintf(*output, "const char %c_g[%u] = {\n", name, ind_t + 1);
	for (int i = 0; i < ind_t; i++) {
		while((t[i] >= color_t[j+1]) && (j < ind_c - 1)) {
			j++;
		}	
		G = (color[j] & 0x0000ff00) >> 8;
		if (i % 8 == 0) {
			fprintf(*output, "\t");
		}
		fprintf(*output, "%u, ", (s[i] * G) / 255);
		if (i % 8 == 7) {
			fprintf(*output, "\n");
		}
	}
	fprintf(*output, "\n\t-1 };\n");
	
	j = 0;
	fprintf(*output, "const char %c_b[%u] = {\n", name, ind_t + 1);
	for (int i = 0; i < ind_t; i++) {
		while((j < ind_c - 1) && (t[i] >= color_t[j+1])) {
			j++;
		}	
		B = (color[j] & 0x000000ff);
		if (i % 8 == 0) {
			fprintf(*output, "\t");
		}
		fprintf(*output, "%u, ", (s[i] * B) / 255);
		if (i % 8 == 7) {
			fprintf(*output, "\n");
		}
	}
	fprintf(*output, "\n\t-1 };\n");

} 

unsigned char ascii_hex_to_value(unsigned char hexin1, unsigned char hexin0)
{
	unsigned char output = 0;
	if ((hexin1 > 47) && (hexin1 < 58)) {
		output += (hexin1 - 48) << 4;
	} else if ((hexin1 > 64) && (hexin1 < 71)) {
		output += (hexin1 - 55) << 4;
	} else if ((hexin1 > 96) && (hexin1 < 103)) {
		output += (hexin1 - 87) << 4;
	}

	if ((hexin0 > 47) && (hexin0 < 58)) {
		output += (hexin0 - 48);
	} else if ((hexin0 > 64) && (hexin0 < 71)) {
		output += (hexin0 - 55);
	} else if ((hexin0 > 96) && (hexin0 < 103)) {
		output += (hexin0 - 87);
	} else {
		output = 0;
	}
	return (output);
}


/* TODO: improve timimg */
int main(int argc, char **argv)
{
	FILE *input, *output;        /*pointer to files*/
	double time_in_us = 0;        /*timer*/
	int bar = 0;                 /*TODO: bar counter for better readality*/
	int data = 0;                /*data read from midi*/
	unsigned char event = 127;   /*midi event*/
	double us_per_tick = 1;      /*how long one tick is*/
	int ticks_per_qnote = 1;     /*ticks per quarter note*/
	char input_filename[FILENAME_SIZE];

	/* time arrays */
	int HEAD_t[ARRAY_SIZE] = {0};
	int INNER_t[ARRAY_SIZE] = {0};	
	int OUTER_t[ARRAY_SIZE] = {0};
	int LEG_t[ARRAY_SIZE] = {0};

	/* 
		* counting index for time arrays
		* count from 1 so arr[ind-1] won't break
		* because I'm lazy and stupid
	*/
	int ind_HEADt = 1;
	int ind_INNERt = 1;
	int ind_OUTERt = 1;
	int ind_LEGt = 1;

	/* luminosity arrays */
	char HEAD_s[ARRAY_SIZE] = {0};
	char INNER_s[ARRAY_SIZE] = {0};	
	char OUTER_s[ARRAY_SIZE] = {0};
	char LEG_s[ARRAY_SIZE] = {0};

	/* time arrays for color */
	int HEAD_ct[ARRAY_SIZE] = {0};
	int INNER_ct[ARRAY_SIZE] = {0};	
	int OUTER_ct[ARRAY_SIZE] = {0};
	int LEG_ct[ARRAY_SIZE] = {0};

	/*
		* counting index for color
	 	* starting from 1 because the first is the default color
	 	* which is set in the config file
	*/
	int ind_HEADc = 1;
	int ind_INNERc = 1;
	int ind_OUTERc = 1;
	int ind_LEGc = 1;

	/* color arrays */
	int HEAD_c[ARRAY_SIZE] = {COLORH};	// ! just init index 0 ???
	int INNER_c[ARRAY_SIZE] = {COLORI};	
	int OUTER_c[ARRAY_SIZE] = {COLORO};
	int LEG_c[ARRAY_SIZE] = {COLORL};

	if (argc == 1){
		printf("please specify the input file\n");
		printf("usage : midi2array input (output)\n");
		return 0;
	}
	/* copy the input file name to input_filename */
	strncpy(input_filename, argv[1], FILENAME_SIZE);
	input = fopen(input_filename, "r"); /*open the file*/
	output = fopen("out.h", "w"); /*open the file*/

	if (!(ticks_per_qnote = read_header(&input))) {
		return 0;
	}
	
	/* default 120 BPM => 500000 us/qnote */
	/* read from ff51 : us/qnote */
	/* read from dt : ticks */
	/* us/tick = (us/qnote)/(tick/qnote)*/
	us_per_tick = 500000.0f / ticks_per_qnote;

	while (1) {
		time_in_us += read_dt(&input, us_per_tick);	
		if (read_event(&input, &data, &event)) {
			write2arr(&output, HEAD_t, HEAD_s, 
						ind_HEADt, ind_HEADc, 'H', HEAD_ct, HEAD_c);
			write2arr(&output, OUTER_t, OUTER_s, 
						ind_OUTERt, ind_OUTERc, 'O', OUTER_ct, OUTER_c);
			write2arr(&output, INNER_t, INNER_s, 
						ind_INNERt, ind_INNERc, 'I', INNER_ct, INNER_c);
			write2arr(&output, LEG_t, LEG_s, 
						ind_LEGt, ind_LEGc, 'L', LEG_ct, LEG_c);
			fclose(input);
			fclose(output);
			return 0; 
		} else {
			switch(event) {
			case 0: /*NOTE OFF*/
				switch(data >> 24) {
				case HEAD:
					HEAD_t[ind_HEADt] = (int)time_in_us;
					HEAD_s[ind_HEADt] = 0;
					ind_HEADt++;
					break;
				
				case INNER:
					INNER_t[ind_INNERt] = (int)time_in_us;
					INNER_s[ind_INNERt] = 0;
					ind_INNERt++;
					break;
				
				case OUTER:
					OUTER_t[ind_OUTERt] = (int)time_in_us;
					OUTER_s[ind_OUTERt] = 0;
					ind_OUTERt++;
					break;
				
				case LEG:
					LEG_t[ind_LEGt] = (int)time_in_us;
					LEG_s[ind_LEGt] = 0;
					ind_LEGt++;
					break;
				
				default:
					break;
				}

				break;

			case 1: /*NOTE ON*/
				switch(data >> 24) {
				case HEAD:
					HEAD_t[ind_HEADt] = (int)time_in_us;
					HEAD_s[ind_HEADt] = (data >> 16) & 0x000000ff;
					ind_HEADt++;
					break;
				
				case INNER:
					INNER_t[ind_INNERt] = (int)time_in_us;
					INNER_s[ind_INNERt] = (data >> 16) & 0x000000ff;
					ind_INNERt++;
					break;
				
				case OUTER:
					OUTER_t[ind_OUTERt] = (int)time_in_us;
					OUTER_s[ind_OUTERt] = (data >> 16) & 0x000000ff;
					ind_OUTERt++;
					break;
				
				case LEG:
					LEG_t[ind_LEGt] = (int)time_in_us;
					LEG_s[ind_LEGt] = (data >> 16) & 0x000000ff;
					ind_LEGt++;
					break;
				
				default:
					break;
				}

				break;
			
			case 30: /* brightness */
				if ((HEAD_t[ind_HEADt - 1] != (int)time_in_us) &&	// ! what is this condition for ?? 
						(HEAD_s[ind_HEADt - 1] != 0)) {
					HEAD_t[ind_HEADt] = (int)time_in_us;
					HEAD_s[ind_HEADt] = (data >> 24);
					ind_HEADt++;
				}
				if ((INNER_t[ind_INNERt - 1] != (int)time_in_us) &&
						(INNER_s[ind_INNERt - 1] != 0)) {
					INNER_t[ind_INNERt] = (int)time_in_us;
					INNER_s[ind_INNERt] = (data >> 24);
					ind_INNERt++;
				}
				if ((OUTER_t[ind_OUTERt - 1] != (int)time_in_us) &&
						(OUTER_s[ind_OUTERt - 1] != 0)) {
					OUTER_t[ind_OUTERt] = (int)time_in_us;
					OUTER_s[ind_OUTERt] = (data >> 24);
					ind_OUTERt++;
				}
				if ((LEG_t[ind_LEGt - 1] != (int)time_in_us) &&
						(LEG_s[ind_LEGt - 1] != 0)) {
					LEG_t[ind_LEGt] = (int)time_in_us;
					LEG_s[ind_LEGt] = (data >> 24);
					ind_LEGt++;
				}
				break;

			case 70: /*temple*/
				us_per_tick = (float)data / (float)ticks_per_qnote;
				break;
			
			case 71: /*time sig*/
				break;
			
			case 73 : /*lyrics, to change color*/
				switch (data >> 24) {
				case 1:
					HEAD_ct[ind_HEADc] = (int)time_in_us;
					HEAD_c[ind_HEADc] = (data & 0x00ffffff);
					ind_HEADc++;
					break;
				
				case 2:
					INNER_ct[ind_INNERc] = (int)time_in_us;
					INNER_c[ind_INNERc] = (data & 0x00ffffff);
					ind_INNERc++;
					break;
				
				case 3:
					OUTER_ct[ind_OUTERc] = (int)time_in_us;
					OUTER_c[ind_OUTERc] = (data & 0x00ffffff);
					ind_OUTERc++;
					break;
				
				case 4:
					LEG_ct[ind_LEGc] = (int)time_in_us;
					LEG_c[ind_LEGc] = (data & 0x00ffffff);
					ind_LEGc++;
					break;
				
				default:
					break;
				}
			case 39 : /*FALLTHROUGH*/
			case 125: /*FALLTHROUGH*/
			case 126: /*FALLTHROUGH*/
			case 127: /*unsupported*/
				break;
			
			default:
				printf("this should not happen\n");
				break;
			}
		}
	}

	return 0;
}