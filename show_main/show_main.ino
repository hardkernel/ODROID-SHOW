/***************************************************
 * This is an example sketch for the Adafruit 2.2" SPI display.
 * This library works with the Adafruit 2.2" TFT Breakout w/SD card
 * ----> http://www.adafruit.com/products/1480
 * Check out the links above for our tutorials and wiring diagrams
 * These displays use SPI to communicate, 4 or 5 pins are required to
 * interface (RST is optional)
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 * 
 * Written by Limor Fried/Ladyada for Adafruit Industries.
 * MIT license, all text above must be included in any redistribution
 ****************************************************/

#include <SPI.h>
#include <Adafruit_ILI9340.h>
#include <Adafruit_GFX.h>
#include <TimerOne.h>

// These are the pins used for the UNO
// for Due/Mega/Leonardo use the hardware SPI pins (which are different)
#define _sclk 13
#define _miso 12
#define _mosi 11
#define _cs 10
#define _dc 9
#define _rst 8

#define NOTSPECIAL	1
#define GOTESCAPE	2
#define GOTBRACKET	3
#define INNUM           4
#define IMGSHOW         5

#define LEFT_EDGE0             0
#define RIGHT_EDGE320          319
#define TOP_EDGE0              0
#define BOTTOM_EDGE240         239

#define DEBUG

const char version[] = "v1.1";

typedef struct cursor {
        uint32_t row;
        uint32_t col;
}
cursor;

cursor cursor_sav = { 
        0, 0 };
cursor startImage, endImage;

uint32_t x = 0;
uint8_t c;

uint8_t current_state = NOTSPECIAL;
uint8_t previous_state = NOTSPECIAL;
uint32_t tmpnum;


uint16_t num, row, col;
uint16_t bottom_edge0 = BOTTOM_EDGE240;
uint16_t right_edge0  = RIGHT_EDGE320;

uint8_t textSize = 2;
uint8_t rotation = 1;
uint16_t foregroundColor, backgroundColor;

uint32_t imgsize = 0;
uint32_t sizecnt = 0;

uint8_t ledPin = 5; // PWM LED Backlight control to digital pin 5

uint8_t rgb565hi, rgb565lo;

uint8_t cntenable = 0;

// Using software SPI is really not suggested, its incredibly slow
//Adafruit_ILI9340 tft = Adafruit_ILI9340(_cs, _dc, _mosi, _sclk, _rst, _miso);
// Use hardware SPI
Adafruit_ILI9340 tft = Adafruit_ILI9340(_cs, _dc, _rst);

void setup() {
        Serial.begin(500000);
        Serial.println("Welcome to the odroid_show");

        tft.begin();
        // initialize the digital pin as an output for LED Backlibht
        pinMode(ledPin, OUTPUT);
        analogWrite(ledPin, 200);

        timer1_setup();
        delay(1500);

        if (analogRead(3) > 450 && analogRead(3) < 550)
                testFillScreen();

        backgroundColor = ILI9340_BLACK;
        tft.fillScreen(backgroundColor);
        foregroundColor = ILI9340_YELLOW;
        tft.setTextColor(foregroundColor, backgroundColor);
        tft.setTextSize(textSize);
        tft.setCursor(0, 0);
        tft.setRotation(rotation);

        tft.setCursor(50, 50);
        tft.print("Hello ODROID!");
        tft.setCursor(250, 200);


        tft.print(version);

        delay(1000);
        tft.fillScreen(backgroundColor);
        tft.setCursor(0, 0);

}

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

unsigned long testFillScreen() {
        unsigned long start = micros();
        tft.fillScreen(ILI9340_RED);
        delay(1000); 
        tft.fillScreen(ILI9340_GREEN);
        delay(1000);
        tft.fillScreen(ILI9340_BLUE);
        delay(1000);
        return micros() - start;   
}

ISR(TIMER1_COMPA_vect) {
        x++;
}

void loop(void) {
        if (current_state == IMGSHOW) {
                if (Serial.available() > 1) {
                        rgb565lo = Serial.read();
                        rgb565hi = Serial.read();
                        tft.spiwrite(rgb565hi);
                        tft.spiwrite(rgb565lo);
                        cntenable = 1;
                        sizecnt++;
                        x = 0;
                }
                else if (cntenable == 1) {
                        if ((sizecnt == imgsize) || (x > 2)) {
                                cntenable = 0;
                                sizecnt = 0;
                                Serial.println(sizecnt);
                                tft.setcsbit();
                                switchstate(NOTSPECIAL);
                                Serial.println("NOTSPECIAL Start");
                        }
                }
        }
        else {
                if (Serial.available()) {
                        if ((c = Serial.read()) > 0) {
                                if (parsechar(c) > 0)
                                        tft.print((char)c);

                        }
                }
        }
}

