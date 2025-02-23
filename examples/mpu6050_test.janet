# libMPSSE-i2c / MPU6050 gyro demo
# 
# Ensure device is wired correctly; TX/RX need to be bridged, and 
#  the bus needs the proper pull-ups if not included on the device.
#  Refer to your FTDI MPSSE chip/cable datasheet for correct pinouts
#  and voltage levels: Cable is 5v, my MPU6050 is 3.3v

(import ./mpu6050 :as mpu)

# Wait until cable is connected or channel available.
(prin "Connecting...")
(while (not (mpu/init))
  (prin ".")
  (os/sleep 1))
(print "")

# WHO_AM_I is used as a method to confirm a properly configured 
#  connection, as the device should return its own i2c address.
(prin "Waiting for device response from 0x68...")
(var me nil)
(while (nil? (set me (mpu/who-am-i)))
  (prin ".")
  (os/sleep 1))
(print "")

(if-not (mpu/who-am-i 0x68)
  (do
    (printf "who-am-i: Device address mismatch: 0x%x" me)
    (os/exit 1)))
(printf "I am : 0x%X" (mpu/who-am-i))
(printf "Temperature: %-.1f°F" (mpu/get-temp :f)) 

# Gyro Full Scale Range to 250 deg/s
(mpu/gyro-config :250)

# Accelerometer Full Scale Range to 2/g
(mpu/accel-config :2)

# A gyro clocksource is recommended for stability
(mpu/clock-source :gyro-x)
(mpu/wake)

(repeat 100
        (each x (mpu/get-gyro)
          (prinf "%3.2f\t" x))
        (print "")
        (os/sleep 0.2))

(mpu/sleep)
(mpu/close)

# Reading a raw register pair:
#  Writing a single byte will tell the device to index at that register,
#  then the subsequent 2-byte read will return the register & register+1
#  In this example, the returned 16-bit value is big-endian, and must be
#  flipped; see (mpu/get-temp)
#
# (def b @"")
# (mpu/i2c-write 1 (mpu/reg :TEMP_H))
# (mpu/i2c-read 2 b)
#
#  Since MPSSE is slow per-transfer due to the overhead, it is
#  more efficient to transfer as much data as you can in one shot.
#  For example, the Gyro registers are adjacent to the Accelerometer
#  registers, with the Temperature in-between; if you set a read point at
#  the GYRO_XOUT_H register, and read 14 bytes, that will get all 3 sets of data.
#  (6 bytes gyro, 2 bytes temp, 6 bytes accel = 14)
#  
#  