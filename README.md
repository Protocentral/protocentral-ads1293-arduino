Protocentral ADS1293, 3-Channel, 24-Bit Analog Front-End sensor breakout board
================================

[![Compile Examples](https://github.com/Protocentral/protocentral-ads1293-arduino/workflows/Compile%20Examples/badge.svg)](https://github.com/Protocentral/protocentral-ads1293-arduino/actions?workflow=Compile+Examples) 

## Don't have one? [Buy it here](https://protocentral.com/product/protocentral-ads1293-breakout-board/)

![ADS1293 Breakout](/docs/assets/product_image.jpg) 

The ADS1293 incorporates all features commonly required in portable, low-power medical, sports, and fitness electrocardiogram (ECG) applications. The ADS1293 features three high-resolution channels, each channel can be independently programmed for a specific sample rate and bandwidth allowing users to optimize the configuration for performance and power. A fourth channel allows external analog pace detection for applications that do not use digital pace detection.

The ADS1293 incorporates a self-diagnostics alarm
system to detect when the system is out of the operating conditions range. 


## Hardware Setup
Connection with the Arduino board is as follows:

|ADS1293 pin label| Arduino Connection  |Pin Function      |
 |:-----------------: |:---------------------:|:------------------:|
 | MISO             | 12  |Slave Out            | 
| MOSI             | 11      | Slave In             |
| SCLK             | 13 | Serial Clock         |  
| CS               | 10 | Chip Select          |  
| Vcc              | +5V | Power          |  
| GND              | GND | GND          | 
| DRDY             | 02 | Data ready           |  


For further details of the board, refer the documentation
<center>

[ ADS1293 breakout board Documentation](https://docs.protocentral.com/getting-started-with-ADS1293/)

</center>

License Information
===================

This product is open source! Both, our hardware and software are open source and licensed under the following licenses:

Hardware
---------

**All hardware is released under [Creative Commons Share-alike 4.0 International](http://creativecommons.org/licenses/by-sa/4.0/).**
![CC-BY-SA-4.0](https://i.creativecommons.org/l/by-sa/4.0/88x31.png)

You are free to:

* Share — copy and redistribute the material in any medium or format
* Adapt — remix, transform, and build upon the material for any purpose, even commercially.
The licensor cannot revoke these freedoms as long as you follow the license terms.

Under the following terms:

* Attribution — You must give appropriate credit, provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
* ShareAlike — If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

Software
--------

**All software is released under the MIT License(http://opensource.org/licenses/MIT).**

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


Please check [*LICENSE.md*](LICENSE.md) for detailed license descriptions.

