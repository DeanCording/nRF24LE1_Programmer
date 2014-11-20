/*
 Read_Mainpage
 
 Reads MainPage flash contents from Nordic nRF24LE1 SOC RF chips using SPI interface from 
 an Arduino.
 
 Upload sketch, start serial monitor, type 'GO' to start sketch running.
 
 Copyright (c) 2014 Dean Cording
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */

#include <SPI.h>
#include "../common/common.c"

void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.setTimeout(3000);

  progSetup();
}

void loop() {
  delay(1000);

  Serial.println("READY");
  // Wait for GO command from Serial
  while (!Serial.find("GO")) {
    Serial.println("READY");
  }
  Serial.println("READYING");
  delay(1000);
  Serial.println("SETTING UP");

  // Put nRF24LE1 into programming mode
  progStart();

  // Set InfoPage bit so InfoPage flash is read 
  if (!disableInfoPage())
    goto done;

  // Read from MainPage
  Serial.println("READING...");
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(READ);
  SPI.transfer(0);
  SPI.transfer(0);
  for (int index = 0; index < 16*1024; index++) {
    byte spi_data = SPI.transfer(0x00);
    Serial.print(index);
    Serial.print(": ");
    Serial.println(spi_data);
  }
  digitalWrite(_FCSN_, HIGH);  

done:    
  Serial.println("DONE");

  // Take nRF24LE1 out of programming mode
  progEnd();
}
