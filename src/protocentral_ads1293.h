//////////////////////////////////////////////////////////////////////////////////////////
//
//  Demo code for the ads1293 board
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


#ifndef protocentral_ads1293_h_
#define protocentral_ads1293_h_

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
#define   SYNCB_CN     0x28
#define   CH_CNFG       0x2f
#define   REVID         0x40


class ads1293
{
  public:
    uint8_t drdyPin;
    uint8_t csPin;

    void ads1293Begin3LeadECG();
    void ads1293Begin5LeadECG();
    void ads1293Begin12LeadECGmaster();
    int32_t getECGdata(uint8_t channel);
    bool readSensorID();
    void setAds1293Pins();
    uint8_t ads1293ReadRegister(uint8_t rdAddress);

    ads1293(uint8_t drdy, uint8_t chipSelect){
      csPin = chipSelect;
      drdyPin = drdy;
    }

  private:
    void ads1293WriteRegister(uint8_t wrAddress, uint8_t data);

    void configDCleadoffDetect();
    void configACleadoffDetect();
    void setSamplingRate();
};


#endif
