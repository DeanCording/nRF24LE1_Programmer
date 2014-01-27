/*
 Programmer.ino
 
 Programmer for flashing Nordic nRF24LE1 SOC RF chips using SPI interface from an Arduino.
 
 Data to write to flash is fed from standard SDCC produced Intel Hex format file using the 
 accompanying programmer.pl perl script.  Start the Arduino script first and then run the 
 perl script.
 
 When uploading, make sure serial port speed is set to 57600 baud.
 
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
 * D07: 
 * D08: nRF24LE1 PROG
 *~D09: nRF24LE1 _RESET_
 *~D10: nRF24LE1 FCSN
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
#define PROG      8   // nRF24LE1 Program
#define _RESET_   9   // nRF24LE1 Reset
#define _FCSN_    10  // nRF24LE1 Chip select

#define TXD       10  // nRF24LE1 UART/TXD
#define RXD       7   // nRF24LE1 UART/RXD

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
 *
 * Use the Read_Infopage sketch to make a backup.
 */

// Flash Status Register (FSR) bits
#define FSR_STP                 B01000000  // Enable code execution start from protected flash area
#define FSR_WEN                 B00100000  // Write enable latch
#define FSR_RDYN                B00010000  // Flash ready flag - active low
#define FSR_INFEN               B00001000  // Flash InfoPage enable


// Hex file processing definitions
#define HEX_REC_START_CODE             ':'

#define HEX_REC_TYPE_DATA               0
#define HEX_REC_TYPE_EOF                1
#define HEX_REC_TYPE_EXT_SEG_ADDR	2
#define HEX_REC_TYPE_EXT_LIN_ADDR	4

#define	HEX_REC_OK                      0
#define HEX_REC_ILLEGAL_CHARS          -1
#define HEX_REC_BAD_CHECKSUM           -2
#define HEX_REC_NULL_PTR               -3
#define HEX_REC_INVALID_FORMAT         -4
#define HEX_REC_EOF                    -5

char inputRecord[521]; // Buffer for line of encoded hex data

typedef struct hexRecordStruct {
  byte   rec_data[256];
  byte   rec_data_len;
  word   rec_address;
  byte   rec_type;
  byte   rec_checksum;
  byte   calc_checksum;	
} 
hexRecordStruct;

hexRecordStruct hexRecord;  // Decoded hex data

size_t numChars;   // Serial characters received counter
byte fsr;          // Flash status register buffer
byte spi_data;     // SPI data transfer buffer
byte infopage[37]; // Buffer for storing InfoPage content

void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.setTimeout(30000);

  // Initialise SPI
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4);

  // Initialise control pins
  pinMode(PROG, OUTPUT);
  digitalWrite(PROG, LOW);
  pinMode(_RESET_, OUTPUT);
  digitalWrite(_RESET_, HIGH);
  pinMode(_FCSN_, OUTPUT);
  digitalWrite(_FCSN_, HIGH);

}


