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

#include <SPI.h>
#include <SoftwareSerial.h>

#include "../common/common.c"

// nRF24LE1 Serial port connections.  These will differ with the different chip
// packages
#define nRF24LE1_TXD   10   // nRF24LE1 UART/TXD
#define nRF24LE1_RXD    7   // nRF24LE1 UART/RXD

SoftwareSerial nRF24LE1Serial(nRF24LE1_TXD, nRF24LE1_RXD);
#define NRF24LE1_BAUD  19200

#define FLASH_TRIGGER   0x01    // Magic character to trigger uploading of flash
#define READ_INFOPAGE_TRIGGER 0x02
#define READ_MAINPAGE_TRIGGER 0x04
#define RESTORE_INFOPAGE_TRIGGER 0x05


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

void flash() {
  bool rewrite_info_page;

  Serial.println("FLASH");

  // Initialise control pins
  progSetup();

  Serial.println("READY");
  if (!Serial.find("GO\n")) {
    Serial.println("TIMEOUT");
    return;
  }

  // Put nRF24LE1 into programming mode
  progStart();

  if (readFSR() == 255) {
    Serial.println("FSR failed to read (FSR=255), check your wiring!");
    goto done;
  }

  // Set InfoPage enable bit so all flash is erased
  if (!enableInfoPage())
    goto done;

  // Read InfoPage content so it can be restored after erasing flash
  Serial.println("SAVING INFOPAGE...");
  flash_read(infopage, 37, 0, 0);
  for (int index = 0; index < 37; index++) {
    Serial.print("INFO ");
    Serial.print(index);
    Serial.print(": ");
    Serial.print(infopage[index]);
    Serial.print("\n");
  }

  rewrite_info_page = true;
  if (infopage[35] == 0xFF && infopage[32] == 0xFF) {
    Serial.println("SKIPPING REWRITE OF INFOPAGE");
    rewrite_info_page = false;
  }

  if (!rewrite_info_page)
    disableInfoPage();

  // Erase flash
  Serial.println("ERASING FLASH...");
  flash_erase_all();

  if (rewrite_info_page) {
    // Restore InfoPage content
    // Clear Flash MB readback protection (RDISMB)
    infopage[35] = 0xFF;
    // Set all pages unprotected (NUPP)
    infopage[32] = 0xFF;

    Serial.println("RESTORING INFOPAGE....");

    // Write back InfoPage content
    flash_program_verify("INFOPAGE", infopage, 37, 0, 0);

    // Clear InfoPage enable bit so main flash block is programed
    if (!disableInfoPage())
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

    // Program and verify data
    if (!flash_program_verify("DATA", hexRecord.rec_data, hexRecord.rec_data_len, highByte(hexRecord.rec_address), lowByte(hexRecord.rec_address)))
      break;
  }

done:
  // Take nRF24LE1 out of programming mode
  progEnd();
  SPI.end();
  Serial.println("DONE");
}

void read_infopage() {
  progSetup();

  Serial.println("READY");
  // Wait for GO command from Serial
  while (!Serial.find("GO")) {
    Serial.println("READY");
  }
  Serial.println("PREPARING");
  delay(1000);
  Serial.println("SETTING UP");

  // Put nRF24LE1 into programming mode
  progStart();

  // Set InfoPage bit so InfoPage flash is read
  if (!enableInfoPage())
    goto done;

  // Read InfoPage contents
  Serial.println("READING...");
  byte buf[37];
  flash_read(buf, sizeof(buf), 0, 0);
  for (int index = 0; index < (int)sizeof(buf); index++) {
    Serial.print(index);
    Serial.print(": ");
    Serial.println(buf[index]);
  }

done:
  progEnd();
  SPI.end();
  Serial.println("DONE");
}

void read_mainpage() {
  progSetup();

  Serial.println("READY");
  // Wait for GO command from Serial
  while (!Serial.find("GO")) {
    Serial.println("READY");
  }
  Serial.println("PREPARING");
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

  // Take nRF24LE1 out of programming mode
  progEnd();
  SPI.end();
  Serial.println("DONE");
}


static byte restore_infopage_data[37] =
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

void restore_infopage() {
  progSetup();

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
  flash_program_verify("INFOPAGE", restore_infopage_data, 37, 0, 0);

  // Clear InfoPage enable bit so main flash block is programed
  disableInfoPage();

done:
  // Take nRF24LE1 out of programming mode
  progEnd();
  SPI.end();
  Serial.println("DONE");
}


void setup() {
  // start serial port:
  Serial.begin(57600);
  Serial.setTimeout(30000);

  // Reset nRF24LE1
  pinMode(PROG, OUTPUT);
  digitalWrite(PROG, LOW);
  pinMode(_RESET_, OUTPUT);
  digitalWrite(_RESET_, HIGH);
  delay(10);
  digitalWrite(_RESET_, LOW);
  delay(10);
  digitalWrite(_RESET_, HIGH);


  nRF24LE1Serial.begin(NRF24LE1_BAUD);
}

char serialBuffer;

void loop() {

  if (nRF24LE1Serial.available() > 0) {
    // Pass through serial data receieved from the nRF24LE1
    Serial.write(nRF24LE1Serial.read());
  }

  if (Serial.available() > 0) {
    serialBuffer = Serial.read();
    // Check if data received on USB serial port is the magic character to start flashing
    switch (serialBuffer) {
    case FLASH_TRIGGER:
      nRF24LE1Serial.end();
      flash();
      nRF24LE1Serial.begin(NRF24LE1_BAUD);
      break;

    case READ_INFOPAGE_TRIGGER:
      nRF24LE1Serial.end();
      read_infopage();
      nRF24LE1Serial.begin(NRF24LE1_BAUD);
      break;

    case READ_MAINPAGE_TRIGGER:
      nRF24LE1Serial.end();
      read_mainpage();
      nRF24LE1Serial.begin(NRF24LE1_BAUD);
      break;

    case RESTORE_INFOPAGE_TRIGGER:
      nRF24LE1Serial.end();
      restore_infopage();
      nRF24LE1Serial.begin(NRF24LE1_BAUD);
      break;

    default:
      // Otherwise pass through serial data received
      nRF24LE1Serial.write(serialBuffer);
    }
  }
}
