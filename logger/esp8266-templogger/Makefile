ARDUINO_CLI ?= arduino-cli
FQBN ?= esp8266:esp8266:d1_mini
PORT ?= /dev/ttyUSB0
SKETCH := esp8266-templogger.ino

.PHONY: build upload


build:
	$(ARDUINO_CLI) compile --fqbn "$(FQBN)" "$(SKETCH)"

installlibs:
	$(ARDUINO_CLI) lib install DHTNEW
	$(ARDUINO_CLI) lib install ArduinoJson


upload: build
	@test -n "$(PORT)" || { echo "Set PORT to the Arduino's serial port (for example, PORT=/dev/ttyACM0)." >&2; exit 1; }
	$(ARDUINO_CLI) upload --fqbn "$(FQBN)" --port "$(PORT)" "$(SKETCH)"


console:
	screen /dev/ttyUSB0 9600

provision:
ifdef LOGGER_DEVIVE_NAME
	echo LOGGER_DEVIVE_NAME is $(LOGGER_DEVIVE_NAME)
	curl http://192.168.4.1/config/devicename/$(LOGGER_DEVIVE_NAME)
	curl http://192.168.4.1/config/provtime/`date`
else
	echo LOGGER_DEVIVE_NAME is not defined
endif


testpythondecoder:
	python3 -m unittest test_decode_measurements.py

downloadbin:
	curl http://192.168.4.1/base64get/measurement.bin > /tmp/measurement.binb64

decodebase64:
	base64 -d /tmp/measurement.binb64 > /tmp/measurement.bin

decodebintocsv:
	python3 decode_measurements.py /tmp/measurement.bin > /tmp/measurement.csv
	date >> /tmp/measurement.csv 
