Ch341Prog
============
A simple command line programmer tool for interfacing with ch341a under Linux and macOS.

Description
------------
There are a lot of cheap SPI/IIC programmers based on ch341a which can be bought
online for as low as $3. However, there were no Linux-based tools associated
with the hardware; the best I could find was a tool called `ch341eepromtool`
(original link dead), which could only deal with IIC EEPROMs. So, I
decided to write my own.

After reading the source for ch341eeprom and sniffing the USB traffic,
I believe I have fully understood the USB protocol. Here is what I have
got so far.

Special thanks to asbokid, the ch341eepromtool's author.
Please see the CONTRIBUTORS file for a full list of people who helped build this.

Build Instructions
------------------
This project uses CMake. To build the project, run:

```bash
mkdir build
cd build
cmake ..
make
```

License
------------
This is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.

Speed Test
-----------
 * Reading an 8M flash (W25Q64) took 63 seconds
 * Writing to an 8M flash (W25Q64) took 86 seconds

Hardware Permissions
------------------
To access the CH341A programmer as a non-root user, you must install the appropriate hardware rules for your operating system.

### Linux (udev)
Run the following command to install the udev rule file into /etc/udev/rules.d:
```bash
sudo make install-rules
```

### FreeBSD (devd)
Run the following command to install the devd configuration file into /usr/local/etc/devd:
```bash
sudo make install-rules
```

Note: After running the install command on either system, you must unplug the CH341A device and plug it back in for the new permissions to take effect.
