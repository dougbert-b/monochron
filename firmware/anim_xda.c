/* ***************************************************************************
// anim.c - the main animation and drawing code for MONOCHRON
// This code is distributed under the GNU Public License
//                which can be found at http://www.gnu.org/licenses/gpl.txt
//
**************************************************************************** */

#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
 
#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "ks0108conf.h"
#include "glcd.h"
#include "font5x7.h"

#ifdef XDALICHRON
extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t old_m, old_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t alarming, alarm_h, alarm_m;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;

extern volatile uint8_t blinkingdots;
uint8_t digitsmutex_xda = 0;

extern volatile uint8_t second_changed, minute_changed, hour_changed;

uint8_t redraw_time_xda = 0;
uint8_t last_score_mode_xda = 0;

extern uint16_t digit_base(int d);  // In rle.c

// Protypes
// Called from dispatcher:
void initanim_xda(void);
void initdisplay_xda(uint8_t);
void drawdisplay(uint8_t);
void step(void);
// Support:
void drawdot_xda(uint8_t x, uint8_t y, uint8_t inverted);
void drawdots_xda(uint8_t inverted);
void drawdigit_xda(uint8_t x, uint8_t y, uint8_t d, uint8_t inverted);
void transitiondigit_xda(uint8_t x, uint8_t y, uint8_t o, uint8_t t, uint8_t inverted);
void bitblit_ram(uint8_t x_origin, uint8_t y_origin, uint8_t *bitmap_p, uint8_t size, uint8_t inverted);
//void blitsegs_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t height, uint8_t inverted);
//void bitblit_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t size, uint8_t inverted);

uint8_t steps = 0;


uint8_t old_digits[4] = {0};
uint8_t new_digits[4] = {0};

void initanim_xda(void) {
  DEBUG(putstring("screen width: "));
  DEBUG(uart_putw_dec(GLCD_XPIXELS));
  DEBUG(putstring("\n\rscreen height: "));
  DEBUG(uart_putw_dec(GLCD_YPIXELS));
  DEBUG(putstring_nl(""));
  initdisplay_xda(0);
}

uint8_t time_loc[4] = {
        DISPLAY_H10_X_XDA, DISPLAY_H1_X_XDA, DISPLAY_M10_X_XDA, DISPLAY_M1_X_XDA
};

void initdisplay_xda(uint8_t inverted) {
  glcdFillRectangle(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, inverted);

  steps = 0;

  uint8_t newleft, newright;
  newleft = hours(time_h);
  newright = time_m;
  
  old_digits[0] = old_digits[1] = old_digits[2] = old_digits[3] = 255;
  new_digits[3] = newright % 10;        
  new_digits[2] = newright / 10;
  new_digits[1] = newleft%10;
  new_digits[0] = newleft/10;
  
  while (steps++ <= MAX_STEPS) {
          for(uint8_t i=0;i<4;i++)
                  transitiondigit_xda(time_loc[i], DISPLAY_TIME_Y_XDA, old_digits[i], new_digits[i], inverted);
    /*transitiondigit_xda(DISPLAY_M1_X_XDA, DISPLAY_TIME_Y_XDA,
                    old_digits[3], new_digits[3], inverted);
    transitiondigit_xda(DISPLAY_M10_X_XDA, DISPLAY_TIME_Y_XDA,
                    old_digits[2], new_digits[2], inverted);
    transitiondigit_xda(DISPLAY_H1_X_XDA, DISPLAY_TIME_Y_XDA,
                    old_digits[1], new_digits[1], inverted);
    transitiondigit_xda(DISPLAY_H10_X_XDA, DISPLAY_TIME_Y_XDA,
                    old_digits[0], new_digits[0], inverted);*/
  }
  putstring("done init");
  for (uint8_t i=0; i<4; i++) {
    old_digits[i] = new_digits[i];
  }

  drawdots_xda(inverted);

  steps = 0;
}

void drawdot_xda(uint8_t x, uint8_t y, uint8_t inverted) {
  glcdFillRectangle(x-1, y-3, 2, 6, !inverted);
  glcdFillRectangle(x-2, y-2, 4, 4, !inverted);
  glcdFillRectangle(x-3, y-1, 6, 2, !inverted);

  //glcdFillCircle(x, y, DOTRADIUS-1, !inverted);
}

void drawdots_xda(uint8_t inverted) {
  drawdot_xda(64, 20, inverted);
  drawdot_xda(64, 64-DOTRADIUS-2, inverted);
}

