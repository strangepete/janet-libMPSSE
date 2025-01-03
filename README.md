# libMPSSE i2c/spi Janet binding

[Janet language](https://janet-lang.org) bindings to [FTDI's libMPSSE library](https://ftdichip.com/software-examples/mpsse-projects/) for I2C and Serial communications.

The Multi-Protocol Synchronous Serial Engine (MPSSE) is a piece of hardware available in some FTDI chips and cables that allows for configurable serial communication with devices, such as I2C, SPI, and JTAG. The primary means of use are through the D2XX driver (available on FTDI's website.) Commands are sent to the MPSSE to execute, such as sending data, setting the clock, various protocol features, or setting GPIO lines. While the chip will also appear as a Virtual COM port, that feature is not used here.

LibMPSSE is a library written by FTDI to simplify I2C, SPI and JTAG development, as configuring the underlying MPSSE hardware is fairly complex. JTAG is not yet implemented here.

This module is an alpha-phase project written by a hobbyist -- all comments and nitpicks welcomed.

API is documented in api.md and is in flux. Function naming and usage generally follows libMPSSE, though some changes were made for ergonomics, *e.g, `I2C_GetChannelInfo` -> `(i2c/info)`*

## Installation

This module has been primarily written and tested on Windows 10 x64. While it will compile on Linux, I don't have a bare-metal system to successfully test it on.

####  Requirements:

* [FTDI D2xx drivers.](https://ftdichip.com/drivers/d2xx-drivers/) Available for Win/Linux/Mac but commonly installed on Windows when connecting a usb cable 
* [`jpm` Package manager](https://github.com/janet-lang/jpm)
* [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/?q=build%20tools) for Windows

Instructions for building `jpm` and Janet on Windows [can be found on the Janet Docs page](https://janet-lang.org/docs/index.html).

libMPSSE can be installed via `jpm` in a Windows Native Tools Command Prompt:
```
jpm install https://github.com/strangepete/janet-libMPSSE.git
```
or built manually:
```
jpm build
jpm test
jpm install
```


Documentation can be generated using [documentarian](https://github.com/pyrmont/documentarian). 


## Usage

> A demo driver for an MPU6050 gyro is in the `/examples` folder

The module import is:
```janet
(use libmpsse)
```
which loads two submodules, `i2c/` and `spi/` which are generally the same, except the I2C functions take a 7-bit I2C address. Many functions can be called as methods on an opened channel, such as `(i2c/err)` or `(:err c)` and `(:close c)`

Getting a channel's information:
```janet
(use libmpsse)
(i2c/channels)                                     # Number of hardware channels; some devices have multiple
# => 1

(i2c/info 1)
# => {:description "C232HM-EDHSL-0"
#     :flags 2
#     :id 67330068
#     :locid 28
#     :serial "00000000"
#     :type 8}
```

This is an example flow for opening a new I2C channel:
```janet
(def numchan (i2c/channels)) 

(if-not (zero? numchan)
  (do
    (def c (i2c/open 1))                           # Open the first channel
    (when (= :ok (i2c/err c))                      # FT_STATUS error is returned as a keyword, such as :ok or :device-not-opened
      (i2c/write-opt c :start :stop)				
      (i2c/read-opt c :start :stop :nak-last-byte) # Transfer settings can be changed prior to each read/write call
      
      (i2c/init c :fast)                           # Initialize to 400kbs

      (:write c 0x3C 2 @"\x40\x00")                # Write 2 bytes to address 0x3C

      (def buf @"")
      (:read c 0x3C 2 buf)                         # Read 2 bytes from address 0x3C to buffer.
                                                   # This will append if buffer not empty.
  
      (:close c))))                                # Currently a closed channel object cannot be reopened; use (i2c/open) to create a new one
```

The various transfer and config options are lightly documented in [api-i2c.md](api-i2c.md) and [api-spi.md](api-spi.md), but for a full description see the FTDI libMPSSE Application Notes 177 (i2c) and 178 (spi) as some options are only available on specific devices.

> The `/read` and `/write` functions are **blocking**, and `/channels` and `/info` are **not thread-safe**

## IMPORTANT!

Incorrect wiring or commands can potentially destroy the device you are working with; be sure of the byte sequences you are sending, or keep a backup device handy.

## Notes

* libMPSSE can **_only operate as an I2C/SPI bus master_**. Many GPS devices also talk as master, and as such cannot be used to get nema messages :(
* JTAG is planned but not yet implemented.
* On linux, [libftdi](https://www.intra2net.com/en/developer/libftdi/) ***is not*** d2xx. Though I think it would be worth targeting next, being open source and easily cross-platform.

## Tested Devices

* C232HM-EDHSL 5v cable
