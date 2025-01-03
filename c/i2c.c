// libMPSSE-I2C functions as documented in Application Note AN_177, Version 1.5

#include "module.h"
#include "../LibMPSSE_1.0.5/release/include/libmpsse_i2c.h"

typedef struct {
    uint32_t        index;          // 1-based, as user-entered
    uint32_t        id;             // unique id per-channel set by libmpsse
    FT_HANDLE       handle;
    ChannelConfig   config;
    uint32_t        read_options;   // these are use per-read/write
    uint32_t        write_options;  //
} channel_t;

static int  channel_get(void *p, Janet key, Janet *out);
static int  channel_gc(void *p, size_t s);
static void channel_string(void *p, JanetBuffer *buffer);

static const JanetAbstractType channel_type = {
    "i2c/channel",
    channel_gc,             // gc
    NULL,                   // gcmark
    channel_get,            // get
    NULL,                   // put
    NULL,                   // marshal
    NULL,                   // unmarshal
    channel_string,         // to-string
    JANET_ATEND_TOSTRING
                            // compare
                            // hash
                            // next
                            // call
                            // length
                            // bytes 
};

/***************/
/* C Functions */
/***************/

// Save the FT return status to dyn :i2c-err, and return the value directly.
static Janet set_status_dyn(FT_STATUS status, Janet value) {
    janet_setdyn("i2c-err", janet_ckeywordv(ft_status_string[status]));
    return value;
}

JANET_FN(cfun_i2c_get_err,
    "(i2c/err)",
    "The return status of the last executed I2C function as a keyword representing an error code. "
    "When called as a method `(:err chan)`, the channel is ignored.\n\n"
    "`FT_STATUS`:\n"
    "* `:ok`\n"
    "* `:invalid-handle`\n"
    "* `:device-not-found`\n"
    "* `:device-not-opened`\n"
    "* `:io-error`\n" // 4
    "* `:insufficient-resources`\n"
    "* `:invalid-parameter`\n"
    "* `:invalid-baud-rate`\n"
    "* `:device-not-opened-for-erase`\n" // 8
    "* `:device-not-opened-for-write`\n"
    "* `:failed-to-write-device`\n"
    "* `:eeprom-read-failed`\n"
    "* `:eeprom-write-failed`\n" // 12
    "* `:eeprom-erase-failed`\n"
    "* `:eeprom-not-present`\n"
    "* `:eeprom-not-programmed`\n"
    "* `:invalid-args`\n" // 16
    "* `:not-supported`\n"
    "* `:other-error`\n"
    "* `:device-list-not-ready`\n\n"
    "Note: currently a wrapper for (dyn :i2c-err)") {
    janet_arity(argc, 0, 1);
    return janet_dyn("i2c-err");
}

JANET_FN(cfun_i2c_channelcount,
    "(i2c/channels)",
    "Get the number of I2C channels that are connected to the host system. "
    "Sets `:err` to return status.\n\n"
    "Note: The number of ports available in each chip is different, but must be an MPSSE chip or cable.\n\n"
    "This function is **not thread-safe**.") {
    janet_fixarity(argc, 0);

    uint32_t chans = 0;
    FT_STATUS status = I2C_GetNumChannels(&chans);
    return set_status_dyn(status, janet_wrap_integer(chans));
}