void drawdisplay_xda(uint8_t inverted) {
  static uint8_t last_mode = 0;
  static uint8_t lastinverted = 0;
  uint8_t i;

  /*
  putstring("**** ");
  uart_putw_dec(time_h);
  uart_putchar(':');
  uart_putw_dec(time_m);
  uart_putchar(':');
  uart_putw_dec(time_s);
  putstring_nl("****");
  */

  if ((score_mode == SCORE_MODE_TIME) || (score_mode == SCORE_MODE_ALARM)) {
    // putstring_nl("time");
    if (last_mode != score_mode) {
      if (! digitsmutex_xda) {
        for (uint8_t i=0; i<4; i++) {
          old_digits[i] = new_digits[i];
        }
        uint8_t newleft, newright;
        if (score_mode == SCORE_MODE_TIME) {
          newleft = hours(time_h);
          newright = time_m;
        } else {
          newleft = hours(alarm_h);
          newright = alarm_m;
        }
        new_digits[3] = newright % 10;        
        new_digits[2] = newright / 10;
        new_digits[0] = newleft/10;
        new_digits[1] = newleft%10;

        drawdots_xda(inverted);
        glcdFillRectangle(60, 35, 7, 5, inverted);

        last_mode = score_mode;
        digitsmutex_xda++;
      }
    }

    if (score_mode == SCORE_MODE_TIME) {        //Prevent the minute/hour transistion if time changes, while alarm time is still being shown.
      if (minute_changed || hour_changed) {
        //putstring_nl("changed");
        if (! digitsmutex_xda) {
          digitsmutex_xda++;
          
          old_digits[3] = new_digits[3];
          new_digits[3] = time_m % 10;
          old_digits[2] = new_digits[2];
          new_digits[2] = time_m / 10;
            
          if (hour_changed) {
            uint8_t newleft = hours(time_h);
            old_digits[0] = new_digits[0];
            old_digits[1] = new_digits[1];
            new_digits[0] = newleft/10;
            new_digits[1] = newleft%10;
            }
            minute_changed = hour_changed = 0;
          }
      }
    }

    if ((lastinverted != inverted) && !digitsmutex_xda) {
      glcdFillRectangle(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, inverted);

      drawdots_xda(inverted);

      /*drawdigit_xda(DISPLAY_M1_X_XDA, DISPLAY_TIME_Y_XDA, new_digits[3], inverted);
      drawdigit_xda(DISPLAY_M10_X_XDA, DISPLAY_TIME_Y_XDA, new_digits[2], inverted);
      drawdigit_xda(DISPLAY_H1_X_XDA, DISPLAY_TIME_Y_XDA, new_digits[1], inverted);
      drawdigit_xda(DISPLAY_H10_X_XDA, DISPLAY_TIME_Y_XDA, new_digits[0], inverted);*/
      for(i=0;i<4;i++)
                drawdigit_xda(time_loc[i], DISPLAY_TIME_Y_XDA, new_digits[i], inverted);
    }
   
  } else if (score_mode == SCORE_MODE_DATE) {
    if (last_mode != score_mode) {
      putstring_nl("date!");
      if (! digitsmutex_xda) {
        digitsmutex_xda++;
        last_mode = score_mode;

        for (uint8_t i=0; i<4; i++) {
          old_digits[i] = new_digits[i];
        }
        uint8_t left, right;
        if (region == REGION_US) {
          left = date_m;
          right = date_d;
        } else {
          left = date_d;
          right = date_m;
        }
        new_digits[0] = left / 10;
        new_digits[1] = left % 10;
        new_digits[2] = right / 10;
        new_digits[3] = right % 10;

        drawdots_xda(!inverted);
      }
    }
  } else if (score_mode == SCORE_MODE_YEAR) {
    if (last_mode != score_mode) {
      putstring_nl("year!");
      if (! digitsmutex_xda) {
        digitsmutex_xda++;
        putstring_nl("draw");
        last_mode = score_mode;

        for (uint8_t i=0; i<4; i++) {
          old_digits[i] = new_digits[i];
        }

        new_digits[0] = 2;
        new_digits[1] = 0;
        new_digits[2] = date_y / 10;
        new_digits[3] = date_y % 10;

        drawdots_xda(!inverted);
        glcdFillRectangle(60, 35, 7, 5, inverted);
      }
    }
  }

  /*
  for (uint8_t i=0; i<4; i++) {
    if (old_digits[i] != new_digits[i]) {
      uart_putw_dec(old_digits[0]);
      uart_putw_dec(old_digits[1]);
      uart_putc(':');
      uart_putw_dec(old_digits[2]);
      uart_putw_dec(old_digits[3]);
      putstring(" -> ");
      uart_putw_dec(new_digits[0]);
      uart_putw_dec(new_digits[1]);
      uart_putc(':');
      uart_putw_dec(new_digits[2]);
      uart_putw_dec(new_digits[3]);
      putstring_nl("");
      break;
    }
  }
  */

  for(i=0;i<4;i++)
    if (old_digits[i] != new_digits[i]) {
            transitiondigit_xda(time_loc[i], DISPLAY_TIME_Y_XDA,
                      old_digits[i], new_digits[i], inverted);
    }

  if (digitsmutex_xda) {
    steps++;
    if (steps > MAX_STEPS) {
      steps = 0;

      for(i=0;i<4;i++) {
        old_digits[i] = new_digits[i];
      }
      digitsmutex_xda--;

      if (score_mode == SCORE_MODE_DATE) {
        glcdFillRectangle(60, 35, 7, 5, !inverted);
      } else if (score_mode == SCORE_MODE_YEAR) {

      } else if (score_mode == SCORE_MODE_TIME) {
        //drawdots(inverted);
      }
    }
  }
  
  lastinverted = inverted;

}



