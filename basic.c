// $Id: basic.c,v 1.79 2021/09/12 05:59:14 stefan Exp stefan $
/*
	Stefan's tiny basic interpreter 

	Playing around with frugal programming. See the licence file on 
	https://github.com/slviajero/tinybasic for copyright/left.
    (GNU GENERAL PUBLIC LICENSE, Version 3, 29 June 2007)

	Author: Stefan Lenz, sl001@serverfabrik.de

	The first set of definions define the target.
	- for any Arduino with serial I/O or a Mac nothing has 
		to be set here. The default settings are correct
		for Arduino boards including MK. 
	- DUE unfortunately needs a special setting as it has no 
		tone() function
	- for ESP8266 boards define this macro (#define ESP8266)
    - for RP2040 boards define this macros (#define RP2040)
      (for both cases PROGMEM is disabled and EEPROM ignored)
	- ARDUINOLCD actives the LCD code, LCDSHIELD automatically 
		defines the right settings for the classical shield modules
	- ARDUINOPS2 activates the PS2 code. Default pins are 2 and 3.
		If you use other pins the respective changes have to be made 
			below. 
	- _if_ LCD and PS2 are both activated STANDALONE cause the Arduino
			to start with keyboard and lcd as standard devices.
	- ARDUINOTFT is not yet implemented
	- ARDUINOEEPROM includes the EEPROM access code
	- HAS* activates or deactives features of the interpreter
	- activating Picoserial is not compatible with keyboard code
		Picoserial doesn't work on MEGA


	SMALLness - Memory footprint of extensions
	
	          FLASH	    RAM
	EEPROM    834  		0
	FOR       804  		34
	LCD       712  		37
	PS2       1930      128
	GOSUB     144  		10
	DUMP 	  130  		0
	PICO     -504 	-179

	The extension flags control features and code size.

*/ 

#undef ARDUINODUE
#undef RP2040
#undef ESP8266
#undef MINGW
#undef ARDUINOLCD
#undef LCDSHIELD
#define ARDUINOEEPROM
#define HASFORNEXT
#define HASGOSUB
#define HASDUMP
#undef USESPICOSERIAL
#define MEMSIZE 512

// these are the definitions to build a standalone 
// computer. All of these extensions are very memory hungry
// input methods
#undef ARDUINOPS2
// output methods
#undef ARDUINOI2C
#undef ARDUINOTFT
#undef ARDUINOPRT
// storage methods
#undef ARDUINOSD
// use the methods above as primary i/o devices
#undef STANDALONE

	
// Don't change the definitions here unless you must
#ifdef ARDUINOPS2
#include <PS2Keyboard.h>
#endif

// if PROGMEM is defined we can asssume we compile on 
// the Arduino IDE. Don't change anything here. 
// This is a little hack to detect where we compile
#ifdef PROGMEM
#define ARDUINOPROGMEM
#else
#undef ARDUINO
#endif

#if defined(ESP8266) || defined(RP2040)
#define PROGMEM
#define ARDUINO
#undef ARDUINOPROGMEM
#undef ARDUINOEEPROM
#endif

#ifdef ARDUINO
#ifdef ARDUINOPROGMEM
#include <avr/pgmspace.h>
#endif
#ifdef ARDUINOEEPROM
#include <EEPROM.h>
#endif
#ifdef ARDUINOLCD
#include <LiquidCrystal.h>
#endif
#ifdef ARDUINOI2C
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#endif
#ifdef USESPICOSERIAL
#include <PicoSerial.h>
#endif
#ifdef ARDUINOSD
#include <SPI.h>
#include <SD.h>
#endif
#else 
#define PROGMEM
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#endif

// Arduino default serial baudrate
const int serial_baudrate = 9600;

// general definitions
#define TRUE  1
#define FALSE 0

// debug mode switches 
#define DEBUG  0

// various buffer sizes
#define BUFSIZE 	92
#define SBUFSIZE	32
#define VARSIZE		26
#define STACKSIZE 	15
#define GOSUBDEPTH 	4
#define FORDEPTH 	4

/*

   	The tokens:

	All single character operators are their own tokens
	ASCII values above 0x7f are used for tokens of keywords.

*/

#define EOL			 0
#define NUMBER   	 -127
#define LINENUMBER   -126
#define STRING   	 -125
#define VARIABLE 	 -124
#define STRINGVAR 	 -123
#define ARRAYVAR     -122
// multi character tokens - BASEKEYWORD (3)
#define GREATEREQUAL -121
#define LESSEREQUAL  -120
#define NOTEQUAL	 -119
// this is the Palo Alto Language Set (19)
#define TPRINT  -118
#define TLET    -117
#define TINPUT  -116
#define TGOTO   -115
#define TGOSUB  -114
#define TRETURN -113
#define TIF     -112
#define TFOR    -111
#define TTO     -110
#define TSTEP   -109
#define TNEXT   -108
#define TSTOP    -107
#define TLIST	-106
#define TNEW    -105
#define TRUN    -104
#define TABS 	-103
#define TRND	-102
#define TSIZE   -101
#define TREM 	-100
// this is the Apple 1 language set in addition to Palo Alto (15)
#define TNOT    -99
#define TAND	-98
#define TOR  	-97
#define TLEN    -96
#define TSGN	-95	
#define TPEEK	-94
#define TDIM	-93
#define TCLR	-92
#define TSCR    -91
#define TLOMEM  -90
#define THIMEM  -89 
#define TTAB 	-88
#define TTHEN   -87
#define TEND    -86
#define TPOKE	-85
// Stefan's tinybasic additions (9)
#define TCONT   -84
#define TSQR	-83
#define TFRE	-82
#define TDUMP 	-81
#define TBREAK  -80
#define TSAVE   -79
#define TLOAD   -78
#define TGET    -77
#define TPUT    -76
#define TSET    -75
// Arduino functions (9)
#define TPINM	-74
#define TDWRITE	-73
#define TDREAD	-72
#define TAWRITE	-71
#define TAREAD  -70
#define TDELAY  -69
#define TMILLIS  -68
#define TTONE   -67
#define TPULSEIN  -66
#define TAZERO	  -65
// the SD card DOS functions (2)
#define TCATALOG -64
#define TDELETE  -63
#define TOPEN 	-62
#define TCLOSE  -61
// low level access of internal routines
#define TUSR	-60
#define TCALL 	-59
// currently unused constants
#define TERROR  -3
#define UNKNOWN -2
#define NEWLINE -1


// the number of keywords, and the base index of the keywords
#define NKEYWORDS	3+19+15+10+10+4+2
#define BASEKEYWORD -121

/*
	Interpreter states 
	SRUN means running from a programm
	SINT means interactive mode
	SERUN means running directly from EEPROM
	(enum would be the right way of doing this.)
	BREAKCHAR is the character stopping the program on Ardunios
*/

#define SINT 0
#define SRUN 1
#define SERUN 2
#define BREAKCHAR '#'

/* 
	Arduino input and output models
*/

#define OSERIAL 1
#define OLCD 2
#define OPRT 4
#define OTFT 8
#define OFILE 16

#define ISERIAL 1
#define IKEYBOARD 2
#define IFILE 16

/*
	All BASIC keywords
*/

const char sge[]   PROGMEM = "=>";
const char sle[]   PROGMEM = "<=";
const char sne[]   PROGMEM = "<>";
// Palo Alto language set
const char sprint[]  PROGMEM = "PRINT";
const char slet[]    PROGMEM = "LET";
const char sinput[]  PROGMEM = "INPUT";
const char sgoto[]   PROGMEM = "GOTO";
const char sgosub[]  PROGMEM = "GOSUB";
const char sreturn[] PROGMEM = "RETURN";
const char sif[]     PROGMEM = "IF";
const char sfor[]    PROGMEM = "FOR";
const char sto[]     PROGMEM = "TO";
const char sstep[]   PROGMEM = "STEP";
const char snext[]   PROGMEM = "NEXT";
const char sstop[]   PROGMEM = "STOP";
const char slist[]   PROGMEM = "LIST";
const char snew[]    PROGMEM = "NEW";
const char srun[]  	 PROGMEM = "RUN";
const char sabs[]    PROGMEM = "ABS";
const char srnd[]    PROGMEM = "RND";
const char ssize[]   PROGMEM = "SIZE";
const char srem[]    PROGMEM = "REM";
// Apple 1 language set
const char snot[]    PROGMEM = "NOT";
const char sand[]    PROGMEM = "AND";
const char sor[]     PROGMEM = "OR";
const char slen[]    PROGMEM = "LEN";
const char ssgn[]    PROGMEM = "SGN";
const char speek[]   PROGMEM = "PEEK";
const char sdim[]    PROGMEM = "DIM";
const char sclr[]    PROGMEM = "CLR";
const char sscr[]    PROGMEM = "SCR";
const char slomem[]  PROGMEM = "LOMEM";
const char shimem[]  PROGMEM = "HIMEM";
const char stab[]    PROGMEM = "TAB";
const char sthen[]   PROGMEM = "THEN";
const char send[]    PROGMEM = "END";
const char spoke[]   PROGMEM = "POKE";
// Stefan's tinybasic additions
const char scont[]   PROGMEM = "CONT";
const char ssqr[]    PROGMEM = "SQR";
const char sfre[]    PROGMEM = "FRE";
const char sdump[]   PROGMEM = "DUMP";
const char sbreak[]  PROGMEM = "BREAK";
const char ssave[]   PROGMEM = "SAVE";
const char sload[]   PROGMEM = "LOAD";
const char sget[]    PROGMEM = "GET";
const char sput[]    PROGMEM = "PUT";
const char sset[]    PROGMEM = "SET";
// Arduino functions
const char spinm[]   PROGMEM = "PINM";
const char sdwrite[] PROGMEM = "DWRITE";
const char sdread[]  PROGMEM = "DREAD";
const char sawrite[] PROGMEM = "AWRITE";
const char saread[]  PROGMEM = "AREAD";
const char sdelay[]  PROGMEM = "DELAY";
const char smillis[]  PROGMEM = "MILLIS";
const char stone[]    PROGMEM = "ATONE";
const char splusein[] PROGMEM = "PULSEIN";
const char sazero[] PROGMEM = "AZERO";
// SD Card DOS functions
const char scatalog[] PROGMEM = "CATALOG";
const char sdelete[] PROGMEM = "DELETE";
const char sfopen[] PROGMEM = "OPEN";
const char sfclose[] PROGMEM = "CLOSE";
// low level access functions
const char susr[] PROGMEM = "USR";
const char scall[] PROGMEM = "CALL";

const char* const keyword[] PROGMEM = {
// Palo Alto BASIC
	sge, sle, sne, sprint, slet, sinput, 
	sgoto, sgosub, sreturn, sif, sfor, sto,
	sstep, snext, sstop, slist, snew, srun,
	sabs, srnd, ssize, srem,
// Apple 1 BASIC additions
	snot, sand, sor, slen, ssgn, speek, sdim,
	sclr, sscr, slomem, shimem, stab, sthen, 
	send, spoke,
// Stefan's additions
	scont, ssqr, sfre, sdump, sbreak, ssave,
	sload, sget, sput, sset, 
// Arduino stuff
    spinm, sdwrite, sdread, sawrite, saread, 
    sdelay, smillis, stone, splusein, sazero,
// SD Card DOS
    scatalog, sdelete, sfopen, sfclose,
// low level access
    susr, scall
// the end 
};

/*
	the message catalogue also moved to progmem
*/

// the messages and errors
#define MFILE        0
#define MPROMPT      1
#define MGREET 		 2
#define EGENERAL 	 3
#define EUNKNOWN	 4
#define ENUMBER      5 
#define EDIVIDE		 6
#define ELINE        7
#define ERETURN      8
#define ENEXT        9
#define EGOSUB       10 
#define EFOR         11
#define EOUTOFMEMORY 12
#define ESTACK 		 13
#define EDIM         14
#define ERANGE 		 15
#define ESTRING      16
#define EVARIABLE	 17
#define EFILE 		 18
#define EFUN 		 19
#define EARGS		 20
#define EEEPROM		 21
#define ESDCARD		 22

const char mfile[]    	PROGMEM = "file.bas";
const char mprompt[]	PROGMEM = "> ";
const char mgreet[]		PROGMEM = "Stefan's Basic 1.2"; // buffer overrun here - harmless
const char egeneral[]  	PROGMEM = "Error";
const char eunknown[]  	PROGMEM = "Syntax";
const char enumber[]	PROGMEM = "Number";
const char edivide[]  	PROGMEM = "Div by 0";
const char eline[]  	PROGMEM = "Unknown Line";
const char ereturn[]    PROGMEM = "Return";
const char enext[]		PROGMEM = "Next";
const char egosub[] 	PROGMEM = "GOSUB";
const char efor[]		PROGMEM = "FOR";
const char emem[]  	   	PROGMEM = "Memory";
const char estack[]    	PROGMEM = "Stack";
const char edim[]		PROGMEM = "DIM";
const char erange[]  	PROGMEM = "Range";
const char estring[]	PROGMEM = "String";
const char evariable[]  PROGMEM = "Variable";
const char efile[]  	PROGMEM = "File";
const char efun[] 	 	PROGMEM = "Function";
const char eargs[]  	PROGMEM = "Args";
const char eeeprom[]	PROGMEM = "EEPROM";
const char esdcard[]	PROGMEM = "SD card";

