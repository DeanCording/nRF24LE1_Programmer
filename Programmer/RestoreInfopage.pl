#!/usr/bin/perl

# Programmer.pl - Program to feed Intel HEX files produced by SDCC to the nRF24LE1 Arduino
# programmer sketch.

#  Copyright (c) 2014 Dean Cording
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#  THE SOFTWARE.

use strict;
use warnings;


if (@ARGV != 1) {
  print "Usage: $0 <Arduino Serial Port>\n";
  exit;
}

# Serial port settings to suit Arduino
system "stty -F $ARGV[0] 10:0:18b1:0:3:1c:7f:15:4:0:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0";


open(SERIAL, "+<", $ARGV[0]) or die "Cannot open $ARGV[0]: $!";

#Wait for Arduino reset
sleep(3);

#Send the restore infopage trigger character
print SERIAL "\x05";

do {
  while (!defined($_ = <SERIAL>)) {}
  print;
  chomp;
} until /READY/;

print SERIAL "GO\n";

while (1) {

  while (!defined($_ = <SERIAL>)) {}

  print;
  chomp;

  last if /DONE/;
}

close(SERIAL);
