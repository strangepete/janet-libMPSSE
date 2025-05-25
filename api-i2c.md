# libmpsse I2C API

[ft/version](#ftversion), [i2c/channels](#i2cchannels), [i2c/close](#i2cclose), [i2c/config](#i2cconfig), [i2c/err](#i2cerr), [i2c/find-by](#i2cfind-by), [i2c/gpio-read](#i2cgpio-read), [i2c/gpio-write](#i2cgpio-write), [i2c/id](#i2cid), [i2c/info](#i2cinfo), [i2c/init](#i2cinit), [i2c/is-open](#i2cis-open), [i2c/open](#i2copen), [i2c/read](#i2cread), [i2c/read-opt](#i2cread-opt), [i2c/write](#i2cwrite), [i2c/write-opt](#i2cwrite-opt)


## ft/version

**cfunction**  | [source][1]

```janet
(ft/version)
```

Return a tuple of the libMPSSE and ftd2xx version numbers as [major minor build]

[1]: c/i2c.c#L580

## i2c/channels

**cfunction**  | [source][2]

```janet
(i2c/channels)
```

Get the number of I2C channels that are connected to the host system. Sets `:err` to return status.

Note: The number of ports available in each chip is different, but must be an MPSSE chip or cable.

This function is **not thread-safe**.

[2]: c/i2c.c#L82

## i2c/close

**cfunction**  | [source][3]

```janet
(i2c/close channel)
```

Closes the specified channel. Returns `true` if successful. Sets `:err` to return status.

[3]: c/i2c.c#L443

## i2c/config

**cfunction**  | [source][4]

```janet
(i2c/config channel &opt kw ...)
```

Set channel config options. Takes zero, or more keywords:

* `:disable-3phase-clocking`
* `:enable-drive-only-zero`

Note: 3-phase clocking only available on hi-speed devices, not the FT2232D. Drive-only-zero is only available on the FT232H.

[4]: c/i2c.c#L373

## i2c/err

**cfunction**  | [source][5]

```janet
(i2c/err)
```

The return status of the last executed I2C function as a keyword representing an error code. When called as a method `(:err chan)`, the channel is ignored.

`FT_STATUS`:
* `:ok`
* `:invalid-handle`
* `:device-not-found`
* `:device-not-opened`
* `:io-error`
* `:insufficient-resources`
* `:invalid-parameter`
* `:invalid-baud-rate`
* `:device-not-opened-for-erase`
* `:device-not-opened-for-write`
* `:failed-to-write-device`
* `:eeprom-read-failed`
* `:eeprom-write-failed`
* `:eeprom-erase-failed`
* `:eeprom-not-present`
* `:eeprom-not-programmed`
* `:invalid-args`
* `:not-supported`
* `:other-error`
* `:device-list-not-ready`

Note: currently a wrapper for (dyn :i2c-err)

[5]: c/i2c.c#L72

## i2c/find-by

**cfunction**  | [source][6]

```janet
(i2c/find-by kw value)
```

Find a channel matching an explicit identifer. Takes a keyword and value:
* `:id`    - unique channel ID (integer)
* `:locid` - USB location ID (integer)
* `:type`  - Device type (integer)
* `:description` - (string)
* `:serial`    - (string)

Returns a channel `index` or `nil` on failure. Sets `:err` to return status.

[6]: c/i2c.c#L190

## i2c/gpio-read

**cfunction**  | [source][7]

```janet
(i2c/gpio-read channel)
```

Read the 8 GPIO lines from the high byte of the MPSSE channel.

Returns an unsigned 8-bit integer, or `nil` on error. Sets `:err` to return status.

Note: **Must call write-gpio to initialize before reading**. See the libMPSSE.

[7]: c/i2c.c#L480

## i2c/gpio-write

**cfunction**  | [source][8]

```janet
(i2c/gpio-write channel dir value)
```

Write to GPIO lines, where `direction` and `value` are an 8-bit value mapping each line. Direction bit 0 for in, and 1 for out. Value is 0 logic low, 1 logic high.

Returns `nil`. Sets `:err` to return status.

Note: libMPSSE cannot use the lower gpio port pins 0-7, such as those exposed in FTDI cable assemblies. Setting bit-6 corresponds to the onboard red LED in some cables.

[8]: c/i2c.c#L462

## i2c/id

**cfunction**  | [source][9]

```janet
(i2c/id channel)
```

Takes an `<i2c/channel>` and returns the unique, per-channel ID assigned by libMPSSE on channel creation.

[9]: c/i2c.c#L136

## i2c/info

**cfunction**  | [source][10]

```janet
(i2c/info index)
```

Retrieve detailed information about an I2C channel, given a 1-based channel `index`, or an `<i2c/channel>` object.
Returns `nil` on error. Sets `:err` to return status.

On success, returns a table:
* `:serial`      - Serial number of the device
* `:description` - Device description
* `:id`          - Unique channel ID
* `:locid`       - USB location ID
* `:handle`      - Device handle (internal pointer)
* `:type`        - Device type
* `:flags`       - Device status flags

This function is **not thread-safe**.

[10]: c/i2c.c#L103

## i2c/init

**cfunction**  | [source][11]

```janet
(i2c/init channel &opt clockrate latency)
```

Initialize an open `channel` with optional `clockrate` and `latency`. Returns `true` if successful, or `false` on error. Sets :err to return status.

Clock rate is one of the following keywords:

* `:standard`   - 100kb/s (default)
* `:fast`       - 400kb/s
* `:fast-plus`  - 1000kb/s
* `:high-speed` - 3.4Mb/s
* or a non-standard clock rate integer from 0 to 3,400,000.

Note: Recommended latency of Full-speed devices (FT2232D) is 2 to 255, and Hi-speed devices (FT232H, FT2232H, FT4232H) is 1 to 255. Default is 255.

[11]: c/i2c.c#L407

## i2c/is-open

**cfunction**  | [source][12]

```janet
(i2c/is-open channel)
```

Returns true if a channel is open, or false if closed or invalid. Sets `:err` to return status.

Takes either an `<i2c/channel>` object, or 1-based `index`.

[12]: c/i2c.c#L276

## i2c/open

**cfunction**  | [source][13]

```janet
(i2c/open index)
```

Open a channel by (1-based) `index`.

Returns an `<i2c/channel>` if succesful, or `nil` on error. Sets `:err` to return status.



[13]: c/i2c.c#L146

## i2c/read

**cfunction**  | [source][14]

```janet
(i2c/read channel address size buffer)
```

Read & append `size` n-bytes to `buffer` from I2C device at `address`.

Returns bytes read. Sets `:err` to return status.

This is a **blocking function**.

[14]: c/i2c.c#L496

## i2c/read-opt

**cfunction**  | [source][15]

```janet
(i2c/read-opt channel &opt kw ...)
```

Set I2C Read transfer options. Takes zero, or more keywords:

* `:start`
* `:stop`
* `:nak-last-byte`
* `:fast-transfer-bytes`
* `:fast-transfer-bits`
* `:no-address`



[15]: c/i2c.c#L357

## i2c/write

**cfunction**  | [source][16]

```janet
(i2c/write channel address size buffer)
```

Write `size` n-bytes of `buffer` to I2C channel/device `address`.

Returns bytes written. Sets `:err` to return status.

This is a **blocking function**.

[16]: c/i2c.c#L531

## i2c/write-opt

**cfunction**  | [source][17]

```janet
(i2c/write-opt channel &opt kw ...)
```

Set I2C Write transfer options. Takes zero, or more keywords:

* `:start`
* `:stop`
* `:break-on-nak`
* `:fast-transfer-bytes`
* `:fast-transfer-bits`
* `:no-address`



[17]: c/i2c.c#L341