const char* const message[] PROGMEM = {
	mfile, mprompt, mgreet,egeneral, eunknown,
	enumber, edivide, eline, ereturn, enext,
	egosub, efor, emem, estack, edim, erange,
	estring, evariable, efile, efun, eargs, 
	eeeprom, esdcard
};


// preparation for numbers and addresses not being 16 bit
// not used consistently! Don't change this right now.
typedef short number_t;
typedef unsigned short address_t;
const int numsize=sizeof(number_t);
const int addrsize=sizeof(address_t);
const int eheadersize=addrsize+1;

/*
	The basic interpreter is implemented as a stack machine
	with global variable for the interpreter state, the memory
	and the arithmetic during run time.

	stack is the stack memory and sp controls the stack.

	ibuffer is an input buffer and *bi a pointer to it.

	sbuffer is a short buffer for arduino progmem access. 

	vars is a static array of 26 single character variables.

	mem is the working memory of the basic interperter.

	x, y, xc, yc are two 16 bit and two 8 bit accumulators.

	z is a mixed 16/8 bit accumulator

	ir, ir2 are general index registers for string processing.

	token contains the actually processes token.

	er is the nontrapable error status

	ert is the trapable error status 

	st, here and top are the interpreter runtime controls.

	nvars is the number of vars the interpreter has stored.

	form is used for number formation Palo Alto BASIC style.

	rd is the random number storage.

	id and od are the input and output model for an arduino
		they are set to serial by default

	fnc counts the depth of for - next loop nesting

	ifd, ofd are the filedescriptors for input/output
	ifile and ofile their Arduino SD card analoga


*/
static number_t stack[STACKSIZE];
static address_t sp=0; 

static char sbuffer[SBUFSIZE];

static char ibuffer[BUFSIZE] = "\0";
static char *bi;

static number_t vars[VARSIZE];

static signed char mem[MEMSIZE];
static address_t himem = MEMSIZE-1;

#ifdef HASFORNEXT
static struct {char varx; char vary; address_t here; number_t to; number_t step;} forstack[FORDEPTH];
static short forsp = 0;
#endif

#ifdef HASGOSUB
static address_t gosubstack[GOSUBDEPTH];
static short gosubsp = 0;
static char fnc; 
#endif

static number_t x, y;
static signed char xc, yc;

struct twobytes {signed char l; signed char h;};
static union accu168 { number_t i; struct twobytes b; signed char c[numsize]; } z;

static char *ir, *ir2;
static signed char token;
static signed char er;
static signed char ert;

static signed char st; 
static address_t here; 
static address_t top;

static address_t nvars = 0; 

static char form = 0;

static unsigned int rd;

static unsigned char id;
static unsigned char od;


void iodefaults() {
#ifdef STANDALONE
	id = IKEYBOARD;
	od = OLCD;
#else
	id = ISERIAL;
	od = ISERIAL;
#endif
}

#ifndef ARDUINO
FILE* ifd;
FILE* ofd;
#else 
#ifdef ARDUINOSD
File ifile;
File ofile;
#define FILE_OWRITE (O_READ | O_WRITE | O_CREAT | O_TRUNC)
#endif
#endif

/* 
	Layer 0 functions 

	variable and memory handling - interface between memory 
 	and variable storage

*/

// heap management 
address_t bmalloc(signed char, char, char, short);
address_t bfind(signed char, char, char);
address_t blength (signed char, char, char);
void clrvars();

// normal variables
void  createvar(char, char);
number_t getvar(char, char);
void  setvar(char, char, number_t);

// low level memory access packing n*8bit bit into n 8 bit objects
// e* is for Arduino EEPROM
number_t getnumber(address_t);
void  setnumber(address_t, number_t);
number_t egetnumber(address_t);
void  esetnumber(address_t, number_t);

// array handling
void  createarry(char, char, number_t);
void  array(char, char, char, number_t, number_t*);

// string handling 
void  createstring(char, char, number_t);
char* getstring(char, char, number_t);
void  setstring(char, char, number_t, char *, number_t);

// access memory dimensions and for strings also the actual length
number_t arraydim(char, char);
number_t stringdim(char, char);
number_t lenstring(char, char);
void setstringlength(char, char, number_t);

// get keyword from PROGMEM
char* getkeyword(signed char);
char* getmessage(char);
void printmessage(char);

// error handling
void error(signed char);
void reseterror();
void debugtoken();
void debug(char*);

// stack stuff
void push(number_t);
number_t pop();
void drop();
void clearst();

// Arduino I/O
void lcdscroll();
void lcdclear();
void lcdwrite(char);
void lcdbegin();

// input output
// these are the platfrom depended lowlevel functions
void ioinit();
void picogetchar(int);
void outch(char);
char inch();
char checkch();
void ins(char*, short); 

// from here on the functions only use the functions above
// there should be no platform depended code here
void outcr();
void outspc();
void outs(char*, short);
void outsc(char*);
char innumber(number_t*);
short parsenumber(char*, number_t*);
void outnumber(number_t);

/*
	Arduino definitions and code
	wrapper functions for the Arduino libraries
	Arduino definitions and code
*/

// EEPROM 
#if defined(ARDUINO) && defined(ARDUINOEEPROM) && ! defined(ESP8266)
address_t elength() { return EEPROM.length(); }
void eupdate(address_t i, short c) { EEPROM.update(i, c); }
short eread(address_t i) { return EEPROM.read(i); }
// save a file to EEPROM
void esave() {
	address_t x=0;
	if (top+eheadersize < elength()) {
		x=0;
		eupdate(x++, 0); // EEPROM per default is 255, 0 indicates that there is a program

		// store a number 
		esetnumber(x, top);
		z.i=top;
		x+=numsize;

		while (x < top+eheadersize){
			eupdate(x, mem[x-eheadersize]);
			x++;
		}
		eupdate(x++,0);
	} else {
		error(EOUTOFMEMORY);
		er=0; //oh oh! check this.
	}
}
// load a file from EEPROM
void eload() {
	address_t x=0;
	if (eread(x) == 0 || eread(x) == 1) { // have we stored a program?
		x++;

		// get a number
		top=egetnumber(x);
		x+=numsize;

		while (x < top+eheadersize){
			mem[x-eheadersize]=eread(x);
			x++;
		}
	} else { // no valid program data is stored 
		error(EEEPROM);
	}
}
#else
address_t elength() { return 0; }
void eupdate(address_t i, short c) { return; }
short eread(address_t i) { return 0; }
void esave() { error(EEEPROM); return; }
void eload() { error(EEEPROM); return; }
#endif

// global variables for the keyboard
#ifdef ARDUINOPS2
const int PS2DataPin = 3;
const int PS2IRQpin =  2;
PS2Keyboard keyboard;
#endif

// global variables for the LCD
#ifdef ARDUINOLCD
#ifdef LCDSHIELD
// LCD shield pins to Arduino
const int lcd_rows = 2;
const int lcd_columns = 16;
const int pin_RS = 8; 
const int pin_EN = 9; 
const int pin_d4 = 4; 
const int pin_d5 = 5; 
const int pin_d6 = 6; 
const int pin_d7 = 7; 
const int pin_BL = 10; 
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);
#else 
// a I2C display connected
const int lcd_rows = 4;
const int lcd_columns = 20;
LiquidCrystal_I2C lcd(0x27, lcd_columns, lcd_rows);
#endif
char lcdbuffer[lcd_rows][lcd_columns];
char lcdmyrow = 0;
char lcdmycol = 0;
#endif

// global variables for a TFT
#ifdef ARDUINOTFT
#endif

// global variables for an Arduino SD card
#ifdef ARDUINOSD
// the SD chip select, set 4 for the Ethernet/SD shield
//const char sd_chipselect = 53;
const char sd_chipselect = 4;
#endif

// global variables for an Ethernet shield
#ifdef ARDUINOETH
const char eth_chipselect = 10;
#endif

// the wrappers of the arduino io functions, to avoid 
// spreading arduino code in the interpreter code 
// also, this would be the place to insert the Wiring code
// for raspberry
#ifdef ARDUINO
void aread(){ push(analogRead(pop())); }
void dread(){ push(digitalRead(pop())); }
void awrite(number_t p, number_t v){
	if (v >= 0 && v<256) analogWrite(p, v);
	else error(ERANGE);
}
void dwrite(number_t p, number_t v){
	if (v == 0) digitalWrite(p, LOW);
	else if (v == 1) digitalWrite(p, HIGH);
	else error(ERANGE);
}
void pinm(number_t p, number_t m){
	if (m>=0 && m<=2)  pinMode(p, m);
	else error(ERANGE); 
}
void bmillis() {
	number_t m;
	m=(number_t) (millis()/pop() % 32768);
	push(m); 
};
void bpulsein() { 
  unsigned long t, pt;
  t=((unsigned long) pop())*1000;
  y=pop(); 
  x=pop(); 
  pt=pulseIn(x, y, t)/10; 
  push(pt);
}
#else
void aread(){ return; }
void dread(){ return; }
void awrite(number_t p, number_t v){}
void dwrite(number_t p, number_t v){}
void pinm(number_t p, number_t m){}
void delay(number_t t) {}
struct timespec start_time;
void bmillis() {
#ifndef MINGW
	struct timespec ts;
	unsigned long dt;
	short m;
	timespec_get(&ts, TIME_UTC);
	dt=(ts.tv_sec-start_time.tv_sec)*1000+(ts.tv_nsec-start_time.tv_nsec)/10000000;
	m=(short) ( dt/pop() % 32768);
	push( (short) m ); 
#else
	push(0);
#endif
};
void bpulsein() { pop(); pop(); pop(); push(0); }
#endif

/* 	
	Layer 1 function, provide data and do the heavy lifting 
	for layer 2.
*/

// lexical analysis
void whitespaces();
void debugtoken();
void nexttoken();

// storeing and retrieving programs
char nomemory(number_t);
void dumpmem(address_t, address_t);
void storetoken(); 
char memread(address_t);
void gettoken();
void firstline();
void nextline();
void findline(address_t);
unsigned short myline(address_t);
void moveblock(address_t, address_t, address_t);
void zeroblock(address_t, address_t);
void diag();
void storeline();

// read arguments from the token stream.
char  termsymbol();
void  parsesubstring();
short parsesubscripts();
void  parsenarguments(char);
short parsearguments();

// mathematics and other functions
void rnd();
void sqr();
void fre();
void peek();
void xabs();
void xsgn();

// string values
char stringvalue();
void streval();

// expression evaluation 
void factor();
void term();
void addexpression();
void compexpression();
void notexpression();
void andexpression();
void expression();

/* 
	Layer 2 - statements call Layer 1 functions and 
	use the global variables 
*/

// basic commands of the core language set
void xprint();
void assignment();
void xinput();
void xgoto();
void xreturn();
void xif();

// optional FOR NEXT loops
#ifdef HASFORNEXT
void findnext();
void xfor();
void xnext();
void xbreak();
void pushforstack();
void popforstack();
void dropforstack();
void clrforstack();
#endif

// optional GOSUB commands
#ifdef HASGOSUB
void xreturn();
void pushgosubstack();
void popgosubstack();
void dropgosubstack();
void clrgosubstack();
#endif

// control commands and misc
void outputtoken();
void xlist();
void xrun();
void xnew();
void xrem();
void xclr();
void xdim();
void xpoke();
void xtab();
void xdump();

// file access 
char* getfilename();
void xsave();
void xload();
void xcatalog();
void xdelete();
void xopen();
void xclose();

// low level I/O in BASIC
void xget();
void xput();
void xset();

// Arduino IO control
void xdwrite();
void xawrite();
void xpinm();
void xdelay();
void xtone();

// low level access functions
void xcall();
void xusr();

// the statement loop
void statement();

/*
	Layer 0 function - variable handling.

	These function access variables, 
	In this implementation variables are a 
	static array and we simply subtract 'A' 
	from the variable name to get the index
	any other way to generate an index from a 
	byte hash can be used just as well.

	delvar and createvar as stubs for further 
	use. They are not yet used consistenty in
	the code.
 */

// allocate a junk of memory for a variable on the heap
// every objects is identified by name (c,d) and type t
// 3 bytes are used here but 2 would be enough
address_t bmalloc(signed char t, char c, char d, short l) {

	address_t vsize;     // the length of the header
	address_t b;


    if (DEBUG) { outsc("** bmalloc with token "); outnumber(t); outcr(); }

	/* 
		how much space is needed
			3 bytes for the token and the 2 name characters
			numsize for every number including array length
			one byte for every string character
	*/

	if ( t == VARIABLE ) vsize=numsize+3; 	
	else if ( t == ARRAYVAR ) vsize=numsize*l+numsize+3;
	else if ( t == STRINGVAR ) vsize=l+numsize+3;
	else { error(EUNKNOWN); return 0; }
	if ( (himem - top) < vsize) { error(EOUTOFMEMORY); return 0;}

	// here we would create the hash, currently simplified
	// the hash is the first digit of the variable plus the token

	// write the header - inefficient - 3 bytes for a hash
	b=himem;
	mem[b--]=c;
	mem[b--]=d;
	mem[b--]=t;

	// for strings and arrays write the length
	if (t == ARRAYVAR || t == STRINGVAR) {

		// store a number
		//z.i=vsize-(numsize+3);
		//mem[b--]=z.b.h;
		//mem[b--]=z.b.l;
		b=b-numsize+1;
		setnumber(b, vsize-(numsize+3));
		b--;
	}

	// reserve space for the payload
	himem-=vsize;
	nvars++;

	return himem+1;
}


