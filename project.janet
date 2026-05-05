(declare-project
  :name "libmpsse"
  :description ```LibMPSSE Bindings for I2C/SPI communication over FTDI's MPSSE 
  (Multi-Protocol Synchronous Serial Engine) supported chips and cables. Requires FTDI D2xx drivers.```
  :author "Peter Rippe"
  :license "MIT"
  :url "https://github.com/strangepete/janet-libMPSSE"
  :version "0.0.4")

(def cflags (case (os/which)
              :windows [;default-cflags
                      # "/DINFRA_DEBUG_ENABLE" # libmpsse offers *verbose* debugging
                        "/DUNICODE"
                        "/D_UNICODE"
                        "/DFT_VER_MAJOR=1" "/DFT_VER_MINOR=0" "/DFT_VER_BUILD=8" # libMPSSE version
                        "/DFTDIMPSSE_STATIC"
                        "/IFTDI_LibMPSSE/release/include"
                        "/IFTDI_LibMPSSE/release/libftd2xx"]
              [;default-cflags
             # "-DINFRA_DEBUG_ENABLE"
               "-DFT_VER_MAJOR=1" "-DFT_VER_MINOR=0" "-DFT_VER_BUILD=8"
               "-DFTDIMPSSE_STATIC"
               "-D_DEFAULT_SOURCE" # needed for usleep()
               "-IFTDI_LibMPSSE/release/include"
               "-IFTDI_LibMPSSE/release/libftd2xx"
               "-IFTDI_LibMPSSE/release/source"]))

(declare-source
  :source ["libmpsse/"])

(declare-native
  :name "_libmpsse"
  :prefix "libmpsse"
  :cflags cflags
  :source @["FTDI_LibMPSSE/release/source/ftdi_mid.c"
            "FTDI_LibMPSSE/release/source/ftdi_infra.c"
            "FTDI_LibMPSSE/release/source/ftdi_spi.c"
            "FTDI_LibMPSSE/release/source/ftdi_i2c.c"
            "c/module.c"
            "c/i2c.c"
            "c/spi.c"])
