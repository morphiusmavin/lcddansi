/* lcdd.c
 * Simple userspace daemon which acts as a driver for HD44780
 * based char LCDs parallel connected to the "LCD" port of the TS72x0
 * board.
 *
 * This version supports ANSI control sequences
 *
 * Jim Jackson
 */
/*
 * Copyright (C) 2005-2010 Jim Jackson           jj@franjam.org.uk
 */
/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program - see the file COPYING; if not, write to
 *  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
 *  MA 02139, USA.
 */
/*
 *  Date     Vers    Comment
 *  -----    -----   ---------------
 *  31Jan13  0.7ansi Added support for 4 row devices
 *                   Row 3 addresses following directly the end of row 1
 *                   ditto row 4 and row 2. Tested with a 20x4 device.
 *   8Apr10  0.6ansi Added option to specify a timed delay between chars
 *                   to give slow display.
 *                   Added option to specify a timed delay between lines
 *                   to give an alternative "slow" display style
 *                   Added pseudo ANSI sequence to dynamically set char
 *                   and line delays  ESC [ ChDelay ; LineDelay w
 *                   Added itimer periodic generator for counts etc.
 *   7Apr08  0.5ansi Fixed bug in -s LCD size option handling.
 *   8Apr07  0.5ansi Started Adding code to support 4 bit data interface
 *                   handling - lots of probs not finished
 *   8Apr07  0.5ansi Fix Bug in ESC [ n E  pseudo ANSI code for setting
 *                   entry mode
 *  12Jan07  0.4ansi Much debugging/fixing and finishing
 *                   fixed horrendous JJ bug decimal 40 should hex 40!!!
 *  10Jan07  0.3ansi Folded in Patches from Yan Seiner <yan@seiner.com>
 *                   to support ANSI Control sequences
 *  30Mar05  0.1     Initial Program Built from adio.c
 */

/***MANPAGE***/
/*
lcdd: (version 0.7ansi) 31Jan13

see the file lcdd.txt

 */

/***INCLUDES***/
/* includes files
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
/* for SYSV variants .... */
/* #include <string.h> */
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include "ts7200io.h"

/***DEFINES***/
/* defines
 */

#define VERSION "0.7aANSI"

#define PVERSION(A) fprintf(stderr,"%s version %s\n",sys,A)

#define DEF_CONFIGFILE "/etc/lcdd.conf"

#define MAXLCDDEFS (12)

#define NAMED_PIPE "/dev/lcd"

#define PIPE_MODE (0622)

#define DEF_LINE_PAUSE (1)						  /* tenths of a sec */

#define DEF_CHAR_PAUSE (0)						  /* tenths of a sec */

#define BELLTIME (300)

#define LCDBUFSIZE (128)

#define DEF_COLS (16)

#define DEF_LINES (2)

#define EFAIL -1

#define TRUE 1
#define FALSE 0
#define nl puts("")

#define chkword(a,b) ((n>=a)&&(strncmp(p,b,n)==0))
#define index(A,B) _strchr(A,B)
#define rindex(A,B) strrchr(A,B)

#define EMODE_LR 0
#define EMODE_RR 1
#define EMODE_RN 2
#define EMODE_LN 3

/***GLOBALS***/
/* global variables
 */

char *sys;
int vflg;										  /* set if -v flag used        */
int timestamp;									  /* -t add timestamp to each line */
int ifmode;
int charpause;
int linepause;
int maxwaitime;

char *pipename;
char *geom;
extern int Lines;								  /* default LCD size */
extern int Cols;
int LCDlineadd[]= { 0x00, 0x40, 0, 0 };

int LCD;										  /* fd for named pipe to lcd */

struct timeval start,tm,tmlast;
struct timezone tz;

int cmdmode=FALSE;
int cmdlen=0;
int linewrap=TRUE;
int fontoffset=0;
int emode=EMODE_RN;

char cmdbuf[512];

int lcdbufsize=LCDBUFSIZE;
char lcdbuf[LCDBUFSIZE];
extern int lcdcurs;
extern int lcdcursave;
extern int lcdline;
int lcdnlpending=0;

/***DECLARATIONS***/
/* declare non-int functions
 */

char *delnl(),*strupper(),*reads(),*_strchr(),*strrchr(),*sindex(),
*sindex_nc(),*tab(),*ctime(),*getcmdline();
FILE *fopen(),*mfopen();

int  chdelaycounter,lndelaycounter;

