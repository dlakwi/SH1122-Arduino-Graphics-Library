// GD_SH1122.ino

/* Tiny Graphics Library v3
 * see http://www.technoblogy.com/show?23OS
 * 
 * David Johnson-Davies
 * www.technoblogy.com
 * 123rd OSeptember 2018
 * ATtiny85 @ 8 MHz (internal oscillator; BOD disabled)
 * 
 * Sino Wealth SH1122 256x64 I2C OLED
 * Donald Johnson
 * www.dlakwi.net
 * 2020-01-28
 * Seeedstudio V4.2 (ATMega328P) - 3.3V
 * https://www.ebay.ca/itm/I2C-2-08-256x64-Monochrome-Graphic-OLED-Display-Module/133135253955
 * LM35
 * 
 * CC BY 4.0
 * Licensed under a Creative Commons Attribution 4.0 International license: 
 * http://creativecommons.org/licenses/by/4.0/
 */

// SH1122 I2C

#include <Wire.h>

// serial debug info
//#define _DEBUG 1

// Constants

// OLED I2C address
const int address     = 0x3C;

// OLED I2C command/data codes
const int commands    = 0x00;
const int data        = 0x40;
const int onecommand  = 0x80;
const int onedata     = 0xC0;

const int width       =  256;  // OLED width
const int height      =   64;  // OLED height
const int pixels_byte =    2;

// 'colours' for plot elements
uint8_t textColour    = 0x0D;
uint8_t axisColour    = 0x04;
uint8_t dataColour    = 0x0F;

// OLED display **********************************************

#include "CharMap.h"

// Initialisation sequence for SH1122 OLED module

const uint8_t Init[] PROGMEM =
{
  0xAE,        // display off
  0xB0, 0x00,  // set row address mode
  0x10,        // set column high address         = 00
  0x00,        // set column low  address         = 00
  0xD5, 0x50,  // set frame frequency             = 50 [default]
  0xD9, 0x08,  // set dis-/pre-_charge period     = 22 [default = 08]
  0x40,        // set display start line (40-7F)  = 40  = 0
  0x81, 0xC0,  // contrast control(00-FF)         = C0
  0xA0,        // set segment re-map (A0|A1)      = A0 (rotation)
  0xC0,        // com scan com1-com64 (C0 or C8)  = C0 [default] (C0: 0..N, C8: N..0)
  0xA4,        // entire display off (A4|A5)      = A4 (A4=black background, A5=white background)
  0xA6,        // set normal display (A6|A7)      = A6 (A6=normal, A7=reverse)
  0xA8, 0x3F,  // set multiplex ratio             = 3F [default]
  0xAD, 0x80,  // set dc/dc booster               = 80
  0xD3, 0x00,  // set display offset (00-3F)      = 00 [default]
  0xDB, 0x35,  // set vcom deselect level (35)    = 35 [default]
  0xDC, 0x35,  // set vsegm level                 = 35 [default]
  0x30,        // Set Discharge VSL Level         = 30
  // before turning the display on, should we clear the display?
  0xAF         // display on
};

#define InitLen sizeof(Init)/sizeof(uint8_t)

// Write a single command

void SingleComm( uint8_t x )
{
  Wire.write( onecommand );
  Wire.write( x );
}

// initialize the display

void InitDisplay()
{
  Wire.beginTransmission( address );
  Wire.write( commands );
  for ( uint8_t i = 0; i < InitLen; i++ )
    Wire.write( pgm_read_byte( &Init[i] ) );
  Wire.endTransmission();
}

// clear the display
// I2C transmission must be less than 32 bytes in length,
//   so, break the 256 nibbles/128 bytes into 8 blocks of 32 nibbles/16 bytes

const int i2cbytes  = 16;
const int i2cblocks = ( width / i2cbytes ) * pixels_byte;

void ClearDisplay()
{
  for ( int y = 0 ; y < height; y++ )
  {
    Wire.beginTransmission( address );
    Wire.write( commands );
    Wire.write( 0xB0 );  Wire.write( y );     // row
    Wire.write( 0x00 );  Wire.write( 0x10 );  // starting at column 0
    Wire.endTransmission();

    for ( int c = 0 ; c < width; c += 32 )
    {
      Wire.beginTransmission( address );
      Wire.write( data );
      for ( int b = 0 ; b < i2cbytes; b++ )  // 16 bytes = 32 nibbles
        Wire.write( 0 );
      Wire.endTransmission();
    }
  }
}

// assumes that x is on even column, and that w is even