void step_xda(void) {
  if(score_mode == SCORE_MODE_TIME)
  {
    if (second_changed)
    {
       second_changed = 0;
       drawdots_xda((time_s%2));
    }
  }
}


void drawdigit_xda(uint8_t x, uint8_t y, uint8_t d, uint8_t inverted) {
//  blitsegs_rom(x, y, digit_base(d), 64, inverted);

  steps = MAX_STEPS;   // draw fully transitioned instead of specialized routine (blitsegs_rom)
  transitiondigit_xda(x, y, d, d, inverted);
}

// Uncompressed is 32 bits per line, but we only need 20 bits/line (4 * 5 bits).
//
// Pack four 5-bit numbers in to bytes:
//
// aaaaa bbbbb ccccc ddddd   phase 0
// eeeee fffff ggggg hhhhh   phase 1
//
// 765 43210
// ---------
// ccc aaaaa  d0
// ccd bbbbb  d1
// ddd eeeee  d2
// dhh fffff  d3
// hhh ggggg  d4
//
void read_line(uint8_t d, uint8_t line, uint8_t *lineinfo, uint8_t *segs)
{
#if 0 // no compression
#define SEG_TERM 255
    for(uint8_t i=0;i<4;i++) {
        lineinfo[i] = pgm_read_byte(digit_base(d) + 4*line + i);
    }
#else // read compressed
#define SEG_TERM 31

    uint16_t x = digit_base(d) + (line/2)*5;          // x is same for odd & even lines
    if(line & 1) {   // phase 1
        uint8_t d2 = pgm_read_byte(x+2);
        uint8_t d3 = pgm_read_byte(x+3);
        uint8_t d4 = pgm_read_byte(x+4);
        lineinfo[0] = d2 & 0x1f;
        lineinfo[1] = d3 & 0x1f;
        lineinfo[2] = d4 & 0x1f;
        lineinfo[3] = (d4>>5) | ((d3&0x60)>>2); 
        
    } else {         // phase 0
        uint8_t d0 = pgm_read_byte(x+0);
        uint8_t d1 = pgm_read_byte(x+1);
        uint8_t d2 = pgm_read_byte(x+2);
        uint8_t d3 = pgm_read_byte(x+3);
        lineinfo[0] = d0 & 0x1f;
        lineinfo[1] = d1 & 0x1f;
        lineinfo[2] = ((d0>>3) & 0x1c) | ((d1>>6) & 3);
        lineinfo[3] = ((d1>>1) & 0x10) | ((d2>>4) & 0x0e) | ((d3>>7) & 1);

    }
//        if(lineinfo[0] == 31) lineinfo[0]=255;
//        if(lineinfo[1] == 31) lineinfo[1]=255;
 //       if(lineinfo[2] == 31) lineinfo[2]=255;
  //      if(lineinfo[3] == 31) lineinfo[3]=255;
#endif
    *segs = 0;
    if (lineinfo[0] != SEG_TERM)
      *segs= *segs+1;
    if (lineinfo[2] != SEG_TERM)
      *segs= *segs+1;
}