void loop() {

  Serial.println("READY");
  while (!Serial.find("GO\n"));

  SPI.begin();

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
  delay(1);
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

  // Read InfoPage content so it can be restored after erasing flash
  Serial.println("SAVING INFOPAGE...");
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(READ);
  SPI.transfer(0);
  SPI.transfer(0);
  for (int index = 0; index < 37; index++) {
    infopage[index] = SPI.transfer(0x00);
  }
  digitalWrite(_FCSN_, HIGH);  

  // Erase flash
  Serial.println("ERASING FLASH...");
  // Set flash write enable latch
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WREN);
  delay(1);
  digitalWrite(_FCSN_, HIGH);

  // Erase all flash pages
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(ERASE_ALL);
  delay(1);
  digitalWrite(_FCSN_, HIGH);  

  // Check flash is ready
  do {
    delay(60);
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(RDSR);
    fsr = SPI.transfer(0x00);
    digitalWrite(_FCSN_, HIGH);
  } 
  while (fsr & FSR_RDYN);  

  // Restore InfoPage content
  // Clear Flash MB readback protection (RDISMB)
  infopage[35] = 0xFF;
  // Set all pages unprotected (NUPP)
  infopage[32] = 0xFF;

  Serial.println("RESTORING INFOPAGE....");
  // Set flash write enable latch
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WREN);
  delay(1);
  digitalWrite(_FCSN_, HIGH);

  // Write back InfoPage content
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(PROGRAM);
  SPI.transfer(0);
  SPI.transfer(0);
  for (int index = 0; index < 37; index++) {
    SPI.transfer(infopage[index]);
  }
  delay(1);
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

  // Verify data that was written
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
      digitalWrite(_FCSN_, HIGH);  
      goto done;
    }
  }
  delay(1);
  digitalWrite(_FCSN_, HIGH);  

  // Clear InfoPage enable bit so main flash block is programed
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);

  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WRSR);
  SPI.transfer(fsr & ~FSR_INFEN);
  delay(1);
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


  while(true){
    // Prompt perl script for data
    Serial.println("OK");

    numChars = Serial.readBytesUntil('\n', inputRecord, 512);

    if (numChars == 0) {
      Serial.println("TIMEOUT");
      goto done;
    }

    switch (ParseHexRecord(&hexRecord, inputRecord, numChars)){
    case HEX_REC_OK:
      break;
    case HEX_REC_ILLEGAL_CHARS:
      Serial.println("ILLEGAL CHARS");
      goto done;
    case HEX_REC_BAD_CHECKSUM:
      Serial.println("BAD CHECKSUM");
      goto done;
    case HEX_REC_NULL_PTR:
      Serial.println("NULL PTR");
      goto done;
    case HEX_REC_INVALID_FORMAT:
      Serial.println("INVALID FORMAT");
      goto done;
    case HEX_REC_EOF:
      Serial.println("EOF");
      goto done;
    }

    // Set flash write enable latch
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(WREN);
    delay(1);
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

    // Program flash
    Serial.println("WRITING...");
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(PROGRAM);
    SPI.transfer(highByte(hexRecord.rec_address));
    SPI.transfer(lowByte(hexRecord.rec_address));
    for (int index = 0; index < hexRecord.rec_data_len; index++) {
      SPI.transfer(hexRecord.rec_data[index]);
    }
    delay(1);
    digitalWrite(_FCSN_, HIGH);


    // Wait for flash to write
    do {
      delay(hexRecord.rec_data_len);  // Wait 1 millisecond per byte written
      digitalWrite(_FCSN_, LOW);
      SPI.transfer(RDSR);
      fsr = SPI.transfer(0x00);
      digitalWrite(_FCSN_, HIGH);
    } 
    while (fsr & FSR_RDYN);

    // Read back flash to verify
    Serial.println("VERIFYING...");
    digitalWrite(_FCSN_, LOW);
    SPI.transfer(READ);
    SPI.transfer(highByte(hexRecord.rec_address));
    SPI.transfer(lowByte(hexRecord.rec_address));
    for (int index = 0; index < hexRecord.rec_data_len; index++) {
      spi_data = SPI.transfer(0x00);
      if ( spi_data != hexRecord.rec_data[index]) {
        Serial.print("FAILED ");
        Serial.print(hexRecord.rec_address + index);
        Serial.print(": ");
        Serial.print(spi_data);
        Serial.print(" ");
        Serial.println(hexRecord.rec_data[index]);
        digitalWrite(_FCSN_, HIGH);      
        goto done;
      }
    }
    digitalWrite(_FCSN_, HIGH);      

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

int ParseHexRecord(struct hexRecordStruct * record, char * inputRecord, int inputRecordLen){
  int index = 0;

  if((record == NULL) || (inputRecord == NULL)) {
    return HEX_REC_NULL_PTR;		
  }

  if(inputRecord[0] != HEX_REC_START_CODE) {
    return HEX_REC_INVALID_FORMAT;
  }

  record->rec_data_len = ConvertHexASCIIByteToByte(inputRecord[1], inputRecord[2]);
  record->rec_address = word(ConvertHexASCIIByteToByte(inputRecord[3], inputRecord[4]), ConvertHexASCIIByteToByte(inputRecord[5], inputRecord[6]));
  record->rec_type = ConvertHexASCIIByteToByte(inputRecord[7], inputRecord[8]);
  record->rec_checksum = ConvertHexASCIIByteToByte(inputRecord[9 + (record->rec_data_len * 2)], inputRecord[9 + (record->rec_data_len * 2) + 1]);
  record->calc_checksum = record->rec_data_len + ((record->rec_address >> 8) & 0xFF) + (record->rec_address & 0xFF) + record->rec_type;

  for(index = 0; index < record->rec_data_len; index++) {
    record->rec_data[index] = ConvertHexASCIIByteToByte(inputRecord[9 + (index * 2)], inputRecord[9 + (index * 2) + 1]);
    record->calc_checksum += record->rec_data[index];
  }

  record->calc_checksum = ~record->calc_checksum + 1;

  if(record->calc_checksum != record->rec_checksum) {
    return HEX_REC_BAD_CHECKSUM;
  }

  if (record->rec_type == HEX_REC_TYPE_EOF) {
    return HEX_REC_EOF;
  }

  return HEX_REC_OK;
}


byte ConvertHexASCIIDigitToByte(char c){
  if((c >= 'a') && (c <= 'f'))
    return (c - 'a') + 0x0A;
  else if ((c >= 'A') && (c <= 'F'))
    return (c - 'A') + 0x0A;
  else if ((c >= '0') && (c <= '9'))
    return (c - '0');
  else
    return -1;
}

byte ConvertHexASCIIByteToByte(char msb, char lsb){
  return ((ConvertHexASCIIDigitToByte(msb) << 4) + ConvertHexASCIIDigitToByte(lsb));
}