void ClearArea( int x, int y, int w, int h )
{
  x >>= 1; w >>= 1;
  for ( int j = y ; j < y+h; j++ )
  {
    Wire.beginTransmission( address );
    Wire.write( commands );
    Wire.write( 0xB0 );  Wire.write( j );     // row
    Wire.write( 0x00 | ( x & 0x0F ) );  Wire.write( 0x10 | ( x >> 4 ) );  // column
    Wire.endTransmission();

    int lastx  = x + w;
    int l      = lastx;
    int blocks = lastx / 16;
    int rem    = lastx % 16;
    for ( int i = 0; i < lastx; i++ )
    {
      Wire.beginTransmission( address );
      Wire.write( data );
      for ( int b = 0; ( ( l > 0 ) && ( b < 16 ) ); l--, b++ )  // 16 bytes = 32 nibbles
        Wire.write( 0 );
      Wire.endTransmission();
    }
  }
}

// ----

// Current plot position
int x0;
int y0;

// Plot one point

void PlotPoint( uint8_t p, int x, int y )
{
  uint8_t o, n;                         // old and new pixels
  uint8_t col = x >> 1;                 // two pixels per byte

  Wire.beginTransmission( address );
  SingleComm( 0xB0 ); SingleComm( y );  // Row start
  SingleComm( 0x00 + (col & 0x0F) );    // Column start lo
  SingleComm( 0x10 + (col >> 4) );      // Column start hi
  SingleComm( 0xE0 );                   // start Read-Modify-Write     // start R-M-W
  Wire.write( onedata );                // needed? (end commands?)
  Wire.endTransmission();

  Wire.requestFrom( address, 2 );
  o = Wire.read();                      // dummy read                  // Read
  o = Wire.read();                      // read current pixels (2)     // 

  if ( ( x & 1 ) == 0 )                 // even                        // Modify
  {                                                                    // 
    n  = p << 4;                        // new point is hi nibble      // 
    o &= 0x0F;                          // keep old lo nibble          // 
  }                                                                    // 
  else                                  // odd                         // 
  {                                                                    // 
    n  = p;                             // new point is lo nibble      // 
    o &= 0xF0;                          // keep old hi nibble          // 
  }                                                                    // 

  Wire.beginTransmission( address );
  Wire.write( onedata );                //                             // Write
  Wire.write( o | n );                  //                             // old | new

  SingleComm( 0xEE );                   // end Read-Modify-Write       // end R-M-W
  Wire.endTransmission();
}

// Move current plot position to x,y

void MoveTo( int x, int y )
{
  x0 = x;
  y0 = y;
}

// Draw a line to x,y
void DrawTo( uint8_t p, int x, int y )
{
  int sx, sy, e2, err;
  int dx = abs( x - x0 );
  int dy = abs( y - y0 );
  if ( x0 < x ) sx = 1; else sx = -1;
  if ( y0 < y ) sy = 1; else sy = -1;
  err = dx - dy;
  for ( ; ; )
  {
    PlotPoint( p, x0, y0 );
    if ( x0 == x && y0 == y ) return;
    e2 = err << 1;
    if ( e2 > -dy ) { err = err - dy; x0 = x0 + sx; }
    if ( e2 <  dx ) { err = err + dx; y0 = y0 + sy; }
  }
}

// plot a character at text line and column
//   even-numbered columns

void PlotChar( char ch, uint8_t x, uint8_t y )
{
  ch = ch - 32;  // CharMap[] ==> 32~127
  uint8_t row = y;
  uint8_t col = x >> 1;

  for ( uint8_t r = 0 ; r < 8; r++ )           // glyph is 8 pixels high
  {
    Wire.beginTransmission( address );
    Wire.write( commands );
    Wire.write( 0x00 + (col & 0x0F) );         // Column start lo
    Wire.write( 0x10 + (col >> 4) );           // Column start hi
    Wire.write( 0xB0 );
    Wire.write( (row+r) & 0x3F );              // Row start
    Wire.endTransmission();

    Wire.beginTransmission( address );
    Wire.write( data );
    for ( uint8_t c = 0 ; c < 3; c++ )         // glyph is 6 pixels wide == 3 bytes wide
    {
      int     adds = &CharMap[ch][c*2];
      uint8_t hi   = pgm_read_byte( adds );    // even pixel
      uint8_t lo   = pgm_read_byte( adds+1 );  // odd  pixel
      uint8_t mask = 1 << r;
      hi = hi & mask ? textColour << 4 : 0;
      lo = lo & mask ? textColour      : 0;
      Wire.write( hi | lo );
    }
    Wire.endTransmission();
  }
}

// plot a character at text line and column
//   odd or even columns

// untested!

