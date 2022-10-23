#if defined(NRF52) && ! defined(__HARDWAREH__)
#define __HARDWAREH__ 


void timeinit() { }

void wiringbegin() { }

/*
 * helper functions OS, heuristic on how much memory is 
 * available in BASIC
 */
long freememorysize() {
	return 8192;
}

long freeRam() {
	return freememorysize();
}


/* 
 * the sleep and restart functions
 */
void restartsystem() {}
void activatesleep(long t) {}


/* 
 * start the SPI bus 
 */
void spibegin() {}


/*
 * DISPLAY driver code section, the hardware models define a set of 
 * of functions and definitions needed for the display driver. These are 
 *
 * dsp_rows, dsp_columns: size of the display
 * dspbegin(), dspprintchar(c, col, row), dspclear()
 * 
 * All displays which have this functions can be used with the 
 * generic display driver below.
 * 
 * Graphics displays need to implement the functions 
 * 
 * rgbcolor(), vgacolor()
 * plot(), line(), rect(), frect(), circle(), fcircle() 
 * 
 * Color is currently either 24 bit or 4 bit 16 color vga.
 */
const int dsp_rows=0;
const int dsp_columns=0;
void dspsetupdatemode(char c) {}
void dspwrite(char c){};
void dspbegin() {};
int dspstat(char c) {return 0; }
char dspwaitonscroll() { return 0; }
char dspactive() {return FALSE; }
void dspsetscrollmode(char c, short l) {}
void dspsetcursor(short c, short r) {}
void rgbcolor(int r, int g, int b) {}
void vgacolor(short c) {}
void plot(int x, int y) {}
void line(int x0, int y0, int x1, int y1)   {}
void rect(int x0, int y0, int x1, int y1)   {}
void frect(int x0, int y0, int x1, int y1)  {}
void circle(int x0, int y0, int r) {}
void fcircle(int x0, int y0, int r) {}
void vgabegin(){}
void vgawrite(char c){}

/* 
 * Keyboard code stubs
 * keyboards can implement 
 * 	kbdbegin()
 * they need to provide
 * 	kbdavailable(), kbdread(), kbdcheckch()
 * the later is for interrupting running BASIC code
 */
void kbdbegin() {}
int kbdstat(char c) {return 0; }
char kbdavailable(){ return 0;}
char kbdread() { return 0;}
char kbdcheckch() { return 0;}

/* Display driver would be here, together with vt52 */


/* 
 * Real Time clock code 
 */

char rtcstring[18] = { 0 }; 

/* identical to arduino code -> isolate */
char* rtcmkstr() {
	return "";
}

short rtcread(char i) {
    return 0;
}
short rtcget(char i) {return rtcread(i);}
void rtcset(char i, short v) {}


/* 
 * Wifi and MQTT code 
 */
void netbegin() {}
char netconnected() { return 0; }
void mqttbegin() {}
int mqttstat(char c) {return 0; }
int  mqttstate() {return -1;}
void mqttsubscribe(char *t) {}
void mqttsettopic(char *t) {}
void mqttouts(char *m, short l) {}
void mqttins(char *b, short nb) { z.a=0; };
char mqttinch() {return 0;};

/* 
 *	EEPROM handling, these function enable the @E array and 
 *	loading and saving to EEPROM with the "!" mechanism
 *	a filesystem based dummy
 */ 

void ebegin(){ 
}

void eflush(){
}

address_t elength() { return 0; }
void eupdate(address_t a, short c) {  }
short eread(address_t a) { return -1;  }

/* 
 *	the wrappers of the arduino io functions, to avoid 
 */	
#ifndef RASPPI
void aread(){ return; }
void dread(){ return; }
void awrite(number_t p, number_t v){}
void dwrite(number_t p, number_t v){}
void pinm(number_t p, number_t m){}
#else
void aread(){ push(analogRead(pop())); }
void dread(){ push(digitalRead(pop())); }
void awrite(number_t p, number_t v){
}
void dwrite(number_t p, number_t v){
}
void pinm(number_t p, number_t m){
}
#endif

