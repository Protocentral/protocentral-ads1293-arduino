//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
//  Author: Joice Tm
//  This example will plot the ECG through openview processing GUI.
//  GUI URL: https://github.com/Protocentral/protocentral_openview.git
//
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

#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA     0x02
#define CES_CMDIF_PKT_STOP      0x0B
#define DATA_LEN                16
#define ZERO                    0

#define DRDY_PIN                02
#define CS_PIN                  06

ads1293 ADS1293(DRDY_PIN, CS_PIN);

bool drdyIntFlag = false;

volatile char DataPacket[16];
const char DataPacketFooter[2] = {ZERO, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ZERO, CES_CMDIF_TYPE_DATA};

void drdyInterruptHndlr(){
  //Serial.println("i");
  drdyIntFlag = true;
}

void enableInterruptPin(){

 // pinMode(ADS1293.drdyPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ADS1293.drdyPin), drdyInterruptHndlr, FALLING);
}


void sendDataThroughUart(int32_t ecgCh1, int32_t ecgCh2, int32_t ecgCh3){

  DataPacket[0] = ecgCh1;
  DataPacket[1] = ecgCh1 >> 8;
  DataPacket[2] = ecgCh1 >> 16;
  DataPacket[3] = ecgCh1 >> 24;

  DataPacket[4] = ecgCh2;
  DataPacket[5] = ecgCh2 >> 8;
  DataPacket[6] = ecgCh2 >> 16;
  DataPacket[7] = ecgCh2 >> 24;

  DataPacket[8] = ecgCh3;
  DataPacket[9] = ecgCh3 >> 8;
  DataPacket[10] = ecgCh3 >> 16;
  DataPacket[11] = ecgCh3 >> 24;

  //send packet header
  for(int i=0; i<5; i++){

    Serial.write(DataPacketHeader[i]);
  }

  //send actual data
  for(int i=0; i<16; i++){

    Serial.write(DataPacket[i]);
  }

  //send packet footer
  for(int i=0; i<2; i++){

    Serial.write(DataPacketFooter[i]);
  }
}


void setup() {

  Serial.begin(9600);
  SPI.begin();

  ADS1293.setAds1293Pins();
  ADS1293.ads1293Begin5LeadECG();
  //enableInterruptPin();
  delay(10);
}

void loop() {

  //if (drdyIntFlag) {
  if (digitalRead(ADS1293.drdyPin) == false){

    drdyIntFlag = false;
    int32_t ecgCh1 = ADS1293.getECGdata(1);
    int32_t ecgCh2 = ADS1293.getECGdata(2);
    int32_t ecgCh3 = ADS1293.getECGdata(3);

    sendDataThroughUart(ecgCh1, ecgCh2, ecgCh3);
  }
}