void timer_handler(sig)
int sig;
{
	if (chdelaycounter) { chdelaycounter-- ; }
	if (lndelaycounter) { lndelaycounter-- ; }
	/* printf("."); */   /***************/
/* fflush(stdout); */
}


/***MAIN***/
/* main - for program  lcdd
 */

main(argc,argv)
int argc;
char **argv;

{
	int i,j,n,t,st;
	FILE *f;
	char *p;
	char buf[130];
	struct sigaction sa;
	struct itimerval timer;

	argv[argc]=NULL;
	sys=*argv++; argc--;
	if ((p=rindex(sys,'/'))!=NULL) { sys=p+1; }

/* now do option setup and parsing... */
	ifmode=8;
	vflg=FALSE;
	LCD=-1;
	geom=NULL;
	Cols=DEF_COLS;
	Lines=DEF_LINES;
	linepause=10*DEF_LINE_PAUSE+1;
	charpause=10*DEF_CHAR_PAUSE;

/* put global and local var. init. here */

	while (argc && **argv=='-')					  /* all flags and options must come */
	{
		n=strlen(p=(*argv++)+1); argc--;		  /* before parameters                */
		if (chkword(1,"size"))					  /* -size option              */
		{
			if (argc && (**argv!='-') && isdigit(**argv))
			{
				geom=*argv++; argc--;
			}
		}										  /* pause between lines */
		else if (chkword(1,"pause"))
		{
			if (argc && (**argv!='-') && isdigit(**argv))
			{
				linepause=10*atoi(*argv++)+1; argc--;
			}
		}										  /* pause between chars */
		else if (chkword(1,"wait"))
		{
			if (argc && (**argv!='-') && isdigit(**argv))
			{
				charpause=10*atoi(*argv++); argc--;
			}
		}										  /* check for single char. flags    */
		else
		{
			for (; *p; p++)
			{
				if (*p=='h') exit(help(EFAIL));
				else if (*p=='v') vflg=TRUE;
				else
				{
					*buf='-'; *(buf+1)=*p; *(buf+2)=0;
					exit(help(err_rpt(EINVAL,buf)));
				}
			}
		}
	}

	if ( argc>1 )
	{
		exit(err_rpt(EINVAL,"Too many command line parameters."));
	}

	if (argc)
	{
		pipename=*argv++; argc--;
	}
	else
	{
		pipename=NAMED_PIPE;
	}

	if ( geom != NULL && setgeom(geom,&Cols,&Lines))
	{
		fprintf(stderr,"LCD Size setting \"%s\" is unrecognised.\n",geom);
		exit(help(EFAIL));
	}

	if ( Cols<8 || Cols>40 || (Lines!=1 && Lines!=2 && Lines!=4) )
	{
		fprintf(stderr,"LCD columns can only be from 8 to 40, and lines only 1, 2 or 4\n");
		exit(help(EFAIL));
	}

	if (vflg)
	{
		fprintf(stderr,"%d Cols by %d Lines LCD on named pipe %s\n",
			Cols,Lines,pipename);
		fprintf(stderr,"char. pause %d millisecs, line pause %d millisecs\n",
			charpause*50,linepause*50);
	}

	if ( Lines==4 ) { LCDlineadd[2]=Cols ; LCDlineadd[3]=0x40+Cols; }

	if ((LCD=open(pipename,O_RDONLY|O_NONBLOCK,PIPE_MODE))==-1)
	{
		if (mkfifo(pipename,PIPE_MODE))
		{
			exit(err_rpt(errno,pipename));
		}
		if ((LCD=open(pipename,O_RDONLY|O_NONBLOCK,PIPE_MODE))==-1)
		{
			exit(err_rpt(errno,pipename));
		}
	}

	if (!isfifo(LCD))
	{
		sprintf(buf,"\"%s\" is not a named pipe.",pipename);
		exit(err_rpt(EINVAL,buf));
	}

	if (fchmod(LCD,PIPE_MODE))
	{
		sprintf(buf,"changing permissions of \"%s\"",pipename);
		err_rpt(errno,buf);
	}

	if (vflg)
	{
		PVERSION(VERSION);
		fprintf(stderr,"Named pipe is \"%s\"\n",pipename);
	}
	fflush(stderr);

	gettimeofday(&start, &tz);
	tmlast=start;

	if (!vflg && daemon(FALSE,FALSE)<0)
	{
		exit(err_rpt(errno,"Trying to become a daemon."));
	}

/* setup itimer ... */
	chdelaycounter=0;
	lndelaycounter=0;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &timer_handler;
	sigaction (SIGALRM, &sa, NULL);
	timer.it_value.tv_sec = 0;					  /* setup a period of 10millisecs */
	timer.it_value.tv_usec = 10000;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 10000;
	setitimer(ITIMER_REAL, &timer, NULL);

/* now do the business....
 */

	exit(dolcd());
}