// bfind passes back the location of the object as result
// the length of the object is in z.i as a side effect 
address_t bfind(signed char t, char c, char d) {

	address_t b = MEMSIZE-1;
	signed char t1;
	char c1, d1;
	short i=0;


	while (i < nvars) { 

		c1=mem[b--];
		d1=mem[b--];
		t1=mem[b--];

		if (t1 == STRINGVAR || t1 == ARRAYVAR) {

			// get a number
			// z.b.h=mem[b--];
			// z.b.l=mem[b--];

			b=b-numsize+1;
			z.i=getnumber(b);
			b--;

		} else 
			z.i=numsize; 

		b-=z.i;

		if (c1 == c && d1 == d && t1 == t) return b+1;
		i++;
	}

	return 0;

}

// the length of an object
address_t blength (signed char t, char c, char d) {
	if (! bfind(t, c, d)) return 0;
	return z.i;
}


// ununsed so far, simple variables are created on the fly
void createvar(char c, char d){
	return;
}

// get and create a variable 
number_t getvar(char c, char d){
	address_t a;

	if (DEBUG) { outsc("* getvar "); outch(c); outch(d); outspc(); outcr(); }

	// the static variable array
	if (c >= 65 && c<=91 && d == 0)
			return vars[c-65];

	// the special variables 
	if ( c == '@' )
		switch (d) {
			case 'S': 
				return ert;
			case 'I':
				return id;
			case 'O':
				return od;
			case 'C':
				if (checkch()) return inch(); else return 0;
		}

	// dynamically allocated vars
	a=bfind(VARIABLE, c, d);
	if ( a == 0) {
		a=bmalloc(VARIABLE, c, d, 0);
		if (er != 0) return 0;
	} 

	return getnumber(a);
}

// set and create a variable 
void setvar(char c, char d, number_t v){
	address_t a;

	if (DEBUG) { outsc("* setvar "); outch(c); outch(d); outspc(); outnumber(v); outcr(); }

	// the static variable array
	if (c >= 65 && c<=91 && d == 0) {
		vars[c-65]=v;
		return;
	}

	// the special variables 
	if ( c == '@' )
		switch (d) {
			case 'S': 
				ert=v;
				return;
			case 'I':
				id=v;
				return;
			case 'O':
				od=v;
				return;
			case 'C':
				outch(v);
				return;
		}


	// dynamically allocated vars
	a=bfind(VARIABLE, c, d);
	if ( a == 0) {
		a=bmalloc(VARIABLE, c, d, 0);
		if (er != 0) return;
	} 

	setnumber(a, v);
}


void clrvars() {
	for (char i=0; i<VARSIZE; i++) vars[i]=0;
	nvars=0;
	himem=MEMSIZE-1;
}

// the program memory access - attention there is a hack here
// this can sometimes also access eeproms for autorun through
// the memread wrapper - still not really redundant to egetnumber
number_t getnumber(address_t m){

	if ( numsize == 2 ) {
		z.b.l=memread(m++);
		z.b.h=memread(m);
	} else {
 		for (int i=0; i<numsize; i++) {
			z.c[i]=memread(m++);
		}
	}
	return z.i;
}

// the eeprom memory access 
number_t egetnumber(address_t m){

	if ( numsize == 2 ) {
		z.b.l=eread(m++);
		z.b.h=eread(m);
	} else {
 		for (int i=0; i<numsize; i++) {
			z.c[i]=eread(m++);
		}
	}
	return z.i;
}


void setnumber(address_t m, number_t v){
	z.i=v;

	if ( numsize == 2 ) {
		mem[m++]=z.b.l;
		mem[m]=z.b.h;
	} else {
 		for (int i=0; i<numsize; i++) {
			mem[m++]=z.c[i];
		}
	}
}

void esetnumber(address_t m, number_t v){
	z.i=v;

	if ( numsize == 2 ) {
		eupdate(m++, z.b.l);
		eupdate(m, z.b.h);
	} else {
 		for (int i=0; i<numsize; i++) {
			eupdate(m++, z.c[i]);
		}
	}
}


void createarry(char c, char d, number_t i) {

	if (bfind(ARRAYVAR, c, d)) { error(EVARIABLE); return; }
	(void) bmalloc(ARRAYVAR, c, d, i);
	if (er != 0) return;
	if (DEBUG) { outsc("* created array "); outch(c); outspc(); outnumber(nvars); outcr(); }

}

// generic array access function 
void array(char m, char c, char d, number_t i, number_t* v) {

	unsigned short a;
	unsigned short h;
	char e = FALSE;

	if (DEBUG) { outsc("* get array "); outch(c); outspc(); outnumber(i); outcr(); }

	// special arrays 
	if (c == '@') {

		switch(d) {

			// Dr. Wangs end of memory array
			case 0: {
				h=(himem-top)/numsize;
				a=himem-numsize*i+1;
				break;
			}

			// EEPROM access 
			case 'E': {
				h=elength()/numsize;
				a=elength()-numsize*i;
				e=TRUE;
			}
		}

	} else {

		// dynamically allocated arrays
		a=bfind(ARRAYVAR, c, d);
		if (a == 0) error(EVARIABLE);
		h=z.i/numsize;
		a=a+(i-1)*numsize;
	}

	// is the index in range
	if ( (i < 1) || (i > h) ) error(ERANGE); 


	// set or get the array
	if (m == 'g') {
		if (! e) {
			*v=getnumber(a);
		} else {
			// z.b.h=eread(a++);
			// z.b.l=eread(a);
			// *v=z.i;	
			*v=egetnumber(a);
		}
	} else if ( m == 's') {
		if (! e) {
			setnumber(a, *v);
		} else {
			// z.i=*v;
			// eupdate(elength() - i*numsize, z.b.h);
			// eupdate(elength() - i*numsize + 1, z.b.l);
			esetnumber(a, *v);
		}
	}
}

void createstring(char c, char d, number_t i) {

	if (bfind(STRINGVAR, c, d)) { error(EVARIABLE); return; }
	(void) bmalloc(STRINGVAR, c, d, i+1);
	if (er != 0) return;
	if (DEBUG) { outsc("Created string "); outch(c); outch(d); outspc(); outnumber(nvars); outcr(); }
}


char* getstring(char c, char d, number_t b) {	
	address_t a;

	if (DEBUG) { outsc("* get string var "); outch(c); outch(d); outspc(); outnumber(b); outcr(); }

	// direct access to the input buffer
	if ( c == '@')
			return ibuffer+b;

	// dynamically allocated strings
	a=bfind(STRINGVAR, c, d);
	if (er != 0) return 0;
	if (a == 0) {
		error(EVARIABLE);
		return 0;
	}

	if ( (b < 1) || (b > z.i) ) {
		error(ERANGE); return 0;
	}

	a=a+b;
	return (char *)&mem[a];
}
 
 // this function is currently not used 
number_t arraydim(char c, char d) {
	if (c == '@')
		switch (d) {
			case 0:
				return (himem-top)/numsize;
			case 'E':
				return elength()/numsize;
		}

	return blength(ARRAYVAR, c, d)/numsize;
}

number_t stringdim(char c, char d) {
	if (c == '@')
		return BUFSIZE-1;
	return blength(STRINGVAR, c, d)-1;
}

number_t lenstring(char c, char d){
	char* b;
	if (c == '@')
		return ibuffer[0];
	
	b=getstring(c, d, 1);
	if (er != 0) return 0;
	return b[-1];
}

void setstringlength(char c, char d, number_t l) {
	address_t a; 

	if (c == '@') {
		*ibuffer=l;
		return;
	}

	a=bfind(STRINGVAR, c, d);
	if (er != 0) return;
	if (a == 0) {
		error(EVARIABLE);
		return;
	}

	if (l < z.i-1)
		mem[a]=l;
	else
		error(ERANGE);

}

void setstring(char c, char d, number_t w, char* s, number_t n) {
	char *b;
	address_t a;

	if (DEBUG) { outsc("* set string "); outch(c); outch(d); outspc(); outnumber(w); outcr(); }


	if ( c == '@') {
		b=ibuffer;
	} else {
		a=bfind(STRINGVAR, c, d);
		if (er != 0) return;
		if (a == 0) {
			error(EVARIABLE);
			return;
		}

		b=(char *)&mem[a+1];

	}

	if ( (w+n-1) <= stringdim(c, d) ) {
		for (int i=0; i<n; i++) { b[i+w]=s[i]; } 
		b[0]=w+n-1; 	
	}
	else 
		error(ERANGE);
}

/* 
	Layer 0 - keyword handling - PROGMEM logic goes here
		getkeyword is the only access to the keyword array
		in the code.  

		Same for messages and errors
*/ 

char* getkeyword(signed char t) {
	if (t < BASEKEYWORD || t > BASEKEYWORD+NKEYWORDS) {
		error(EUNKNOWN);
		return 0;
	} else 
#ifndef ARDUINOPROGMEM
	return (char *) keyword[t-BASEKEYWORD];
#else
	strcpy_P(sbuffer, (char*) pgm_read_word(&(keyword[t-BASEKEYWORD]))); 
	return sbuffer;
#endif
}

char* getmessage(char i) {

	if (i >= NKEYWORDS) return NULL;

#ifndef ARDUINOPROGMEM
	return (char *) message[i];
#else
	strcpy_P(sbuffer, (char*) pgm_read_word(&(message[i]))); 
	return sbuffer;
#endif
}

void printmessage(char i){
	outsc((char *)getmessage(i));
}

/*
  Layer 0 - error handling

  The general error handler. The static variable er
  contains the error state. 

  Strategy: the error function writes the message and then 
  clears the stack. All calling functions must check er and 
  return after funtion calls with no further messages etc.
  reseterror sets the error state to normal and end the 
  run loop.

*/ 

void error(signed char e){
	er=e;
	if (st != SINT) {
		outnumber(myline(here));
		outch(':');
		outspc();
	}
	printmessage(e);
	outspc();
	printmessage(EGENERAL);
	outcr();
	clearst();
	clrforstack();
	clrgosubstack();
	iodefaults();
}

void reseterror() {
	er=0;
	here=0;
	st=SINT;
}

#ifdef DEBUG
void debugtoken(){
	outsc("* token: ");
	switch(token) {
		case LINENUMBER: 
			outsc("LINE ");
			outputtoken();
			outcr();
			break;
		case NUMBER:
			outsc("NUMBER ");
			outputtoken();
			outcr();
			break;
		case VARIABLE:
			outsc("VARIABLE ");
			outputtoken();
			outcr();
			break;	
		case ARRAYVAR:
			outsc("ARRAYVAR ");
			outputtoken();
			outcr();
			break;		
		case STRING:
			outsc("STRING ");
			outputtoken();
			outcr();
			break;
		case STRINGVAR:
			outsc("STRINGVAR ");
			outputtoken();
			outcr();
			break;
		case EOL: 
			outsc("EOL");
			outcr();
			break;	
		default:
			outputtoken();
			outcr();	
	}
}

void debug(char *c){
	outch('*');
	outspc();
	outsc(c); 
	debugtoken();
}
#endif

/*
	Arithmetic and runtime operations are mostly done
	on a stack of 16 bit objects.
*/

void push(number_t t){
	if (DEBUG) {outsc("** push sp= "); outnumber(sp); outcr(); }
	if (sp == STACKSIZE)
		error(ESTACK);
	else
		stack[sp++]=t;
}

number_t pop(){
	if (DEBUG) {outsc("** pop sp= "); outnumber(sp); outcr(); }
	if (sp == 0) {
		error(ESTACK);
		return 0;
	}
	else
		return stack[--sp];	
}

void drop() {
	if (DEBUG) {outsc("** drop sp= "); outnumber(sp); outcr(); }
	if (sp == 0)
		error(ESTACK);
	else
		--sp;	
}

void clearst(){
	sp=0;
}

/* 

	Stack handling for gosub and for

*/

#ifdef HASFORNEXT
void pushforstack(){
	if (forsp < FORDEPTH) {
		forstack[forsp].varx=xc;
		forstack[forsp].vary=yc;
		forstack[forsp].here=here;
		forstack[forsp].to=x;
		forstack[forsp].step=y;
		forsp++;	
		return;	
	} else 
		error(EFOR);
}


void popforstack(){
	if (forsp>0) {
		forsp--;
	} else {
		error(EFOR);
		return;
	} 
	xc=forstack[forsp].varx;
	yc=forstack[forsp].vary;
	here=forstack[forsp].here;
	x=forstack[forsp].to;
	y=forstack[forsp].step;
}

void dropforstack(){
	if (forsp>0) {
		forsp--;
	} else {
		error(EFOR);
		return;
	} 
}

void clrforstack() {
	forsp=0;
	fnc=0;
}
#endif


/* 

	Input and output functions.

	On Mac/Linux the interpreter is implemented to use only putchar 
	and getchar.

	These functions need to be provided by the stdio lib 
	or by any other routine like a serial terminal or keyboard 
	interface of an arduino.
 
 	ioinit(): called at setup to initialize what ever io is needed
 	outch(): prints one ascii character 
 	inch(): gets one character (and waits for it)
 	checkch(): checks for one character (non blocking)
 	ins(): reads an entire line (uses inch except for pioserial)

 	For picoserial blocking I/O makes little sense. We read directly
 	into the input buffer through the interrupt routine. 

 	In addition the this a few "device driver functions" are also 
 	included to simplify keyboard, LCD and TFT code 

*/

