#include "new_midi2array.h"
#include "structure_of_ws2812.h"
#include "new_config.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    /* 
        TODO: read argument
        TODO: handle header chunk and get ticks_per_qnote
        TODO: get us_per_tick

        while (end condition) {
            get time
            save data
        }
        data(array) to struct
        fprintf(include ws2812 struture)
        write to output file
    */

    FILE *input, *output;
	double time_in_us = 0;
	int flag = 0;
	uint64_t data = 0;
	uint8_t event = 127;
	double us_per_tick = 1;
	int ticks_per_qnote = 1;
	char input_filename[FILENAME_SIZE];

    ws2812 part_A[ARRAY_SIZE];
    ws2812 part_B[ARRAY_SIZE];
    ws2812 part_C[ARRAY_SIZE];
    ws2812 part_D[ARRAY_SIZE];
    ws2812 part_E[ARRAY_SIZE];
    ws2812 part_F[ARRAY_SIZE];

    if (argc == 1){
		printf("please specify the input file\n");
		printf("usage : midi2array input (output)\n");
		return 0;
	}

    strncpy(input_filename, argv[1], FILENAME_SIZE);
	input = fopen(input_filename, "r"); /*open the file*/
	output = fopen("out.h", "w"); /*open the file*/

    if (!(ticks_per_qnote = readHeader(&input))) {
        printf("no ticks per qnote\n");
		return 0;
	}

    us_per_tick = 500000.0f / ticks_per_qnote;

    while (1) {
        time_in_us += read_dt(&input, us_per_tick);
        if (readEvent(&input, &data, &event)) break;
        saveData(data, event, time_in_us, &us_per_tick, ticks_per_qnote);
    }
    data2struct(partA, part_A);
    data2struct(partB, part_B);
    data2struct(partC, part_C);
    data2struct(partD, part_D);
    data2struct(partE, part_E);
    data2struct(partF, part_F);

    fprintf(output, "#include \"structure_of_ws2812.h\"\n\n");

    write2file(&output, partA, part_A);
    write2file(&output, partB, part_B);
    write2file(&output, partC, part_C);
    write2file(&output, partD, part_D);
    write2file(&output, partE, part_E);
    write2file(&output, partF, part_F);

    fclose(input);
    fclose(output);

    return 0;
}