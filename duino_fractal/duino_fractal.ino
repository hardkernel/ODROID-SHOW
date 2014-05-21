// fractal_softfloat.c
// (c) Copyright Declan Malone, 2014
//
// Permission granted to modify and distribute this program under the
// terms of the GNU Public License v2 or later
//
// This program comes with no warranty
//

#include <SPI.h>
#include <Adafruit_ILI9340.h>
#include <Adafruit_GFX.h>
#include <TimerOne.h>

// Arduino UNO pin assignments
#define _cs 10
#define _dc 9
#define _rst 8

// PWM output for LED backlight
uint8_t ledPin = 5;
#define BACKLIGHT_MAX 255

// Pins used as inputs
// ODROID Show brings out digital pins 2, 11, 12 and 13 and analogue A3, A4 and A5
// Any of those could be used
#define POT_INPUT A5
#define ZOOM_IN   11
#define ZOOM_OUT  2

// hard-wire the screen dimensions
#define COLS 320
#define ROWS 240

// Initialise graphics controller class, telling it to use hardware SPI
Adafruit_ILI9340 tft = Adafruit_ILI9340(_cs, _dc, _rst);

// Other variables relating to the screen
uint16_t backgroundColour = ILI9340_BLACK;

// Allow for a choice of calculation methods
enum { MATHS_FLOAT, MATHS_FIXEDPOINT };
int maths = MATHS_FLOAT;

// floating-point variables relating to Mandelbrot set

// complex coordinates for the entire window area
float x_left_f   = -2.5;
float x_right_f  = 1.0;
float y_bottom_f = -1.0;
float y_top_f    = 1.0;


// a simple palette (matches mandelbrot.c)
uint16_t text_palette[] = {  
  ILI9340_RED,
  ILI9340_GREEN,
  ILI9340_YELLOW,
  ILI9340_BLUE,
  ILI9340_MAGENTA,
  ILI9340_CYAN,
  ILI9340_WHITE,
  ILI9340_BLACK,
};

typedef enum { COLOUR_NA, COLOUR_SCALED, COLOUR_MOD,
               COLOUR_TEST,
               COLOUR_SENTINEL } colouring_method;

colouring_method painter = COLOUR_MOD;

uint16_t maxiter = 1000;

// To allow easy navigation via the serial interface, I'll divide the screen
// up into nine different areas
enum { PANEL_ALL, PANEL_CENTRE, PANEL_N, PANEL_NE, PANEL_E, PANEL_SE, PANEL_S,
  PANEL_SW, PANEL_W, PANEL_NW, PANEL_NONE };

// Unfortunately, 320 pixels in X direction isn't a multiple of three, so
// not all panels are of equal size. The values below are taken in pairs,
// so that, eg, the NW panel has x varying between 0 and 106, inclusive.
int x_regions[] = { 0, 106, 107, 107 + 105, 107 + 106, 319 };
int y_regions[] = { 0, 79,  80, 159, 160, 239 };

// Table lookup to translate between panel enums and respective x1,y1 
// and x2,y2 pairs
uint8_t panel_lookup[][4] = {
  { 0, 0, 5, 5 }, // PANEL_ALL; 
  { 2, 2, 3, 3 }, // PANEL_CENTRE
  { 2, 0, 3, 1 }, // PANEL_N
  { 4, 0, 5, 1 }, // PANEL_NE
  { 4, 2, 5, 3 }, // PANEL_E
  { 4, 4, 5, 5 }, // PANEL_SE
  { 2, 4, 3, 5 }, // PANEL_S
  { 0, 4, 1, 5 }, // PANEL_SW
  { 0, 2, 1, 3 }, // PANEL_W
  { 0, 0, 1, 1 }  // PANEL_NW
  // PANEL_NONE is just a sentinel and shouldn't be used as an index
};

// when panning, I'll copy screen data from one area to another, then call
// the mandelbrot function to paint the areas that were initially off-screen.
// We don't have enough space to copy an entire panel into local ram, so
// I allocate enough to store just a line at a time.
static uint16_t line_buffer[107+106];


