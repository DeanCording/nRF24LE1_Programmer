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
 * D07: nRF24LE1 UART/RXD
 * D08: nRF24LE1 PROG
 *~D09: nRF24LE1 _RESET_
 *~D10: nRF24LE1 FCSN, nRF24LE1 UART/TXD (P0.5)
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

// Specify pins in use
#define PROG      8   // nRF24LE1 Program
#define _RESET_   9   // nRF24LE1 Reset
#define _FCSN_    10  // nRF24LE1 Chip select

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

/* NOTE: The InfoPage area DSYS are used to store nRF24LE1 system and tuning
 * parameters. Erasing the content of this area WILL cause changes to device
 * behavior and performance. InfoPage area DSYS should ALWAYS be read out and
 * stored prior to using ERASE ALL. Upon completion of theerase the DSYS
 * information must be written back to the flash InfoPage.
 *
 * Use the Read_Infopage sketch to make a backup.
 */

// Flash Status Register (FSR) bits
#define FSR_STP                 B01000000  // Enable code execution start from protected flash area
#define FSR_WEN                 B00100000  // Write enable latch
#define FSR_RDYN                B00010000  // Flash ready flag - active low
#define FSR_INFEN               B00001000  // Flash InfoPage enable

/* Setup the programmer, initialize the SPI and set the PROG, RESET and CSN bits. */
void progSetup() {
  // Initialise SPI
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV4);
  SPI.begin();

  pinMode(PROG, OUTPUT);
  digitalWrite(PROG, LOW);

  pinMode(_RESET_, OUTPUT);
  digitalWrite(_RESET_, HIGH);

  pinMode(_FCSN_, OUTPUT);
  digitalWrite(_FCSN_, HIGH);
}

/* Set the programming state, LOW is out of programming and HIGH is in programming mode */
void progSet(uint8_t val) {
  digitalWrite(PROG, val);
  Serial.print("PROG SET ");
  Serial.println(val);
  digitalWrite(_RESET_, LOW);
  delay(10);
  digitalWrite(_RESET_, HIGH);
  delay(10);
}

#define progStart() progSet(HIGH)
#define progEnd() progSet(LOW)

/* Read the FSR byte */
byte readFSR() {
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(RDSR);
  byte fsr = SPI.transfer(0x00);
  digitalWrite(_FCSN_, HIGH);
  return fsr;
}

/* Write the FSR byte */
void writeFSR(byte val) {
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(WRSR);
  SPI.transfer(val);
  delay(1);
  digitalWrite(_FCSN_, HIGH);
}

bool enableInfoPage() {
  byte fsr1 = readFSR();
  if (fsr1 == 255) {
    Serial.println("FSR READ AS 255, CHECK YOUR WIRING!");
    return false;
  }

  writeFSR(fsr1 | FSR_INFEN);

  byte fsr2 = readFSR();
  if ((fsr2 & FSR_INFEN) == 0) {
    Serial.print("INFOPAGE ENABLE FAILED (FSR BEFORE = ");
    Serial.print(fsr1);
    Serial.print(", AFTER = ");
    Serial.print(fsr2);
    Serial.println(")");
    return false;
  }

  return true;
}

bool disableInfoPage() {
  byte fsr1 = readFSR();
  writeFSR(fsr1 & ~FSR_INFEN);
  if (fsr1 == 255) {
    Serial.println("FSR READ AS 255, CHECK YOUR WIRING!");
    return false;
  }

  byte fsr2 = readFSR();
  if ((fsr2 & FSR_INFEN) != 0) {
    Serial.print("INFOPAGE DISABLE FAILED (FSR BEFORE = ");
	Serial.print(fsr1);
	Serial.print(", AFTER = ");
	Serial.print(fsr2);
	Serial.println(")");
    return false;
  }

  return true;
}

void waitForFlashReady(int msec) {
  // Check flash is ready
  byte i = 0;
  byte fsr = 0;
  do {
    delay(msec);
    fsr = readFSR();
	i++;
  }
  while (fsr & FSR_RDYN);
  if (i > 1) {
	  Serial.print("WAITED FOR FLASH READY ");
	  Serial.print(i);
	  Serial.println(" ITERATIONS");
  }
}

/* Send a single byte command */
void singleCmd(byte val) {
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(val);
  digitalWrite(_FCSN_, HIGH);
}

void flash_program(byte *buf, int buflen, byte addr_high, byte addr_low)
{
  int i;

  singleCmd(WREN);
  waitForFlashReady(1);

  digitalWrite(_FCSN_, LOW);
  SPI.transfer(PROGRAM);
  SPI.transfer(addr_high);
  SPI.transfer(addr_low);
  for (i = 0; i < buflen; i++) {
	SPI.transfer(buf[i]);
  }
  delay(1);
  digitalWrite(_FCSN_, HIGH);
}

void flash_read(byte *buf, int buflen, byte addr_high, byte addr_low)
{
  digitalWrite(_FCSN_, LOW);
  SPI.transfer(READ);
  SPI.transfer(addr_high);
  SPI.transfer(addr_low);
  for (int index = 0; index < buflen; index++) {
    buf[index] = SPI.transfer(0x00);
  }
  digitalWrite(_FCSN_, HIGH);
}

bool flash_verify(byte *buf, int buflen, byte addr_high, byte addr_low)
{
  int index;
  bool res = true;

  digitalWrite(_FCSN_, LOW);
  SPI.transfer(READ);
  SPI.transfer(addr_high);
  SPI.transfer(addr_low);
  for (index = 0; index < buflen; index++) {
    byte val = SPI.transfer(0x00);
    if (val != buf[index]) {
      Serial.print("VERIFY FAILED ");
      Serial.print(index);
      Serial.print(": WROTE ");
      Serial.print(buf[index]);
      Serial.print(" READ ");
      Serial.println(val);
      res = false;
      break;
    }
  }

  digitalWrite(_FCSN_, HIGH);
  return res;
}

bool flash_program_verify(const char *name, byte *buf, int buflen, byte addr_high, byte addr_low)
{
  Serial.print("PROGRAM ");
  Serial.println(name);
  singleCmd(WREN);
  flash_program(buf, buflen, addr_high, addr_low);
  waitForFlashReady(buflen); // 1msec per byte written
  Serial.print("VERIFY ");
  Serial.println(name);
  return flash_verify(buf, buflen, addr_high, addr_low);
}

void flash_erase_all(void)
{
  // Set flash write enable latch
  singleCmd(WREN);

  // Erase all flash pages
  singleCmd(ERASE_ALL);

  waitForFlashReady(60);
}
