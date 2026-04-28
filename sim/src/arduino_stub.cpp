#include "ArduinoStub.h"
#include "SPI.h"
#include "WString.h"

SerialStub Serial;
SPIClass SPI;

String SerialStub::readStringUntil(char) { return String(""); }
