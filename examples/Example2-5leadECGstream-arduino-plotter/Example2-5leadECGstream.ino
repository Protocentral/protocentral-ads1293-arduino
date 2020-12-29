//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
//  Author: Joice Tm
//  This example plots the ecg through arduino plotter.
//  Copyright (c) 2020 ProtoCentral
//
//  Arduino uno connections:
//
//  |pin label         | Pin Function         |Arduino Connection|
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


#include "protocentral_ads1293.h"
#include <SPI.h>

#define DRDY_PIN                02
#define CS_PIN                  06

ads1293 ADS1293(DRDY_PIN, CS_PIN);

bool drdyIntFlag = false;

void drdyInterruptHndlr(){
  //Serial.println("i");
  drdyIntFlag = true;
}

void enableInterruptPin(){

 // pinMode(ADS1293.drdyPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ADS1293.drdyPin), drdyInterruptHndlr, FALLING);
}


void setup() {

  Serial.begin(9600);
  SPI.begin();

  ADS1293.setAds1293Pins();
  ADS1293.ads1293Begin5LeadECG();
  enableInterruptPin();
  delay(10);
}

void loop() {

  if (drdyIntFlag) {
  //if (digitalRead(ADS1293.drdyPin) == false){

    drdyIntFlag = false;
    int32_t ecgCh1 = ADS1293.getECGdata(1);
    int32_t ecgCh2 = ADS1293.getECGdata(2);
    int32_t ecgCh3 = ADS1293.getECGdata(3);

    //Serial.print(ecgCh1);
    //Serial.print(",");

    //Serial.print(ecgCh2);
    //Serial.print(",");

    Serial.print(ecgCh3);
    Serial.println(",");
  }
}
