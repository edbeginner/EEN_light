#include "new_midi2array.h"
#include "structure_of_ws2812.h"
#include "new_config.h"
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *input, *output;
	float time_in_us = 0;
	uint64_t data = 0;
	uint8_t event = 127;
	float us_per_tick = 1;
	int ticks_per_qnote = 1;
	char input_filename[FILENAME_SIZE];
    char output_filename[FILENAME_SIZE + 3];

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

    strncpy(input_filename, argv[1], FILENAME_SIZE - 1);
    input_filename[FILENAME_SIZE - 1] = '\0';

    strncpy(output_filename, input_filename, sizeof(output_filename) - 1);
    output_filename[sizeof(output_filename) - 1] = '\0';

    char *dot = strrchr(output_filename, '.');

    if (dot != NULL) {
        strcpy(dot, ".h");
    } else {
        strncat(output_filename, ".h", sizeof(output_filename) - strlen(output_filename) - 1);
    }

    input = fopen(input_filename, "r"); 
    output = fopen(output_filename, "w");

    if (input == NULL) {
        printf("failed to open input file: %s\n", input_filename);
        if (output != NULL) {
            fclose(output);
        }
        return 1;
    }

    if (output == NULL) {
        printf("failed to open output file: %s\n", output_filename);
        fclose(input);
        return 1;
    }

    if (!(ticks_per_qnote = readHeader(&input))) {
        printf("no ticks per qnote\n");
		return 0;
	}

    us_per_tick = 500000.0f / ticks_per_qnote;

    while (1) { // there is an infinite loop inside this one
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

    write2file(&output, 'A', part_A);
    write2file(&output, 'B', part_B);
    write2file(&output, 'C', part_C);
    write2file(&output, 'D', part_D);
    write2file(&output, 'E', part_E);
    write2file(&output, 'F', part_F);

    fclose(input);
    fclose(output);

    return 0;
}