void switchstate(int newstate) {
        previous_state = current_state;
        current_state = newstate;

}

void cursorDown() {
        if (tft.cursor_y < bottom_edge0) {
                tft.cursor_y += textSize*8;
                tft.setCursor(tft.cursor_x, tft.cursor_y);
        }

}

int parsechar(unsigned char current_char) {
        switch(current_state) {
        case NOTSPECIAL:
                if (current_char == 033) {        // Escape
                        switchstate(GOTESCAPE);
                        return 0;
                } 
                else if (current_char == 015) {        // CR
                        tft.setCursor(tft.cursor_x = 0, tft.cursor_y);
                        return 0;
                }	
                else if (current_char == 012) {        // LF
                        cursorDown();
                        return 0;
                } 
                else {
                        switchstate(NOTSPECIAL);
                        return (current_char);
                }
                break;

        case GOTESCAPE:
                switch(current_char) {
                case '[':
                        switchstate(GOTBRACKET);
                        return 0;

                case 'D':        // Cursor Down
                        cursorDown();
                        break;  

                case 'M':        // Cursor Up
                        if (tft.cursor_y == TOP_EDGE0)
                                break;
                        tft.setCursor(tft.cursor_x, tft.cursor_y -= textSize*8);
                        break;

                case 'E':        // Cursor down to row 1
                        tft.cursor_x = 0;
                        cursorDown();
                        switchstate(NOTSPECIAL);
                        return 0;

                case 'c':        // Reset
                        tft.fillScreen(ILI9340_BLACK);
                        tft.setCursor(0, 0);
                        switchstate(NOTSPECIAL);
                        return 0;

                default:
                        switchstate(NOTSPECIAL);
                        return current_char;
                }
                switchstate(NOTSPECIAL);
                return 0;

        case GOTBRACKET:
                if (isdigit(current_char)) {
                        switchstate(INNUM);
                        tmpnum = 0;
                        tmpnum = tmpnum*10 + (current_char - '0');
                        return 0;
                } 
                else {
                        switch (current_char) {

                        case 'A':        // Keyboard UP Arrow
                                if (tft.cursor_y == TOP_EDGE0)
                                        break;
                                tft.setCursor(tft.cursor_x, tft.cursor_y -= textSize*8);
                                break;

                        case 'B':        // Keyboard Down Arrow
                                cursorDown();
                                break;

                        case 'C':        // Keyboard Right Arrow
                                row = (right_edge0 > (row = tft.cursor_x + textSize*6)) ? row : right_edge0;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y);
                                break;

                        case 'D':        // Keyboard Left Arrow
                                row = (LEFT_EDGE0 < (row = tft.cursor_x - textSize*6)) ? row : LEFT_EDGE0;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y);
                                break;

                        case 'H':        // Cursor to Home
                                tft.setCursor(0, 0);
                                break;

                        case 's':        // Save cursor pos
                                cursor_sav.col = tft.cursor_y;
                                cursor_sav.row = tft.cursor_x;
                                break;

                        case 'u':        // Restore cursor pos
                                tft.cursor_y = cursor_sav.col;
                                tft.cursor_x = cursor_sav.row;
                                tft.setCursor(tft.cursor_x, tft.cursor_y);
                                break;

                        case '=':
                                switchstate(INNUM);
                                tmpnum = 0;
                                return 0;

                        default:
                                break;
                        }
                        switchstate(NOTSPECIAL);
                        return 0;
                }
                break;

        case INNUM:

                if (isdigit(current_char)) {
                        tmpnum = tmpnum*10 + (current_char -'0');
                        return 0;
                }
                else {
                        switch(current_char) {
                        case ';':        //Delimiter between row, col
                                // tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                //row = (tmpnum > right_edge0) ? right_edge0 : tmpnum;
                                row = tmpnum;
                                tmpnum = 0;

                                return 0;

                        case 'H':        // Move cursor to row, col
                        case 'f':        // ditto
                                tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                col = (tmpnum > bottom_edge0) ? bottom_edge0 : tmpnum;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y = col);
                                break;

                        case 'A':
                        case 'F':
                                tmpnum = (tmpnum > 0) ? tmpnum : 1;
                                col = (TOP_EDGE0 < (col = tft.cursor_y - tmpnum)) ? col : TOP_EDGE0;
                                tft.setCursor(tft.cursor_x = (current_char == 'A') ? row : LEFT_EDGE0, tft.cursor_y = col);
                                break;

                        case 'B':
                        case 'E':
                                tmpnum = (tmpnum > 0) ? tmpnum : 1;
                                col = (bottom_edge0 > (col = tft.cursor_y + tmpnum)) ? col : bottom_edge0;
                                tft.setCursor(tft.cursor_x = (current_char == 'B') ? row : LEFT_EDGE0, tft.cursor_y = col);
                                break;

                        case 'C':
                                tmpnum = (tmpnum > 0) ? tmpnum : 1;
                                row = (right_edge0 > (row = tft.cursor_x + tmpnum)) ? row : right_edge0;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y = col);
                                break;

                        case 'D':
                                tmpnum = (tmpnum > 0) ? tmpnum : 1;
                                row = (LEFT_EDGE0 < (row = tft.cursor_x - tmpnum)) ? row : LEFT_EDGE0;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y = col);
                                break;

                        case 'G':
                                tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                row = (tmpnum > right_edge0) ? right_edge0 : tmpnum;
                                tft.setCursor(tft.cursor_x = row, tft.cursor_y = col);
                                break;

                        case 's':    //text size
                                textSize = tmpnum;
                                tft.setTextSize(textSize);
                                break;

                        case 'r':    //rotation
                                rotation = tmpnum;
                                tft.setRotation(rotation);
                                break;

                        case 'm':
                                if (tmpnum/10 == 3) {
                                        foregroundColor = change_mColor(tmpnum%30);
                                        tft.setTextColor(foregroundColor, backgroundColor);
                                } 
                                else if (tmpnum/10 == 4) {
                                        backgroundColor = change_mColor(tmpnum%30);
                                        tft.setTextColor(foregroundColor, backgroundColor);
                                }
                                break;

                        case 'J':
                                if (tmpnum == 2) {
                                        tft.fillScreen(ILI9340_BLACK);
                                        tft.setCursor(0, 0);
                                }
                                break;

                        case 'q':
                                analogWrite(ledPin, tmpnum);
                                break;

                        case 'n':
                                if (tmpnum == 5) {

                                } 
                                else if (tmpnum == 6) {
                                        Serial.print("row=");
                                        Serial.print(tft.cursor_x + 1, DEC);
                                        Serial.print(", col=");
                                        Serial.println(tft.cursor_y + 1, DEC);
                                }
                                break;

                        case ',':
                                // tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                col = tmpnum;
                                startImage.row = row;
                                startImage.col = col;
                                tmpnum = 0;

                                Serial.print("Starting row: ");
                                Serial.println(row);
                                Serial.print("Starting col: ");
                                Serial.println(col);

                                return 0;
                                
                        case 'i':
                                // tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                col = tmpnum;
                                endImage.row = row;
                                endImage.col = col;
                                imgsize = (endImage.row - startImage.row + 1)*(endImage.col - startImage.col + 1);    

                                Serial.print("Ending row: ");
                                Serial.println(row);
                                Serial.print("Ending col: ");
                                Serial.println(col);

                                tft.setAddrWindow(startImage.row, startImage.col, endImage.row, endImage.col);
                                tft.setdcbit();
                                tft.clearcsbit();
                                switchstate(IMGSHOW);
                                Serial.print("cat the raw data image; imgsize is ");
                                Serial.println(imgsize);
                                return 0;
                                
                        }
                        switchstate(NOTSPECIAL);
                        return 0;
                }


        default:
                switchstate(NOTSPECIAL);
                return current_char;
                break;

        }
}

uint16_t change_mColor(int opt) {
        switch (opt) {
        case 0:
                return ILI9340_BLACK;
        case 1:
                return ILI9340_RED;
        case 2:
                return ILI9340_GREEN;
        case 3:
                return ILI9340_YELLOW;
        case 4:
                return ILI9340_BLUE;
        case 5:
                return ILI9340_MAGENTA;
        case 6:
                return ILI9340_CYAN;
        case 7:
                return ILI9340_WHITE;
        case 9:
                return ILI9340_BLACK;
        } 
}