JANET_FN(cfun_i2c_getchannelinfo,
    "(i2c/info index)",
    "Retrieve detailed information about an I2C channel, "
    "given a 1-based channel `index`, or an `<i2c/channel>` object.\n"
    "Returns `nil` on error. Sets `:err` to return status.\n\n"
    "On success, returns a table:\n"
    "* `:serial`      - Serial number of the device\n"
    "* `:description` - Device description\n"
    "* `:id`          - Unique channel ID\n"
    "* `:locid`       - USB location ID\n"
    "* `:handle`      - Device handle (internal pointer)\n"
    "* `:type`        - Device type\n"
    "* `:flags`       - Device status flags\n\n"
    "This function is **not thread-safe**.") {
    janet_fixarity(argc, 1);

    uint32_t index = 0;
    if (janet_checktype(argv[0], JANET_NUMBER)) {
        index = janet_getuinteger(argv, 0);
        if (index < 1)
            return set_status_dyn(FT_INVALID_HANDLE, janet_wrap_nil());
    } else if (janet_checktype(argv[0], JANET_ABSTRACT)) {
        channel_t *p = (channel_t *)janet_getabstract(argv, 0, &channel_type);
        index = p->index;
    } else
        janet_panicf("invalid type, expected <i2c/channel> or index, got %t", argv[0]);
    
    FT_DEVICE_LIST_INFO_NODE chaninfo;
    FT_STATUS status = I2C_GetChannelInfo(--index, &chaninfo);
    if (status != FT_OK)
        return set_status_dyn(status, janet_wrap_nil());

    JanetKV *out = janet_struct_begin(7);
    janet_struct_put(out, janet_ckeywordv("serial"), janet_cstringv(chaninfo.SerialNumber));
    janet_struct_put(out, janet_ckeywordv("description"), janet_cstringv(chaninfo.Description));
    janet_struct_put(out, janet_ckeywordv("id"), janet_wrap_integer(chaninfo.ID));
    janet_struct_put(out, janet_ckeywordv("locid"), janet_wrap_integer(chaninfo.LocId));
    janet_struct_put(out, janet_ckeywordv("handle"), janet_wrap_pointer(chaninfo.ftHandle));
    janet_struct_put(out, janet_ckeywordv("type"), janet_wrap_integer(chaninfo.Type));
    janet_struct_put(out, janet_ckeywordv("flags"), janet_wrap_integer(chaninfo.Flags));
    
    return set_status_dyn(FT_OK, janet_wrap_struct(janet_struct_end(out)));
}

JANET_FN(cfun_i2c_get_id,
    "(i2c/id channel)",
    "Takes an `<i2c/channel>` and returns the unique, per-channel ID assigned by libMPSSE on channel creation.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    return janet_wrap_integer(c->id);
}

JANET_FN(cfun_i2c_openchannel,
    "(i2c/open index)",
    "Open a channel by (1-based) `index`.\n\n"
    "Returns an `<i2c/channel>` if succesful, or `nil` on error. Sets `:err` to return status.\n\n") {
    janet_fixarity(argc, 1);

    uint32_t index = janet_getuinteger(argv, 0);
    if (index < 1)
        return set_status_dyn(FT_INVALID_HANDLE, janet_wrap_nil());
    
    channel_t *c = (channel_t *)janet_abstract(&channel_type, sizeof(channel_t));
    memset(&c->config, 0x0, sizeof(ChannelConfig));
    c->index = index;

    FT_STATUS status = I2C_OpenChannel((index - 1), &c->handle);
    if (status != FT_OK)
        return set_status_dyn(status, janet_wrap_nil());

    // D2XX USB latency config option; requires d2xx linked
    // FT_SetLatencyTimer(c->handle, 2); // set USB latency from default of 16 ms to 2

    FT_DEVICE_LIST_INFO_NODE chaninfo;
    status = I2C_GetChannelInfo((index - 1), &chaninfo);
    if (status != FT_OK) { // bailout if we fail here
        I2C_CloseChannel(c->handle); // probably will also quietly fail but better to be sure
        c->handle = NULL;
        janet_panicf("failed to get channel info on a newly opened channel: %s", ft_status_string[status]);
    }
    c->id = chaninfo.ID;

    return set_status_dyn(FT_OK, janet_wrap_abstract(c));
}

#define KW_ID 0
#define KW_LOCID 1
#define KW_TYPE 2
#define KW_SERIAL 3
#define KW_DESC 4

