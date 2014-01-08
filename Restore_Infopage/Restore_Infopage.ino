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

/*
  Arduino Uno connections:
 
  See nRF24LE1 Product Specification for corresponding pin numbers.
  
  NOTE: nRF24LE1 is a 3.3V device.  Level converters are required to connect it to a
  5V Arduino.

 * D00: Serial RX
 * D01: Serial TX
 * D02: 
 *~D03: 
 * D04:  
 *~D05: 
 *~D06: 
 * D07: nRF24LE1 PROG
 * D08: nRF24LE1 _RESET_
 *~D09: nRF24LE1 FCSN
 *~D10: 
 *~D11: SPI MOSI, nRF24LE1 FMOSI
 * D12: SPI MISO, nRF24LE1 FMISO
 * D13: SPI SCK, On board Status LED, nRF24LE1 FSCK
 * A0: 
 * A1:
 * A2:
 * A3:
 * A4: I2C SDA
 * A5: I2C SCL
 * 5V: 
 * 3.3V: nRF24LE1 VDD
 * AREF:
 * GND:  nRF24LE1 VCC
 
 (~ PWM)
 
 Interupts:
 0:  
 1:
 
 */
#include <SPI.h>


// Specify pins in use
#define PROG      7   // nRF24LE1 Program
#define _RESET_   8   // nRF24LE1 Reset
#define _FCSN_    9   // nRF24LE1 Chip select

// SPI Flash operations commands
#define WREN 		0x06  // Set flase write enable latch
#define WRDIS		0x04  // Reset flash write enable latch
#define RDSR		0x05  // Read Flash Status Register (FSR)
#define WRSR		0x01  // Write Flash Status Register (FSR)
#define READ		0x03  // Read data from flash
#define PROGRAM		0x02  // Write data to flash
#define ERASE_PAGE	0x52  // Erase addressed page
#define ERASE_ALL	0x62  // Erase all pages in flash info page^ and/or main block
#define RDFPCR		0x89  // Read Flash Protect Configuration Register (FPCR)
#define RDISMB		0x85  // Enable flash readback protection
#define ENDEBUG		0x86  // Enable HW debug features

/* NOTE: The InfoPage area DSYS are used to store nRF24LE1 system and tuning parameters. 
 * Erasing the content of this area WILL cause changes to device behavior and performance. InfoPage area
 * DSYS should ALWAYS be read out and stored prior to using ERASE ALL. Upon completion of the
 * erase the DSYS information must be written back to the flash InfoPage.
 */

// Flash Status Register (FSR) bits
#define FSR_STP                 B01000000  // Enable code execution start from protected flash area
#define FSR_WEN                 B00100000  // Write enable latch
#define FSR_RDYN                B00010000  // Flash ready flag - active low
#define FSR_INFEN               B00001000  // Flash InfoPage enable


byte fsr;          // Flash status register buffer
byte spi_data;     // SPI data transfer buffer

// NOTE: Enter values for your own chip here (as read by Read_Infopage sketch)  These values will not
// work for you.
byte infopage[37] = // {90, 90, 67, 49, 71, 54, 50, 54, 8, 48, 15, 165, 161, 210, 237, 176, 33,
                     177, 80, 13, 24, 255, 255, 255, 255, 130, 255, 255, 255, 255, 0, 0, 255,
                     255, 255, 255, 255}; // Buffer for storing InfoPage content

void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.setTimeout(30000);

  // Initialise SPI
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.begin();

  // Initialise control pins
  pinMode(PROG, OUTPUT);
  digitalWrite(PROG, LOW);
  pinMode(_RESET_, OUTPUT);
  digitalWrite(_RESET_, HIGH);
  pinMode(_FCSN_, OUTPUT);
  digitalWrite(_FCSN_, HIGH);

  Serial.println("READY");
  while (!Serial.find("GO\n"));

  // Put nRF24LE1 into programming mode
  digitalWrite(PROG, HIGH);
  digitalWrite(_RESET_, LOW);
  delay(10);
  digitalWrite(_RESET_, HIGH);

  delay(10);

  // Set InfoPage enable bit so all flash is erased
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);

  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WRSR);
  SPI.transfer(fsr | FSR_INFEN);
  digitalWrite(_FCSN_, HIGH);

  // Check InfoPage enable bit was set
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);

  if (!(fsr & FSR_INFEN)) {
    Serial.println("INFOPAGE ENABLE FAILED");
    goto done;
  }

  // Erase flash
  Serial.println("ERASING FLASH...");
  // Set flash write enable latch
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WREN);
  digitalWrite(_FCSN_, HIGH);

  // Erase all flash pages
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(ERASE_ALL);
  digitalWrite(_FCSN_, HIGH);  

  // Check flash is ready
  do {
    delay(600);
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(RDSR);
    fsr = SPI.transfer(0x00);
    digitalWrite(_FCSN_, HIGH);
  } 
  while (fsr & FSR_RDYN);  

  // Restore InfoPage content
  // Set flash write enable latch
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WREN);
  digitalWrite(_FCSN_, HIGH);

  // Write InfoPage content
  Serial.println("RESTORING INFOPAGE....");
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(PROGRAM);
  SPI.transfer(0);
  SPI.transfer(0);
  for (int index = 0; index < 37; index++) {
    SPI.transfer(infopage[index]);
  }
  digitalWrite(_FCSN_, HIGH);      

  // Check flash is ready
  do {
    delay(10);
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(RDSR);
    fsr = SPI.transfer(0x00);
    digitalWrite(_FCSN_, HIGH);
  } 
  while (fsr & FSR_RDYN);

  // Read back data to verify flash write
  Serial.println("VERFIYING INFOPAGE....");
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(READ);
  SPI.transfer(0);
  SPI.transfer(0);
  for (int index = 0; index < 37; index++) {
    spi_data = SPI.transfer(0x00);
    if (infopage[index] != spi_data) {
      Serial.print("INFOPAGE VERIFY FAILED ");
      Serial.print(index);
      Serial.print(": WROTE ");
      Serial.print(infopage[index]);
      Serial.print(" READ ");
      Serial.println(spi_data);
      goto done;
    }
  }
  digitalWrite(_FCSN_, HIGH);  

  // Clear InfoPage enable bit so main flash block is programed
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);

  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WRSR);
  SPI.transfer(fsr & ~FSR_INFEN);
  digitalWrite(_FCSN_, HIGH);

  // Check InfoPage enable bit was cleared
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);

  if (fsr & FSR_INFEN) {
    Serial.println("INFOPAGE DISABLE FAILED");
    goto done;
  }




done:
  // Take nRF24LE1 out of programming mode
  digitalWrite(PROG, LOW);
  digitalWrite(_RESET_, LOW);
  delay(10);
  digitalWrite(_RESET_, HIGH);
  
  SPI.end();

  Serial.println("DONE");
}



void loop() {
  // Do nothing
  delay(1000);
}