/* dolcd - main loop
 *         loop on read named pipe open on LCD, writing chars
 *         read to the LCD device.
 *
 * This is going to end up as sort of terminal emulator.
 * HD44780 based LCDs are mainly either 1, 2 or 4 line displays
 * with 8, 16, 20, 32 or 40 chars per line.
 * The displays have memory locations 0x0-0x27 for line 1, 0x40-0x67 for line 2
 *
 * Basic operation is ascii 0x20 - 0x7E are display chars and are written
 * sequentially along the current line until getting to last position
 * on the line, where they either stick (nowrap mode) or start on next line
 * (wrap mode). At end of second line in scroll mode, the top line is lost,
 * the 2nd line is copied to the first line, and new line empty 2nd line
 * is started. Or in noscroll mode chars just keep overwiritng the last
 * position on screen.
 *
 * Control Char functions:
 *
 *  13 0x0D \r  -  CR cursor moves to col 0 on current line
 *  10 0x0A \n  -  LF move down a line to same col on line below
 *                 if in scroll mode, and already on second line, move
 *                 line 2 to line 1 and clear second line
 *                  LF implementation is flagged and action delayed till
 *                  the first char of the next line. Otherwise the
 *                  last line of the display is always blank - as most apps
 *                  send each line LF terminated!
 *  12 0x0C \f  -  FF move cursor to start of line 1 and clear all data
 *                 to spaces
 *   8 0x08 \b  -  BS move cursor back one position, unless at start of line
 *
 * this needs some research - do I really want to write a an ANSI driver????
 * Well Yan Seiner did!
 *
 * ANSI command stuff originally courtesy of Yan - bugs an' all :-)
 */

dolcd()
{
	char *p,c,buf[2];
	int i,n,st;

	if (n=lcd_init(ifmode,2)) { return(n); }

	lcdline=lcdcursave=lcdcurs=0;
	lcd_erase();
	lndelaycounter=10; while (lndelaycounter) { waitalarm(); }

	sprintf(lcdbuf,"%s v%s",sys,VERSION);
	lcd_puts(lcdbuf,Cols);
	lcd_nl();

	if (vflg) { fprintf(stderr,"%-*s\n",Cols,lcdbuf); }

	for (p=buf;;)
	{
		if ((n=read(LCD,p,1))<=0)
		{
			if (n<0 && errno!=EAGAIN) return(errno);
			waitalarm(); continue;
		}

		if (vflg) { fprintf(stderr,"--> 0x%02x \n",*p); }

		if ( cmdmode )
		{
			cmdbuf[cmdlen++] = *p;
			cmdbuf[cmdlen] = '\0';
			if ( ! isdigit(*p) && *p!=';' && *p!='[' )
			{
				donlpending();
				while (chdelaycounter) { waitalarm(); }
				do_ansi();
				chdelaycounter=charpause;
				cmdmode=FALSE; cmdlen=0;
			}
			else if (cmdlen > 510)
			{
				cmdmode = FALSE;
				cmdlen = 0;
			}
			continue;
		}

		switch (*p)
		{
			case 0x9B:							  // CSI - 8 bit replacement for ESC [
				cmdmode = TRUE;
				cmdbuf[0] = '[';
				cmdbuf[1] = 0;
				cmdlen = 1;
				break;
			case '\e':							  // command mode
				cmdmode = TRUE;
				cmdbuf[0] = '\0';
				cmdlen = 0;
				break;
			case '\n':
				donlpending();
				lcdnlpending++;
				break;
			case '\r':
				while (chdelaycounter) { waitalarm(); }
				lcd_cr();
				lndelaycounter=linepause;
				break;
			case 12:
				while (lndelaycounter) { waitalarm(); }
				lcd_ff();
				lndelaycounter=linepause;
				lcdnlpending=0;
				break;
			case 7:								  /* BEL - make a noise :-) */
				while (chdelaycounter) { waitalarm(); }
				lcd_bel(BELLTIME);
				chdelaycounter=charpause;
				break;
			case 8:								  /* BS backspace */
				while (chdelaycounter) { waitalarm(); }
				lcd_bs();
				chdelaycounter=charpause;
				break;
			default:
				while (lndelaycounter+chdelaycounter) { waitalarm(); }
				donlpending();
				lcd_put(*p); lcdbuf[lcdcurs++]=*p;
				chdelaycounter=charpause;
				for ( st=i=0; i<Lines ; i++ ) { st=( st || (lcdcurs==LCDlineadd[i]) ); }
				if ( st )
				{
					lcdnlpending++ ; lndelaycounter=linepause;
				}
		}
	}
	return(ENOMSG);
}


