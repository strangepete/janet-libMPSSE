# libmpsse SPI API

[ft/version](#ftversion), [spi/channels](#spichannels), [spi/close](#spiclose), [spi/config](#spiconfig), [spi/err](#spierr), [spi/find-by](#spifind-by), [spi/gpio-read](#spigpio-read), [spi/gpio-write](#spigpio-write), [spi/id](#spiid), [spi/info](#spiinfo), [spi/init](#spiinit), [spi/is-busy](#spiis-busy), [spi/is-open](#spiis-open), [spi/open](#spiopen), [spi/read](#spiread), [spi/read-opt](#spiread-opt), [spi/readwrite](#spireadwrite), [spi/write](#spiwrite), [spi/write-opt](#spiwrite-opt)

## ft/version

**cfunction**  | [source][1]

```janet
(ft/version)
```

Return a tuple of the libMPSSE and ftd2xx version numbers as [major minor build]

[1]: c/i2c.c#L580

## spi/channels

**cfunction**  | [source][18]

```janet
(spi/channels)
```

Get the number of SPI channels that are connected to the host system. Sets `:err` to return status.

Note: The number of ports available in each chip is different, but must be an MPSSE chip or cable.

This function is **not thread-safe**.

[18]: c/spi.c#L82

## spi/close

**cfunction**  | [source][19]

```janet
(spi/close channel)
```

Closes the specified channel. Returns `true` if successful. Sets `:err` to return status.

[19]: c/spi.c#L423

## spi/config

**cfunction**  | [source][20]

```janet
(spi/config channel &opt kw ...)
```

Set channel config options. Takes zero, or more keywords:

* `:mode0`             - captured on Rising, propagated on Falling edge
* `:mode1`             - captured on Falling, propagated on rising edge
* `:mode2`             - captured on Falling, propagated on Rising edge
* `:mode3`             - captured on Rising, propagated on Falling edge
* `:bus_`              - Use chip select bus line `:cs-bus3` to `7`
* `:active-low`        - Set chip select line to Active Low (default is High)

Note: Bus corresponds to lines ADBUS0 - ADBUS7 if the first MPSSE channel is used, otherwise it corresponds to lines BDBUS0 - BDBUS7 if the second MPSSEchannel (i.e., if available in the chip) is used.

[20]: c/spi.c#L351

## spi/err

**cfunction**  | [source][21]

```janet
(spi/err)
```

The return status of the last executed SPI function as a keyword representing an error code. When called as a method `(:err chan)`, the channel is ignored.

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

Note: currently a wrapper for (dyn :ft-err)

[21]: c/spi.c#L72

## spi/find-by

**cfunction**  | [source][22]

```janet
(spi/find-by kw value)
```

Find a channel matching an explicit identifer. Takes a keyword and value:
* `:id`    - unique channel ID (integer)
* `:locid` - USB location ID (integer)
* `:type`  - Device type (integer)
* `:description` - (string)
* `:serial`    - (string)

Returns a channel `index` or `nil` on failure. Sets `:err` to return status.

[22]: c/spi.c#L187

## spi/gpio-read

**cfunction**  | [source][23]

```janet
(spi/gpio-read channel)
```

Read the 8 GPIO lines from the high byte of the MPSSE channel.

Returns an unsigned 8-bit integer, or `nil` on error. Sets `:err` to return status.

Note: **Must call write-gpio to initialize before reading**. See the libMPSSE AN-178.

[23]: c/spi.c#L577

## spi/gpio-write

**cfunction**  | [source][24]

```janet
(spi/gpio-write channel dir value)
```

Write to GPIO lines, where `direction` and `value` are an 8-bit value mapping each line. Direction bit 0 for in, and 1 for out. Value is 0 logic low, 1 logic high.

Returns `nil`. Sets `:err` to return status.

Note: libMPSSE cannot use the lower gpio port pins 0-7, such as those exposed in FTDI cable assemblies. Setting bit-6 corresponds to the onboard red LED in some cables.

[24]: c/spi.c#L559

## spi/id

**cfunction**  | [source][25]

```janet
(spi/id channel)
```

Takes an `<spi/channel>` and returns the unique, per-channel ID assigned by libMPSSE on channel creation.

[25]: c/spi.c#L136

## spi/info

**cfunction**  | [source][26]

```janet
(spi/info index)
```

Retrieve detailed information about an SPI channel, given a 1-based channel `index`, or an `<spi/channel>` object.
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

[26]: c/spi.c#L103

## spi/init

**cfunction**  | [source][27]

```janet
(spi/init channel clockrate &opt latency)
```

Initialize an open `channel`, `clockrate` and optional`latency`. Returns `true` if successful, or `false` on error. Sets :err to return status.

* clockrate   - 0 to 30,000,000 Hz
* latency     - 0 to 255 (default)

Note: Recommended latency of Full-speed devices (FT2232D) is 2 to 255, and Hi-speed devices (FT232H, FT2232H, FT4232H) is 1 to 255. Default is 255.

[27]: c/spi.c#L397

## spi/is-busy

**cfunction**  | [source][28]

```janet
(spi/is-busy channel)
```

Reads the state of the MISO line without clocking the SPI bus.

Returns boolean state. Sets `:err` to return status.

[28]: c/spi.c#L541

## spi/is-open

**cfunction**  | [source][29]

```janet
(spi/is-open channel)
```

Returns true if a channel is open, or false if closed or invalid. Sets `:err` to return status.

Takes either an `<spi/channel>` object, or 1-based `index`.

[29]: c/spi.c#L273

## spi/open

**cfunction**  | [source][30]

```janet
(spi/open index)
```

Open a channel by (1-based) `index`.

Returns an `<spi/channel>` if succesful, or `nil` on error. Sets `:err` to return status.



[30]: c/spi.c#L146

## spi/read

**cfunction**  | [source][31]

```janet
(spi/read channel size buffer)
```

Read & append `size` n-bytes to `buffer`

Returns bytes read. Sets `:err` to return status.

This is a **blocking function**.

[31]: c/spi.c#L440

## spi/read-opt

**cfunction**  | [source][32]

```janet
(spi/read-opt channel &opt kw ...)
```

Set SPI Read transfer options. Takes zero, or more keywords:

* `:size-in-bits`      - Transfer size in bits (default is bytes)
* `:cs`                - Chip-select line asserted before beginning transfer



[32]: c/spi.c#L332

## spi/readwrite

**cfunction**  | [source][33]

```janet
(spi/readwrite channel size sendbuf recvbuf)
```

Simultaneously read & write `size` n-bytes to `channel`.

Returns bytes transfered. Sets `:err` to return status.

Note: Uses the `write-opt` transfer option for both operations.

This is a **blocking function**.

[33]: c/spi.c#L508

## spi/write

**cfunction**  | [source][34]

```janet
(spi/write channel size buffer)
```

Write `size` n-bytes of `buffer`

Returns bytes written. Sets `:err` to return status.

This is a **blocking function**.

[34]: c/spi.c#L470

## spi/write-opt

**cfunction**  | [source][35]

```janet
(spi/write-opt channel &opt kw ...)
```

Set SPI Write transfer options. Takes zero, or more keywords:

* `:size-in-bits`      - Transfer size in bits (default is bytes)
* `:cs`                - Chip-select line asserted before beginning transfer



[35]: c/spi.c#L320
