# ADS1293,3-Channel,24-BitAnalogFront-End

![ads1293](https://github.com/Protocentral/protocentral_ads1293/blob/main/docs/img/IMG_20201109_210016288.jpg)

If you don't already have one, you can buy [ProtoCentral ADS1293 Breakout here.](https://protocentral.com/product/protocentral-ads1293-breakout-board/)

Features:
* Three High-Resolution Digital ECG Channels With Simultaneous Pace Output 
* Low Power: 0.3 mW/channel 
* Differential Input Voltage Range: ±400 mV
* On board 40 MHz clock source 
* Logic level selection between 3.3 and 5 V through jumper.

Includes:
----------
* 1x ProtoCentral ADS1293 breakout board
* 1x Set of stackable Arduino headers
* 2x ECG electrode cables
* 20x disposable ECG electrodes

Wiring the Breakout to your Arduino
------------------------------------
 If you have bought the breakout the connection with the Arduino board is as follows:
 
|pin label         | Pin Function         |Arduino Connection|
|----------------- |:--------------------:|-----------------:|
| MISO             | Slave Out            |  12              |
| MOSI             | Slave In             |  11              |
| SCLK             | Serial Clock         |  13              |
| CS               | Chip Select          |  10              |
| VCC              | Digital VDD          |  +5V             |
| GND              | Digital Gnd          |  Gnd             |
| DRDY             | Data ready           |  02              |


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

