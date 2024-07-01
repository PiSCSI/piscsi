
# pinctrl

pinctrl is a more powerful replacement for raspi-gpio, a tool for displaying
and modifying the GPIO and pin muxing state of a system. It accesses the
hardware directly, bypassing the kernel drivers, and as such requires root
privilege (run with "sudo").

The improvements over raspi-gpio include:

* GPIO controllers are detected at runtime from Device Tree.
* GPIOs can be referred to by name or number.
* Pin mode (-p) switches the UI to be in terms of 40-way header pin numbers.
* The "poll" command causes it to constantly monitor the specified pins,
  displaying any level changes it sees. For slow signals (up to a few hundred
  kHz) it can act as a basic logic analyser.
* The "get" and "set" keywords are optional in most cases.
* Splitting into a general gpiolib library and a separate client application
  allows new applications to be added easily.

**Build Instructions**

Install the prerequisites with "sudo apt install cmake" - you need at least version 3.10 of cmake. Run the following commands, either here or in the top-level directory to build and install everything:

 - *cmake .*
 - *make*
 - *sudo make install*

**Usage**

* `sudo pinctrl`              (Display the state of all recognised GPIOs)
* `sudo pinctrl -p`           (Show the state of the 40-way header pins)
* `sudo pinctrl 4,6 op dl`    (Make GPIOs 4 and 6 outputs, driving low)
* `sudo pinctrl poll BT_CTS,BT_RTS`    (Monitor the levels of the Bluetooth flow control signals)
* `pinctrl funcs 9-11`        (List the available alternate functions on GPIOs 9, 10 and 11)
* `pinctrl help`              (Show the full usage guide)
