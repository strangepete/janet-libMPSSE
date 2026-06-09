# libMPSSE C232HM Blinking LED demo
# 
# Blink the built-in red LED (ACBUS6) on the C232HM-x cables.
# 
# Note: The libmpsse gpio-write/read functions only act on the ACBUS, to
# use the ADBUS requires MPSSE configuration using the D2XX API.

(use libmpsse)

(prin "Waiting for connection...")
(while (zero? (i2c/channels))
         (do
           (prin ".") (flush)
           (ev/sleep 1)))

# Restrict to C232HM cables only
(var info (i2c/info 1))
(if-not (string/has-prefix? "C232HM-" (info :description))
  (errorf "C232HM-x series expected, but found \"%s\". The LED is hardware-specific" (info :description)))

(with [chan (i2c/open 1)]
      (prinf "success.\nBlinking: ")

      (defn LED "Switch the LED at ACBUS6 on/off"
        [state]
        (def pin {:on 0x00    # all low
                  :off 0x40  # ACBUS6, bit 7: high
                  :dir 0x7f}) # direction: all out except ACBUS7(VBUSDTCT) input
        (i2c/gpio-write chan
                        (pin :dir)
                        (if state (pin :on) (pin :off)))
        (when (not= (i2c/err) :ok)
          (errorf "gpio-write failed: %v" (i2c/err))))

      (var state false)
      (repeat 8
              (LED (toggle state))
              (prinf "%s " (if state "on" "off"))
              (flush)
              (ev/sleep 0.5))

      (LED false))