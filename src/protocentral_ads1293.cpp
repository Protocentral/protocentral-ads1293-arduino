//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
//
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


#include "Arduino.h"
#include <SPI.h>
#include "protocentral_ads1293.h"


int32_t ads1293::getECGdata(uint8_t channel){

  uint8_t rawData[3];
  int32_t ecgData;

  if(channel < 1 || channel > 3){
    return -1;    //defaults to channel 1 on wrong input
  }else {
    channel -= 1;
  }

  rawData[0] = ads1293ReadRegister(0x37 + (channel * 3));
  rawData[1] = ads1293ReadRegister(0x38 + (channel * 3));
  rawData[2] = ads1293ReadRegister(0x39 + (channel * 3));

  uint32_t tempData = (uint32_t)rawData[0]<<16;
  tempData = (uint32_t)rawData[1]<< 8;
  tempData |= rawData[2];
  tempData = tempData << 8;

  
  ecgData = (int32_t) (tempData); 
  return (ecgData );
}

void ads1293::setAds1293Pins(){

  pinMode(drdyPin, INPUT_PULLUP);
  pinMode(csPin,   OUTPUT);
}

void ads1293::ads1293Begin3LeadECG(){


  ads1293WriteRegister(FLEX_CH1_CN, 0x11);
  delay(1);

  ads1293WriteRegister(FLEX_CH2_CN, 0x19);
  delay(1);

  ads1293WriteRegister(CMDET_EN, 0x07);
  delay(1);

  ads1293WriteRegister(RLD_CN, 0x04);
  delay(1);

  ads1293WriteRegister(OSC_CN, 0x04);
  delay(1);

  ads1293WriteRegister(AFE_SHDN_CN, 0x24);
  delay(1);

  ads1293WriteRegister(R2_RATE, 0x02);
  delay(1);

  ads1293WriteRegister(R3_RATE_CH1, 0x02);
  delay(1);

  ads1293WriteRegister(R3_RATE_CH2, 0x02);
  delay(1);

  ads1293WriteRegister(DRDYB_SRC, 0x08);
  delay(1);

  ads1293WriteRegister(CH_CNFG, 0x30);
  delay(1);

  ads1293WriteRegister(CONFIG, 0x01);
  delay(1);
}

void ads1293::ads1293Begin5LeadECG(){

  ads1293WriteRegister(0x01, 0x11);
  delay(1);

  ads1293WriteRegister(0x02, 0x19);
  delay(1);

  ads1293WriteRegister(0x03, 0x2e);
  delay(1);

  ads1293WriteRegister(0x0a, 0x07);
  delay(1);

  ads1293WriteRegister(0x0c, 0x04);
  delay(1);

  ads1293WriteRegister(0x0d, 0x01);
  delay(1);

  ads1293WriteRegister(0x0e, 0x02);
  delay(1);

  ads1293WriteRegister(0x0f, 0x03);
  delay(1);

  ads1293WriteRegister(0x10, 0x01);
  delay(1);

  ads1293WriteRegister(0x12, 0x04);
  delay(1);

//debug
  ads1293WriteRegister(0x14, 0x04);
  delay(1);

  ads1293WriteRegister(0x21, 0x02);
  delay(1);

  ads1293WriteRegister(0x22, 0x02);
  delay(1);

  ads1293WriteRegister(0x23, 0x02);
  delay(1);

  ads1293WriteRegister(0x24, 0x02);
  delay(1);

  ads1293WriteRegister(0x27, 0x08);
  delay(1);

  ads1293WriteRegister(0x2f, 0x70);
  delay(1);

  ads1293WriteRegister(00, 0x01);
  delay(1);
}



void ads1293::ads1293WriteRegister(uint8_t wrAddress, uint8_t data){

  uint8_t dataToSend = (wrAddress & WREG);
  digitalWrite(csPin, LOW);
  SPI.transfer(dataToSend);
  SPI.transfer(data);
  digitalWrite(csPin, HIGH);
}

uint8_t ads1293::ads1293ReadRegister(uint8_t rdAddress){

  uint8_t rdData;
  uint8_t dataToSend = (rdAddress  | RREG);
  digitalWrite(csPin, LOW);
  SPI.transfer(dataToSend);
  rdData = SPI.transfer(0);
  digitalWrite(csPin, HIGH);

  return (rdData);
}

bool ads1293::readSensorID(){

  uint8_t ID=0xff;
  ID = ads1293ReadRegister(REVID);
  Serial.println(ID);
  if(ID == 0x01){
    return true;
  }else return false;
}


void ads1293::configDCleadoffDetect(){


}


void ads1293::configACleadoffDetect(){


}


void ads1293::setSamplingRate(){


}
