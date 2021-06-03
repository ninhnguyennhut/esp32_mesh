#include "mdf_common.h"
#include "mwifi.h"
#include "driver/gpio.h"
#include "mlink.h"

typedef struct {
	unsigned char node;
	unsigned char cmd;
	unsigned char data[32];
} control_t;