do_ansi()
{
/* ANSI sequence decoding.
 * Only a subset of the ANSI sequences are decoded here.
 * If it's not here then send JJ a patch or a request for that ANSI/VT
 * sequence to implemented.
 *
 * Reset Device              ESC 'c'
 *
 * All rest assume leading ESC char. :
 *
 * Enable Line Wrap		[7h            [?7h
 * Disable Line Wrap		[7l            [?7l
 *
 * Font Set G0		(
 * Font Set G1		)
 *
 * Scroll Down		D
 * Scroll Up			M
 *
 * Erase to end of screen    [0J  or [J
 * Erase cursor to start     [1J
 * Erase Screen		[2J
 *
 * Erase to End of Line	[0K  or [K
 * Erase cursor to start     [1K
 * Erase Line                [2K
 *
 * Save cursor postion       [s
 * Restore cursor position   [u
 *
 * Non-standard codes for entry mode
 *
 * Left Reverse		[4E
 * Right Reverse		[5E
 * Right Normal		[6E
 * Left Normal		[7E
 *
 * Non-standard codes for cursor control
 *
 * underline cursor		[X
 * block cursor		[Y
 * hide cursor		[Z
 *
 * Non-standard code for setting character and line delay
 *
 * Set char and line delays  [{cd};{ld}w
 *
 * Variable length commands
 *
 * Cursor Position Set	[%d;%dH
 * Cursor Up			[%dA
 * Cursor Down		[%dB
 * Cursor Right		[%dC
 * Cursor Left		[%dD
 */

	int l, r, c;
	char *p,*s,cmd;

	p=cmdbuf;
	l = strlen(cmdbuf) - 1; cmd=cmdbuf[l];
	if (vflg)
	{
		fprintf(stderr,"Curs %d ANSI cmd \"%c\" seq %d bytes 0x1b, 0x%02.2x",
			lcdcurs,cmd,l+2,*p);
		for (r=1; r<=l; r++) fprintf(stderr," , 0x%02.2x",cmdbuf[r]);
		fprintf(stderr,"\n");
	}
	switch(*p++)
	{
		case 'c':
			while (lndelaycounter+chdelaycounter) {}
			lcd_init(ifmode,2);
			break;
		case ')':
			fontoffset = 0;
			break;
		case '(':
			fontoffset = 0x40;
			break;
		case 'D':
			while (lndelaycounter) {}
			lcd_nl();
			break;
		case 'M':
			while (lndelaycounter) {}
			lcd_su();
			break;
		case '[':
			switch (*p)
			{
				case '?':
					c=strtol(p,&s,10);
					if ( *s=='h' ) { r=TRUE; }
					else if ( *s=='l' ) { r=FALSE; }
					else { break; }

					switch (c)
					{
						case 7:
							linewrap=r;
							break;
					}
					break;
			}
			cmd=cmdbuf[l];
			switch (cmd)
			{
				case 'h':						  //???? linewrap ANSI is ESC [ ? 7 h surely
					if(*p == '7') linewrap = TRUE;
					break;
				case 'l':
					if(*p == '7') linewrap = FALSE;
					break;
				case 'J':
					switch (*p)
					{
						case '2':
							while (lndelaycounter) {}
							lcd_erase();		  // erase all screen
							break;
						case 'J':				  // if no number default is '0'
						case '0':
// erase from cursor to end of screen
							while (lndelaycounter) {}
							lcd_c2eos();
							break;
						case '1':
// erase from cursor to start of screen
							while (lndelaycounter) {}
							lcd_sos2c();
							break;
					}
					break;
				case 'K':
					switch (*p)
					{
						case '2':
							while (lndelaycounter) {}
							lcd_eln();			  // erase all line
							break;
						case 'K':				  // if no number default is '0'
						case '0':
// erase from cursor to end of line
							while (lndelaycounter) {}
							lcd_eeol();
							break;
						case '1':
// erase from cursor to start of line
							while (lndelaycounter) {}
							lcd_esol();
							break;
					}
					break;
				case 'E':						  // non-standard extra FUNCTION to set the LCD
// entry mode
					c=*p - '0';
					if ( c >= 4 && c <= 7 ) { lcd_cmd(c); }
					break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
					c=strtol(p,&s,10);
					if (c==0) c=1;
					if (*s == cmd)
					{
						switch (cmd)
						{
							case 'A':
								while (lndelaycounter) {}
								lcd_cup(c); break;
							case 'B':
								while (lndelaycounter) {}
								lcd_cdn(c); break;
							case 'C':
								while (chdelaycounter) {}
								lcd_crt(c); break;
							case 'D':
								while (chdelaycounter) {}
								lcd_clt(c); break;
						}
					}
					break;
				case 's':						  /* save cursor position */
					lcdcursave=lcdcurs;
					break;
				case 'u':						  /* restore (unsave) cursor pos. */
					lcd_cursunsave();
					break;
				case 'X':						  // underline cursor
					lcd_cmd(0x0e);
					break;
				case 'Y':						  // block cursor
					lcd_cmd(0x0f);
					break;
				case 'Z':						  // hide cursor
					lcd_cmd(0x0c);
					break;
				case 'H':						  // Cursor positioning - in general CSIr;cH
// r and c are ascii strings for row and
// column number (counting from 1 - hence need
// to -1 before passing to lcd_cpos
// if r and/or c are missing their default is 1
					c=1;
					r=strtol(p,&s,10);
					if ( *s == ';' ) { p=++s; c = strtol(p,&s,10); }
					if (r<=0) r=1;
					if (r>Lines) r=Lines;
					if (c<=0) c=1;
					if (c>Cols) c=Cols;
					if (vflg)
					{
						fprintf(stderr,"handling cur pos: row %d, col %d\n",r,c);
					}
					lcd_cpos(c-1,r-1);
					break;
				case 'w':						  // setting pause (wait) time after each char
// and end of line - to slow things down
					charpause=10*strtol(p,&s,10);
					if ( *s == ';' ) { p=++s; linepause = 10*strtol(p,&s,10)+1; }
					if (vflg)
					{
						fprintf(stderr,"setting pause times: line %d, char %d\n",
							linepause,charpause);
					}
					break;
			}
	}
}


