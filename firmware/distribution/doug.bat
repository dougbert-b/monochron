avrdude -p atmega328p -c arduino -b 57600 -P com8 -B 1 -U flash:w:MultiChron.hex -U eeprom:w:MultiChron.eep
