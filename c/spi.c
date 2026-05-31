// libMPSSE-SPI functions as documented in Application Note AN_178, Version 1.2

#include "module.h"
#include "../FTDI_LibMPSSE/release/include/libmpsse_spi.h"


typedef struct {
    BOOL            is_initialized;
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
    "spi/channel",
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

// Save the FT return status to dyn :ft-err, and return the value directly.
static Janet set_status_dyn(FT_STATUS status, Janet value) {
    janet_setdyn("ft-err", janet_ckeywordv(ft_status_string(status)));
    return value;
}

JANET_FN(cfun_spi_get_err,
    "(spi/err)",
    "The return status of the last executed SPI function as a keyword representing an error code. "
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
    "Note: currently a wrapper for (dyn :ft-err)") {
    janet_arity(argc, 0, 1);
    return janet_dyn("ft-err");
}

JANET_FN(cfun_spi_channelcount,
    "(spi/channels)",
    "Get the number of SPI channels that are connected to the host system. "
    "Sets `:err` to return status.\n\n"
    "Note: The number of ports available in each chip is different, but must be an MPSSE chip or cable.\n\n"
    "This function is **not thread-safe**.") {
    janet_fixarity(argc, 0);

    uint32_t chans = 0;
    FT_STATUS status = SPI_GetNumChannels(&chans);
    return set_status_dyn(status, janet_wrap_integer(chans));
}

JANET_FN(cfun_spi_getchannelinfo,
    "(spi/info index)",
    "Retrieve detailed information about an SPI channel, "
    "given a 1-based channel `index`, or an `<spi/channel>` object.\n"
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
        janet_panicf("invalid type, expected <spi/channel> or index, got %t", argv[0]);
    
    FT_DEVICE_LIST_INFO_NODE chaninfo;
    FT_STATUS status = SPI_GetChannelInfo(--index, &chaninfo);
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

JANET_FN(cfun_spi_get_id,
    "(spi/id channel)",
    "Takes an `<spi/channel>` and returns the unique, per-channel ID assigned by libMPSSE on channel creation.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    return janet_wrap_integer(c->id);
}

JANET_FN(cfun_spi_openchannel,
    "(spi/open index)",
    "Open a channel by (1-based) `index`.\n\n"
    "Returns an `<spi/channel>` if succesful, or `nil` on error. Sets `:err` to return status.\n\n") {
    janet_fixarity(argc, 1);

    uint32_t index = janet_getuinteger(argv, 0);
    if (index < 1)
        return set_status_dyn(FT_INVALID_HANDLE, janet_wrap_nil());
    
    channel_t *c = (channel_t *)janet_abstract(&channel_type, sizeof(channel_t));
    memset(c, 0x0, sizeof(channel_t));
    
    c->is_initialized = FALSE;
    c->index = index;
    FT_STATUS status = SPI_OpenChannel((index - 1), &c->handle);
    if (status != FT_OK)
        return set_status_dyn(status, janet_wrap_nil());

    FT_DEVICE_LIST_INFO_NODE chaninfo;
    status = SPI_GetChannelInfo((index - 1), &chaninfo);
    if (status != FT_OK) { // bailout if we fail here
        SPI_CloseChannel(c->handle); // probably will also quietly fail but better to be sure
        c->handle = NULL;
        janet_panicf("failed to get channel info on a newly opened channel: %s", ft_status_string(status));
    }
    c->id = chaninfo.ID;

    return set_status_dyn(FT_OK, janet_wrap_abstract(c));
}

#define KW_ID 0
#define KW_LOCID 1
#define KW_TYPE 2
#define KW_SERIAL 3
#define KW_DESC 4

JANET_FN(cfun_spi_find,
    "(spi/find-by kw value)",
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
    FT_STATUS status = SPI_GetNumChannels(&chans);
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
                id = janet_getuinteger(argv, 1);
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
        status = SPI_GetChannelInfo(i, &chaninfo);
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

JANET_FN(cfun_spi_is_open,
    "(spi/is-open channel)",
    "Returns true if a channel is open, or false if closed or invalid. Sets `:err` to return status.\n\n"
    "Takes either an `<spi/channel>` object, or 1-based `index`.") {
    janet_fixarity(argc, 1);

    uint32_t index = 0;
    if (janet_checktype(argv[0], JANET_ABSTRACT)) {
        channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
        if (NULL == c->handle)
            return set_status_dyn(FT_INVALID_HANDLE, janet_wrap_boolean(FALSE));
        index = c->index;
    } // fall thru if we have a channel index
    
    if (janet_checktype(argv[0], JANET_NUMBER) || index > 0) {
        FT_DEVICE_LIST_INFO_NODE chaninfo;
        if (index == 0)
            if ((index = janet_getuinteger(argv, 0)) == 0)
                return set_status_dyn(FT_INVALID_HANDLE, janet_wrap_boolean(FALSE));

        FT_STATUS status = SPI_GetChannelInfo((index - 1), &chaninfo);
        if (status != FT_OK)
            return set_status_dyn(status, janet_wrap_boolean(FALSE));

        return set_status_dyn(status, janet_wrap_boolean(chaninfo.Flags & FT_FLAGS_OPENED));
    }
    janet_panicf("invalid type %t, expected <spi/channel> or index.", argv[0]);
}

uint32_t spi_transfer_option_keywords(int32_t argc, Janet *argv) {
    uint32_t options = 0;
    for (int i = 1; i < argc; i++) { // argv[0] is the channel object
        if (janet_checktype(argv[i], JANET_KEYWORD)) {
            JanetKeyword opt = janet_unwrap_keyword(argv[i]);
            if (strcmp(opt, "size-in-bits") == 0)
                options |= SPI_TRANSFER_OPTIONS_SIZE_IN_BITS;
            else if (strcmp(opt, "cs") == 0)
                options |= SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE;
            else
                janet_panicf("invalid SPI transfer option %p", argv[i]);
        } else
            janet_panicf("invalid SPI transfer option type; expected keyword, got %t", argv[i]);
    }
    return options;
}

JANET_FN(cfun_spi_set_write_options,
    "(spi/write-opt channel &opt kw ...)",
    "Set SPI Write transfer options. Takes zero, or more keywords:\n\n"
    "* `:size-in-bits`      - Transfer size in bits (default is bytes)\n"
    "* `:cs`                - Chip-select line asserted before beginning transfer\n\n") {
    janet_arity(argc, 1, 3);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    c->write_options = spi_transfer_option_keywords(argc, argv);
    return set_status_dyn(FT_OK, janet_wrap_nil());
}

JANET_FN(cfun_spi_set_read_options,
    "(spi/read-opt channel &opt kw ...)",
    "Set SPI Read transfer options. Takes zero, or more keywords:\n\n"
    "* `:size-in-bits`      - Transfer size in bits (default is bytes)\n"
    "* `:cs`                - Chip-select line asserted before beginning transfer\n\n") {
    janet_arity(argc, 1, 3);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    c->read_options = spi_transfer_option_keywords(argc, argv);
    return set_status_dyn(FT_OK, janet_wrap_nil());
}

JANET_FN(cfun_spi_set_config_options,
    "(spi/config channel &opt kw ...)",
    "Set channel config options. Takes one or more keywords:\n\n"
    "* `:mode0`             - CPOL=0 CPHA=0 (default)\n"
    "* `:mode1`             - CPOL=0 CPHA=1\n"
    "* `:mode2`             - CPOL=1 CPHA=0\n"
    "* `:mode3`             - CPOL=1 CPHA=1\n"
    "* `:bus_`              - Use chip select bus line `:bus3` to `7` (default :bus3)\n"
    "* `:active-low`        - Set chip select line to Active Low\n"
    "* `:active-high`       - Set chip select line to Active High (default)\n\n"
    "Passing `nil` will reset to default; passing only the channel returns the config value\n\n"
    "Note: \n"
    "* Bus corresponds to lines ADBUS0 - ADBUS7 if the first MPSSE channel "
    "is used, otherwise it corresponds to lines BDBUS0 - BDBUS7 if the second MPSSE" 
    "channel (i.e., if available in the chip) is used.\n"
    "* FT2xxH/FT2232D only support Modes 0 & 2") {
    janet_arity(argc, 1, 4);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    uint32_t options = c->config.configOptions;
    
    if (argc == 1) 
        return set_status_dyn(FT_OK, janet_wrap_integer(options));        /* (spi/config chan) -> raw config value */
    if (argc == 2 && janet_checktype(argv[1], JANET_NIL))
        options = 0x00;                                     /* (spi/config chan nil) resets to defaults (mode0, bus3, active-high) */
    else {
        for (int i = 1; i < argc; i++) {
            if (janet_checktype(argv[i], JANET_KEYWORD)) {
                JanetKeyword opt = janet_unwrap_keyword(argv[i]);
                if (strcmp(opt, "mode0") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_MODE_MASK) | SPI_CONFIG_OPTION_MODE0;
                else if (strcmp(opt, "mode1") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_MODE_MASK) | SPI_CONFIG_OPTION_MODE1;
                else if (strcmp(opt, "mode2") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_MODE_MASK) | SPI_CONFIG_OPTION_MODE2;
                else if (strcmp(opt, "mode3") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_MODE_MASK) | SPI_CONFIG_OPTION_MODE3;
                else if (strcmp(opt, "bus3") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_CS_MASK) | SPI_CONFIG_OPTION_CS_DBUS3;
                else if (strcmp(opt, "bus4") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_CS_MASK) | SPI_CONFIG_OPTION_CS_DBUS4;
                else if (strcmp(opt, "bus5") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_CS_MASK) | SPI_CONFIG_OPTION_CS_DBUS5;
                else if (strcmp(opt, "bus6") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_CS_MASK) | SPI_CONFIG_OPTION_CS_DBUS6;
                else if (strcmp(opt, "bus7") == 0)
                    options = (options & ~SPI_CONFIG_OPTION_CS_MASK) | SPI_CONFIG_OPTION_CS_DBUS7;
                else if (strcmp(opt, "active-low") == 0)
                    options |= SPI_CONFIG_OPTION_CS_ACTIVELOW;
                else if (strcmp(opt, "active-high") == 0)
                    options &= ~SPI_CONFIG_OPTION_CS_ACTIVELOW; // clear flag, ie _ACTIVEHIGH
                else
                    janet_panicf("invalid SPI config option %p, in slot #%d", argv[i], i+1);
            } else
                janet_panicf("invalid SPI config option type, expected keyword but got %t in slot #%d", argv[i], i+1);
        }
    }
    c->config.configOptions = options;

    FT_STATUS status = FT_OK;
    if (c->is_initialized == TRUE && c->handle)
        status = SPI_ChangeCS(c->handle, c->config.configOptions);

    return set_status_dyn(status, janet_wrap_nil());
}

JANET_FN(cfun_spi_togglecs,
    "(spi/toggle-cs channel bool)",
    "Toggles the current chip select line on or off") {
    janet_fixarity(argc, 2);
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());
    
    BOOL value = janet_getboolean(argv, 1);
    FT_STATUS status = SPI_ToggleCS(c->handle, value);
    return set_status_dyn(status, janet_wrap_nil());
}

JANET_FN(cfun_spi_pins,
    "(spi/pins channel [init] [close])",
    "Set the direction and values of the current channel on initialization or close.\n"
    "`init`, `close` are tuples of 8-bit `[direction value]` bytes.\n\n"
    "* direction:   output = 1, input = 0\n"
    "* value:       logic high = 1, low = 0\n\n"
    "Returns the computed 32-bit option value for testing.") {
    janet_fixarity(argc, 3);
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());
    
    JanetTuple init = janet_gettuple(argv, 1);
    JanetTuple close = janet_gettuple(argv, 2);
    if (janet_tuple_length(init) != 2 || janet_tuple_length(close) != 2)
        janet_panic("invalid number of tuple arguments, expected 2 each");

    uint32_t init_dir = janet_getuinteger(init, 0);
    if (init_dir > 255) janet_panicf("init direction too large, expected < 255 but got %d", init_dir);
    uint32_t init_val = janet_getuinteger(init, 1);
    if (init_val > 255) janet_panicf("init value too large, expected < 255 but got %d", init_val);
    uint32_t close_dir = janet_getuinteger(close, 0);
    if (close_dir > 255) janet_panicf("close direction too large, expected < 255 but got %d", close_dir);
    uint32_t close_val = janet_getuinteger(close, 1);
    if (close_val > 255) janet_panicf("close value too large, expected < 255 but got %d", close_val);

    uint32_t pins = (init_dir & 0xFF)
                  | (init_val & 0xFF) << 8
                  | (close_dir & 0xFF) << 16
                  | (close_val & 0xFF) << 24;
    c->config.Pin = pins;
    return set_status_dyn(FT_OK, janet_wrap_number(pins));
}

JANET_FN(cfun_spi_initchannel,
    "(spi/init channel clockrate &opt latency)",
    "Initialize an opened `channel`, `clockrate` and optional `latency`. "
    "Returns `true` if successful, or `false` on error. Sets :err to return status.\n\n"
    "* clockrate   - 0 to 30,000,000 Hz\n"
    "* latency     - 0 to 255 (default)\n\n"
    "Note: Recommended latency of Full-speed devices (FT2232D) is 2 to 255, "
    "and Hi-speed devices (FT232H, FT2232H, FT4232H) is 1 to 255. Default is 255.") {
    janet_arity(argc, 2, 3);
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_boolean(FALSE));

    uint32_t clock = janet_getuinteger(argv, 1);
        if (clock > 30000000)
            janet_panicf("clockrate %d is out of range. Expected 0 to 30,000,000 Hz", clock);
    c->config.ClockRate = clock;

    uint32_t latency = janet_optuinteger(argv, argc, 2, 255);
    if (latency > 255)
        janet_panicf("latency %d out of range. expected 0 to 255", latency);
    c->config.LatencyTimer = (uint8_t)latency;

    FT_STATUS status = SPI_InitChannel(c->handle, &c->config);
    if (status == FT_OK)
        c->is_initialized = TRUE;
    return set_status_dyn(status, janet_wrap_boolean(status == FT_OK));
}