lcd_puts(s,n)
char *s;
int n;
{
	int i;

	for (i=0; i<n && s[i] ; i++) { lcd_put(s[i]); }
}


donlpending()
{
	for ( ; lcdnlpending ; lcdnlpending-- )
	{
		while (lndelaycounter) { waitalarm(); }
		lcd_nl();
		lndelaycounter=linepause;
	}
	while (lndelaycounter) { waitalarm(); }
}


/* waitalarm()
 *  sigsuspend untill the next alarm signal
 */

waitalarm()
{
	sigset_t ss;

	sigemptyset(&ss);
/*  sigaddset (&ss, SIGALRM);  */
	sigsuspend(&ss);
}


/* isfifo(fd) check if fd is a FIFO - return true/false
 */

isfifo(fd)
int fd;
{
	struct stat stats;
	mode_t m;

	if ((fd<0) || fstat(fd,&stats))
	{
		return(FALSE);
	}
	m=stats.st_mode;
	return(S_ISFIFO(m));
}


/* setgeom(g,cp,lp)  decode geometry string g into number of Cols
 *                   and number of Lines and set *cp and *lp
 */

setgeom(g,cp,lp)
char *g;
int *cp,*lp;
{
	char *p;
	int c,l;

	c=strtol(g,&p,10);
	if (*p != 'x' && *p != 'X') { return(1); }
	l=strtol(++p,&g,10);
	if (*g) { return(1); }
	*cp=c; *lp=l;
	return(0);
}


/***HELP***/
/* help(e)  print brief help message - return parameter given
 */

help(e)
int e;
{
	PVERSION(VERSION);
	puts("\n\
lcdd -  Simple userspace daemon which acts as a driver for\n\
        HD44780 based char LCDs parallel connected to the\n\
        LCD port of the TS72x0 board.\n\
        ");
	printf("\
Usage:   %s [-v] [-s COLSxLINES] [-p pause] [-w pause] [named_pipe]\n\n\
        ",sys);
	printf("\
  pause  values are in tenths of a second - e.g. 5 for half a sec\n\n");
	return(e);
}


/***ERR_RPT***/
/* err_rpt(err,msg)
 */

err_rpt(err,msg)
short int err;
char *msg;

{
	if (err) fprintf(stderr,"[%s] (%d) %s : %s\n",sys,err,strerror(err),msg);
	return(err);
}
