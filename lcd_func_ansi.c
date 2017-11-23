/* lcd_func_ansi.c
 *
 * LCD functions to support ANSI control sequences
 *
 * Yan Seiner
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lcd_setcurs(C) if (vflg) { \
	fprintf(stderr,"set LCDcursor to %d\n",C);\
} \
lcd_cmd(0x80 + C)

int lcdcursave;
int lcdcurs;
int lcdline;
extern char lcdbuf[];
extern int lcdbufsize;
int Cols,Lines;
extern int LCDlineadd[];

extern int linepause;
extern int vflg;

/* lcd_su() scroll screen up
 */

lcd_su()
{
	int i,l;

	for ( l=0; l<(Lines-1); l++ )
	{
		lcd_setcurs(LCDlineadd[l]);				  /* address start of line l */
		lcd_puts(lcdbuf+LCDlineadd[l+1],Cols);	  /* display line l+1 on line l */
		memcpy(lcdbuf+LCDlineadd[l], lcdbuf+LCDlineadd[l+1], Cols);
/* move line l+1 to line l */
	}
	l=LCDlineadd[Lines-1];
	for ( i=0; i<Cols; i++ )					  /* clear last line */
	{
		lcdbuf[l+i]=' ';
	}
	lcd_setcurs(l);								  // curs to start of last line
	lcd_puts(lcdbuf+l,Cols);					  // write the cleared last line
	lcd_setcurs(lcdcurs);						  // restore cursor position
}


lcd_eln()										  // erase all of current line leaving cursor intact
{
	int i,n;

	i=LCDlineadd[lcdline];
	n=i+Cols;
	lcd_setcurs(i);
	for ( ; i<n ; i++ ) { lcdbuf[i]=' ' ; lcd_put(' '); }
	lcd_setcurs(lcdcurs);						  // restore cursor position
}


lcd_eeol()										  // erase from cursor to end of line
{
	int i,n;

	n=LCDlineadd[lcdline]+Cols;
	lcd_setcurs(lcdcurs);						  // restore cursor
	for(i = lcdcurs; i < n ; i++) { lcdbuf[i]=' '; lcd_put(' '); }
	lcd_setcurs(lcdcurs);						  // restore cursor
}


lcd_esol()										  // erase from start of line to cursor
{
	int i;

	i=LCDlineadd[lcdline];
	lcd_setcurs(i);
	for (  ; i<lcdcurs ; i++)
	{
		lcdbuf[i]=' '; lcd_put(' ');
	}
	lcd_setcurs(lcdcurs);						  // restore cursor
}


lcd_erase()										  // erase all screen, cursor unchanged
{
	int i;

	lcd_cmd(0x01);
	for ( i = 0; i < lcdbufsize; i++) { lcdbuf[i]=' '; }
	lcd_setcurs(lcdcurs);						  // restore cursor
}


lcd_c2eos()										  // erase from cursor to end of screen, cursor unchanged
{
	int i,l;

	lcd_eeol();									  // erase to end of current line
	for ( l=lcdline+1; l<Lines; l++ )
	{
		lcd_setcurs(LCDlineadd[l]);
		for ( i=LCDlineadd[l]; i<(LCDlineadd[l]+Cols); i++)
		{
			lcdbuf[i]=' '; lcd_put(' ');
		}
	}
	lcd_setcurs(lcdcurs);						  // restore cursor
}


lcd_sos2c()										  // erase from start of screen to cursor, cursor unchanged
{
	int i,l;

	lcd_esol();									  // erase to start of current line
	for ( l=lcdline-1; i>=0; i-- )
	{
		lcd_setcurs(LCDlineadd[l]);
		for ( i=LCDlineadd[l]; i<(LCDlineadd[l]+Cols); i++)
		{
			lcdbuf[i]=' '; lcd_put(' ');
		}
	}
	lcd_setcurs(lcdcurs);						  // restore cursor
}


lcd_cup(int r)
{
	if (r>0)
	{
		int c=lcdcurs-LCDlineadd[lcdline];
		int l=lcdline-r;
		if ( l<0 ) { l=0; }
		lcdcurs=LCDlineadd[l]+c;
		lcdline=l;
		lcd_setcurs(lcdcurs);
	}
}


lcd_cdn(int r)
{
	if (r>0)
	{
		int c=lcdcurs-LCDlineadd[lcdline];
		int l=lcdline+r;
		if ( l>(Lines-1) ) { l=Lines-1; }
		lcdcurs=LCDlineadd[l]+c;
		lcdline=l;
		lcd_setcurs(lcdcurs);
	}
}


lcd_crt(int c)
{
	int col=lcdcurs-LCDlineadd[lcdline];

	if ( c>0 )
	{
		col+=c;
		lcdline=lcdline+(col/Cols);
		if ( lcdline>(Lines-1) ) { lcdline=Lines-1; }
		col=col%Cols;
		lcdcurs=LCDlineadd[lcdline]+col;
		lcd_setcurs(lcdcurs);
	}
}


lcd_clt(int c)
{
	int col=(lcdline*Cols)+(lcdcurs-LCDlineadd[lcdline]);

	if ( c>0 )
	{
		col-=c;
		if ( col<0 ) { col=0; }
		lcdline=(col/Cols);
		if ( lcdline>(Lines-1) ) { lcdline=Lines-1; }
		col=col%Cols;
		lcdcurs=LCDlineadd[lcdline]+col;
		lcd_setcurs(lcdcurs);
	}
}


/* lcd_cpos(c,r)  goto position column c and row r
 *                counting from 0
 */

lcd_cpos(c,r)
int c,r;
{
	if (vflg)
	{
		fprintf(stderr,"in lcd_cpos: row %d, col %d\n",r,c);
	}
	if ( r>(Lines-1) ) { r=Lines-1; }
	else if (r<0) r=0;
	if ( c>(Cols-1) ) { c=Cols-1; }
	else if (c<0) c=0;

	lcdline=r;
	lcdcurs = LCDlineadd[r] + c;
	lcd_setcurs(lcdcurs);
}


lcd_cursave()
{
	lcdcursave=lcdcurs;
	return;
}


lcd_cursunsave()
{
	lcdcurs=lcdcursave;
	lcd_setcurs(lcdcurs);						  // restore cursor position
	return;
}


lcd_bs()
{
	lcd_clt(1);
	lcd_put(' ');
	lcd_setcurs(lcdcurs);
}


lcd_cr()
{
	lcdcurs=LCDlineadd[lcdline];
	lcd_setcurs(lcdcurs);
}


lcd_ff()
{
	int i;

	lcd_erase();
	lcdcurs=LCDlineadd[lcdline=0];
	lcd_setcurs(lcdcurs);
}


lcd_nl()
{
	int i;

	if ( lcdline<(Lines-1) )
	{
		lcdline++;
	}
	else
	{
		lcd_su();
	}
	lcdcurs=LCDlineadd[lcdline];
	lcd_setcurs(lcdcurs);
}


lcd_bel(period)
int period;
{
	if (period)
	{
// make a noise - to be done
	}
	return;
}
