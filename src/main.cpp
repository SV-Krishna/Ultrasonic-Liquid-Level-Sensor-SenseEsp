/*
Project: DS1603L Sensor Data Collection

This Arduino project is designed to collect data from a DS1603L sensor. The DS1603L is a 
high-precision, low-power consumption ultrasonic sensor. The sensor is attached to the underside
of the tank to be measured. Refer to online documentation on the sensor to understand its limitations.

The sensor communicates with the Arduino board via a serial connection. In this project, 
we're using a SoftwareSerial object to establish this connection. The SoftwareSerial 
library allows serial communication on other digital pins of the Arduino, using software 
to replicate the functionality of the built-in hardware serial.

The `rxPin` and `txPin` variables represent the pins on the Arduino that are connected to 
the sensor's RX and TX pins, respectively. These pins are used for the serial data transmission.

The `sensorSerial` object is an instance of the SoftwareSerial class. It's initialized 
with the `rxPin` and `txPin` as arguments, which sets up a two-way serial communication on those pins.

The `sensor` object is an instance of the DS1603L class. It's initialized with a pointer 
to the `sensorSerial` object, which tells the DS1603L library to use that serial connection 
to communicate with the sensor.

The main loop of the program reads data from the sensor and processes it every two seconds. Given
that the project is intended to be installed on a boat, the data is averaged over 10 samples to
smooth out any fluctuations in the sensor readings. This aims to reduce the effect of boat heel on 
level readings.

The processed data is then sent to a Signal K server, which is a standard Open Source protocol 
for sharing marine data between devices.

The tank level is calculated as a percentage based on the maximum height of the tank in millimeters.
The sensor status is also sent to the Signal K server to monitor the sensor's health and connectivity.
*/

// Include the necessary libraries

// Boilerplate #includes:
    #include "sensesp_app_builder.h"
    #include "sensesp/signalk/signalk_output.h"

// Sensor-specific #includes:
    #include <DS1603L.h>
    #include <Stream.h>

// For RepeatSensor:
    #include "sensesp/sensors/sensor.h"
    #include <SoftwareSerial.h>
    #include "sensesp/transforms/moving_average.h"
    #include "sensesp/transforms/linear.h"
    #include "sensesp/transforms/integrator.h"

// Internal SensESP code is wrapped in a sensesp namespace
    using namespace sensesp;

// SensESP builds upon the ReactESP framework. Every ReactESP application
// must instantiate the "app" object.
    reactesp::ReactESP app;

// tx of the Arduino to rx of the sensor - adjust to your own board.
    const byte txPin = 17;      

// rx of the Arduino to tx of the sensor - adjust to your own board.
    const byte rxPin = 16;                               

// Pass the sensor object to the sensor constructor.
    SoftwareSerial sensorSerial(rxPin, txPin);

// Create an instance of the sensor using the SoftwareSerial object.
    DS1603L sensor(&sensorSerial);

// Define the function that will be called every time we want
// an updated level from the sensor. The sensor reads in mm.
    float read_level_callback () { return sensor.readSensor(); }

// This function determines the status of the sensor and reports back. It will return 
// 1 if the sensor is getting a reading and 0 if it is not. 
// Refer to DS1603L documentation for more information on the sensor status values, timeouts etc.
    float read_sensor_status () { return sensor.getStatus(); }