void PlotCharXY( char ch, uint8_t x, uint8_t y )
{
  if ( y % 2 == 0 )
  {
    // even column
    PlotChar( ch, x, y );
    return;
  }

  ch = ch - 32;
  uint8_t row = x;
  uint8_t col = y >> 1;

  // odd column

  for ( uint8_t r = 0 ; r < 8; r++ )
  {
    Wire.beginTransmission( address );
    Wire.write( commands );
    Wire.write( 0x00 + (col & 0x0F) );         // Column start lo
    Wire.write( 0x10 + (col >> 4) );           // Column start hi
    Wire.write( 0xB0 );
    Wire.write( (row+r) & 0x3F );              // Row start
    Wire.endTransmission();

    // first column
    {
      int     adds = &CharMap[ch][0];
      uint8_t hi;
      uint8_t lo = pgm_read_byte( adds );      // glyph column 1
      uint8_t mask = 1 << r;
      lo = lo & mask ? textColour : 0;         // new lo nibble
      Wire.beginTransmission( address );
      Wire.write( onecommand );
      Wire.write( 0xE0 );                      // start Read-Modify-Write
      Wire.requestFrom( address, 2 );
      hi = Wire.read();                        // Dummy read
      hi = Wire.read();                        // hi nibble to be kept
      Wire.write( onedata );
      Wire.write( hi | lo );
      Wire.write( onecommand );
      Wire.write( 0xEE );                      // end Read-Modify-Write
      Wire.endTransmission();
    }

    // columns 2~5
    Wire.beginTransmission( address );
    Wire.write( data );
    for ( uint8_t c = 1 ; c < 3; c++ )
    {
      int     adds = &CharMap[ch][c*2-1];
      uint8_t lo   = pgm_read_byte( adds );    // glyph column i
      uint8_t hi   = pgm_read_byte( adds+1 );  // glyph column i+1
      uint8_t mask = 1 << r;
      lo = lo & mask ? textColour      : 0;    // odd
      hi = hi & mask ? textColour << 4 : 0;    // even
      Wire.write( lo | hi );
    }
    Wire.endTransmission();

    // column 6
    {
      int     adds = &CharMap[ch][5];
      uint8_t hi = pgm_read_byte( adds );      // glyph column 6
      uint8_t lo;
      uint8_t mask = 1 << r;
      hi = hi & mask ? textColour << 4 : 0;    // new hi nibble
      Wire.beginTransmission( address );
      Wire.write( onecommand );
      Wire.write( 0xE0 );                      // start Read-Modify-Write
      Wire.requestFrom( address, 2 );
      lo = Wire.read();                        // Dummy read
      lo = Wire.read();
      Wire.write( onedata );
      Wire.write( hi | lo );
      Wire.write( onecommand );
      Wire.write( 0xEE );                      // end Read-Modify-Write
      Wire.endTransmission();
    }
  }
}

// Plot text starting at the current plot position

void PlotText( PGM_P s )
{
  int p = (int)s;
  while ( 1 )
  {
    char c = pgm_read_byte( p++ );
    if ( c == 0 ) return;
    PlotChar( c, x0, y0 );
    x0 = x0 + 6;  // glyphs are 6 columns wide
  }
}

// ----
// OLED origin is at upper-left
// character origin is at upper left
// plot origin is at lower left
//   OLED y = 63 - plot y
// 

// data area coordinates

const int dx =  40;
const int dy =  12;
const int dw = 181;
const int dh =  41;
const int ax = dx-1;
const int ay = dy+dh;

const int tx =  10;  // horizontal tick spacing

const int ty =   5;  // vertical tick spacing
const int sy =   2;  // vertical scale
const int zy = ty*sy;
const int ny =   5;

void drawGraph( void )
{
  // horizontal axis
  MoveTo( ax, ay ); DrawTo( axisColour, ax+dw+1, ay );
  // horizontal ticks
  for ( int i = 0, t = 0; i <= dw; i += tx, t++ )
  {
    int tick_x = dx + i;
    int th = 2;  // tick height
    if ( ( t % 6 ) == 0 )
      th = 6;
    else if ( ( t % 3 ) == 0 )
      th = 4;
    MoveTo( tick_x, ay+1 ); DrawTo( axisColour, tick_x, ay+th );
  }

  // vertical axis
  MoveTo( ax, ay-1 ); DrawTo( axisColour, ax, ay-dh-1 );
  // vertical ticks and scale
  for ( int i = ty; i <= ny*ty; i += ty )
  {
    int tick_y = dy+dh-1 - i*sy + 10;
    MoveTo( ax-1, tick_y ); DrawTo( axisColour, ax-4, tick_y );
    int tens = i/10;
    if ( tens != 0 )
      PlotChar( tens+'0', ax-18, tick_y-3 );
    PlotChar( i%10+'0', ax-12, tick_y-3 );
  }

  // title
  MoveTo( dx+12, 2 ); PlotText( PSTR("Temperature ~C") );
}

