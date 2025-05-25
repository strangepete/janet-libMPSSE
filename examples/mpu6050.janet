# Basic MPU6050 gyro driver for libMPSSE-i2c
#
#   See the gyrotest.janet example for basic usage, and mpu6050.md for API documentation.
#

# Note:
# Reading and writing to the I2C interface is done through wrapper functions, which use
#  a dyn variable :i2c-channel that is set in (mpu/init) with the <i2c/channel> object
#  returned by (i2c/open); besides (mpu/close), this is the only binding to the i2c module.

# TODO:
#  * offset calibration
#  * self-test meaningful output (in %?)
#  * FIFO
# 
# WISHLIST:
#  * DMP
#  * Secondary i2c sensors
#  * sleep modes
#  * interrupts

(use libmpsse)

(def- *default-mpu6050-address* 0x68)

# non-exhaustive list, only those necessary at the time.
(def reg {:XA_OFFSET_H  0x06 # Accel offset from i2cdevlib header; doesn't correspond to 6500 or 6050 datasheet.
          :XA_OFFSET_L  0x07
          :YA_OFFSET_H  0x08
          :YA_OFFSET_L  0x09
          :ZA_OFFSET_H  0x0A
          :ZA_OFFSET_L  0x0B # ^^^
          :SELF_TEST_X  0x0D
          :SELF_TEST_Y  0x0E
          :SELF_TEST_Z  0x0F
          :SELF_TEST_A  0x10
          :XG_OFFSET_H  0x13 # Gyro usr offset, from 6500 datasheet...not in 6050
          :XG_OFFSET_L  0x14
          :YG_OFFSET_H  0x15
          :YG_OFFSET_L  0x16
          :ZG_OFFSET_H  0x17
          :ZG_OFFSET_L  0x18 # ^^^
          :SMPLRT_DIV   0x19
          :CONFIG       0x1A
          :GYRO_CONFIG  0x1B
          :ACCEL_CONFIG 0x1C
          :ACCEL_XOUT_H 0x3B
          :ACCEL_XOUT_L 0x3C
          :ACCEL_YOUT_H 0x3D
          :ACCEL_YOUT_L 0x3E
          :ACCEL_ZOUT_H 0x3F
          :ACCEL_ZOUT_L 0x40
          :TEMP_H       0x41
          :TEMP_L       0x42
          :GYRO_XOUT_H  0x43
          :GYRO_XOUT_L  0x44
          :GYRO_YOUT_H  0x45
          :GYRO_YOUT_L  0x46
          :GYRO_ZOUT_H  0x47
          :GYRO_ZOUT_L  0x48
          :PWR_MGMT_1   0x6B
          :PWR_MGMT_2   0x6C
          :WHO_AM_I     0x75})

(defn i2c-write
  "libmpsse i2c/write wrapper. Takes a buffer, or an 8-bit integer."
  [len data]
  (def chan (dyn :i2c-channel))
  (def addr (dyn :mpu6050-addr))
  (i2c/write chan addr len data))

(defn i2c-read
  "libmpsse i2c/read wrapper."
  [len buf]
  (def chan (dyn :i2c-channel))
  (def addr (dyn :mpu6050-addr))
  (i2c/read chan addr len buf))

(defn c->f
  "Convert Celsius to Fahrenheit"
  [c]
  (+ (* c 1.8) 32))

(defn sign16
  "Convert a 16-bit signed integer to unsigned"
  [uint]
  (if (one? (band 1 (brshift uint 15)))
    (- uint 65536)
    uint))

(defn unsign16
  "Convert a 16-bit unsigned integer to signed"
  [int]
  (if (< int 0)
    (+ int 65536)
    int))

(defn who-am-i
  "Should respond with device address, which is 0x68 by default. Used to confirm an established connection.\n\n
   Returns a 7-bit address, or nil on error. Given optional expected address, returns a boolean comparison."
  [&opt address]
  (def b @"")
  (i2c-write 1 (reg :WHO_AM_I))
  (i2c-read 1 b)
  (if (= :ok (i2c/err))
    (if address
      (= address (get b 0))
      (get b 0))
    nil))