/* 
	 device driver code - emulates a lcd_columns * lcd_rows 
	 very dumb ascii terminal
*/

#ifdef ARDUINOLCD
void lcdscroll(){
	short r,c;
	short i;

  	for (r=1; r<lcd_rows; r++)
    	for (c=0; c<lcd_columns; c++)
      		lcdbuffer[r-1][c]=lcdbuffer[r][c];

   for (c=0; c<lcd_columns; c++) lcdbuffer[lcd_rows-1][c]=0;

   lcd.clear();
   lcd.home();

	for (r=0; r<lcd_rows-1; r++) {
   		lcd.setCursor(0, r);
    	for (c=0; c<lcd_columns; c++) {
      		if (lcdbuffer[r][c] >= 32) {
        		lcd.write(lcdbuffer[r][c]);
      		}
    	}
   }
   lcdmyrow=lcd_rows-1;
   return;
}

void lcdclear() {
	short r,c;
	for (r=0; r<lcd_rows; r++)
		for (c=0; c<lcd_columns; c++)
      		lcdbuffer[r][c]=0;
	lcd.clear();
  	lcd.home();
  	lcdmyrow=0;
  	lcdmycol=0;
  	return;
}

void lcdwrite(char c) {

	// the special characters the LCD need to know
  	switch(c) {
    	case 02: // STX is used for Home
    		lcd.home();
    		lcdmyrow=0;
    		lcdmycol=0;
    		break;
    	case 8: // back one character
    		if (lcdmycol > 0) lcdmycol--;
    		break;
    	case 9: // forward one character 
    		if (lcdmycol < lcd_columns) lcdmycol++;
    		break;
  		case 10: // this is LF Unix style doing also a CR
    		lcdmyrow=(lcdmyrow + 1);
    		if (lcdmyrow >= lcd_rows) {
      			lcdscroll(); 
    		}
    		lcdmycol=0;
    		break;
    	case 11: // one char down 
    		lcdmyrow=(lcdmyrow+1) % lcd_rows;
    		break;
    	case 12: // form feed is clear screen
    		lcdclear();
    		return;
    	case 13: // classical carriage return 
    		lcdmycol=0;
    		break;
    	case 14:
    		lcd.cursor();
    		return; 
    	case 15:
    		lcd.noCursor();
    		return;
    	case 127: // delete
    		if (lcdmycol > 0) {
      			lcdmycol--;
      			lcdbuffer[lcdmyrow][lcdmycol]=0;
      			lcd.setCursor(lcdmycol, lcdmyrow);
      			lcd.write(" ");
      			lcd.setCursor(lcdmycol, lcdmyrow);
      			return;
    		}
  	}


  	lcd.setCursor(lcdmycol, lcdmyrow);
	if (c < 32 ) return; 

	lcd.write(c);
	lcdbuffer[lcdmyrow][lcdmycol++]=c;
	if (lcdmycol == lcd_columns) {
		lcdmycol=0;
		lcdmyrow=(lcdmyrow + 1);
    	if (lcdmyrow >= lcd_rows) {
      		lcdscroll(); 
    	}
		lcd.setCursor(lcdmycol, lcdmyrow);
	}
}

void lcdbegin() {
#ifndef ARDUINOI2C
	lcd.begin(lcd_columns, lcd_rows);
#else 
	lcd.init(); 
	lcd.backlight();
#endif
}

int lcdactive() {
	return (od & OLCD);
}

#else 
void lcdwrite(char c) {}
void lcdbegin() {}
int lcdactive() {return 0; }
#endif


/* 

	Platform dependend IO functions, implemented models are
		- Standard C library
		- Arduino Serial 
		- Arduino Picoserial

*/


// wrapper around file access
void filewrite(char c) {
#ifndef ARDUINO
	if (ofd) fputc(c, ofd); else ert=1;
#else
#ifdef ARDUINOSD
	if (ofile) ofile.write(c); else ert=1;
#endif
#endif
	return;
}

char fileread(){
	char c;
#ifndef ARDUINO
	if (ifd) c=fgetc(ifd); else { ert=1; return 0; }
#else
#ifdef ARDUINOSD
	if (ifile) c=ifile.read(); else { ert=1; return 0; }
#endif
#endif
	if (c == -1 ) {
		ert=-1;
	}
	return c;
}

// wrapper around console output
void serialwrite(char c) {
#ifndef ARDUINO
	putchar(c);
#else 
#ifndef USESPICOSERIAL
	Serial.write(c);
#else
	PicoSerial.print(c);
#endif
#endif
	return;	
}

// printer wrappers
void prtbegin() {
#ifdef ARDUINOPRT
	Serial1.begin(9600);
#endif
}

void prtwrite(char c) {
#ifdef ARDUINOPRT
	Serial1.write(c);
#endif
}

#ifndef ARDUINO
/* 
	this is C standard library stuff, we branch to file input/output
	if there is a valid file descriptor in fd.
*/
void ioinit() {
	iodefaults();
	return;
}

char inch(){
	char c;
	if (id == ISERIAL)
		return getchar(); 
	if (id == IFILE) 
		return fileread();
	return 0;
}

char checkch(){
	return TRUE;
}

void ins(char *b, short nb) {
	char c;
	short i = 1;
	while(i < nb-1) {
		c=inch();
		if (c == '\n' || c == '\r') {
			b[i]=0x00;
			b[0]=i-1;
			break;
		} else {
			b[i++]=c;
		} 
	}
}

#else 
#ifndef USESPICOSERIAL
/*

	This is standard Arduino code Serial code. 
	In inch() we wait for input by looping. 
	LCD output is controlled by the flag od.
	Keyboard code here is totally beta

*/ 

void ioinit() {
	Serial.begin(serial_baudrate);
   	lcdbegin(); 
   	prtbegin();
#ifdef ARDUINOPS2
	keyboard.begin(PS2DataPin, PS2IRQpin, PS2Keymap_German);
#endif
	iodefaults();
}

char inch(){
	char c=0;
	if (id == ISERIAL) {
		do 
			if (Serial.available()) c=Serial.read();
		while(c == 0); 
		return c;
	}
#ifdef ARDUINOPS2	
	if (id == IKEYBOARD) {
		do 
			if (keyboard.available()) c=keyboard.read();
		while(c == 0);	
    	if (c == 13) c=10;
    	if (c == '^') c='@';
		return c;
	}
#endif
}

char checkch(){
	if (Serial.available() && id == ISERIAL) return Serial.peek(); 
#ifdef ARDUINOPS2
	if (keyboard.available() && id == IKEYBOARD) return keyboard.read();
#endif
}

void ins(char *b, short nb) {
  	char c;
  	short i = 1;
  	while(i < nb-1) {
    	c=inch();
    	outch(c);
    	if (c == '\n' || c == '\r') {
      		b[i]=0x00;
      		b[0]=i-1;
      		break;
    	} else if (c == 127 && i>1) {
      		i--;
    	} else {
      		b[i++]=c;
    	} 
  	}
}

#else 

/*

	Picoserial allows to define an own input buffer and an 
	interrupt function. This is used to fill the input buffer 
	directly on read. Write is standard like in the Serial code.

*/ 

volatile static char picochar;
volatile static char picoa = FALSE;
volatile static char* picob = NULL;
static short picobsize = 0;
volatile static short picoi = 1;

void ioinit() {
	(void) PicoSerial.begin(serial_baudrate, picogetchar);
   	lcdbegin();  
   	iodefaults();
}

void picogetchar(int c){
	if (picob && (! picoa) ) {
    	picochar=c;
		if (picochar != '\n' && picochar != '\r' && picoi<picobsize-1) {
			picob[picoi++]=picochar;
			outch(picochar);
		} else {
			picoa = TRUE;
			picob[picoi]=0;
			picob[0]=picoi;
			picoi=1;
		}
		picochar=0; // every buffered byte is deleted
	} else {
    	if (c != 10) picochar=c;
	}
}

char inch(){
	char c;
	c=picochar;
	picochar=0;
	return c;
}

char checkch(){
    return picochar;
}

void ins(char *b, short nb) {
	picob=b;
	picobsize=nb;
	picoa=FALSE;
	while (! picoa);
	//outsc(b+1); 
	outcr();
}
#endif
#endif

/* 

	The generic IO code 

*/ 


// output one character to a stream
void outch(char c) {
	if (od == OSERIAL)
		serialwrite(c);
	if (od == OPRT) 
		prtwrite(c);
	if (od == OFILE) 
		filewrite(c); 
	if (od == OLCD) 
		lcdwrite(c);
}

// send a newline
void outcr() {
	outch('\n');
} 

// send a space
void outspc() {
	outch(' ');
}

// output a string of length x at index ir - basic style
void outs(char *ir, short l){
	for(int i=0; i<l; i++) outch(ir[i]);
}

// output a zero terminated string at ir - c style
void outsc(char *c){
	while (*c != 0) outch(*c++);
}


// reading a number from a char buffer 
// maximum number of digits is adjusted to 16
// ugly here, testcode when introducting 
// number_t was strictly 16 bit before
short parsenumber(char *c, number_t *r) {
	int nd = 0;
	*r=0;
	while (*c >= '0' && *c <= '9' && *c != 0) {
		*r=*r*10+*c++-'0';
		nd++;
		if (nd == SBUFSIZE) break;
	}
	return nd;
}

// use sbuffer as a char buffer to get a number input 
char innumber(number_t *r) {
	short i = 1;
	short s = 1;

again:
	ins(sbuffer, SBUFSIZE);
	while (i < SBUFSIZE) {
		if (sbuffer[i] == ' ' || sbuffer[i] == '\t') i++;
		if (sbuffer[i] == BREAKCHAR) return BREAKCHAR;
		if (sbuffer[i] == 0) return 1;
		if (sbuffer[i] == '-') {
			s=-1;
			i++;
		}
		if (sbuffer[i] >= '0' && sbuffer[i] <= '9') {
			(void) parsenumber(&sbuffer[i], r);
			*r*=s;
			return 0;
		} else {
			printmessage(ENUMBER); 
			outspc(); 
			printmessage(EGENERAL);
			outcr();
			*r=0;
			s=1;
			i=1;
			goto again;
		}
	}
	return 0;
}

// prints a 16 bit number
void outnumber(number_t n){
	char c, co;
	number_t d;
	short i=1;
	if (n<0) {
		outch('-');
		i++;
		n=-n;
	}

	// totally ugly test code remove soon
	d=10000;
	if (numsize == 4)
		d=1000000;
	if (numsize == 8)
		d=1000000000000000000;

	c=FALSE; // surpress leading 0s
	while (d > 0){
		co=n/d;
		n=n%d;
		if (co != 0 || d == 1 || c) {
			co=co+48;
			outch(co);
			i++;
			c=TRUE;
		}
		d=d/10;
	}

	// number formats in Palo Alto style
	while (i < form) {outspc(); i++; };

}

/* 

	Layer 1 functions - providing data into the global variable and 
	changing the interpreter state

*/


/*

	Lexical analyser - tokenizes the input line.

	nexttoken() increments the input buffer index bi and delivers values in the global 
	variable token, with arguments in the accumulator x and the index register ir
	xc is used in the routine. 

	xc, ir and x change values in nexttoken and deliver the result to the calling
	function.

	bi and ibuffer should not be changed or used for any other function in 
	interactive node as they contain the state of nexttoken(). In run mode 
	bi and ibuffer are not used as the program is fully tokenized in mem.

	debugtoken() is the debug function for the tokenizer
	whitespaces() skips whitespaces

*/ 


void whitespaces(){
	while (*bi == ' ' || *bi == '\t') bi++;
}