JANET_FN(cfun_i2c_find,
    "(i2c/find-by kw value)",
    "Find a channel matching an explicit identifer. Takes a keyword and value:\n"
    "* `:id`    - unique channel ID (integer)\n"
    "* `:locid` - USB location ID (integer)\n"
    "* `:type`  - Device type (integer)\n"
    "* `:description` - (string)\n"
    "* `:serial`    - (string)\n\n"
    "Returns a channel `index` or `nil` on failure. Sets `:err` to return status.") {
    janet_fixarity(argc, 2);

    if (!janet_checktype(argv[0], JANET_KEYWORD))
        janet_panicf("expected keyword but got %t", argv[0]);
    if (janet_checktype(argv[1], JANET_NIL))
        janet_panic("value cannot be nil");

    uint32_t chans = 0;
    FT_STATUS status = I2C_GetNumChannels(&chans);
    if (status != FT_OK)
        return set_status_dyn(status, janet_wrap_nil());
    
    int filter = 0;
    JanetKeyword kw = janet_getkeyword(argv, 0);

    if (strcmp(kw, "id") == 0)
        filter = KW_ID;
    else if (strcmp(kw, "locid") == 0)
        filter = KW_LOCID;
    else if (strcmp(kw, "type") == 0)
        filter = KW_TYPE;
    else if (strcmp(kw, "serial") == 0)
        filter = KW_SERIAL;
    else if (strcmp(kw, "description") == 0)
        filter = KW_DESC;
    else
        janet_panicf("invalid keyword %v", argv[0]);

    JanetString str;
    uint32_t id;
    switch (filter) {
        case KW_ID:
        case KW_LOCID:
        case KW_TYPE:
            if (janet_checktype(argv[1], JANET_NUMBER))
                id = janet_getinteger(argv, 1);
            else
                janet_panicf("expected integer value, got %t", argv[1]);
            break;
        case KW_SERIAL:
        case KW_DESC:
            if (janet_checktype(argv[1], JANET_STRING))
                str = janet_getstring(argv, 1);
            else
                janet_panicf("expected string value, got %t", argv[1]);
    }

    for (int i = 0; i < chans; i++) {
        FT_DEVICE_LIST_INFO_NODE chaninfo;
        status = I2C_GetChannelInfo(i, &chaninfo);
        if (status != FT_OK) {
            return set_status_dyn(status, janet_wrap_nil());
        } else {        
            switch (filter) {
                case KW_ID:
                    if (chaninfo.ID == id)
                        goto found;
                    continue;
                case KW_LOCID:
                    if (chaninfo.LocId == id)
                        goto found;
                    continue;
                case KW_TYPE:
                    if (chaninfo.Type == id)
                        goto found;
                    continue;
                case KW_SERIAL:
                    if (strcmp(chaninfo.SerialNumber, str) == 0)
                        goto found;
                    continue;
                case KW_DESC:
                    if (strcmp(chaninfo.Description, str) == 0)
                        goto found;
                    continue;
            }
            found:
                return set_status_dyn(FT_OK, janet_wrap_integer(i+1));
        }    
    }
    return set_status_dyn(FT_DEVICE_NOT_FOUND, janet_wrap_nil());
}

JANET_FN(cfun_i2c_is_open,
    "(i2c/is-open channel)",
    "Returns true if a channel is open, or false if closed or invalid. Sets `:err` to return status.\n\n"
    "Takes either an `<i2c/channel>` object, or 1-based `index`.") {
    janet_fixarity(argc, 1);

    uint32_t index = 0;
    if (janet_checktype(argv[0], JANET_ABSTRACT)) {
        channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
        if (NULL == c->handle)
            return janet_wrap_boolean(FALSE);
        index = c->index;
    } // fall thru if we have a channel index

    if (janet_checktype(argv[0], JANET_NUMBER) || index > 0) {
        FT_DEVICE_LIST_INFO_NODE chaninfo;
        if (index == 0)
            if ((index = janet_getuinteger(argv, 0)) == 0)
                return janet_wrap_boolean(FALSE);

        FT_STATUS status = I2C_GetChannelInfo((index - 1), &chaninfo);
        if (status != FT_OK)
            return set_status_dyn(status, janet_wrap_boolean(FALSE));

        return set_status_dyn(status, janet_wrap_boolean((chaninfo.Flags & FT_FLAGS_OPENED) ? TRUE : FALSE));
    }
    janet_panicf("invalid type %t, expected <i2c/channel> or index.", argv[0]);
}

#define READ_TRANSFER_OPT 0
#define WRITE_TRANSFER_OPT 1

/* Set option bits from Janet keywords. 'rw' is either READ_ or WRITE_TRANSFER_OPT */
uint32_t transfer_option_keywords(int32_t argc, Janet *argv, int rw) {
    uint32_t options = 0;
    for (int i = 1; i < argc; i++) { // argv[0] is the channel object
        if (janet_checktype(argv[i], JANET_KEYWORD)) {
            JanetKeyword opt = janet_unwrap_keyword(argv[i]);
            if (strcmp(opt, "start") == 0)
                options |= I2C_TRANSFER_OPTIONS_START_BIT;
            else if (strcmp(opt, "stop") == 0)
                options |= I2C_TRANSFER_OPTIONS_STOP_BIT;
            else if ((rw == WRITE_TRANSFER_OPT) && (strcmp(opt, "break-on-nak") == 0))
                options |= I2C_TRANSFER_OPTIONS_BREAK_ON_NACK;
            else if ((rw == READ_TRANSFER_OPT) && (strcmp(opt, "nak-last-byte") == 0))
                options |= I2C_TRANSFER_OPTIONS_NACK_LAST_BYTE;
            else if (strcmp(opt, "fast-transfer-bits") == 0)
                options |= I2C_TRANSFER_OPTIONS_FAST_TRANSFER_BITS;
            else if (strcmp(opt, "fast-transfer-bytes") == 0)
                options |= I2C_TRANSFER_OPTIONS_FAST_TRANSFER_BYTES;
            else if (strcmp(opt, "no-address") == 0)
                options |= I2C_TRANSFER_OPTIONS_NO_ADDRESS;
            else
                janet_panicf("invalid I2C transfer option %p", argv[i]);
        } else
            janet_panicf("invalid I2C transfer option type; expected keyword, got %t", argv[i]);
    }
    return options;
}

