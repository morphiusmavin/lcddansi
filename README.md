<h1>LCDDANSI</h1>

31st Jan 2013
                                                        Version 0.7ANSI

lcdd - a user space driver for HD44780 based char. lcds 
       ANSI version - obeys a subset of ANSI terminal commands
       in so far as it can :-)  
       
This has been tested with 2 row, 16 and 20 col devices. Other
2 row devices should also work, as should 1 row devices too, but 
I have not tested these devices. Feedback on whether they do work 
would be appreciated.

Version 0.7ANSI introduced support for 4 row  by 20 col devices, where 
it is assumed that row 3 starts at the column after where row 1 ends:
and similarly row 4 starts at the col after row 2 ends.

For 2 row devices, row 1 starts at address 0 (0x00) and row 1 starts at 
64 (0x40). So for a 4 row by 20 col device, row 3 starts at address 20 
(0x14) and row 4 starts at 84 (0x54).

Initial ANSI code from a patch set from Yan Seiner  <yan@seiner.com>

While the Software is specific to Technologics TS72x0 ARM SBCs based
on EP9302 micros, it can be patched to work on other platforms.

see http://www.embeddedx86.com/
(couldn't find this source on the website or the ftp site)
  
for TS72xx board details.

This source code is provided under the terms of the GNU General Public License
- see the file COPYING for details.

<h2>Compiling</h2>
---------

Makefile is setup to use the Technologic Systems provided Cross compilation 
tool chain. See 
  
  http://embeddedx86.com/downloads/Linux/ARM/crosstool-linux.tar.bz2

Edit the Makefile for your setup.

compilation should be fairly simple just type "make".

Installation is a little complex in cross-compiling environments.
In the Makefile there is an install rule - ensure you edit the Makefile
to install in the correct place for your system, or simply copy the executable
"lcdd" to the right place.

The man page is not done. There is info on the program in the file
lcdd.txt which explains the command line options.

The ts72x0 specific routines are in the file ts7200io.c with header file
ts7200io.h (only a prototype at moment). These routines can be used in other
programs. At the moment you'll have to read the source files for more info.
Sorry.

Jim Jackson
<jj@franjam.org.uk>
(dead links?)
http://www.comp.leeds.ac.uk/jj
http://www.comp.leeds.ac.uk/jj/linux/arm-sbc.html