JANET_FN(cfun_spi_closechannel,
    "(spi/close channel)",
    "Closes the specified channel. "
    "Returns `true` if successful. Sets `:err` to return status.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);   
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_boolean(FALSE));
    
    FT_STATUS status = SPI_CloseChannel(c->handle);
    c->handle = NULL;
    c->is_initialized = FALSE;

    return set_status_dyn(status, janet_wrap_boolean(status == FT_OK));
}

JANET_FN(cfun_spi_deviceread,
    "(spi/read channel buffer size)",
    "Read & append `size` n-bytes to `buffer`\n\n"
    "Returns bytes read. Sets `:err` to return status.\n\n"
    "This is a **blocking function**.") {
    janet_fixarity(argc, 3);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_integer(0));
    
    uint32_t size = janet_getuinteger(argv, 2);
    if (size < 1)
        janet_panic("read size must be greater than 0");

    JanetBuffer *buffer = janet_getbuffer(argv, 1);
    janet_buffer_extra(buffer, size);

    uint32_t readsz = 0;
    FT_STATUS status = SPI_Read(c->handle,
                                (buffer->data + buffer->count), 
                                size,
                                &readsz, 
                                c->read_options);
    if (readsz > 0)
        buffer->count += readsz;

    return set_status_dyn(status, janet_wrap_integer(readsz));
}