// the spinner shows that the circuit is still working
// by drawing points on a circular path
// this spinner spins once a second

static uint8_t spin = 0;
unsigned long spinStep = 50;  // 50 mS per step * 20 = 1 second per complete spin
unsigned long previousSpin;

typedef struct { uint8_t x; uint8_t y; } xypair;

// coordinates for an 8x8 circle
xypair pair[20] =
{
  { 4, 0 }, { 5, 0 }, { 6, 1 }, { 7, 2 }, { 7, 3 },
  { 7, 4 }, { 7, 5 }, { 6, 6 }, { 5, 7 }, { 4, 7 },
  { 3, 7 }, { 2, 7 }, { 1, 6 }, { 0, 5 }, { 0, 4 },
  { 0, 3 }, { 0, 2 }, { 1, 1 }, { 2, 0 }, { 3, 0 }
};

// draw the spinner in the lower-left corner
const int spin_x =  8;
const int spin_y = 54;

void nextSpin( void )
{
  int spin_1 = ( spin == 0 ) ? 19 : spin - 1;
  PlotPoint( 0x0, spin_x + pair[spin_1].x, spin_y + pair[spin_1].y );  // set spin-2 to  0

  PlotPoint( 0x4, spin_x + pair[spin  ].x, spin_y + pair[spin  ].y );  // set spin-1 to  8
  spin = ( spin == 19 ) ? 0 : spin + 1;
  PlotPoint( 0xD, spin_x + pair[spin  ].x, spin_y + pair[spin  ].y );  // set spin   to 14
}

uint8_t SampleNo = 1;

void plotTemp( void )
{
  // in setup() add:
  //   analogReference(INTERNAL1V1);
  // Since the TMP37 gives a voltage of 20mV per degree C, and 0mV at 0 C, we need to divide the analogRead() value
  // by (1024/1100)*10 or about 9.3 to get the number of half degrees. A good approximation to this is 233/25:
  //   int Temperature = ( ( analogRead( A2 ) * 25 ) / 233 );
  // If you're using the TMP36, which gives a voltage of 10mV per degree C and 500mV at 0 C, change the line to:
  //   int Temperature = ( ( analogRead( A2 ) - 465 ) * 50 ) / 233;

  // TMP37
//unsigned long  Temperature =   ( analogRead( A2 )           * 25UL ) / 233UL;
  // TMP36
//unsigned long  Temperature = ( ( analogRead( A2 ) - 465UL ) * 50UL ) / 233UL;
  // TMP35 or LM35
  unsigned long Temperature  =   ( analogRead( A2 )           * 50UL ) / 233UL;

  // the temperature starts at 5 and with a scale of 2, we need to subtract 10 from the temperature reading
  // the OLED origin is at the upper-left, so we subtract the Temerature reading and not add it
  int t = dy+dh - Temperature + zy;
  t = min( dy+dh, max( dy, t ) );    // just in case the reading is too high or low

#ifdef _DEBUG
  Serial.print( SampleNo ); Serial.print( " = " ); Serial.println( t );
#endif

  PlotPoint( dataColour, dx + SampleNo, t );
}

// Setup **********************************************

unsigned long period = 1000UL * 60UL;  // 1 minute, in mS !!! these numbers must also be unsigned long (UL)
unsigned long previousMillis;

void setup()
{
  analogReference( INTERNAL );  // 1.1V reference = analog reading of 1023

#ifdef _DEBUG
  Serial.begin( 9600 );
  Serial.println( "GD_1122" );
#endif

  Wire.begin();

  InitDisplay();
  ClearDisplay();

  drawGraph();

  SampleNo       = 0;
  previousMillis = millis();
  previousSpin   = millis();

  plotTemp();
}

void loop()
{
  unsigned long currentMillis = millis();

  if ( ( currentMillis - previousMillis ) >= period )
  {
    // take a new reading
    previousMillis = currentMillis;
    SampleNo++;
    if ( SampleNo > dw )
    {
      SampleNo = 0;
      ClearArea( dx, dy, dw+1, dh );  // dw is odd - ensure that the last column of data is erased
    }

    // plot the temperature reading
    plotTemp();
  }

  if ( ( currentMillis - previousSpin ) >= spinStep )
  {
    // step the spinner
    previousSpin = currentMillis;
    nextSpin();
  }
}

// --fin--
