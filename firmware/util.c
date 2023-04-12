#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <stdlib.h>
#include "ratt.h"
#include "util.h"
#include "ks0108.h"
#include "glcd.h"

extern volatile uint8_t time_format;
extern volatile uint8_t displaystyle;
extern volatile uint8_t timeoutcounter;
extern volatile uint8_t screenmutex;
extern volatile uint8_t displaymode;
extern volatile uint8_t last_buttonstate, just_pressed, pressed;


// Creates a 8N1 UART connect
// remember that the BBR is #defined for each F_CPU in util.h
void uart_init(uint16_t BRR) {
  UBRR0 = BRR;               // set baudrate counter

  UCSR0B = _BV(RXEN0) | _BV(TXEN0);
  UCSR0C = _BV(USBS0) | (3<<UCSZ00);
  DDRD |= _BV(1);
  DDRD &= ~_BV(0);
}

// Some basic delays...
void delay_10us(uint8_t ns)
{
  uint8_t i;

  while (ns != 0) {
    ns--;
    for (i=0; i< 30; i++) {
      nop;
    }
  }
}

void delay_ms(uint16_t ms)
{
	uint16_t temp = ms;
	while(temp)
	{
		_delay_ms(10);
		if(temp >= 10)
			temp-=10;
		else
			temp=0;
	}
}

void delay_s(uint8_t s) {
  while (s--) {
    delay_ms(1000);
  }
}

// Some uart functions for debugging help
int uart_putchar(char c)
{
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

char uart_getchar(void) {
  while (!(UCSR0A & _BV(RXC0)));
  return UDR0;
}

char uart_getch(void) {
  return (UCSR0A & _BV(RXC0));
}

void ROM_putstring(const char *str, uint8_t nl) {
  uint8_t i;

  for (i=0; pgm_read_byte(&str[i]); i++) {
    uart_putchar(pgm_read_byte(&str[i]));
  }
  if (nl) {
    uart_putchar('\n'); uart_putchar('\r');
  }
}

void uart_puts(const char* str)
{
  while(*str)
    uart_putc(*str++);
}


void uart_putc_hex(uint8_t b)
{
  /* upper nibble */
  if((b >> 4) < 0x0a)
    uart_putc((b >> 4) + '0');
  else
    uart_putc((b >> 4) - 0x0a + 'a');

  /* lower nibble */
  if((b & 0x0f) < 0x0a)
    uart_putc((b & 0x0f) + '0');
  else
    uart_putc((b & 0x0f) - 0x0a + 'a');
}

void uart_putw_hex(uint16_t w)
{
  uart_putc_hex((uint8_t) (w >> 8));
  uart_putc_hex((uint8_t) (w & 0xff));
}

void uart_putdw_hex(uint32_t dw)
{
  uart_putw_hex((uint16_t) (dw >> 16));
  uart_putw_hex((uint16_t) (dw & 0xffff));
}

void uart_putw_dec(uint16_t w)
{
  uint16_t num = 10000;
  uint8_t started = 0;

  while(num > 0)
    {
      uint8_t b = w / num;
      if(b > 0 || started || num == 1)
	{
	  uart_putc('0' + b);
	  started = 1;
	}
      w -= b * num;

      num /= 10;
    }
}

void uart_put_dec(int8_t w)
{
  uint16_t num = 100;
  uint8_t started = 0;

  if (w <0 ) {
    uart_putc('-');
    w *= -1;
  }
  while(num > 0)
    {
      int8_t b = w / num;
      if(b > 0 || started || num == 1)
	{
	  uart_putc('0' + b);
	  started = 1;
	}
      w -= b * num;

      num /= 10;
    }
}

void uart_putdw_dec(uint32_t dw)
{
  uint32_t num = 1000000000;
  uint8_t started = 0;

  while(num > 0)
    {
      uint8_t b = dw / num;
      if(b > 0 || started || num == 1)
	{
	  uart_putc('0' + b);
	  started = 1;
	}
      dw -= b * num;

      num /= 10;
    }
}

#ifdef OPTION_DOW_DATELONG
// Date / Time Routines

uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr)
{
  uint16_t month, year;

    // Calculate day of the week
    
    month = mon;
    year = 2000 + yr;
    if (mon < 3)  {
      month += 12;
      year -= 1;
    }
    return (day + (2 * month) + (6 * (month+1)/10) + year + (year/4) - (year/100) + (year/400) + 1) % 7;
}

extern uint8_t DOWText[];
extern uint8_t MonthText[];

uint8_t sdotw(uint8_t dow, uint8_t ix) {
 return eeprom_read_byte(&DOWText[(dow*3) + ix]);
}

uint8_t smon(uint8_t date_m, uint8_t ix) {
 return eeprom_read_byte(&MonthText[(date_m*3) + ix]);
}
#else
 // GPS Needs the DOTW function
 // This includes DOTW for GPS if DateLong disabled
 #ifdef GPSENABLE
 uint8_t dotw(uint8_t mon, uint8_t day, uint8_t yr)
  {
   uint16_t month, year; 

    // Calculate day of the week
    
    month = mon;
    year = 2000 + yr;
    if (mon < 3)  {
      month += 12;
      year -= 1;
    }
    return (day + (2 * month) + (6 * (month+1)/10) + year + (year/4) - (year/100) + (year/400) + 1) % 7;
 }
 #endif
#endif