JANET_FN(cfun_spi_devicewrite,
    "(spi/write channel buffer &opt size)",
    "Write optional `size` n-bytes of `buffer`\n\n"
    "Returns bytes written. Sets `:err` to return status.\n\n"
    "This is a **blocking function**.") {
    janet_arity(argc, 2, 3);
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);    
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_integer(0));
    
    uint32_t size = 0;
    if (argc == 3) { // optional size
        size = janet_getuinteger(argv, 2);
        if (size == 0)
            janet_panicf("buffer size out of range. Expected > 0");
    }

    FT_STATUS status;
    uint32_t writesz = 0;
    uint8_t *buf = NULL;
    uint8_t b8;
    if (janet_checktype(argv[1], JANET_NUMBER)) {
        size = 1;
        uint32_t b = janet_getuinteger(argv, 1);
        if (b > 255)
            janet_panicf("invalid integer length, expected value <= 255, got %d", b);
        b8 = (uint8_t)b;
        buf = &b8;
    } else {
        if (janet_checktype(argv[1], JANET_STRING)) {
            JanetString s = janet_getstring(argv, 1);
            size_t len = janet_string_length(s);
            if (size == 0)
                size = len;
            else if (size > len)
                janet_panicf("write size %d larger than string size %d", size, len);
            buf = (uint8_t *)s;
        } else {
            JanetBuffer *buffer = janet_getbuffer(argv, 1);
            if (size == 0)
                size = buffer->count;
            else if (size > buffer->count)
                janet_panicf("write size %d larger than buffer size %d", size, buffer->count);
            buf = buffer->data;
        }
    }
    status = SPI_Write(c->handle,
                        buf, 
                        size,     
                        &writesz, 
                        c->write_options);
    return set_status_dyn(status, janet_wrap_integer(writesz));
}

