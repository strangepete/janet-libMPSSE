(use /build/libmpsse)

# I2C
(def chans (i2c/channels))
(assert (= :ok (i2c/err)))
(print "I2C channels found: " chans)
(if (> chans 0)
  (do
    (print "Opening I2C channel #1...")
    (var c (i2c/open 1))
    (pp c)
    (assert (= :ok (:err c)))
    (assert (not (nil? c)))
    (assert (:is-open c))
    (var ci (i2c/info c))
    (assert (struct? ci))
    (assert (not (nil? (i2c/find-by :id (get ci :id)))))
    (assert (not (nil? (i2c/find-by :serial (get ci :serial)))))
    (assert (nil? (i2c/find-by :serial "nope")))
    (eachp [k v] ci
      (print k ":"
             (string/repeat " " (- 13 (length k)))
             v))
    (assert (:close c))
    (assert (= :ok (:err c)))
    (assert (not (:is-open c)))
    (print "I2C channel successfully closed.")))

# SPI
(def chans (spi/channels))
(assert (= :ok (spi/err)))
(print "SPI channels found: " chans)
(if (> chans 0)
  (do
    (print "Opening SPI channel #1...")
    (var c (spi/open 1))
    (pp c)
    (assert (= :ok (:err c)))
    (assert (not (nil? c)))
    (assert (:is-open c))
    (assert (:close c))
    (assert (= :ok (:err c)))
    (assert (not (:is-open c)))
    (print "SPI channel successfully closed.")))