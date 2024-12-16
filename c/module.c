#include "module.h"

const char *ft_status_string[] = { // FT_[error]
    "ok",
    "invalid-handle",
    "device-not-found",
    "device-not-opened",
    "io-error", // 4
    "insufficient-resources",
    "invalid-parameter",
    "invalid-baud-rate",
    "device-not-opened-for-erase", // 8
    "device-not-opened-for-write",
    "failed-to-write-device",
    "eeprom-read-failed",
    "eeprom-write-failed", // 12
    "eeprom-erase-failed",
    "eeprom-not-present",
    "eeprom-not-programmed",
    "invalid-args", // 16
    "not-supported",
    "other-error",
    "device-list-not-ready",
};

/****************/
/* Module Entry */
/****************/

JANET_MODULE_ENTRY(JanetTable *env) {
    /* The I2C and SPI modules are nearly identical, but their headers
        have conflicting types (ChannelConfig) and have to be separated, yay
        It does make it easier to add other modules in the future (ie, JTAG) */
    spi_register(env);
    i2c_register(env);

#ifdef _MSC_VER
    Init_libMPSSE();
#endif
}