// function takes very different parameters to those in mandelbrot.c
// Some parameters have become global, while others are new to account for
// being able to zoom around and paint different areas (both of the screen and
// the complex plane)
void mandelbrot_float(int panel) {

  int screen_x1, screen_y1, screen_x2, screen_y2;
  
  screen_x1 = x_regions[panel_lookup[panel][0]];
  screen_y1 = y_regions[panel_lookup[panel][1]];
  screen_x2 = x_regions[panel_lookup[panel][2]];
  screen_y2 = y_regions[panel_lookup[panel][3]];

  Serial.print("screen x1 ");
  Serial.println(screen_x1);
  Serial.print("screen y1 ");
  Serial.println(screen_y1);
  Serial.print("screen x2 ");
  Serial.println(screen_x2);
  Serial.print("screen y2 ");
  Serial.println(screen_y2);
  

  // calculate some variables for translating pixel coordinates into
  // points on the complex plane (pixel 0,0 mapping to the complex
  // point x_left, y_bottom

  float x_delta = (x_right_f - x_left_f)   / (float) COLS;
  float y_delta = (y_top_f   - y_bottom_f) / (float) ROWS;

  float x0,y0; // complex value
  float temp,x,y;

  // palette-related
  int  palette_size = sizeof(text_palette) >> 1; // palette is now 16 bits wide
  uint16_t colour;
  
  // scale factor used for linear scaling (multiplied by iter later)
  float scale_factor = (float) (palette_size - 2) / (float) maxiter;

  int i,j, iter, cindex;

  // Rather than use drawPixel, we write to the screen with one big memory transfer
  // First, we have to tell the display controller where we're going to be writing to
  // My idea of the screen geometry was a bit messed up, unfortunately (x and y reversed)
  tft.setAddrWindow(screen_y1, screen_x1, screen_y2, screen_x2);
  tft.setdcbit();
  tft.clearcsbit();


  for (x0 = x_left_f + x_delta * screen_x1, i=screen_x1; i <= screen_x2; ++i, x0 += x_delta) {
    for (y0 = y_bottom_f + y_delta * screen_y1, j=screen_y1; j <= screen_y2; ++j, y0 += y_delta) {

      x = y = 0.0; iter = 0;

      while ( x*x + y*y < 4.0 && iter < maxiter ) {
          temp = x*x - y*y + x0;
          y    = 2*x*y + y0;
          x    = temp;
          iter++;
      }
      switch (painter) {
      case COLOUR_MOD:
        colour = text_palette
          [ iter >= maxiter ? palette_size - 1 : iter % (palette_size-1) ];
        break;
      case COLOUR_SCALED:     // linear scale; log would be much better
        if (iter >= maxiter) {
          cindex = palette_size - 1;
        } else {
          cindex = (int) floor(0.5 + (float) iter * scale_factor);
          // assert(cindex < palette_size - 1);
        }
        colour = text_palette[cindex];
        break;
      case COLOUR_TEST:
        colour = text_palette[(i+j) % (palette_size - 1)];
        break;
      }
      
      // draw pixel by writing colour value into display memory
      tft.spiwrite(colour >> 8);
      tft.spiwrite(colour & 255);
    }
  }
  
  tft.setcsbit();

}

// copied directly from show_main.ino (though doesn't seem to be needed)
void timer1_setup() {
        cli();
        TCCR1A = 0;
        TCCR1B = 0;
        OCR1A = (16000000 / 1024) -1;
        TCCR1B |= (1 << WGM12);
        TCCR1B |= (1 << CS12) | (1 << CS10);
        TIMSK1 |= (1 << OCIE1A);
        sei();
}

uint32_t x = 0;
ISR(TIMER1_COMPA_vect) {
        x++;
}

void test_panels () {
}
  

void setup() {
        int i;
        unsigned long timer1, timer2;
  
        Serial.begin(500000);
        Serial.println("Mandelbrot Set Explorer v1.0");

        tft.begin();
        
        // turn on backlight
        pinMode(ledPin, OUTPUT);
        analogWrite(ledPin, BACKLIGHT_MAX);
       
        // clear the screen and set up orientation
        tft.fillScreen(backgroundColour);
        tft.setRotation(0);

        // timer1_setup();
        delay(1500);

        timer1 = millis();
        for (i=1; i< PANEL_NONE; ++i)
          mandelbrot_float(i);
        timer2 = millis();
        Serial.print("Time taken for rendering (ms): ");
        Serial.println(timer2 - timer1);

}

void loop() {
  
  
}
