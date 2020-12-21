//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
//  This example plots the ecg through arduino plotter.
//  Copyright (c) 2020 ProtoCentral
//
//  Arduino uno connections:
//
//  |MAX30003 pin label| Pin Function         |Arduino Connection|
//  |----------------- |:--------------------:|-----------------:|
//  | MISO             | Slave Out            |  12              |
//  | MOSI             | Slave In             |  11              |
//  | SCLK             | Serial Clock         |  13              |
//  | CS               | Chip Select          |  10              |
//  | VCC              | Digital VDD          |  +5V             |
//  | GND              | Digital Gnd          |  Gnd             |
//  | DRDY             | Data ready           |  02              |
//
//  This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//  For information on how to use, visit https://github.com/Protocentral/protocentral-ads1293-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////


#include "ads1293.h"
#include <SPI.h>

ads1293 ADS1293(02/*DRDY PIN*/, 10/*CS Pin*/);

bool drdyIntFlag = false;

void drdyInterruptHndlr(){
  //Serial.println("i");
  drdyIntFlag = true;
}

void enableInterruptPin(){
//ToDo

 // pinMode(ADS1293.drdyPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ADS1293.drdyPin), drdyInterruptHndlr, FALLING);
}


void setup() {

  Serial.begin(9600);
  SPI.begin();

  ADS1293.ads1293Begin3LeadECG();
  //enableInterruptPin();
  delay(10);
}

void loop() {

  //if (drdyIntFlag) {
  if (digitalRead(ADS1293.drdyPin) == false){

    drdyIntFlag = false;
    int32_t ecg = ADS1293.getECGdata(1);

    Serial.println(ecg);
  }
}
