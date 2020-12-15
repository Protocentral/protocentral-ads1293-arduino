#ifndef ads1293_h_
#define ads1293_h_

#define WREG 0x7f
#define RREG 0x80

#define   CONFIG  0x00
#define   FLEX_CH1_CN   0x01
#define   FLEX_CH2_CN   0x02
#define   FLEX_CH3_CN   0x03
#define   FLEX_PACE_CN  0x04
#define   FLEX_VBAT_CN  0x05
#define   LOD_CN        0x06
#define   LOD_EN        0x07
#define   LOD_CURRENT   0x08
#define   LOD_AC_CN     0x09
#define   CMDET_EN      0x0a
#define   CMDET_CN      0x0b
#define   RLD_CN        0x0c
#define   REF_CN        0x11
#define   OSC_CN        0x12
#define   AFE_RES       0x13
#define   AFE_SHDN_CN   0x14
#define   AFE_FAULT_CN  0x15
#define   AFE_PACE_CN   0x17
#define   R2_RATE       0x21
#define   R3_RATE_CH1   0x22
#define   R3_RATE_CH2   0x23
#define   R3_RATE_CH3   0x24
#define   R1_RATE       0x25
#define   DIS_EFILTER   0x26
#define   DRDYB_SRC     0x27
#define   CH_CNFG       0x2f
#define   REVID         0x40


class ads1293
{
  public:
    uint8_t drdyPin;
    uint8_t csPin;

    void ads1293Begin();
    int32_t getECGdata(uint8_t channel);
    bool readSensorID();
    
    ads1293(uint8_t drdy, uint8_t chipSelect){
      csPin = chipSelect;
      drdyPin = drdy;     
    }
    

  private:
    void ads1293WriteRegister(uint8_t wrAddress, uint8_t data);
    uint8_t ads1293ReadRegister(uint8_t rdAddress);
    void configDCleadoffDetect();
    void configACleadoffDetect();
    void setSamplingRate();
};


#endif