JANET_FN(cfun_i2c_set_write_options,
    "(i2c/write-opt channel &opt kw ...)",
    "Set I2C Write transfer options. Takes zero, or more keywords:\n\n"
    "* `:start`\n"
    "* `:stop`\n"
    "* `:break-on-nak`\n"
    "* `:fast-transfer-bytes`\n"
    "* `:fast-transfer-bits`\n"
    "* `:no-address`\n\n") {
    janet_arity(argc, 1, 6);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    c->write_options = transfer_option_keywords(argc, argv, WRITE_TRANSFER_OPT);
    return set_status_dyn(FT_OK, janet_wrap_nil());
}

JANET_FN(cfun_i2c_set_read_options,
    "(i2c/read-opt channel &opt kw ...)",
    "Set I2C Read transfer options. Takes zero, or more keywords:\n\n"
    "* `:start`\n"
    "* `:stop`\n"
    "* `:nak-last-byte`\n"
    "* `:fast-transfer-bytes`\n"
    "* `:fast-transfer-bits`\n"
    "* `:no-address`\n\n") {
    janet_arity(argc, 1, 6);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    c->read_options = transfer_option_keywords(argc, argv, READ_TRANSFER_OPT);
    return set_status_dyn(FT_OK, janet_wrap_nil());
}

#define I2C_ENABLE_DRIVE_ONLY_ZERO 0x0002 // Documented in AN-177, but missing in libmpsse header?

JANET_FN(cfun_i2c_set_config_options,
    "(i2c/config channel &opt kw ...)",
    "Set channel config options. Takes zero, or more keywords:\n\n"
    "* `:disable-3phase-clocking`\n"
    "* `:enable-drive-only-zero`\n\n"
    "Note: 3-phase clocking only available on hi-speed devices, not the FT2232D. "
    "Drive-only-zero is only available on the FT232H.") {
    janet_arity(argc, 1, 3);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);

    uint32_t options = 0;
    for (int i = 1; i < argc; i++) {
        if (janet_checktype(argv[i], JANET_KEYWORD)) {
            JanetKeyword opt = janet_unwrap_keyword(argv[i]);
            if (strcmp(opt, "disable-3phase-clocking") == 0)
                options |= I2C_DISABLE_3PHASE_CLOCKING;
            else if (strcmp(opt, "enable-drive-only") == 0)
                options |= I2C_ENABLE_DRIVE_ONLY_ZERO;
            else
                janet_panicf("invalid I2C config option %p, in slot #%d", argv[i], i+1);
        } else
            janet_panicf("invalid I2C config option type, expected keyword but got %t in slot #%d", argv[i], i+1);
    }
    c->config.Options = options;

    return set_status_dyn(FT_OK, janet_wrap_nil());
}

