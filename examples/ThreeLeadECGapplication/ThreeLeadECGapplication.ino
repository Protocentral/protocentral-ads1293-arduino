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
  
  if (ADS1293.readSensorID()){
    Serial.println("ads1292 sensor begin");
  }else{
    while(1){ //stay here
      Serial.println("sensor communication error");
      //Serial.println(ADS1293.ads1293ReadRegister(FLEX_CH1_CN));
      delay(10000);
    }
  }
  
  ADS1293.ads1293Begin();
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