void nexttoken() {

	// RUN mode vs. INT mode
	if (st == SRUN || st == SERUN) {
		gettoken();
		return;
	}

	// after change in buffer logic the first byte is reserved for the length
	if (bi == ibuffer) bi++;

	// remove whitespaces outside strings
	whitespaces();

	// end of line token 
	if (*bi == '\0') { 
		token=EOL; 
		if (DEBUG) debugtoken();
		return; 
	}

	// unsigned numbers, value returned in x
	if (*bi <='9' && *bi>= '0'){
		bi+=parsenumber(bi, &x);
		token=NUMBER;
		if (DEBUG) debugtoken();
		return;
	}

	// strings between " " or " EOL, value returned in ir
	if (*bi == '"'){
		x=0;
		bi++;
		ir=bi;
		while(*bi != '"' && *bi !='\0') {
			x++;
			bi++;
		} 
		bi++;
		token=STRING;
		if (DEBUG) debugtoken();
		return;
	}

	// single character operators are their own tokens
	if (*bi == '+' || *bi == '-' || *bi == '*' || *bi == '/' || *bi == '%'  ||
		*bi == '\\' || *bi == ':' || *bi == ',' || *bi == '(' || *bi == ')' ) { 
			token=*bi; 
			bi++; 
			if (DEBUG) debugtoken();
			return; 
	}  


	// relations
	// single character relations are their own token
	// >=, =<, =<, =>, <> ore tokenized
	if (*bi == '=') {
		bi++;
		whitespaces();
		if (*bi == '>') {
			token=GREATEREQUAL;
			bi++;
		} else if (*bi == '<'){
			token=LESSEREQUAL;
			bi++;
		} else {
			token='=';
		}
		if (DEBUG) debugtoken();
		return;
	}

	if (*bi == '>'){
		bi++;
		whitespaces();
		if (*bi == '=') {
			token=GREATEREQUAL;
			bi++;
		} else  {
			token='>';
		}
		if (DEBUG) debugtoken();
		return;
	}

	if (*bi == '<'){
		bi++;
		whitespaces();
		if (*bi == '=') {
			token=LESSEREQUAL;
			bi++;
		} else  if (*bi == '>') {
			token=NOTEQUAL;
			bi++;
		} else {
			token='<';
		}
		if (DEBUG) debugtoken();
		return;
	}

	// keyworks and variables
	// isolate a word, bi points to the beginning, x is the length of the word
	// ir points to the end of the word after isolating.
	// @ is a letter here to make the special @ arrays possible

	// if (DEBUG) printmessage(EVARIABLE);
	x=0;
	ir=bi;
	while (-1) {
		// toupper 
		if (*ir >= 'a' && *ir <= 'z') {
			*ir-=32;
			ir++;
			x++;
		} else if (*ir >= '@' && *ir <= 'Z'){
			ir++;
			x++;
		} else {
			break;
		}
	}

/* 

	ir is reused here to implement string compares
	scanning the keyword array. 
	Once a keyword is detected the input buffer is advanced 
	by its length, and the token value is returned. 

	keywords are an arry of null terminated strings.

*/

	token=BASEKEYWORD;
	while (token < NKEYWORDS+BASEKEYWORD){
		ir=getkeyword(token);
		xc=0;
		while (*(ir+xc) != 0) {
			if (*(ir+xc) != *(bi+xc)){
				token++;
				xc=0;
				break;
			} else 
				xc++;
		}
		if (xc == 0)
			continue;
		if ( *(bi+xc) < 'A' || *(bi+xc) > 'Z' ) {
			bi+=xc;
			if (DEBUG) debugtoken();
			return;
		} else {
			bi+=xc;
			token=UNKNOWN;
			return;
		}
	}

/*
	a variable has length 1 in the first version
	 its literal value is returned in xc
	in addition to this, a number or $ is accepted as 
	second character like in Apple 1 BASIC

*/
	if ( x == 1 || (x == 2 && *bi == '@') ){
		token=VARIABLE;
		xc=*bi;
		yc=0;
		bi++;
		//whitespaces();
		if (*bi >= '0' && *bi <= '9') { 
			yc=*bi;
			bi++;
		} 
		if (xc == '@' && x == 2) {
			yc=*bi;
			bi++;
		}
		//whitespaces();
		if (*bi == '$') {
			token=STRINGVAR;
			bi++;
		}
		whitespaces();
		if (token == VARIABLE && *bi == '(' ) { 
			token=ARRAYVAR;
		}	
		if (DEBUG) debugtoken();
		return;
	}


// single letters are parsed and stored - not really good

	token=*bi;
	bi++;
	if (DEBUG) debugtoken();
	return;
}

/* 
	Layer 1 - program editor 

	Editing the program, the structure of a line is 
	LINENUMBER linenumber(2bytes) token(n bytes)

	store* stores something to memory 
	get* retrieves information

	No matter how long int is in the C implementation
	we pack into 2 bytes, this is clumsy but portable
	the store and get commands all operate on here 
	and advance it

	Storetoken operates on the variable top. 
	We always append at the end and the sort. 

	Gettoken operate on the variable here 
	which will also be the key variable in run mode.

	Tokens are stored including their payload.

	This group of functions changes global states and
	cannot be called at program runtime with saving
	the relevant global variable to the stack.

	Error handling is still incomplete.
*/

char nomemory(number_t b){
	if (top >= himem-b) return TRUE; else return FALSE;
}

// store a token - check free memory before changing anything
void storetoken() {
	short i=x;
	switch (token) {
		case NUMBER:
		case LINENUMBER:
			if ( nomemory(numsize+1) ) break;
			mem[top++]=token;	
			//z.i=x;
			//mem[top++]=z.b.l;
			//mem[top++]=z.b.h;
			setnumber(top, x);
			top+=numsize;
			return;
		case ARRAYVAR:
		case VARIABLE:
		case STRINGVAR:
			if ( nomemory(3) ) break;
			mem[top++]=token;
			mem[top++]=xc;
			mem[top++]=yc;
			return;
		case STRING:
			if ( nomemory(x+2) ) break;
			mem[top++]=token;
			mem[top++]=i;
			while (i > 0) {
				mem[top++] = *ir++;
				i--;
			}
			return;
		default:
			if ( nomemory(1) ) break;
			mem[top++]=token;
			return;
	}
	error(EOUTOFMEMORY);
} 


// wrapper around mem access for eeprom autorun on small Arduinos
char memread(address_t i){
#ifndef ARDUINOEEPROM
	return mem[i];
#else 
	if (st != SERUN) {
		return mem[i];
	} else {
		return eread(i+eheadersize);
	}
#endif
}


// get a token from memory
void gettoken() {

	// if we have reached the end of the program, EOL is always returned
	// we don't rely on mem having a trailing EOL
	if (here >= top) {
		token=EOL;
		return;
	}

	token=memread(here++);
	switch (token) {
		case NUMBER:
		case LINENUMBER:	
			x=getnumber(here);
			here+=numsize;	
			break;
		case ARRAYVAR:
		case VARIABLE:
		case STRINGVAR:
			xc=memread(here++);
			yc=memread(here++);
			break;
		case STRING:
			x=(unsigned char)memread(here++);  // if we run interactive or from mem, pass back the mem location
#ifndef ARDUINO
			ir=(char *)&mem[here];
			here+=x;
#else 
			if (st == SERUN) { // we run from the EEROM and cannot simply pass a pointer
				for(int i=0; i<x; i++) {
					ibuffer[i]=memread(here+i);   // we (ab)use the input buffer which is not needed here
				}
				ir=ibuffer;
			} else {
				ir=(char *)&mem[here];
			}
			here+=x;	
#endif
		}
}

// goto the first line of a program
void firstline() {
	if (top == 0) {
		x=0;
		return;
	}
	here=0;
	gettoken();
}

// goto the next line
void nextline() {
	while (here < top) {
		gettoken();
		if (token == LINENUMBER)
			return;
		if (here >= top) {
			here=top;
			x=0;
			return;
		}
	}
}

// find a line
void findline(address_t l) {
	here=0;
	while (here < top) {
		gettoken();
		if (token == LINENUMBER && x == l ) return;
	}
	error(ELINE);
}

// finds the line of a location
address_t myline(address_t h) {
	address_t l=0; 
	address_t l1=0;
	address_t here2;

	here2=here;
	here=0;
	gettoken();
	while (here < top) {
		if (token == LINENUMBER) {
			l1=l;
			l=x;
		}
		if (here >= h) { break; }
		gettoken();
	}
	here=here2;
	if (token == LINENUMBER)
		return l1;
	else 
		return l;
}

/*

   	Move a block of storage beginng at b ending at e
  	to destination d. No error handling here!!

*/

void moveblock(address_t b, address_t l, address_t d){
	address_t i;

	//outsc("** Moving block: "); outnumber(b); outspc(); outnumber(l); outspc(); outnumber(d); outcr();
	if (d+l > himem) {
		error(EOUTOFMEMORY);
		return;
	}
	
	if (l<1) 
		return;

	if (b < d)
		for (i=l; i>0; i--)
			mem[d+i-1]=mem[b+i-1]; 
	else 
		for (i=0; i<l; i++) 
			mem[d+i]=mem[b+i]; 

	//outsc("** Done moving /n");
}

void zeroblock(address_t b, address_t l){
	address_t i;

	if (b+l > himem) {
		error(EOUTOFMEMORY);
		return;
	}
	if (l<1) 
		return;
	for (i=0; i<l+1; i++) mem[b+i]=0;
}

/*

	Line editor: 

	stage 1: no matter what the line number is - store at the top
   				remember the location in here.
	stage 2: see if it is only an empty line - try to delete this line
	stage 3: caculate lengthes and free memory and make room at the 
				appropriate place
	stage 4: copy to the right place 

	Very fragile code. 

	zeroblock statements commented out after EOL code was fixed
	
*/

#ifdef DEBUG
void diag(){
	outsc("top, here, y and x\n");
	outnumber(top); outspc();
	outnumber(here); outspc();
	outnumber(y); outspc();	
	outnumber(x); outspc();		
	outcr();
}
#endif

void storeline() {
	short linelength;
	number_t newline; 
	address_t here2, here3; 

	// zero is an illegal line number
	if (x == 0) {
		error(ELINE);
		return;
	}

/*
	stage 1: append the line at the end of the memory,
	remember the line number on the stack and the old top in here

*/
    push(x);			
    here=top;		
    newline=here;	 
	token=LINENUMBER;
	do {
		storetoken();
		if (er != 0 ) {
			top=newline;
			here=0;
			return;
		}
		nexttoken();
	} while (token != EOL);

	x=pop();				// recall the line number
	linelength=top-here;	// calculate the number of stored bytes

/* 
	stage 2: check if only a linenumber stored - then delete this line
	
*/

	if (linelength == (numsize+1)) {  		
		top-=(numsize+1);
		y=x;					
		findline(y);
		if (er != 0) return;	
		y=here-(numsize+1);							
		nextline();			
		here-=(numsize+1);
		if (x != 0) {						
			moveblock(here, top-here, y);	
			top=top-(here-y);
		} else {
			top=y;
		}
		return;
	}

/* 
	stage 3, a nontrivial line with linenumber x is to be stored
	try to find it first by walking through all lines 
*/
	else {	
		y=x;
		here2=here;
		here=numsize+1;
		nextline();
		if (x == 0) { // there is no nextline after the first line, we are done
			return;
		}
		here=0;       // go back to the beginning
		here2=0;
		while (here < top) {
			here3=here2;
			here2=here;
			nextline();
			if (x > y) break;
		}

		// at this point y contains the number of the line to be inserted
		// x contains the number of the first line with a higher line number
		// or 0 if the line is to be inserted at the end
		// here points to the following line and here2 points to the previous line

		if (x == 0) { 
			here=here3-(numsize+1);
			gettoken();
			if (token == LINENUMBER && x == y) { // we have a double line at the end
				here2-=(numsize+1);
				here-=(numsize+1);
				moveblock(here2, linelength, here);
				top=here+linelength;
			}
			return;
		}
		here-=(numsize+1);
		push(here);
		here=here2-(numsize+1);
		push(here);
		gettoken();
		if (x == y) {     // the line already exists and has to be replaced
			here2=pop();  // this is the line we are dealing with
			here=pop();   // this is the next line
			y=here-here2; // the length of the line as it is 
			if (linelength == y) {     // no change in line legth
				moveblock(top-linelength, linelength, here2);
				top=top-linelength;
			} else if (linelength > y) { // the new line is longer than the old one
				moveblock(here, top-here, here+linelength-y);
				here=here+linelength-y;
				top=top+linelength-y;
				moveblock(top-linelength, linelength, here2);
				top=top-linelength;
			} else {					// the new line is short than the old one
				moveblock(top-linelength, linelength, here2);
				top=top-linelength;
				moveblock(here, top-here, here2+linelength);
				top=top-y+linelength;
			}
		} else {         // the line has to be inserted in between
			drop();
			here=pop();
			moveblock(here, top-here, here+linelength);
			moveblock(top, linelength, here);
		}
	}
}



/* 
 
 calculates an expression, with a recursive descent algorithm
 using the functions term, factor and expression
 all function use the stack to pass values back. We use the 
 Backus-Naur form of basic from here https://rosettacode.org/wiki/BNF_Grammar
 implementing a C style logical expression model

*/

char termsymbol() {
	return ( token == LINENUMBER ||  token == ':' || token == EOL);
}

// parses a list of expression
short parsearguments() {
	char args=0;
	if (termsymbol()) return args;

nextexpression:
	expression();
	if (er != 0) { clearst(); return 0; }

	args++;

	if (token == ',') {
		nexttoken();
		goto nextexpression;
	} else 
		return args;
}

void parsenarguments(char n) {
	char args=0;
	nexttoken();

	args=parsearguments();
	if (er != 0 ) return;

	if (args == n) {
		return;
	}
	error(EARGS);
}


// parse a function argument ae is the number of 
// expected expressions in the argument list
void parsefunction(void (*f)(), short ae){
	char args;

	nexttoken();
	args=parsesubscripts();
	if (er != 0) return;

	if (args == ae) {
		f();
	} else {
		error(EARGS);
		return;
	}
}

// helper function in the recursive decent parser
void parseoperator(void (*f)()) {
	nexttoken();
	f();
	if (er !=0 ) return;
	y=pop();
	x=pop();
}

// counts the number of arguments given in brakets
short parsesubscripts() {
	char args = 0;

	if (token != '(') {return 0; }
	nexttoken();
	args=parsearguments();
	if (er != 0) {return 0; }
	if (token != ')') {error(EARGS); return 0; }
	return args;
}

// substring evaluation, mind the rewinding here - a bit of a hack
void parsesubstring() {
	char xc1, yc1; 
	short args;
	address_t h1; // remember the here
	char * bi1;

	// remember the string name
    xc1=xc;
    yc1=yc;

    if (st == SINT) // this is a hack - we rewind a token !
    	bi1=bi;
    else 
    	h1=here; 
    nexttoken();
    args=parsesubscripts();

    if (er != 0) {return; }
    switch(args) {
    	case 2: 
    		break;
		case 1:
			push(lenstring(xc1, yc1));
			break;
		case 0: 
			if ( st == SINT) // this is a hack - we rewind a token !
				bi=bi1;
			else 
				here=h1; 
			push(1);
			push(lenstring(xc1, yc1));	
			break;
    }
}