JANET_FN(cfun_i2c_initchannel,
    "(i2c/init channel &opt clockrate latency)",
    "Initialize an open `channel` with optional `clockrate` and `latency`. "
    "Returns `true` if successful, or `false` on error. Sets :err to return status.\n\n"
    "Clock rate is one of the following keywords:\n\n"
    "* `:standard`   - 100kb/s (default)\n"
    "* `:fast`       - 400kb/s\n"
    "* `:fast-plus`  - 1000kb/s\n"
    "* `:high-speed` - 3.4Mb/s\n"
    "* or a non-standard clock rate integer from 0 to 3,400,000.\n\n"
    "Note: Recommended latency of Full-speed devices (FT2232D) is 2 to 255, "
    "and Hi-speed devices (FT232H, FT2232H, FT4232H) is 1 to 255. Default is 255.") {
    janet_arity(argc, 1, 3);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    
    I2C_CLOCKRATE rate = I2C_CLOCK_STANDARD_MODE;
    if (janet_checktype(argv[1], JANET_KEYWORD)) {
        JanetKeyword clock = janet_optkeyword(argv, argc, 1, janet_cstring("standard"));
        if (strcmp(clock, "fast") == 0)
            rate = I2C_CLOCK_FAST_MODE;
        else if (strcmp(clock, "fast-plus") == 0)
            rate = I2C_CLOCK_FAST_MODE_PLUS;
        else if (strcmp(clock, "high-speed") == 0)
            rate = I2C_CLOCK_HIGH_SPEED_MODE;
    } else if (janet_checktype(argv[1], JANET_NUMBER)) {
        rate = janet_getuinteger(argv, 2);
        if (rate > 3400000)
            janet_panicf("clock rate %d is out of range. Expected 0 to 3,400,000");
    }
    c->config.ClockRate = rate;

    uint8_t latency = janet_optinteger(argv, argc, 2, 255);
    if (latency < 1 || latency > 255)
        janet_panicf("latency %d out of range. expected 1 to 255", latency);
    c->config.LatencyTimer = latency;

    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_boolean(FALSE));

    FT_STATUS status = I2C_InitChannel(c->handle, &c->config);
    return set_status_dyn(status, janet_wrap_boolean(status == FT_OK? TRUE : FALSE));
}

JANET_FN(cfun_i2c_closechannel,
    "(i2c/close channel)",
    "Closes the specified channel. "
    "Returns `true` if successful. Sets `:err` to return status.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);   
    if ((NULL == c) || (NULL == c->handle))
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_boolean(FALSE));
    
    FT_STATUS status = I2C_CloseChannel(c->handle);
    c->handle = NULL;

    return set_status_dyn(status, janet_wrap_boolean(status == FT_OK? TRUE : FALSE));
}

JANET_FN(cfun_ft_gpio_write,
    "(i2c/gpio-write channel dir value)",
    "Write to GPIO lines, where `direction` and `value` are an 8-bit value mapping each line. "
    "Direction bit 0 for in, and 1 for out. Value is 0 logic low, 1 logic high.\n\n"
    "Returns `nil`. Sets `:err` to return status.\n\n"
    "Note: libMPSSE cannot use the lower gpio port pins 0-7, such as those exposed in "
    "FTDI cable assemblies. Setting bit-6 corresponds to the onboard red LED in some cables.") {
    janet_fixarity(argc, 3);

    uint8_t dir = janet_getinteger(argv, 1);
    uint8_t value = janet_getinteger(argv, 2);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());

    FT_STATUS status = FT_WriteGPIO(c->handle, dir, value);
    return set_status_dyn(status, janet_wrap_nil());
}

JANET_FN(cfun_ft_gpio_read, 
    "(i2c/gpio-read channel)", 
    "Read the 8 GPIO lines from the high byte of the MPSSE channel.\n\n"
    "Returns an unsigned 8-bit integer, or `nil` on error. Sets `:err` to return status.\n\n"
    "Note: **Must call write-gpio to initialize before reading**. See the libMPSSE.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());

    uint8_t value = 0;
    FT_STATUS status = FT_ReadGPIO(c->handle, &value);
    return set_status_dyn(status, janet_wrap_integer(value));
}

JANET_FN(cfun_i2c_deviceread,
    "(i2c/read channel address size buffer)",
    "Read & append `size` n-bytes to `buffer` from I2C device at `address`.\n\n"
    "Returns bytes read. Sets `:err` to return status.\n\n"
    "This is a **blocking function**.") {
    janet_fixarity(argc, 4);

    uint32_t address = janet_getuinteger(argv, 1);
    if (address > 127)
        janet_panicf("i2c address %d is out of range. Expected 7-bit address <= 127.", address);
        
    uint32_t size = janet_getuinteger(argv, 2);
    if (size < 1)
        janet_panic("read size must be greater than 0");

    JanetBuffer *buffer = janet_getbuffer(argv, 3);
    janet_buffer_extra(buffer, size);
    
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_integer(0));

    uint32_t readsz = 0;
    FT_STATUS status = I2C_DeviceRead(c->handle,
                                      address,
                                      size, 
                                      (buffer->data + buffer->count), 
                                      &readsz, 
                                      c->read_options);
    if (readsz > 0)
        buffer->count += readsz;

    return set_status_dyn(status, janet_wrap_integer(readsz));
}