// The setup function performs one-time application initialization.
    void setup() {

      // Initialise debug information to send to a serial monitor.
            #ifndef SERIAL_DEBUG_DISABLED
              SetupSerialDebug(115200);
            #endif

            Serial.begin(115200);                             // Here the sensor output is printed.
            delay(1000);                                      // Wait for the sensor to start up.
            sensorSerial.begin(9600);                         // Sensor transmits its data at 9600 bps.
            sensor.begin();                                   // Initialise the sensor library.
            Serial.println(F("Setup done."));                 // Print a message to the serial monitor.

        // Create the global SensESPApp() object
            SensESPAppBuilder builder;
              sensesp_app = (&builder)
                              // Set a custom hostname for the app. Connecting to https://<hostname>.local
                              // will show the web interface. 
                              ->set_hostname("SensESP")
                              // Optionally, hard-code the WiFi and Signal K server
                              // settings. This is normally not needed.
                              // ->set_wifi("SSID", "password")
                              // ->set_sk_server("0.0.0.0", 3000)
                              // Log the sensor uptime and send that to Signal K.
                              ->enable_uptime_sensor()
                              ->get_app();
        
        // Read the sensor every 2 seconds
          unsigned int read_interval = 2000;
        
        // Create a RepeatSensor with float output that reads the fluid level
        // using the read_level_callback function defined above.
          auto* tank_level =
            new RepeatSensor<float>(read_interval, read_level_callback);

        // Create a RepeatSensor with float output that provides the status of the sensor as a 1 or 0. 
        // If 0 then check connections of the sensor to the arduino and to the underside of the container.
          auto* sensor_status =
            new RepeatSensor<float>(read_interval, read_sensor_status);

        // To make the 1s and 0s of the sensor more understandable use a Lambda Transform which takes the 
        // Integer value of sensor status and converts to true or false.
          auto int_to_bool_function = [](int sensor_status) ->bool {
            if (sensor_status == 1) {
              return true;
            }
            else { // read_sensor_status == 0
              return false;
            }
            };
          auto int_to_bool_transform = new LambdaTransform<int, bool>(int_to_bool_function);

        // Send the transformed sensor status to the Signal K server as a Bool.
          sensor_status 
                        ->connect_to(int_to_bool_transform)
                        ->connect_to(new SKOutputBool("/tanks_fuel_currentLevel/sensor_status"));

      // Set the Signal K Path for the output of the sensor.   
          const char* sk_path = "tanks.fuel.currentLevel";

      // Taking the empty level of the tank as 0 and the full level as 1000 mm, we can 
      // calculate the level of the tank as a percentage. Adjust 'full_value' to the
      // maximum height of the tank in question in mm. 
      // Full details of the method used below can be found here - 
      // https://signalk.org/SensESP/pages/tutorials/tank_level/#example-3-a-sensor-that-outputs-something-other-than-0-when-the-tank-is-empty  
          
          const char* tank_config_path = "/tanks_fuel_currentLevel/tankHeight";
                 
          const float empty_value = 0; // in mm 
          const float full_value = 1000; // in mm  
          const float range = full_value - empty_value; // 200 - 0 = 200
          const float divisor = range / 100.0; //200 / 100 = 2
          const float multiplier = 1.0 / divisor; //  (1 / 2 = 0.5)
          const float offset = 100.0 - full_value * multiplier; // (100 - (200 x 0.5) = 0)

      // Set the configuration paths for the Linear and MovingAverage transforms
      // that will be used to process the sensor data. This makes these values available
      // for run time configuration in the SensESP web interface.
          const char *ultrasonic_in_config_path = "/tanks_fuel_currentLevel/ultrasonic_in";
          const char *linear_config_path = "/tanks_fuel_currentLevel/linear";
          const char *ultrasonic_ave_samples = "/tanks_fuel_currentLevel/samples";

      // Now, you add Linear to your main.cpp with your calculated multiplier and offset values:
          float scale = 1.0;

      // Send the fluid level of the tank to the Signal K server as a Float
          tank_level  
                      ->connect_to(new Linear(multiplier, offset, linear_config_path))
                      ->connect_to(new MovingAverage(10, scale, ultrasonic_ave_samples))
                      ->connect_to(new SKOutputFloat(sk_path));

      // Start the SensESP application running.
          sensesp_app->start();
      }

// Loop simply calls `app.tick()` which will then execute all reactions as needed.
    void loop() {
      app.tick();
    }
