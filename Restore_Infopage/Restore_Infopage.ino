/*
 Restore_Infopage
 
 Restores previously saved InfoPage contents by flashing Nordic nRF24LE1 SOC RF chips using SPI
 interface from an Arduino.
 
 InfoPage contents need to be previously read out with Read_Infopage sketch.  Infopage contents
 hardcoded in this sketch are for my chip - your chip's contents may be different.
 
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

// NOTE: Enter values for your own chip here (as read by Read_Infopage sketch)  These values will not
// work for you.
byte infopage[37] = // {90, 90, 67, 49, 71, 54, 50, 54, 8, 48, 15, 165, 161, 210, 237, 176, 33,
                    // 177, 80, 13, 24, 255, 255, 255, 255, 130, 255, 255, 255, 255, 0, 0, 255,
                    // 255, 255, 255, 255}; // Buffer for storing InfoPage content
{
 90,  90,  67,  65,
 87,  48,  49,  55,
 12,  49,  33,  36,
 42,  84, 136, 159,
 38, 177,  81,  16,
 11, 255, 255, 255,
255, 130, 255, 255,
255, 255,   0,   0,
255, 255, 255, 255,
255,
};

void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.setTimeout(3000);

  progSetup();
}

void loop() {
  delay(1000);

  Serial.println("READY");
  while (!Serial.find("GO")) {
    Serial.println("READY");
  };

  // Put nRF24LE1 into programming mode
  progStart();

  // Set InfoPage enable bit so all flash is erased
  if (!enableInfoPage())
    goto done;

  // Erase flash
  Serial.println("ERASING FLASH...");
  flash_erase_all();

  // Restore InfoPage content
  Serial.println("RESTORING INFOPAGE....");
  flash_program_verify("INFOPAGE", infopage, 37, 0, 0);

  // Clear InfoPage enable bit so main flash block is programed
  disableInfoPage();

done:
  // Take nRF24LE1 out of programming mode
  progEnd();
  
  Serial.println("DONE");
}
