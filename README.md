I was most interested in a solution that would send data on my water and fuel tanks to SignalK as a percentage of overall volume. This is acheived by attaching a DS1603L to the underside of the tank to be measured, connecting that to an ESP-32, which connects through the boats wi-fi, to my SignalK server.

Install the code on a esp32 wroom from AZ-Delivery using Visual Code. Before uploading the code go to line 148 and change the tank height to the height of your own tank.

Connect all four leads from the sensor to the corresponding pins on the esp32. The yellow lead of the sensor goes to Pin 16, the white to pin 17. Connect sensor ground to ESP32 Ground, and Sensor power to the ESP32 5v pin. Note, no resistors, or hardware serial converters etc are required between the sensor and the esp32.

Power up the ESP32​​.

The ESP32 will create a wireless access point. Connect to the wireless access point using the password 'thisisfine'.

Once connected to the wireless access point a Wi-Fi configuration page will be displayed on 192.168.4.1. Use that page to connect to your boats Wi-Fi.

Using a mobile phone or similar that is connected to your boats Wi-Fi you will then be able to access the ESP32 using the address http://sensesp.local.

From this portal you can check the status of the ESP32 and make run time configuration changes to Wi-Fi settings and sample rates as well as rebooting the device. At this point you can also change the device hostname sensesp.local to something more meaningful to your set up - e.g. watersensor.local.

Configure SignalK to accept data from the sensor following SensESP documentation

Test the sensor using methods outlined on the internet and then install.