JANET_FN(cfun_spi_readwrite,
    "(spi/readwrite channel sendbuf size recvbuf)",
    "Simultaneously read & write `size` n-bytes to `channel`.\n\n"
    "Returns bytes transfered. Sets `:err` to return status.\n\n"
    "Note: Uses the `write-opt` transfer option for both operations.\n\n"
    "This is a **blocking function**.") {
    janet_fixarity(argc, 4);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_integer(0));

    uint32_t size = janet_getuinteger(argv, 2);
    if (size == 0)
        janet_panicf("buffer size %d is out of range. Expected > 0", size);

    JanetBuffer *sendbuf = janet_getbuffer(argv, 1);
    if (size > sendbuf->count)
        janet_panicf("write size %d larger than sendbuf size %d", size, sendbuf->count);

    JanetBuffer *recvbuf = janet_getbuffer(argv, 3);
    janet_buffer_extra(recvbuf, size);

    uint32_t transfer_sz = 0;
    FT_STATUS status = SPI_ReadWrite(c->handle,
                                    (recvbuf->data + recvbuf->count),
                                    sendbuf->data,
                                    size, 
                                    &transfer_sz,
                                    c->write_options);
    if (transfer_sz > 0)
        recvbuf->count += transfer_sz;
    return set_status_dyn(status, janet_wrap_integer(transfer_sz));
}

