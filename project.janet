(declare-project
  :name "libmpsse"
  :description ```LibMPSSE Bindings for I2C/SPI communication over FTDI's MPSSE 
  (Multi-Protocol Synchronous Serial Engine) supported chips and cables. Requires FTDI D2xx drivers.```
  :author "Peter Rippe"
  :license "MIT"
  :url "https://github.com/strangepete/janet-libMPSSE"
  :version "0.0.2")

(if (= :linux (os/which))
  (eprintf "WARNING: Untested on Linux, please submit feedback.\n"))

(def cflags (case (os/which)
              :windows [;default-cflags
                        "/DUNICODE"
                        "/D_UNICODE"
                        "/DFT_VER_MAJOR=1" "/DFT_VER_MINOR=0" "/DFT_VER_BUILD=5" # libMPSSE version
                        "/DFTDIMPSSE_STATIC"
                        "/ILibMPSSE_1.0.5/release/include"
                        "/ILibMPSSE_1.0.5/release/libftd2xx"]
              :linux [;default-cflags # linux is untested; compiles for me but always dies with OTHER_ERROR
                      "-DFT_VER_MAJOR=1" "-DFT_VER_MINOR=0" "-DFT_VER_BUILD=5"
                      "-DFTDIMPSSE_STATIC"
                      "-D_DEFAULT_SOURCE" # needed for usleep()
                      "-ILibMPSSE_1.0.5/release/include"
                      "-ILibMPSSE_1.0.5/release/libftd2xx"
                      "-ILibMPSSE_1.0.5/release/source"]))

(def ldflags (case (os/which)
               :windows [;default-ldflags
                         "/DEBUG"]
               :linux [;default-ldflags]))
(declare-native
  :name "libmpsse"
  :cflags cflags
  :ldflags ldflags
  :source @["LibMPSSE_1.0.5/release/source/ftdi_mid.c"
            "LibMPSSE_1.0.5/release/source/ftdi_infra.c"
            "LibMPSSE_1.0.5/release/source/ftdi_spi.c"
            "LibMPSSE_1.0.5/release/source/ftdi_i2c.c"
            "c/module.c"
            "c/i2c.c"
            "c/spi.c"])
