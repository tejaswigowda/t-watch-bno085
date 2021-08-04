# T-Watch + bno085 === Wearable IoT IMU

![Main](main.jpg)
![Wearable](wearable.png)

A simple project that fuses T-watch with bno08x

## Hardware

### Requirements
- TTGO T-watch (<https://www.aliexpress.com/item/1005001581849024.html>; <https://www.amazon.com/LILYGO-GPS-Lora-Programmable-Environmental/dp/B07ZWXV5FQ>)
- bno08x (<https://www.aliexpress.com/item/4000263329804.html>; <https://www.amazon.com/Taidacent-Nine-axis-High-Precision-Accelerometer-Magnetometer/dp/B0836WJLVH/>)
- wires
- soldering equipment 

![Make1](make1.jpg)
![Make2](make2.jpg)


Solder the bno08x chip to the TTGO T-watch baseplate (I2C connectivity). Test connectivity using the [I2C Test Code](https://github.com/tejaswigowda/t-watch-bno085/blob/main/i2cAddr/i2cAddr.ino) (change pin numbers if necessary).


## Sample code

### Test code
  `pod_mcu_code-08x-test`
  Load this code[https://github.com/tejaswigowda/t-watch-bno085/blob/main/pod_mcu_code-08x-test/pod_mcu_code-08x-test.ino] using the Arduino IDE. 
  
  If everything goes right you should have the quaternion output over the serial port. You can test using [Webserial here](https://tejaswigowda.com/webserial-imu-debug/).

### Wifi Visualization
-`pod_mcu_code-08x-wifi`: Load using arduino IDE. Please add your wifi login ceredentialis in `wifilogin.h`.

-`pod_mcu_code-08x-wifi`: cd to this folder and run the server using `node server.js`. 

Then open <http://localhost:1234#[mac_address]> in your browser to visulize the IMU data.







<script>
  $(".credits.right").fadeOut(0);
  </script>