(defn reset
  "Hard reset device registers to default values, and puts the device to sleep." # per datasheet, 107 is reset to 0x40
  []
  (def b @"")
  (i2c-write 2 (buffer/from-bytes (reg :PWR_MGMT_1) 0x80)))

(defn sleep
  "Send device to sleep. All readings will freeze at last value until woke."
  []
  (def b @"")
  (i2c-write 1 (reg :PWR_MGMT_1))
  (i2c-read 1 b)
  (buffer/bit-set b 6)
  (i2c-write 2 (buffer/from-bytes (reg :PWR_MGMT_1) (get b 0))))

(defn wake
  "Wake device from sleep mode."
  []
  (def b @"")
  (i2c-write 1 (reg :PWR_MGMT_1))
  (i2c-read 1 b)
  (buffer/bit-clear b 6)
  (i2c-write 2 (buffer/from-bytes (reg :PWR_MGMT_1) (get b 0))))

(defn clock-source
  ``Set device clock source. Will default to internal, but recommended to use a gyro reference. 
   Returns configured clock source, or sets the source if given one of the following:
  * :internal - 8Mhz oscillator
  * :gyro-x - PLL with X axis gyroscope reference
  * :gyro-y
  * :gyro-z
  * :external-32khz - PLL with external 32.768KHz reference
  * :external-19mhz - PLL with external 19.2MHz reference
  * :stop - Stop the clock and keep the timing generator in reset.``
  [&opt source]
  (def b @"")
  (i2c-write 1 (reg :PWR_MGMT_1))
  (i2c-read 1 b)
  (if (nil? source)
    (break b))
  (def c (case source
           :internal 0
           :gyro-x 1
           :gyro-y 2
           :gyro-z 3
           :external-32khz 4
           :external-19mhz 5
           :stop 7
           0))
  (def s (bor (get b 0) c))
  (i2c-write 2 (buffer/from-bytes (reg :PWR_MGMT_1) s)))

(defn accel-config
  ``Set accelerometer Full Scale Range /g to one of the following:
   * :2
   * :4
   * :8
   * :16
  
   If flag omitted, returns raw register byte.``
  [&opt scale]
  (def b @"")
  (if (nil? scale)
    (do
      (i2c-write 1 (reg :ACCEL_CONFIG))
      (i2c-read 1 b)
      (break b)))
  (def s (case scale
           :2 0
           :4 8
           :8 16
           :16 24
           0))
  (def cmd (buffer/from-bytes (reg :ACCEL_CONFIG) s))
  (i2c-write 2 cmd))

(defn- accel-scale
  ``Divide accelerometer value by full range scale specified:
   * :2  - 16384 LSB/g
   * :4  - 8192 LSB/g
   * :8  - 4096 LSB/g
   * :16 - 2048 LSB/g``
  [value scale]
  (def s (case scale
           :2 16384
           :4 8192
           :8 4096
           :16 2048
           16384))
  (/ value s))

(defn accel-offset
  "Read or set the accelerometer XYZ offset values. Takes an array of xyz offsets.\n\n
  Note: Writes to chip register are cleared on power off."
  [&opt offset]
  (def b (buffer/from-bytes (reg :XA_OFFSET_H)))
  (if (= (length offset) 3)
    (each x offset
      (buffer/push-uint16 b :be x))
    (buffer/push b @"\0\0\0\0\0\0"))
  (i2c-write 6 b))

(defn gyro-config
  ``Set gyro Full Scale Range °/s to one of the following:
   * :250
   * :500
   * :1000
   * :2000
  
  If flag omitted, returns raw register byte.``
  [&opt scale]
  (def b @"")
  (if (nil? scale) # read config
    (do
      (i2c-write 1 (reg :GYRO_CONFIG))
      (i2c-read 1 b)
      (break b)))
  (def s (case scale
           :250 0
           :500 8
           :1000 16
           :2000 24
           0))
  (def cmd (buffer/from-bytes (reg :GYRO_CONFIG) s))
  (i2c-write 2 cmd))

