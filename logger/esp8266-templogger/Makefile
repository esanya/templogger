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
