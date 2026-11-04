The logger should take temperature measurements in periodic, and store the following values:
- reference to the current time (or iteration)
- temperature
- relative humidity
- dew point
- etc...

After storing the values the device should go into deep-sleep to save energy in the battery.

The logger should support multiple operation modes.
1. standalone mode: without any WIFI / network infrastructure.
2. backend mode: with a [[backend]] service running and to collect the data
3. administration mode: to configure the logger with only a computer
4. ...?

The logger uses the [[https://arduino-esp8266.readthedocs.io/en/latest/index.html|ESP8266]] platform. ESP8266 supports deep-sleep, WIFI, file-system, interrupts, etc. 