JANET_FN(cfun_spi_is_busy,
    "(spi/is-busy channel)",
    "Reads the state of the MISO line without clocking the SPI bus.\n\n"
    "Returns boolean state. Sets `:err` to return status.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_boolean(FALSE));
    
    BOOL state = FALSE;
    FT_STATUS status = SPI_IsBusy(c->handle, &state);
    return set_status_dyn(status, janet_wrap_boolean(state));
}

JANET_FN(cfun_spi_gpio_write,
    "(spi/gpio-write channel dir value)",
    "Write to GPIO lines, where `direction` and `value` are an 8-bit value mapping each line. "
    "Direction bit 0 for in, and 1 for out. Value is 0 logic low, 1 logic high.\n\n"
    "Returns `nil`. Sets `:err` to return status.\n\n"
    "Note: libMPSSE cannot use the lower gpio port pins 0-7, such as those exposed in "
    "FTDI cable assemblies. Setting bit-6 corresponds to the onboard red LED in some cables.") {
    janet_fixarity(argc, 3);

    uint32_t dir = janet_getuinteger(argv, 1);
    uint32_t value = janet_getuinteger(argv, 2);
    if (dir > 255)
        janet_panic("value must be <= 255, in slot #1");
    if (value > 255)
        janet_panic("value must be <= 255, in slot #2");

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());

    FT_STATUS status = FT_WriteGPIO(c->handle, (uint8_t)dir, (uint8_t)value);
    return set_status_dyn(status, janet_wrap_nil());
}

JANET_FN(cfun_spi_gpio_read, 
    "(spi/gpio-read channel)", 
    "Read the 8 GPIO lines from the high byte of the MPSSE channel.\n\n"
    "Returns an unsigned 8-bit integer, or `nil` on error. Sets `:err` to return status.\n\n"
    "Note: **Must call write-gpio to initialize before reading**. See the libMPSSE AN-178.") {
    janet_fixarity(argc, 1);

    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());

    uint8_t value = 0;
    FT_STATUS status = FT_ReadGPIO(c->handle, &value);
    return set_status_dyn(status, janet_wrap_integer(value));
}

