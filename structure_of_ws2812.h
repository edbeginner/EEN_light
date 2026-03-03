#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

// * ws2812 struture

typedef struct {
	union {
		struct {
			uint32_t time;
			uint8_t red, green, blue;
		} light;
		struct {
			uint32_t time;
			uint8_t red, green, blue;
			uint8_t SPX_type;
			uint16_t SPX_duration; // use ms ?
		} strip;
	};
} ws2812;

#endif