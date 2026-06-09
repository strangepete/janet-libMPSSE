(declare-project
  :name "libmpsse"
  :description ```LibMPSSE Bindings for I2C/SPI communication over FTDI's MPSSE 
  (Multi-Protocol Synchronous Serial Engine) supported chips and cables. Requires FTDI D2xx drivers.```
  :author "Peter Rippe"
  :license "MIT"
  :url "https://github.com/strangepete/janet-libMPSSE"
  :repo "git+https://github.com/strangepete/janet-libMPSSE.git"
  :version "0.0.5")

(def debug-level (os/getenv "INFRA_DEBUG_LEVEL")) # level of verbosity 0-7
(def debugging (if (or (os/getenv "INFRA_DEBUG") debug-level) true false))
(def windows? (if (= (os/which) :windows) true false))
(def version (string/format "%02x%02x%02x" ;(string/split "." (get (dyn :project) :version))))

(defmacro ? "platform-specific argument" [& body] ~(string (if windows? "/" "-") ,;body))
(defn if-debug [x] (if debugging x []))

(def cflags # platform specific
  (case (os/which)
    :windows [;default-cflags
              "/DUNICODE"
              "/D_UNICODE"
              ;(if-debug
               ["/fsanitize=address"
                "/Z7"])]
    [;default-cflags
     "-D_DEFAULT_SOURCE"])) # needed for usleep()

(declare-source
  :source ["libmpsse/"])

(declare-native
  :name "_libmpsse"
  :cflags [;cflags
           (? "DFTDIMPSSE_STATIC")
           (? "DJANET_LIBMPSSE_VERSION=0x" version)
           (? "DFT_VER_MAJOR=1") (? "DFT_VER_MINOR=0") (? "DFT_VER_BUILD=9")
           (? "IFTDI_LibMPSSE/release/include")
           (? "IFTDI_LibMPSSE/release/libftd2xx")
           (? "IFTDI_LibMPSSE/release/source")
           ;(if-debug
            [(? "D_DEBUG")
             (? "DINFRA_DEBUG_ENABLE")
             ;(if debug-level [(? "DINFRA_DEBUG_LEVEL=" debug-level)] [])])]
  :ldflags [;default-ldflags
            ;(if (and debugging windows?) ["/DEBUG"] [])]
  :source @["FTDI_LibMPSSE/release/source/ftdi_mid.c"
            "FTDI_LibMPSSE/release/source/ftdi_infra.c"
            "FTDI_LibMPSSE/release/source/ftdi_spi.c"
            "FTDI_LibMPSSE/release/source/ftdi_i2c.c"
            "c/module.c"
            "c/i2c.c"
            "c/spi.c"])