JANET_FN(cfun_spi_loopback,
    "(spi/loopback channel bool)",
    "Enables the `channel`s internal loopback. Returns `nil`.") {
    janet_fixarity(argc, 2);
    channel_t *c = (channel_t *)janet_getabstract(argv, 0, &channel_type);
    if (NULL == c->handle)
        return set_status_dyn(FT_DEVICE_NOT_OPENED, janet_wrap_nil());

    BOOL enable = janet_getboolean(argv, 1);
    FT_STATUS status;
    status = Mid_SetDeviceLoopbackState(c->handle, enable);
    if (status != FT_OK)
        janet_panicf("failed to set device loopback: %s", ft_status_string(status));
    return set_status_dyn(FT_OK, janet_wrap_nil());
}

static JanetMethod channel_methods[] = {
    {"err",             cfun_spi_get_err},
    {"info",            cfun_spi_getchannelinfo},
    {"id",              cfun_spi_get_id},
    {"is-open",         cfun_spi_is_open},
    {"is-busy",         cfun_spi_is_busy},
    {"close",           cfun_spi_closechannel},
    {"init",            cfun_spi_initchannel},
    {"read",            cfun_spi_deviceread},
    {"write",           cfun_spi_devicewrite},
    {"readwrite",       cfun_spi_readwrite},
    {"read-opt",        cfun_spi_set_read_options},
    {"write-opt",       cfun_spi_set_write_options},
    {"config",          cfun_spi_set_config_options},
    {"toggle-cs",       cfun_spi_togglecs},
    {"pins",            cfun_spi_pins},
    {NULL, NULL}
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
    if (c->handle != NULL) {
        SPI_CloseChannel(c->handle);
        c->handle = NULL;
    }
    return 0;
}

static void channel_string(void *p, JanetBuffer *buffer) {
    channel_t *c = (channel_t *)p;
    char buf[16 + 6 + 1] = {0}; /* 16 hex + 6 (#99_0x) + null */
    snprintf(buf, sizeof(buf), "#%d 0x%" PRIXPTR, c->index, (uintptr_t)c);
    size_t len = strnlen(buf, sizeof(buf));
    janet_buffer_push_bytes(buffer, buf, len);
}

void spi_register(JanetTable *env) {
    JanetRegExt cfuns[] = {
        JANET_REG("spi/err",            cfun_spi_get_err),
        JANET_REG("spi/channels",       cfun_spi_channelcount),
        JANET_REG("spi/info",           cfun_spi_getchannelinfo),
        JANET_REG("spi/find-by",        cfun_spi_find),
        JANET_REG("spi/id",             cfun_spi_get_id),
        JANET_REG("spi/read-opt",       cfun_spi_set_read_options),
        JANET_REG("spi/write-opt",      cfun_spi_set_write_options),
        JANET_REG("spi/config",         cfun_spi_set_config_options),
        JANET_REG("spi/toggle-cs",      cfun_spi_togglecs),
        JANET_REG("spi/open",           cfun_spi_openchannel),
        JANET_REG("spi/is-open",        cfun_spi_is_open),
        JANET_REG("spi/is-busy",        cfun_spi_is_busy),
        JANET_REG("spi/init",           cfun_spi_initchannel),
        JANET_REG("spi/close",          cfun_spi_closechannel),
        JANET_REG("spi/read",           cfun_spi_deviceread),
        JANET_REG("spi/write",          cfun_spi_devicewrite),
        JANET_REG("spi/readwrite",      cfun_spi_readwrite),
        JANET_REG("spi/gpio-read",      cfun_spi_gpio_read),
        JANET_REG("spi/gpio-write",     cfun_spi_gpio_write),
        JANET_REG("spi/loopback",       cfun_spi_loopback),
        JANET_REG("spi/pins",           cfun_spi_pins),
        JANET_REG_END
    };
    janet_cfuns_ext(env, "spi", cfuns);
}