uint8_t hours(uint8_t h)
{
	return (time_format == TIME_12H ? ((h + 23) % 12 + 1) : h);
}

extern volatile uint8_t time_s, time_m, time_h;
uint32_t rval[2]={0,0};
uint32_t key[4];

void encipher(void) {  // Using 32 rounds of XTea encryption as a PRNG.
  uint32_t v0=rval[0], v1=rval[1], sum=0, delta=0x9E3779B9;
  for (unsigned int i=0; i < 32; i++) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
  }
  rval[0]=v0; rval[1]=v1;
}

void init_crand_consistent(uint8_t h, uint8_t m, uint8_t s)
{
  key[0]=0x2DE9716E;  //Initial XTEA key. Grabbed from the first 16 bytes
  key[1]=0x993FDDD1;  //of grc.com/password.  1 in 2^128 chance of seeing
  key[2]=0x2A77FB57;  //that key again there.
  key[3]=0xB172E6B0;
  key[0]^=s;
  key[1]^=m;
  key[2]^=h;
  rval[0]=0;
  rval[1]=0;
  encipher();
}

void init_crand(void) {
  //uint32_t temp;
  init_crand_consistent(time_h,time_m,time_s);
}

uint16_t crand(uint8_t type) {
// Dataman - Compiler didn't like this logic, and I don't blame it,
// Code path could be simplified.
//  if((type==0)||(type>2)) {
//    wdt_reset();
//    encipher();
//    return (rval[0]^rval[1])&RAND_MAX;
//  } 
//   else
 if (type==1) {
  	return ((rval[0]^rval[1])>>15)&3;
  } 
    else if (type==2) {
   return ((rval[0]^rval[1])>>17)&1;
  }
  wdt_reset();
  encipher();
  return (rval[0]^rval[1])&RAND_MAX; 
}

#ifdef RATTDEATH
extern unsigned char BigFont[];
void drawbigdigit(uint8_t x, uint8_t y, uint8_t n, uint8_t inverted) {
  uint8_t i, j;
  uint8_t sizex=2, sizey=2;
  
#ifdef DEATHCHRON
  if(displaystyle == STYLE_DEATH)
  {
  	  sizex = 3; sizey = 5;
  }
#endif
  
  for (i = 0; i < 4; i++) {
    uint8_t d = eeprom_read_byte(&BigFont[(n*4)+i]);
    for (j=0; j<8; j++) {
      if (d & _BV(7-j)) {
	glcdFillRectangle(x+i*sizex, y+j*sizey, sizex, sizey, !inverted);
      } else {
	glcdFillRectangle(x+i*sizex, y+j*sizey, sizex, sizey, inverted);
      }
    }
  }
}

uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1,
		      uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2) {
  // yer everyday intersection tester
  // check x coord first
  if (x1+w1 < x2)
    return 0;
  if (x2+w2 < x1)
    return 0;

  // check the y coord second
  if (y1+h1 < y2)
    return 0;
  if (y2+h2 < y1)
    return 0;

  return 1;
}
#endif

//Config menu related functions
void print_menu_advance(){
  print_menu("MENU","advance","SET","set");
  // Press MENU to avance
  // Press SET to set
}

void print_menu_exit(){
  print_menu("MENU","exit","SET","save");
  //Press MENU to exit
  //Press SET to set
}

void print_menu_change(){
 print_menu_opts("change","save");
 // Press + to change
 // Press SET to save
}

void PRINT_MENU_OPTS(const char *Opt1, const char *Opt2){
 PRINT_MENU(PSTR("+"),Opt1,PSTR("SET"),Opt2);
 // Press + to X
 // Press SET to X
}

void PRINT_MENU(const char *Button1, const char *Opt1, const char *Button2, const char *Opt2){
 glcdFillRectangle(0, 48, GLCD_XPIXELS, 16, NORMAL);
 PRINT_MENU_LINE(6,Button1,Opt1);
 PRINT_MENU_LINE(7,Button2,Opt2);
}

void PRINT_MENU_LINE(uint8_t line, const char *Button, const char *Action){
  glcdSetAddress(0, line);
  glcdPutStr("Press ",NORMAL);
  glcdPutStr_rom(Button,NORMAL);
  glcdPutStr(" to ",NORMAL);
  glcdPutStr_rom(Action,NORMAL);
}

uint8_t check_timeout(void)
{
	if((displaymode != SET_TIME)&&(displaystyle<=STYLE_ROTATE))
	{
		screenmutex++;
		print_time(time_h, time_m, time_s, SET_TIME);
		screenmutex--;
	}
	if (just_pressed & 0x1) { // mode change
      return 1;
    }
    if (just_pressed || pressed) {
      timeoutcounter = INACTIVITYTIMEOUT;  
      // timeout w/no buttons pressed after 3 seconds?
    } else if (!timeoutcounter) {
      //timed out!
      displaymode = SHOW_TIME;     
      return 2;
    }
    return 0;
}

void add_month(volatile uint8_t *month, volatile uint8_t *day, uint16_t year)
{
  int maxday;

  if (*month >= 13)
    *month = 1;

  if (*month == 2) {
    maxday =  leapyear(year) ? 29 : 28;
  } else if ((*month == 4) || (*month == 6) || (*month == 9) || (*month == 11)) {
    maxday = 30;
  } else {
    maxday = 31;
  }

  if (*day > maxday)
      *day = 1;
}