/* handling time - raspberry take delay function from wiring */
#if ! defined(MSDOS) && ! defined(MINGW) && ! defined(RASPPI)
void delay(number_t t) {/*usleep(t*1000);*/}
#endif
// ms style stuff
#if defined(MINGW)
void delay(number_t t) {Sleep(t);}
#endif

void bmillis() {
}
// we need to to millis by hand except for RASPPI with wiring
#if ! defined(RASPPI)
long millis() { push(1); bmillis(); return pop(); }
#endif
void bpulsein() { pop(); pop(); pop(); push(0); }
void btone(short a) { pop(); pop(); if (a == 3) pop(); }

/* 
 *	the byield function is called after every statement
 *	it allows two levels of background tasks. 
 */
void byield() {}


/* 
 *	The file system driver - all methods needed to support BASIC fs access
 *	MSDOS to be done 
 *
 * file system code is a wrapper around the POSIX API
 */
void fsbegin(char v) {}

/* POSIX OSes always have filesystems */
int fsstat(char c) {return 1; }

/*
 *	File I/O function on an Arduino
 * 
 * filewrite(), fileread(), fileavailable() as byte access
 * open and close is handled separately by (i/o)file(open/close)
 * only one file can be open for write and read at the same time
 */
void filewrite(char c) { }

char fileread(){
	return 0;
}

char ifileopen(const char* filename){
	return 0;
}

void ifileclose(){
}

char ofileopen(char* filename, const char* m){
	return 0; 
}

void ofileclose(){  }

int fileavailable(){ return 0; }

/*
 * directory handling for the catalog function
 * these methods are needed for a walkthtrough of 
 * one directory
 *
 * rootopen()
 * while rootnextfile()
 * 	if rootisfile() print rootfilename() rootfilesize()
 *	rootfileclose()
 * rootclose()
 */
void rootopen() {}

int rootnextfile() { return 0; }

int rootisfile() { return 0; }

const char* rootfilename() { 
  return 0; 
}

int rootfilesize() { return 0; }
void rootfileclose() {}
void rootclose(){}

/*
 * remove method for files
 */
void removefile(char *filename) {
}

/*
 * formatting for fdisk of the internal filesystems
 */
void formatdisk(short i) {
	outsc("Format not implemented on this platform\n");
}

/*
 *	Primary serial code uses putchar / getchar
 */
void serialbegin(){}
int serialstat(char c) {
  return 0;
}
//void serialwrite(char c) { outchar(c); }
//char serialread() { return getchar(); }
short serialcheckch(){ return TRUE; }
short serialavailable() {return TRUE; }

/*
 * reading from the console with inch 
 * this mixes interpreter levels as inch is used here 
 * this code needs to go to the main interpreter section after 
 * thorough rewrite
 */
void consins(char *b, short nb) {
    char c;	
    z.a = 1;
    while(z.a < nb) {
        c=inch();
        if (id == ISERIAL || id == IKEYBOARD) outch(c);
        if (c == '\r') c=inch(); 			/* skip carriage return */
        if (c == '\n' || c == -1) { 	/* terminal character is either newline or EOF */
            break;
        } else if ( (c == 127 || c == 8) && z.a>1) {
            z.a--;
        } else {
            b[z.a++]=c;
        } 
    }
    b[z.a]=0;
    z.a--;
    b[0]=(unsigned char)z.a; 
}

/* handling the second serial interface */
void prtbegin() {}
int prtstat(char c) {return 0; }
void prtset(int s) {}
void prtwrite(char c) {}
char prtread() {return 0;}
short prtcheckch(){ return FALSE; }
short prtavailable(){ return 0; }


/* 
 *	Read from the radio interface, radio is always block 
 *	oriented. 
 */
int radiostat(char c) {return 0; }
void radioset(int s) {}
void radioins(char *b, short nb) { b[0]=0; b[1]=0; z.a=0; }
void radioouts(char *b, short l) {}
void iradioopen(char *filename) {}
void oradioopen(char *filename) {}
short radioavailable() { return 0; }

/* Arduino sensors */
void sensorbegin() {}
number_t sensorread(short s, short v) {return 0;};




#endif