#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "mytypes.h"

#define GPIOBASE    0x80840000
//#define JUMPERS		0x10800000
#define PADR    0							// address offset of LCD
#define PADDR   (0x10 / sizeof(UINT))		// address offset of DDR LCD
#define PHDR    (0x40 / sizeof(UINT))		// bits 3-5: EN, RS, WR
#define PHDDR   (0x44 / sizeof(UINT))		// DDR for above

// These delay values are calibrated for the EP9301
// CPU running at 166 Mhz, but should work also at 200 Mhz
#define SETUP   15
#define PULSE   36
#define HOLD    22

#define COUNTDOWN(x)    asm volatile ( \
"1:\n"\
"subs %1, %1, #1;\n"\
"bne 1b;\n"\
: "=r" ((x)) : "r" ((x)) \
);

volatile UINT *gpio;
volatile UINT *jumper;
volatile UINT *phdr;
volatile UINT *phddr;
volatile UINT *padr;
volatile UINT *paddr;

void command(UINT);
void writechars(UCHAR *);
UINT lcdwait(void);
void lcdinit(void);

/* This program takes lines from stdin and prints them to the
 * 2 line LCD connected to the TS-7200 LCD header.  e.g
 *
 *    echo "hello world" | lcdmesg
 *
 * It may need to be tweaked for different size displays
 */

//int jumper_on;

int main(int argc, char **argv)
{
	int i = 0;
#if 0
	doesn't work, just shows dbdbdbdbdb...
	size_t pagesize;
	UINT jump;
	
	int fd = open("/dev/mem", O_RDWR);

	jumper = (UINT *)mmap(0, getpagesize(),
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, JUMPERS);
	jump = *jumper;

	for(i = 0;i < 100;i++)
		printf("%2x ",*(jumper + i));
	printf("\n");

	printf("jumper:%4x %4x %4x\n",jumper,*jumper,jump);

	if((jump & 0x01) == 1)
	{
		jumper_on = 1;
		printf("jumper on: %2x \n",jump);
	}
	else
	{
		printf("jumper off: %2x \n",jump);
		jumper_on = 0;
	}
	
//	if(munmap((void *)jumper,pagesize) == -1)
//		perror("error un-mapping file\n");
//	close(fd);
#endif
	
	lcdinit();

	if(argc > 3)
		return 0;
	
	if (argv[1])
	{
		writechars((UCHAR*)argv[1]);
	}

	if (argv[2])
	{
		lcdwait();
		command(0xa8);							  // set DDRAM addr to second row
		writechars((UCHAR*)argv[2]);
	}

	while(!feof(stdin))
	{
		char buf[512];

		lcdwait();
		if (i)
		{
// XXX: this seek addr may be different for different
// LCD sizes!  -JO
			command(0xa8);						  // set DDRAM addr to second row
		}
		else
		{
			command(0x2);						  // return home
		}
		i = i ^ 0x1;
		if (fgets(buf, sizeof(buf), stdin) != NULL)
		{
			UINT len;
			buf[0x27] = 0;
			len = strlen(buf);
			if (buf[len - 1] == '\n') buf[len - 1] = 0;
			writechars((UCHAR*)buf);
		}
	}
	return 0;
}


void lcdinit(void)
{
	int fd = open("/dev/mem", O_RDWR);
	int psize = getpagesize();
	gpio = (UINT *)mmap(0, getpagesize(),
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIOBASE);
	phdr = &gpio[PHDR];
	padr = &gpio[PADR];
	paddr = &gpio[PADDR];
	phddr = &gpio[PHDDR];
//	printf("1:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
	*paddr = 0x0;								  // All of port A to inputs
	*phddr |= 0x38;								  // bits 3:5 of port H to outputs
	*phdr &= ~0x18;								  // de-assert EN, de-assert RS
	usleep(15000);
	command(0x38);								  // two rows, 5x7, 8 bit
	usleep(4100);
	command(0x38);								  // two rows, 5x7, 8 bit
	usleep(100);
	command(0x38);								  // two rows, 5x7, 8 bit
	command(0x6);								  // cursor increment mode
	lcdwait();
	command(0x1);								  // clear display
	lcdwait();
	command(0xc);								  // display on, blink off, cursor off
	lcdwait();
	command(0x2);								  // return home
	printf("pagesize: %d\n",psize);
//	printf("%4x %4x \n",gpio,*gpio);
//	printf("%6x %6x \n",gpio,*gpio);
}


UINT lcdwait(void)
{
	int i, dat, tries = 0;
	UINT ctrl = *phdr;

	*paddr = 0x0;								  // set port A to inputs
	ctrl = *phdr;

	do
	{
// step 1, apply RS & WR
		ctrl |= 0x30;							  // de-assert WR
		ctrl &= ~0x10;							  // de-assert RS
		*phdr = ctrl;

// step 2, wait
		i = SETUP;
		COUNTDOWN(i);

// step 3, assert EN
		ctrl |= 0x8;
		*phdr = ctrl;

// step 4, wait
		i = PULSE;
		COUNTDOWN(i);

// step 5, de-assert EN, read result
		dat = *padr;
		ctrl &= ~0x8;							  // de-assert EN
		*phdr = ctrl;

// step 6, wait
		i = HOLD;
		COUNTDOWN(i);
	} while (dat & 0x80 && tries++ < 1000);
	return dat;
}


void command(UINT cmd)
{
	int i;
	UINT ctrl = *phdr;

	*paddr = 0xff;								  // set port A to outputs

// step 1, apply RS & WR, send data
	*padr = cmd;
	ctrl &= ~0x30;								  // de-assert RS, assert WR
	*phdr = ctrl;

// step 2, wait
	i = SETUP;
	COUNTDOWN(i);

// step 3, assert EN
	ctrl |= 0x8;
	*phdr = ctrl;

// step 4, wait
	i = PULSE;
	COUNTDOWN(i);

// step 5, de-assert EN
	ctrl &= ~0x8;								  // de-assert EN
	*phdr = ctrl;

// step 6, wait
	i = HOLD;
	COUNTDOWN(i);
}


void writechars(UCHAR *dat)
{
	int i;
	UINT ctrl = *phdr;

	do
	{
		lcdwait();
//		printf("1a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
		*paddr = 0xff;							  // set port A to outputs
//		printf("2a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);

// step 1, apply RS & WR, send data
//		printf("3a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
		*padr = *dat++;
//		printf("4a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
		ctrl |= 0x10;							  // assert RS
//		printf("5a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
		ctrl &= ~0x20;							  // assert WR
//		printf("6a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);
		*phdr = ctrl;
//		printf("7a:  %4x %4x \n", gpio[PADR],gpio[PADDR]);

// step 2
		i = SETUP;
		COUNTDOWN(i);

// step 3, assert EN
		ctrl |= 0x8;
		*phdr = ctrl;

// step 4, wait 800 nS
		i = PULSE;
		COUNTDOWN(i);

// step 5, de-assert EN
		ctrl &= ~0x8;							  // de-assert EN
		*phdr = ctrl;

// step 6, wait
		i = HOLD;
		COUNTDOWN(i);
	} while(*dat);
}