JANET_FN(cfun_i2c_devicewrite,
    "(i2c/write channel address size buffer)",
    "Write `size` n-bytes of `buffer` to I2C channel/device `address`.\n\n"
    "Returns bytes written. Sets `:err` to return status.\n\n"
    "This is a **blocking function**.") {
    janet_fixarity(argc, 4);

    uint32_t address = janet_getinteger(argv, 1);
    if (address > 127)
        janet_panicf("i2c address %d is out of range. Expected <= 127.", address);

    uint32_t size = janet_getinteger(argv, 2);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);    
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_integer(0));
    
    FT_STATUS status;
    uint32_t writesz = 0;
    uint8_t *buf = NULL;
    if (janet_checktype(argv[3], JANET_NUMBER)) {
        if (size > 1)
            janet_panicf("expected size == 1 when passed an integer, got %d", size);
        uint8_t b = (uint8_t)janet_getuinteger(argv, 3);
        buf = &b;
    } else {
        JanetBuffer *buffer = janet_getbuffer(argv, 3);
        if (size > buffer->count)
            janet_panicf("write size %d larger than buffer count %d", size, buffer->count);
        buf = buffer->data;
    }
    status = I2C_DeviceWrite(c->handle, 
                             address,
                             size, 
                             buf, 
                             &writesz, 
                             c->write_options);
    return set_status_dyn(status, janet_wrap_integer(writesz));
}

static JanetMethod channel_methods[] = {
    {"err",             cfun_i2c_get_err},
    {"info",            cfun_i2c_getchannelinfo},
    {"id",              cfun_i2c_get_id},
    {"is-open",         cfun_i2c_is_open},
    {"close",           cfun_i2c_closechannel},
    {"init",            cfun_i2c_initchannel},
    {"read",            cfun_i2c_deviceread},
    {"write",           cfun_i2c_devicewrite},
    {"read-opt",        cfun_i2c_set_read_options},
    {"write-opt",       cfun_i2c_set_write_options},
    {"config",          cfun_i2c_set_config_options}
};

static int channel_get(void *p, Janet key, Janet *out) {
    (void) p;
    if (!janet_checktype(key, JANET_KEYWORD))
        janet_panicf("expected keyword, but got %t", key);
    return janet_getmethod(janet_unwrap_keyword(key), channel_methods, out);
}

static int channel_gc(void *p, size_t s) {
    (void) s;
    channel_t *c = (channel_t *)p;
    FT_STATUS status = FT_DEVICE_NOT_OPENED;
    if (c != NULL) {
        if (c->handle != NULL) {
            status = I2C_CloseChannel(c->handle);
            c->handle = NULL;
        }
    }
    set_status_dyn(status, janet_wrap_nil());
    return 0;
}

static void channel_string(void *p, JanetBuffer *buffer) {
    channel_t *c = (channel_t *)p;
    janet_formatb(buffer, "#%d 0x%X", c->index, c);
}

void i2c_register(JanetTable *env) {
    JanetRegExt cfuns[] = {
        JANET_REG("i2c/err",            cfun_i2c_get_err),
        JANET_REG("i2c/channels",       cfun_i2c_channelcount),
        JANET_REG("i2c/info",           cfun_i2c_getchannelinfo),
        JANET_REG("i2c/find-by",        cfun_i2c_find),
        JANET_REG("i2c/id",             cfun_i2c_get_id),
        JANET_REG("i2c/read-opt",       cfun_i2c_set_read_options),
        JANET_REG("i2c/write-opt",      cfun_i2c_set_write_options),
        JANET_REG("i2c/config",         cfun_i2c_set_config_options),
        JANET_REG("i2c/open",           cfun_i2c_openchannel),
        JANET_REG("i2c/is-open",        cfun_i2c_is_open),
        JANET_REG("i2c/init",           cfun_i2c_initchannel),
        JANET_REG("i2c/close",          cfun_i2c_closechannel),
        JANET_REG("i2c/read",           cfun_i2c_deviceread),
        JANET_REG("i2c/write",          cfun_i2c_devicewrite),
        JANET_REG("i2c/gpio-read",      cfun_ft_gpio_read),
        JANET_REG("i2c/gpio-write",     cfun_ft_gpio_write),
        JANET_REG_END
    };
    janet_cfuns_ext(env, "i2c", cfuns);
}