// absolute value, 
number_t babs(number_t n) {
	if (n<0) return -n; else return n;
}

void xabs(){
	push(babs(pop()));
}

// sign
void xsgn(){
	number_t n;
	n=pop();
	if (n>0) n=1;
	if (n<0) n=-1;
	push(n);
}

// on an arduino, negative values of peek address 
// the EEPROM range -1 .. -1024 on an UNO
void peek(){
	address_t amax;
	number_t a;
	a=pop();

	// this is a hack again, 16 bit numbers can't peek big addresses
	if (MEMSIZE > 32767) amax=32767; else amax=MEMSIZE;

	if (a >= 0 && a<amax) 
		push(mem[a]);
	else if (a < 0 && -a < elength())
		push(eread(-a-1));
	else {
		error(ERANGE);
		return;
	}
}

// the fre function 
void xfre() {
	if (pop() >=0 )
		push(himem-top);
	else 
		push(elength());
}


// very basic random number generator with constant seed.
void rnd() {
	number_t r;
	r=pop();
	rd = (31421*rd + 6927) % 0x10000;
	if (r>=0) 
		push((long)rd*r/0x10000);
	else 
		push((long)rd*r/0x10000+1);
}

// a very simple approximate square root formula. 
void sqr(){
	number_t t,r;
	number_t l=0;
	r=pop();
	t=r;
	while (t > 0) {
		t>>=1;
		l++;
	}
	l=l/2;
	t=1;
	t<<=l;
	do {
		l=t;
		t=(t+r/t)/2;
	} while (babs(t-l)>1);
	push(t);
}

// evaluates a string value, FALSE if there is no string
char stringvalue() {
	char xcl;
	char ycl;
	if (token == STRING) {
		ir2=ir;
		push(x);
	} else if (token == STRINGVAR) {
		xcl=xc;
		ycl=yc;
		parsesubstring();
		if (er != 0) return FALSE;
		y=pop();
		x=pop();
		ir2=getstring(xcl, ycl, x);
		push(y-x+1);
		xc=xcl;
		yc=ycl;
	} else {
		return FALSE;
	}
	return TRUE;
}


// (numerical) evaluation of a string expression used for 
// comparison and for string rightvalues as numbers
void streval(){
	char *irl;
	number_t xl;
	char xcl;
	signed char t;
	address_t h1;

	if ( ! stringvalue()) {
		error(EUNKNOWN);
		return;
	} 
	if (er != 0) return;
	irl=ir2;
	xl=pop();

	h1=here;
	nexttoken();
	if (token != '=' && token != NOTEQUAL) {
		here=h1;
		push(irl[1]);
		return; 
	}

	t=token;
	nexttoken();

	if (! stringvalue() ){
		error(EUNKNOWN);
		return;
	} 
	x=pop();
	if (er != 0) return;

	if (x != xl) goto neq;
	for (x=0; x < xl; x++) if ( irl[x] != ir2[x] ) goto neq;

	if (t == '=') push(1); else push(0);
	return;
neq:
	if (t == '=') push(0); else push(1);
	return;
}


void factor(){
	short args;
	if (DEBUG) debug("factor\n");
	switch (token) {
		case NUMBER: 
			push(x);
			break;
		case VARIABLE: 
			push(getvar(xc, yc));
			break;
		case ARRAYVAR:
			push(yc);
			push(xc);
			nexttoken();
			args=parsesubscripts();
			if (er != 0 ) return;
			if (args != 1) { error(EARGS); return; }	
			x=pop();
			xc=pop();
			yc=pop();
			array('g', xc, yc, x, &y);
			push(y); 
			break;
		case '(':
			nexttoken();
			expression();
			if (er != 0 ) return;
			if (token != ')') { error(EARGS); return; }
			break;

// Palo Alto BASIC functions
		case TABS: 
			parsefunction(xabs, 1);
			break;
		case TRND: 
			parsefunction(rnd, 1);
			break;
		case TSIZE:
			push(himem-top);
			break;
// Apple 1 BASIC functions
		case TSGN: 
			parsefunction(xsgn, 1);
			break;
		case TPEEK: 
			parsefunction(peek, 1);
			break;
		case TLEN: 
			nexttoken();
			if ( token != '(') {
				error(EARGS);
				return;
			}
			nexttoken();
			if (! stringvalue()) {
				error(EUNKNOWN);
				return;
			}
			if (er != 0) return;
			nexttoken();
			if (token != ')') {
				error(EARGS);
				return;	
			}
			break;
		case TLOMEM:
			push(0);
			break;
		case THIMEM:
			push(himem);
			break;
// Apple 1 string compare code
		case STRING:
		case STRINGVAR:
			streval();
			//debugtoken(); outsc("after streval in factor"); outcr();
			if (er != 0 ) return;
			break;
//  Stefan's tinybasic additions
		case TSQR: 
			parsefunction(sqr, 1);
			break;
		case TFRE: 
			parsefunction(xfre, 1);
			break;
// Arduino I/O
		case TAREAD: 
			parsefunction(aread, 1);
			break;
		case TDREAD: 
			parsefunction(dread, 1);
			break;
		case TMILLIS: 
			parsefunction(bmillis, 1);
			break;	
		case TPULSEIN:
			parsefunction(bpulsein, 3);
			break;
		case TAZERO:
#ifdef ARDUINO
			push(A0);
#else 
			push(0);
#endif			
			break;
		case TUSR:
			parsefunction(xusr, 2);
			break;
// unknown function
		default:
			error(EUNKNOWN);
			return;
	}
}

void term(){
	if (DEBUG) debug("term\n"); 
	factor();
	if (er != 0) return;

nextfactor:
	nexttoken();
	if (DEBUG) debug("in term\n");
	if (token == '*'){
		parseoperator(factor);
		if (er != 0) return;
		push(x*y);
		goto nextfactor;
	} else if (token == '/'){
		parseoperator(factor);
		if (er != 0) return;
		if (y != 0)
			push(x/y);
		else {
			error(EDIVIDE);
			return;	
		}
		goto nextfactor;
	} else if (token == '%'){
		parseoperator(factor);
		if (er != 0) return;
		if (y != 0)
			push(x%y);
		else {
			error(EDIVIDE);
			return;	
		}
		goto nextfactor;
	} 
}

void addexpression(){
	if (DEBUG) debug("addexp\n");
	if (token != '+' && token != '-') {
		term();
		if (er != 0) return;
	} else {
		push(0);
	}

nextterm:
	if (token == '+' ) { 
		parseoperator(term);
		if (er != 0) return;
		push(x+y);
		goto nextterm;
	} else if (token == '-'){
		parseoperator(term);
		if (er != 0) return;
		push(x-y);
		goto nextterm;
	}
}

void compexpression() {
	if (DEBUG) debug("compexp\n"); 
	addexpression();
	if (er != 0) return;
	switch (token){
		case '=':
			parseoperator(compexpression);
			if (er != 0) return;
			push(x == y);
			break;
		case NOTEQUAL:
			parseoperator(compexpression);
			if (er != 0) return;
			push(x != y);
			break;
		case '>':
			parseoperator(compexpression);
			if (er != 0) return;
			push(x > y);
			break;
		case '<':
			parseoperator(compexpression);
			if (er != 0) return;
			push(x < y);
			break;
		case LESSEREQUAL:
			parseoperator(compexpression);
			if (er != 0) return;
			push(x <= y);
			break;
		case GREATEREQUAL:
			parseoperator(compexpression);
			if (er != 0) return;
			push(x >= y);
			break;
	}
}

void notexpression() {
	if (DEBUG) debug("notexp\n");
	if (token == TNOT) {
		nexttoken();
		compexpression();
		if (er != 0) return;
		x=pop();
		push(!x);
	} else 
		compexpression();
}

void andexpression() {
	if (DEBUG) debug("andexp\n");
	notexpression();
	if (er != 0) return;
	if (token == TAND) {
		parseoperator(expression);
		if (er != 0) return;
		push(x && y);
	} 
}

void expression(){
	if (DEBUG) debug("exp\n"); 
	andexpression();
	if (er != 0) return;
	if (token == TOR) {
		parseoperator(expression);
		if (er != 0) return;
		push(x || y);
	}  
}

/* 
	The commands and their helpers
    
    Palo Alto BASIC languge set - print, let, input, goto, gosub, return,
    	if, for, to, next, step, (break), stop (end), list, new, run, rem
    	break is not Palo ALto but fits here, end is identical to stop.

*/

/*
   print 
*/ 

void xprint(){
	char semicolon = FALSE;
	char oldod;
	char modifier;

	form=0;
	oldod=od;

	nexttoken();

processsymbol:

	if (termsymbol()) {
		if (! semicolon) outcr();
		nexttoken();
		od=oldod;
		return;
	}
	semicolon=FALSE;

	if (stringvalue()) {
		if (er != 0) return;
 		outs(ir2, pop());
 		nexttoken();
		goto separators;
	}

	// modifiers of the print statement
	if (token == '#' || token == '&') {
		modifier=token;
		nexttoken();
		expression();
		if (er != 0) return;
		switch(modifier) {
			case '#':
				form=pop();
				break;
			case '&':
				od=pop();
				break;
		}
		modifier=0;
		goto processsymbol;
	}

	if (token != ',' && token != ';' ) {
		expression();
		if (er != 0) return;
		outnumber(pop());
	}

separators:
	if (token == ',') {
		outspc();
		nexttoken();	
	}
	if (token == ';'){
		semicolon=TRUE;
		nexttoken();
	}

	goto processsymbol;
}

/* 
	
	assigment code for various lefthand and righthand side. 

*/

void assignment() {
	signed char t=token;  // remember the left hand side token until the end of the statement, type of the lhs
	char ps=TRUE;  // also remember if the left hand side is a pure string of something with an index 
	char xcl, ycl; // to preserve the left hand side variable names
	short i=1;      // and the beginning of the destination string  
	short lensource;
	short args;

	// this code evaluates the left hand side
	ycl=yc;
	xcl=xc;
	switch (token) {
		case VARIABLE:
			nexttoken();
			break;
		case ARRAYVAR:
			nexttoken();
			args=parsesubscripts();
			nexttoken();
			if (er != 0) return;
			if (args != 1) {
				error(EARGS);
				return;
			}
			i=pop();
			break;
		case STRINGVAR:
			nexttoken();
			args=parsesubscripts();
			if (er != 0) return;
			switch(args) {
				case 0:
					i=1;
					ps=TRUE;
					break;
				case 1:
					ps=FALSE;
					nexttoken();
					i=pop();
					break;
				default:
					error(EARGS);
					return;
			}
			break;
		default:
			error(EUNKNOWN);
			return;
	}

	
	// the assignment part
	if (token != '=') {
		error(EUNKNOWN);
		return;
	}
	nexttoken();

	// here comes the code for the right hand side
	// rewritten 

	// stringvalue is a nasty function with many side effects
	// in ir2 and pop() we have the the adress and length of the source string, 
	// xc is the name, y contains the end and x the beginning index 

	if (! stringvalue () ) {
		if (er != 0 ) return;

		expression();
		if (er != 0 ) return;

		switch (t) {
			case VARIABLE:
				x=pop();
				setvar(xcl, ycl , x);
				break;
			case ARRAYVAR: 
				x=pop();	
				array('s', xcl, ycl, i, &x);
				break;
			case STRINGVAR:
				ir=getstring(xcl, ycl, i);
				if (er != 0) return;
				ir[0]=pop();
				if (ps)
					setstringlength(xcl, ycl, 1);
				else 
					if (lenstring(xcl, ycl) < i && i < stringdim(xcl, ycl)) setstringlength(xcl, ycl, i);
				break;
		}		
	} else {

		switch (t) {
			case VARIABLE: // a scalar variable gets assigned the first string character 
				setvar(xcl, ycl , ir2[0]);
				drop();
				break;
			case ARRAYVAR: 	
				x=ir2[0];
				array('s', xcl, ycl, i, &x);
				drop();
				break;
			case STRINGVAR: // a string gets assigned a substring - copy algorithm
				lensource=pop();

				// the destination adress
				ir=getstring(xcl, ycl, i);
				if (er != 0) return;

				if ((lenstring(xcl, ycl)+lensource-1) > stringdim(xcl, ycl)) { error(ERANGE); return; }

				// this code is needed to make sure we can copy one string to the same string 
				// without overwriting stuff, we go either left to right or backwards

				if (x > i) 
					for (int j=0; j<lensource; j++) { ir[j]=ir2[j];}
				else
					for (int j=lensource-1; j>=0; j--) ir[j]=ir2[j]; 
				setstringlength(xcl, ycl, i+lensource-1);
		}

		nexttoken();
	} 

}

/*
	input ["string",] variable [,["string",] variable]* 

*/