void transitiondigit_xda(uint8_t x, uint8_t y, uint8_t o, uint8_t t, uint8_t inverted) {
  uint8_t oline[4], tline[4], i;
  uint8_t osegs, tsegs;
  uint8_t bitmap[DIGIT_WIDTH * DIGIT_HEIGHT / 8] = {0};

  for (uint8_t line=0; line<64; line++) {
    /*
      putstring("Line #");
    uart_putw_dec(line);
    putstring_nl("");
    */
    if (o == 255) {
      oline[0] = oline[1] = oline[2] = oline[3] = 0;
      osegs = 2;
    } else {
      read_line(o, line, oline, &osegs);
    }
    read_line(t, line, tline, &tsegs);

    uint8_t segs = (osegs > tsegs ? osegs : tsegs);
    uint8_t oseg[2], tseg[2];
    uint16_t cseg[2];

    for (uint8_t j=0; j<segs; j++) {
    
      if (oline[j*2] != SEG_TERM) {
        oseg[0] = oline[j*2];
        oseg[1] = oline[j*2+1];
      } else {
        oseg[0] = oline[0];
        oseg[1] = oline[1];
      }

      if (tline[j*2] != SEG_TERM) {
        tseg[0] = tline[j*2];
        tseg[1] = tline[j*2+1];
      } else {
        tseg[0] = tline[0];
        tseg[1] = tline[1];
      }
      
      for(i=0;i<2;i++) {
        cseg[i] = tseg[i] - oseg[i];
        cseg[i] *= steps;
        cseg[i] += MAX_STEPS/2;
        cseg[i] /= MAX_STEPS;
        cseg[i] += oseg[i];
        cseg[i] &= 0xff;
      }

      /*
      putstring("orig seg = (");
      uart_putw_dec(oseg[0]);
      putstring(", "); 
      uart_putw_dec(oseg[1]);
      putstring_nl(")");

      putstring("target seg = (");
      uart_putw_dec(tseg[0]);
      putstring(", "); 
      uart_putw_dec(tseg[1]);
      putstring_nl(")");

      putstring("current seg = (");
      uart_putw_dec(cseg[0]);
      putstring(", "); 
      uart_putw_dec(cseg[1]);
      putstring_nl(")");
      */

      //     uart_getchar();

      while (cseg[0] < cseg[1]) {
        //bitmap[cseg[0] + (i*DIGIT_WIDTH)/8 ] |= _BV(i%8);
        /*
          putstring("byte #");
          uart_putw_dec(cseg[0] + ( (line/8) *DIGIT_WIDTH));
          putstring_nl("");
        */

        bitmap[cseg[0] + (line/8)*DIGIT_WIDTH ] |= _BV(line%8);

        //glcdSetDot(x+cseg[0], y+i);
        cseg[0]++;
      }
    }
  }
  bitblit_ram(x,y, bitmap, DIGIT_HEIGHT*DIGIT_WIDTH/8, inverted);
}



void bitblit_ram(uint8_t x_origin, uint8_t y_origin, uint8_t *bitmap_p, uint8_t size, uint8_t inverted) {
  uint8_t x, y, p;

  for (uint8_t i = 0; i<size; i++) {
    p = bitmap_p[i];

    x = i % DIGIT_WIDTH;
    if (x == 0) {
      y = i / DIGIT_WIDTH;
      glcdSetAddress(x+x_origin, (y_origin/8)+y);
    }
    if (inverted) 
      glcdDataWrite(~p);  
    else 
      glcdDataWrite(p);  
  }
}

// number of segments to expect
#define SEGMENTS 2


#if 0
// not used... we just draw the transitioned state @ 100%
void blitsegs_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t height, uint8_t inverted) {
  uint8_t bitmap[DIGIT_WIDTH * DIGIT_HEIGHT / 8] = {0};
  uint8_t i;

  for (uint8_t line = 0; line<height; line++) {
    for(i=0;i<2;i++) {
    uint8_t start = pgm_read_byte(bitmap_p+4*line+(i*2));
    uint8_t stop = pgm_read_byte(bitmap_p+4*line+(i*2)+1);
    
     while (start < stop) {
        bitmap[start + (line/8)*DIGIT_WIDTH ] |= _BV(line%8);
        start++;
      }
    }
  }
  bitblit_ram(x_origin, y_origin, bitmap, DIGIT_HEIGHT*DIGIT_WIDTH/8, inverted);


 /*
  for (uint8_t i = 0; i<height; i++) {
    uint8_t start = pgm_read_byte(bitmap_p+4*i);
    uint8_t stop = pgm_read_byte(bitmap_p+4*i+1);
    if (start == SEG_TERM)
      continue;
    while (start < stop) {
      if (inverted)
        glcdClearDot(x_origin+start, y_origin+i);
      else
        glcdSetDot(x_origin+start, y_origin+i);
      start++;
    }
    start = pgm_read_byte(bitmap_p+4*i+2);
    stop = pgm_read_byte(bitmap_p+4*i+3);
    if (start == SEG_TERM)
      continue;
    while (start < stop) {
      if (inverted)
        glcdClearDot(x_origin+start, y_origin+i);
      else
        glcdSetDot(x_origin+start, y_origin+i);

      start++;
    }
  }
 */
}
#endif

#if 0
// Not used:
void bitblit_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t size, uint8_t inverted) {
  uint8_t x, y;

  for (uint8_t i = 0; i<size; i++) {
    uint8_t p = pgm_read_byte(bitmap_p+i);

    y = i / DIGIT_WIDTH;
    x = i % DIGIT_WIDTH;
      
    glcdSetAddress(x+x_origin, (y_origin/8)+y);
    glcdDataWrite(p);  
  }
}
#endif

//#ifdef XDALICHRON
#endif

