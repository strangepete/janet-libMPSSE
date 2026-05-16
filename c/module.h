#ifndef _MODULE_H_
#define _MODULE_H_
#include <inttypes.h>
#include <janet.h>
#include "../FTDI_LibMPSSE/release/libftd2xx/ftd2xx.h"

extern const char *ft_status[];
extern const char *ft_status_string();
extern void i2c_register(JanetTable*);
extern void spi_register(JanetTable*);
#endif