#include "src/firmware_app.h"

FirmwareApp app;

void setup() {
  app.begin();
}

void loop() {
  app.tick();
}