(defn- gyro-scale
  ``Divide gyro value by full range scale specified:
   * :250  - 131 LSB/°/s
   * :500  - 65.5 LSB/°/s
   * :1000 - 32.8 LSB/°/s
   * :2000 - 16.4 LSB/°/s``
  [value scale]
  (def s (case scale
           :250 131.0
           :500 65.5
           :1000 32.8
           :2000 16.4
           131.0))
  (/ value s))

(defn gyro-offset
  "Read or set the gyro XYZ offset values. Takes an array of xyz offsets.\n\n
  Note: Writes to chip register are cleared on power off."
  [&opt offset]
  (def b (buffer/from-bytes (reg :XG_OFFSET_H)))
  (if (= (length offset) 3)
    (each x offset
      (buffer/push-uint16 b :be (unsign16 x))) # cast signed to uint16
    (buffer/push b @"\0\0\0\0\0\0"))
  (i2c-write 6 b))

(def- byte16-grammar
  '(any (int-be 2)))

(defn- get-xyz
  "Get 6 bytes, return 3 signed integers. Expects big-endian words."
  [register]
  (def b @"")
  (i2c-write 1 register)
  (if (= 6 (i2c-read 6 b))
    (peg/match byte16-grammar b)
    [nil nil nil]))

(defn get-gyro
  "Get gyro data, returns array of [x y z]."
  []
  (map (fn [arg] (gyro-scale arg :250))
       (get-xyz (reg :GYRO_XOUT_H))))

(defn get-accel
  "Get accel data, returns array of [x y z]."
  []
  (map (fn [arg] (accel-scale arg :2))
       (get-xyz (reg :ACCEL_XOUT_H))))

# This could be refactored to use the byte16-grammar PEG used by (get-xyz) above
# but I will leave it as an example of bit shifting
(defn get-temp
  ``Get the ambient temperature. Takes optional flags:
  * :c - Celsius (default) 
  * :f - Fahrenheit``
  [&opt units]
  (default units :c)
  (var temp 0)
  (def b @"")
  (i2c-write 1 (reg :TEMP_H))
  (i2c-read 2 b)
  (set temp (bor (blshift (get b 0) 8) (get b 1)))
  (set temp (sign16 temp))
  (set temp (+ (/ temp 340) 36.53))
  (if (= units :f)
    (c->f temp)
    temp))

(defn gyro-self-test
  "Enable or disable the gyro self-test feature on xyz axis. Sets gyro full scale range to +-250deg/s.\n\n
   Self-test response = self-test-enabled output -- self-test-disabled output"
  [enable]
  (if enable
    (i2c-write 2 (buffer/from-bytes (reg :GYRO_CONFIG) 0xF8))
    (i2c-write 2 (buffer/from-bytes (reg :GYRO_CONFIG) 0x00))))

(defn init
  "Initialize an MPU6050. Takes optional i2c address; 0x68 is default.
  Creates a new i2c channel unless one is supplied. Returns true if successful."
  [&opt addr chan]
  (if (zero? (i2c/channels))
    (break false))
  (default chan (i2c/open 1))
  (if (nil? chan)
    (break false))
  (def addr
    (setdyn :mpu6050-addr
            (default addr *default-mpu6050-address*)))
  (setdyn :i2c-channel chan)

  # MPU specific config:
  (i2c/write-opt chan :start :stop)
  (i2c/read-opt chan :start :stop :nak-last-byte)
  (if (i2c/init chan :fast) # 400kbs is mpu6050 maximum thruput, in practice :fast-plus (1Mbs) works too
    true
    (do
      (eprint "i2c/init failed: " (i2c/err))
      (i2c/close chan)
      false)))

(defn close
  "Deinit MPU, and disconnect i2c"
  []
  (i2c/close (dyn :i2c-channel)))