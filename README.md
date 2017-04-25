MotionLog
=====

<b>psychosonda_bt_ng192</b><br>
main source code<br>
<b>psychosonda_sd_bootloader192</b><br>
Bootloader inside the devices<br>
<b>psychosonda_bt_master</b><br>
Comunication script for debug<br>

External library and code
=====

We are using following libraries:

<b>FatFs</b> - (C)ChaN (http://elm-chan.org/fsw/ff/00index_e.html)<br>
/src/storage/FatFs<br>
<b>LUFA</b> - (C)Dean Camera (www.lufa-lib.org)<br>
/src/tasks/task_usb/LUFA<br>


Buid info
=====

Tools we are using:

Eclipse IDE for C/C++ Developers<br>
https://eclipse.org/downloads/packages/eclipse-ide-cc-developers/lunasr2<br>
AVR Eclipse plugin<br>
http://avr-eclipse.sourceforge.net/wiki/index.php/The_AVR_Eclipse_Plugin<br>
PyDev Eclipse plugin<br>
http://pydev.org/<br>
Atmel AVG GCC Toolchain:<br>
http://www.atmel.com/tools/ATMELAVRTOOLCHAINFORLINUX.aspx<br>

Programming
=====

<ul>
<li>Run python script <tt>psychosonda_sd_bootloader192/util/main.py <i>[HEX file]</i> UPDATE.BIN</tt></li>
<li>Copy UPDATE.BIN to root directory on the SD card</li>
</ul>



