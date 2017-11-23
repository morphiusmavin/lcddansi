#
# Make file for lcdd
# daemon that reads named pipe (fifo) and sends anything read from 
#  that to a parallel connected HD44 compatible char. LCD
# Software specific to TS-72x0 ARM SBC
# 
# Makefile for arm-linux cross compilation on i386 debian box,
# using the arm crosstool kit provided by Technologic Systems
# for the TS-72x0 ARM SBC

SHELL		= /bin/sh

DEMOS		= 
PROGS		= lcdd

srcdir		= .

# **** CUSTOMISE THESE ******

includedir	= $(HOME)/include
INSDIR		= $(HOME)/export/work/bin
#MANDIR		= /usr/local/man/man1
#LOCALINS	= $(HOME)/bin
#LOCALMAN	= $(HOME)/man/man1

lcdd		= lcdd.o lcd_func_ansi.o ts7200io.o

# ***************************
#CC		= arm-linux-gcc
#CC		= gcc
#CFLAGS		= -O -msoft-float
CFLAGS		= -O2
CPPFLAGS	= -I. -I$(includedir)

CCFLAGS		= $(CFLAGS) $(CPPFLAGS)

LINK		= $(CC)

#
# libraries

#LDFLAGS	= 
#LDFLAGS	= 
#LDFLAGS	= -static

# if there is an include file uncomment this. Add anyother dependancies
#adio.o: adio.h

.c.o:
	$(CC) -c $(CCFLAGS) $<

all:	$(PROGS)

clean:
	rm -f *~ *.o $(PROGS)

backup:	dist

dist:	lcdd
	rm -f *~ *.o
	(d=`basename $$PWD` ; cd .. ; tar cfz $$d.tgz $$d)

lcdd:	$(lcdd)
	$(CC) $(lcdd) $(LDFLAGS) -o $@

ts7200io.o:	ts7200io.c ts7200io.h

install:	$(PROGS)
	@for f in $(PROGS) $(DEMOS) ; do \
	 echo "Copying $$f to $(INSDIR)" ;\
	 install -D --backup $$f $(INSDIR)/$$f; done


