Ch341Prog
============
A simple programmer based on ch341a multi-functional chip.

Description
------------
There are a lot cheap SPI/IIC programmers based on ch341a, which can be
as low as $3.  However, the best I can find is a tool called `ch341eeprom'
which can only deal with IIC EEPROMs, so I decide to write my own.

After reading the source for ch341eeprom and sniffering the usb traffic,
I thinks I have fully understood the usb protocol. So, here is what I have
got so far.

TODO List
------------
 * find and configure ch341 (done).
 * read flash ID (done).
 * read flash contents (done).
 * clear flash
 * blank check
 * write to flash memory