void xinput(){
	short args;
	short oldid = -1;

	nexttoken();

	// modifiers of the input statement
	if (token == '&') {
		nexttoken();
		expression();
		if (er != 0) return;
		oldid=id;
		id=pop();
		if ( token != ',') {
			error(EUNKNOWN);
			return;
		} else 
			nexttoken();
	}


nextstring:
	if (token == STRING && id != IFILE) {   
		outs(ir, x);
		nexttoken();
		if (token != ',' && token != ';') {
			error(EUNKNOWN);
			return;
		} else 
			nexttoken();
	}

nextvariable:
	if (token == VARIABLE) {   
		if (id != IFILE) outsc("? ");
		if (innumber(&x) == BREAKCHAR) {
			setvar(xc, yc, 0);
			st=SINT;
			nexttoken();
			id=oldid;
			return;
		} else {
			setvar(xc, yc, x);
		}
	} 

	if (token == ARRAYVAR) {

		nexttoken();
		args=parsesubscripts();
		if (er != 0 ) return;
		if (args != 1) {
			error(EARGS);
			return;
		}

		if (id != IFILE) outsc("? ");
		if (innumber(&x) == BREAKCHAR) {
			x=0;
			array('s', xc, yc, pop(), &x);
			st=SINT;
			nexttoken();
			id=oldid;
			return;
		} else {
			array('s', xc, yc, pop(), &x);
		}
	}

	if (token == STRINGVAR) {
		ir=getstring(xc, yc, 1); 
		if (id != IFILE) outsc("? ");
		ins(ir-1, stringdim(xc, yc));
 	}

	nexttoken();
	if (token == ',' || token == ';') {
		nexttoken();
		goto nextstring;
	}

	if (oldid != -1) id=oldid;

}

/*

	goto, gosub, return and its helpers

*/

#ifdef HASGOSUB
void pushgosubstack(){
	if (gosubsp < GOSUBDEPTH) {
		gosubstack[gosubsp]=here;
		gosubsp++;	
	} else 
		error(EGOSUB);
}

void popgosubstack(){
	if (gosubsp>0) {
		gosubsp--;
	} else {
		error(ERETURN);
		return;
	} 
	here=gosubstack[gosubsp];
}

void dropgosubstack(){
	if (gosubsp>0) {
		gosubsp--;
	} else {
		error(EGOSUB);
	} 
}

void clrgosubstack() {
	gosubsp=0;
}
#endif

void xgoto() {
	short t=token;

	nexttoken();
	expression();
	if (er != 0) return;

#ifdef HASGOSUB
	if (t == TGOSUB) pushgosubstack();
	if (er != 0) return;
#endif

	x=pop();
	findline(x);
	if ( er != 0 ) return;
	if (st == SINT) st=SRUN;
	nexttoken();
}

void xreturn(){ 
#ifdef HASGOSUB
	popgosubstack();
	if (er != 0) return;
#endif
	nexttoken();
}


/* 

	if and then

*/


void xif() {
	
	nexttoken();
	expression();
	if (er != 0 ) return;

	x=pop();
	if (DEBUG) { outnumber(x); outcr(); } 
	if (! x) // on condition false skip the entire line
		do nexttoken();	while (token != LINENUMBER && token !=EOL && here <= top);
		
	if (token == TTHEN) {
		nexttoken();
		if (token == NUMBER) {
			findline(x);
			if (er != 0) return;		
		}
	} 
}

/* 

	for, next and the apocryphal break

*/ 

#ifdef HASFORNEXT

// find the NEXT token or the end of the program
void findnext(){
	while (TRUE) {
	    if (token == TNEXT) {
	    	if (fnc == 0) return;
	    	else fnc--;
	    }
	    if (token == TFOR) fnc++;
	    if (here >= top) {
	    	error(TFOR);
	    	return;
	    }
		nexttoken(); 
	}
}


/*
	for variable = expression to expression [STEP expression]
	for stores the variable, the increment and the boudary on the 
	for stack. Changing steps and boundaries during the execution 
	of a loop has no effect 
*/

void xfor(){
	
	nexttoken();
	if (token != VARIABLE) {
		error(EUNKNOWN);
		return;
	}
	push(yc);
	push(xc);
	nexttoken();
	if (token != '=') { 
		error(EUNKNOWN); 
		return; 
	}
	nexttoken();
	expression();
	if (er != 0) return;
	x=pop();	
	xc=pop();
	yc=pop();
	setvar(xc, yc, x);
	if (DEBUG) { outch(xc); outspc(); outnumber(x); outcr(); }
	push(xc);
	if (token != TTO){
		error(EUNKNOWN);
		return;
	}
	nexttoken();
	expression();
	if (er != 0) return;

	if (token == TSTEP) {
		nexttoken();
		expression();
		if (er != 0) return;
		y=pop();
	} else 
		y=1;
	if (DEBUG) { debugtoken(); outnumber(y); outcr(); }
	if (! termsymbol()) {
		error(UNKNOWN);
		return;
	}
	x=pop();
	xc=pop();
	if (st == SINT)
		here=bi-ibuffer;
	pushforstack();
	if (er != 0) return;

/*
	this tests the condition and stops if it is fulfilled already from start 
	there is an apocryphal feature here: STEP 0 is legal triggers an infinite loop
*/
	if ( (y > 0 && getvar(xc, yc)>x) || (y < 0 && getvar(xc, yc)<x ) ) { 
		dropforstack();
		findnext();
		nexttoken();
		return;
	}
}

/*
	an apocryphal feature here is the BREAK command ending a look
	doesn't work well for nested loops - to be tested carefully
*/
void xbreak(){
	dropforstack();
	findnext();
	nexttoken();
}

void xnext(){
	char xcl=0;
	char ycl;
	number_t t;

	nexttoken();
	if (termsymbol()) goto plainnext;
	if (token == VARIABLE) {
		xcl=xc;
		ycl=yc;
		nexttoken();
		if (! termsymbol()) {
			error(EUNKNOWN);
			return;
		}
	}

plainnext:
	push(here);
	popforstack();
	if (xcl) {
		if (xcl != xc || ycl != yc ) {
			error(EUNKNOWN);
			return;
		} 
	}
	if (y == 0) goto backtofor;
	t=getvar(xc, yc)+y;
	setvar(xc, yc, t);
	if (y > 0 && t <= x) goto backtofor;
	if (y < 0 && t >= x) goto backtofor;

	// last iteration completed
	here=pop();
	nexttoken();
	return;

	// next iteration
backtofor:
	pushforstack();
	if (st == SINT)
		bi=ibuffer+here;
	drop();
	nexttoken();
	return;

}

#else
void xfor(){
	nexttoken();
}
void xbreak(){
	nexttoken();
}
void xnext(){
	nexttoken();
}
#endif

/* 
	
	list - this is also used in save

*/

void outputtoken() {

	switch (token) {
		case NUMBER:
			outnumber(x);
			return;
		case LINENUMBER: 
			outnumber(x);
			outspc();
			return;
		case ARRAYVAR:
		case STRINGVAR:
		case VARIABLE:
			outch(xc); 
			if (yc != 0) outch(yc);
			if (token == STRINGVAR) outch('$');
			return;
		case STRING:
			outch('"'); 
			outs(ir, x); 
			outch('"');
			return;
		default:
			if (token < -3) {
				if (token == TTHEN || token == TTO || token == TSTEP) outspc();
				outsc(getkeyword(token)); 
				if (token != GREATEREQUAL && token != NOTEQUAL && token != LESSEREQUAL) outspc();
				return;
			}	
			if (token >= 32) {
				outch(token);
				return;
			} 
			outch(token); outspc(); outnumber(token);
	}

}


void xlist(){
	number_t b, e;
	char oflag = FALSE;

	nexttoken();
 	y=parsearguments();
	if (er != 0) return;
	switch (y) {
		case 0: 
			b=0;
			e=32767;
			break;
		case 1: 
			b=pop();
			e=b;
			break;
		case 2: 
			e=pop();
			b=pop();
			break;
		default:
			error(EARGS);
			return;
	}

	if ( top == 0 ) { nexttoken(); return; }

	here=0;
	gettoken();
	while (here < top) {
		if (token == LINENUMBER && x >= b) oflag=TRUE;
		if (token == LINENUMBER && x >  e) oflag=FALSE;
		if (oflag) outputtoken();
		gettoken();
		if (token == LINENUMBER && oflag) {
			outcr();
			if ( lcdactive() ) { if ( inch() == 27 ) break;}
		}
	}
	if (here == top && oflag) outputtoken();
    if (e == 32767 || b != e) outcr(); // supress newlines in "list 50" - a little hack

	nexttoken();
 }


void xrun(){
	if (token == TCONT) {
		st=SRUN;
		nexttoken();
		goto statementloop;
	} 
	nexttoken();
	y=parsearguments();
	if (er != 0 ) return;
	if (y > 1) { error(EARGS); return; }
	if (y == 0) 
		here=0;
	else
		findline(pop());
	if ( er != 0 ) return;
	if (st == SINT) st=SRUN;

	xclr();

statementloop:
	while ( (here < top) && (st == SRUN || st == SERUN) && ! er) {	
		statement();
	}
	st=SINT;
}



void xnew(){ // the general cleanup function
	clearst();
	himem=MEMSIZE-1;
	top=0;
	zeroblock(top,himem);
	reseterror();
	st=SINT;
	nvars=0;

#ifdef HASGOSUB
	clrgosubstack();
#endif
#ifdef HASFORNEXT
	clrforstack();
#endif
}


void xrem() {
	while (token != LINENUMBER && token != EOL && here <= top) nexttoken(); 
}

/* 

	The Apple 1 BASIC additions

*/


/* 

	clearing variable space

*/

void xclr() {
	clrvars();
#ifdef HASGOSUB
	clrgosubstack();
#endif
#ifdef HASFORNEXT
	clrforstack();
#endif
	nexttoken();
}

/* 

	the dimensioning of arrays and strings from Apple 1 BASIC

*/

void xdim(){
	char args, xcl, ycl; 
	signed char t;

	nexttoken();

nextvariable:
	if (token == ARRAYVAR || token == STRINGVAR ){
		
		t=token;
		xcl=xc;
		ycl=yc;

		nexttoken();

		args=parsesubscripts();
		if (er != 0) return;
		if (args != 1) {error(EARGS); return; }
		x=pop();
		if (x<=0) {error(ERANGE); return; }
		if (t == STRINGVAR) {
			createstring(xcl, ycl, x);
		} else {
			createarry(xcl, ycl, x);
		}	
	} else {
		error(EUNKNOWN);
		return;
	}
	nexttoken();

	if (token == ',') {	
		nexttoken();
		goto nextvariable;
	}
	nexttoken();
}


/* 
	
	low level poke to the basic memory, works only up to 32767

*/

void xpoke(){
	address_t amax;
	number_t a;

	// like in peek
	if (MEMSIZE > 32767) amax=32767; else amax=MEMSIZE;
	parsenarguments(2);
	if (er != 0) return;
	y=pop();
	a=pop();
	if (a >= 0 && a<amax) 
		mem[a]=y;
	else if (a < 0 && a >= -elength())
			eupdate(-a-1, y);
	else {
		error(ERANGE);
	}
}

/*

	the TAB spaces command of Apple 1 BASIC

*/

void xtab(){
	parsenarguments(1);
	if (er != 0) return;
	x=pop();
	while (x-- > 0) outspc();	
}

/*

	Stefan's additions 

*/

#ifdef HASDUMP
void xdump() {
	address_t a;
	
	nexttoken();
	y=parsearguments();
	if (er != 0) return;
	switch (y) {
		case 0: 
			x=0;
			a=MEMSIZE-1;
			break;
		case 1: 
			x=pop();
			a=MEMSIZE-1;
			break;
		case 2: 
			a=pop();
			x=pop();
			break;
		default:
			error(EARGS);
			return;
	}

	form=6;
	dumpmem(a/8+1, x);
	form=0;
	nexttoken();
}

void dumpmem(address_t r, address_t b) {
	address_t j, i;	
	address_t k;

	k=b;
	i=r;
	while (i>0) {
		outnumber(k); outspc();
		for (j=0; j<8; j++) {
			outnumber(mem[k++]); outspc();
			delay(1); // slow down a little here for low serial baudrates
			if (k > MEMSIZE-1) break;
		}
		outcr();
		i--;
		if (k > MEMSIZE-1) break;
	}
#ifdef ARDUINOEEPROM
	printmessage(EEEPROM); outcr();
	i=r;
	k=0;
	while (i>0) {
		outnumber(k); outspc();
		for (j=0; j<8; j++) {
			outnumber(eread(k++)); outspc();
			if (k > elength()) break;
		}
		outcr();
		i--;
		if (k > elength()) break;	
	}
#endif
	outsc("top: "); outnumber(top); outcr();
	outsc("himem: "); outnumber(himem); outcr();
}
#endif

char * getfilename() {
	nexttoken();
	if (token == STRING && x > 0 && x < SBUFSIZE) {
		sbuffer[x--]=0;
		while (x >= 0) { sbuffer[x]=ir[x]; x--; }
		return sbuffer;
	} else if (termsymbol()) {
		return getmessage(MFILE);
	} else {
		error(EUNKNOWN);
		return NULL;
	}
}


