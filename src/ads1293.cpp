#include "Arduino.h"
#include <SPI.h>
#include "ads1293.h"


int32_t ads1293::getECGdata(uint8_t channel){

    uint8_t rawData[3];
    int32_t ecgData;
    
    if(channel < 1 || channel > 3){
      channel = 0;    //defaults to channe; on wrong input 
    }else channel -= 1;
    
    rawData[0] = ads1293ReadRegister(0x37 + (channel * 3));
    rawData[1] = ads1293ReadRegister(0x38 + (channel * 3));
    rawData[2] = ads1293ReadRegister(0x39 + (channel * 3));

    uint32_t tempData = (uint32_t)rawData[0]<<16;
    tempData = (uint32_t)rawData[1]<< 8;
    tempData |= rawData[2]; 
    tempData = tempData << 8;

    ecgData = (int32_t) (tempData);
    return (ecgData >> 8);
}


void ads1293::ads1293Begin(){

  pinMode(drdyPin, INPUT_PULLUP);
  pinMode(csPin,   OUTPUT);

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