// save a file either to disk or to EEPROM
void xsave() {
	char * filename;
	address_t here2;

	filename=getfilename();
	if (er != 0 || filename == NULL) return;

	// save the output mode
	push(od);

	if (filename[0] == '!') {
		esave();
	} else {
		od=OFILE;


#ifndef ARDUINO
		ofd=fopen(filename, "w");
		if (!ofd) {
			error(EFILE);
			nexttoken();
			return;
		} 
		
		// the core list function
		// we step away from list 
		here2=here;
		here=0;
		gettoken();
		while (here < top) {
			outputtoken();
			gettoken();
			if (token == LINENUMBER) outcr();
		}
		if (here == top) outputtoken();
   		outcr(); 
   		here=here2;

   		// clean up
		fclose(ofd);
		ofd=0;

#else 
#ifdef ARDUINOSD
		ofile=SD.open(filename, FILE_WRITE);
		if (!ofile) {
			error(EFILE);
			nexttoken();
			return;
		} 

		// the core list function
		// we step away from list 
		here2=here;
		here=0;
		gettoken();
		while (here < top) {
			outputtoken();
			gettoken();
			if (token == LINENUMBER) outcr();
		}
		if (here == top) outputtoken();
   		outcr(); 
   		here=here2;

   		// clean up
		ofile.close();
#else
      
#endif
#endif

	}
	// restore the output mode
	od=pop();

	// and continue
	nexttoken();
	return;
}

void xload() {
	char * filename;
	char ch;
	address_t here2;
	char chain = FALSE;

	filename=getfilename();
	if (er != 0 || filename == NULL) return; 

	if (filename[0] == '!') {
		eload();
		nexttoken();
	} else {

		// if load is called during runtime it chains
		// load the program as new but perserve the variables
		// gosub and for stacks are cleared 
		if ( st == SRUN ) { 
			chain=TRUE; 
			st=SINT; 
			top=0;
#ifdef HASGOSUB
			clrgosubstack();
#endif
#ifdef HASFORNEXT
			clrforstack();
#endif
		}

#ifndef ARDUINO
		ifd=fopen(filename, "r");
		if (!ifd) {
			error(EFILE);
			nexttoken();
			return;
		}
		while (fgets(ibuffer+1, BUFSIZE, ifd)) {
			bi=ibuffer+1;
			while(*bi != 0) { if (*bi == '\n' || *bi == '\r') *bi=' '; bi++; };
				bi=ibuffer+1;
				nexttoken();
				if (token == NUMBER) storeline();
				if (er != 0 ) break;
		}
		fclose(ifd);	
		ifd=0;
#else 
#ifdef ARDUINOSD
		ifile=SD.open(filename, FILE_READ);
		if (!ifile) {
			error(EFILE);
			nexttoken();
			return;
		} 

    	bi=ibuffer+1;
		while (ifile.available()) {
      		ch=ifile.read();
      		//Serial.print(ch);
      		if (ch == '\n' || ch == '\r') {
        	//Serial.println("<NEWLINE>");
        		*bi=0;
        		bi=ibuffer+1;
        		nexttoken();
        		//Serial.println("After nexttoken :");
        		//Serial.println((int) token);
        		//Serial.println(x);
        		if (token == NUMBER) storeline();
        		if (er != 0 ) break;
        		bi=ibuffer+1;
      		} else {
        		*bi++=ch;
      		}
      		if ( (bi-ibuffer) > BUFSIZE ) {
        		error(EOUTOFMEMORY);
        		break;
      		}
		}   	
		ifile.close();
#else 

#endif
#endif
		// go back to run mode and start from the first line
		if (chain) {
			st=SRUN;
			here=0;
		}
		nexttoken();
	}
}

/*
	get just one character from input or send one 
*/

void xget(){
	nexttoken();
	if (token == VARIABLE) {
		if (checkch()) setvar(xc, yc, inch()); else setvar(xc, yc, 0);
	} else if (token == STRINGVAR) {
		// need to implement string left value
		error(EUNKNOWN);
	} else {
		error(EUNKNOWN);
	}
	nexttoken();
}

/*
	PUT writes one character to the default output stream
*/

void xput(){
	parsenarguments(1);
	if (er != 0) return;
	x=pop();
	outch(x);
}

/* 
	the set command itself is also apocryphal it is a low level
	control command setting certain properties
	syntax, currently it is only 

	set expression
*/

void xset(){
	parsenarguments(2);
	if (er != 0) return;
	x=pop();
	y=pop();
	switch (y) {		
		case 1: // autorun/run flag of the EEPROM 255 for clear, 0 for prog, 1 for autorun
			eupdate(0, x);
			break;
		case 2: // serial = 0, LCD = 1 
			switch (x) {
				case 0:
					od=OSERIAL;
					break;
				case 1:
					od=OLCD;
					break;
			}		
			break;
		case 4: // serial = 0, keyboard = 1 
			switch (x) {
				case 0:
					id=ISERIAL;
					break;
				case 1:
					id=IKEYBOARD;
					break;
			}		
			break;			
	}
}


/*

	The arduino io functions.

*/

void xdwrite(){
	parsenarguments(2);
	if (er != 0) return; 
	x=pop();
	y=pop();
	dwrite(y, x);	
}

void xawrite(){
	parsenarguments(2);
	if (er != 0) return; 
	x=pop();
	y=pop();
	awrite(y, x);
}

void xpinm(){
	parsenarguments(2);
	if (er != 0) return; 
	x=pop();
	y=pop();
	pinm(y, x);	
}

void xdelay(){
	parsenarguments(1);
	if (er != 0) return;
	x=pop();
	delay(x);	
}

void xtone(){
	short args;

	nexttoken();
	args=parsearguments();
	if (er != 0) return;
	if (args>3 || args<2) {
		error(EARGS);
		return;
	}

#if !defined(ARDUINO) || defined(ARDUINODUE)
	clearst();
	return;
#else 
	x=pop();
	y=pop();
	if (args=2) {
		tone(y,x);
	} else {
		z.i=pop();
		tone(z.i, y, x);
	}
#endif		
}

/*

  	SD card DOS - basics to access an SD card as mass storage from BASIC

*/

void xcatalog() {
#ifdef ARDUINOSD
	File root, file;
	char c = 0;

	if ( od == OLCD ) { lcdwrite(12); }
	root=SD.open("/");
	while (TRUE) {
		file=root.openNextFile();
		if (! file) break;
		if (! file.isDirectory()) { 
		  outsc(file.name()); outch(' '); outnumber(file.size()); outcr();
		  c++;
		  if (lcdactive() && (c == lcd_rows-1)) { if ( inch() == 27 ) break;  c=0; }
	  }
    file.close(); 
	}

	root.close();
#else 
#ifndef ARDUINO 
	DIR *dp;
	struct dirent *ep;     
  	dp = opendir ("./");

  	if (dp != NULL) {
    	while ( (ep = readdir(dp)) ) {
    		if (ep->d_type == DT_REG) {
    		    outsc(ep->d_name); 
      			outcr();	
    		}
    	}
    	(void) closedir (dp);
  	} else
    	ert=1; 
#endif
#endif

	nexttoken();
}

void xdelete() {
	char * filename;
	filename=getfilename();
	if (er != 0 || filename == NULL) return; 

#ifdef ARDUINOSD	
	SD.remove(filename);
#else 
#ifndef ARDUINO
	remove(filename);
#endif
	nexttoken();
#endif
}

void xopen() {
	char * filename;
	short args=0;
	char mode;

	filename=getfilename();
	if (er != 0 || filename == NULL) return; 

	nexttoken();
	if (token == ',') { 
		nexttoken(); 
		args=parsearguments();
	}

	if (args == 0 ) { 
		mode=0; 
	} else if (args == 1) {
		mode=pop();
	} else {
		error(EARGS);
		return;
	}

#ifndef ARDUINO
	if (mode == 1) {
		if (ofd) fclose(ofd);
		if ((ofd=fopen(filename, "w"))) {
			ert=0;
		} else {
			ert=1;
		}
	} else if (mode == 0) {
		if (ifd) fclose(ifd);
		if ((ifd=fopen(filename, "r"))) {
			ert=0;
		} else {
			ert=1;
		}
	}
#else 
#ifdef ARDUINOSD
	if (mode == 1) {
		if (ofile) ofile.close();
		if (ofile=SD.open(filename, FILE_OWRITE)) {
			ert=0;
		} else {
			ert=1;
		}
	} else if (mode == 0) {
		if (ifile) ifile.close();
		if (ifile=SD.open(filename, FILE_READ)) {
			ert=0;
		} else {
			ert=1;
		}
	}
#endif
#endif

	nexttoken();
}

void xclose() {
	char mode;

	parsenarguments(1);
	mode=pop();

#ifndef ARDUINO
	if (mode == 1) {
		if (ofd) fclose(ofd);
	} else if (mode == 0) {
		if (ifd) fclose(ifd);
	}
#else 
#ifdef ARDUINOSD
	if (mode == 1) {
		if (ofile) ofile.close();
	} else if (mode == 0) {
		if (ifile) ifile.close();
	}
#endif
#endif


	nexttoken();
}

// low level function access of the interpreter

void xusr() {
	y=pop();
	x=pop();
	switch(x) {
		case 1:
			break;
	}
	push(y);
}

void xcall() {
	nexttoken();
}


/* 

	statement processes an entire basic statement until the end 
	of the line. 

	The statement loop is a bit odd and requires some explanation.
	A statement function called in the central switch here must either
	call nexttoken as its last action to feed the loop with a new token 
	and then break or it must return which means that the rest of the 
	line is ignored. A function that doesn't call nexttoken and just 
	breaks causes an infinite loop.

*/

void statement(){
	if (DEBUG) debug("statement \n"); 
	while (token != EOL) {
		switch(token){
			case LINENUMBER:
				nexttoken();
				break;
// Palo Alto BASIC language set + BREAK
			case TPRINT:
				xprint();
				break;
			case TLET:
				nexttoken();
				if ((token != ARRAYVAR) && (token != STRINGVAR) && (token != VARIABLE)){
					error(EUNKNOWN);
					break;
				}
			case STRINGVAR:
			case ARRAYVAR:
			case VARIABLE:		
				assignment();
				break;
			case TINPUT:
				xinput();
				break;
#ifdef HASGOSUB
			case TRETURN:
				xreturn();
				break;
			case TGOSUB:
#endif
			case TGOTO:
				xgoto();	
				break;
			case TIF:
				xif();
				break;
#ifdef HASFORNEXT
			case TFOR:
				xfor();
				break;		
			case TNEXT:
				xnext();
				break;
			case TBREAK:
				xbreak();
				break;
#endif
			case TSTOP:
			case TEND: // return here because new input is needed
				st=SINT;
				return;
			case TLIST:
				xlist();
				break;
			case TSCR:
			case TNEW: // return here because new input is needed
				xnew();
				return;
			case TCONT:
			case TRUN:
				xrun();
				return;	
			case TREM:
				xrem();
				break;
// Apple 1 language set 
			case TDIM:
				xdim();
				break;
			case TCLR:
				xclr();
				break;
			case TTAB:
				xtab();
				break;	
			case TPOKE:
				xpoke();
				break;
// Stefan's tinybasic additions
#ifdef HASDUMP
			case TDUMP:
				xdump();
				break;
#endif
			case TSAVE:
				xsave();
				break;
			case TLOAD: 
				xload();
				return; // load doesn't like break as the ibuffer is messed up;
			case TGET:
				xget();
				break;
			case TPUT:
				xput();
				break;			
			case TSET:
				xset();
				break;
// Arduino IO
			case TDWRITE:
				xdwrite();
				break;	
			case TAWRITE:
				xawrite();
				break;
			case TPINM:
				xpinm();
				break;
			case TDELAY:
				xdelay();
				break;
			case TTONE:
				xtone();
				break;	
// SD card DOS function 
			case TCATALOG:
				xcatalog();
				break;
			case TDELETE:
				xdelete();
				break;
			case TOPEN:
				xopen();
				break;
			case TCLOSE:
				xclose();
				break;
// low level functions 
			case TCALL:
				xcall();
				break;	
// and all the rest
			case UNKNOWN:
				error(EUNKNOWN);
				return;
			case ':':
				nexttoken();
				break;
			default: // very tolerant - tokens are just skipped 
				nexttoken();
		}
#ifdef ARDUINO
		if (checkch() == BREAKCHAR) {st=SINT; xc=inch(); return;};  // on an Arduino entering "#" at runtime stops the program
#endif
		if (er) return;
	}
}

// the setup routine - Arduino style
void setup() {


#ifndef ARDUINO
#ifndef MINGW
	timespec_get(&start_time, TIME_UTC);
#endif
#endif
	ioinit();
	printmessage(MGREET); outspc();
	printmessage(EOUTOFMEMORY); outspc(); outnumber(MEMSIZE); outspc();
	outnumber(numsize); outcr();
	//outnumber(elength()); outcr();

 	xnew();	
#ifdef ARDUINOSD
 	SD.begin(sd_chipselect);
#endif	
#ifdef ARDUINOEEPROM
  	if (eread(0) == 1){ // autorun from the EEPROM
		top=(unsigned char) eread(1);
		top+=((unsigned char) eread(2))*256;
  		st=SERUN;
  	} 
#endif
}

// the loop routine for interactive input 
void loop() {

	if (st != SERUN) {

		iodefaults();

		printmessage(MPROMPT);
    	ins(ibuffer, BUFSIZE);
        
        bi=ibuffer;
		nexttoken();

    	if (token == NUMBER) {
         	storeline();		
    	} else {
      		st=SINT;
      		statement();   
    	}

    	// here, at last, all errors need to be catched
    	if (er) reseterror();

	} else {
		xrun();
		// cleanup needed after autorun, top is the EEPROM top
    	top=0;
	}
}


#ifndef ARDUINO
int main(){
	setup();
	while (TRUE)
		loop();
}
#endif
