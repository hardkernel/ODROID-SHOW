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
#include <Wire.h>
#include <Adafruit_ILI9340.h>
#include <Adafruit_GFX.h>
#include "TimerOne.h"

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

#define SERIAL_RX_BUFFER_SIZE 64

// button 0 events
#define BTN0_PRESSED "0P"
#define BTN0_RELEASED "0R"

// button 1 events
#define BTN1_PRESSED "1P"
#define BTN1_RELEASED "1R"

// button 2 events
#define BTN2_PRESSED "2P"
#define BTN2_RELEASED "2R"

volatile uint8_t _rx_buffer_head = 0;
volatile uint8_t _rx_buffer_tail = 0;
uint8_t _rx_buffer[SERIAL_RX_BUFFER_SIZE];
uint8_t length = 0;
uint8_t lengthFlag = 0;
uint8_t ackFlag = 0;
uint8_t readFlag = 0;
uint8_t strcnt = 0;


const char version[] = "v2.0";

typedef struct cursor {
        uint32_t row;
        uint32_t col;
}
cursor;

cursor cursor_sav = { 
        0, 0 };
cursor startImage, endImage;

uint32_t imgDelay = 0;
uint32_t serialDelay = 0;
uint8_t c;
uint8_t ch;

uint8_t current_state = NOTSPECIAL;
uint8_t previous_state = NOTSPECIAL;
uint32_t tmpnum;


uint16_t num, row, col;
uint16_t bottom_edge0 = BOTTOM_EDGE240;
uint16_t right_edge0  = RIGHT_EDGE320;

uint8_t pwm = 255;
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

void setup()
{
        Serial.begin(500000);
        Serial.println("Welcome to the ODROID-SHOW");

        tft.begin();
        // initialize the digital pin as an output for LED Backlibht
        initPins();

        tft.setRotation(rotation);
        tft.setTextSize(textSize);
        tft.setCursor(50, 50);
        tft.print("Hello ODROID-SHOW!");
        tft.setCursor(250, 200);
        tft.print(version);

        delay(1000);
        tft.fillScreen(backgroundColor);
        tft.setCursor(0, 0);
        
        Timer1.initialize(20000);
        Timer1.attachInterrupt(timerCallback);
}
void initPins()
{
	pinMode(ledPin, OUTPUT);
	pinMode(3, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(7, INPUT);
	pinMode(A0, INPUT);
	pinMode(A1, INPUT);

	analogWrite(ledPin, pwm);
}

void serialEvent() {
	if (current_state != IMGSHOW) {
		while (Serial.available()) {
			serialDelay = 0;
			if (readFlag) {
				bufferWrite(Serial.read());
				strcnt++;
				if (strcnt == length)
					readFlag = 0;
			} else if (ackFlag) {
				length = Serial.read() - 48;
				lengthFlag = 1;
				ackFlag = 0;
			} else if ((ch = Serial.read()) == 006) {
				ackFlag = 1;
				strcnt = 0;
			} else {
				bufferWrite(ch);
			}
		}
	}
}

void timerCallback()
{
        imgDelay++;
	serialDelay++;
        readBtn();

	if (serialDelay > 20) {
		lengthFlag = 0;
		readFlag = 0;
		ackFlag = 0;
	}
	if (lengthFlag) {
		if (getBufferSize() > (length + 2)) {
			readFlag = 1;
			Serial.print(6);
			lengthFlag = 0;
		}
	}
}

unsigned char btn0Presses = 0;
unsigned char btn0Releases = 0;
unsigned char btn1Presses = 0;
unsigned char btn1Releases = 0;
unsigned char btn2Presses = 0;
unsigned char btn2Releases = 0;

unsigned char btn0Pushed = 0;
unsigned char btn1Pushed = 0;
unsigned char btn2Pushed = 0;

void readBtn()
{
        if (!digitalRead(A1) && (btn2Presses == 0)) {
                btn2Presses = 1;
                btn2Releases = 0;
                btn2Pushed = 1;
                digitalWrite(6, LOW);
                Serial.print(BTN2_PRESSED);
        }

        if (digitalRead(A1) && (btn2Releases == 0)) {
                btn2Releases = 1;
                btn2Presses = 0;
                btn2Pushed = 0;
                digitalWrite(6, HIGH);
                Serial.print(BTN2_RELEASED);
        }

        if (!digitalRead(7) && (btn0Presses == 0)) {
                btn0Presses = 1;
                btn0Releases = 0;
                btn0Pushed = 1;
                if (pwm > 225)
                        pwm = 255;
                else
                        pwm += 30;
                analogWrite(ledPin, pwm);
                digitalWrite(3, LOW);
                Serial.print(BTN0_PRESSED);
        }

        if (digitalRead(7) && (btn0Releases == 0)) {
                btn0Releases = 1;
                btn0Presses = 0;
                btn0Pushed = 0;
                digitalWrite(3, HIGH);
                Serial.print(BTN0_RELEASED);
        }

        if (!digitalRead(A0) && (btn1Presses == 0)) {
                btn1Presses = 1;
                btn1Releases = 0;
                btn1Pushed = 1;
                if (pwm < 30)
                        pwm = 0;
                else
                        pwm -= 30;
                analogWrite(ledPin, pwm);
                digitalWrite(4, LOW);
                Serial.print(BTN1_PRESSED);
        }

        if (digitalRead(A0) && (btn1Releases == 0)) {
                btn1Releases = 1;
                btn1Presses = 0;
                btn1Pushed = 0;
                digitalWrite(4, HIGH);
                Serial.print(BTN1_RELEASED);
        }
}

void loop(void)
{
        if (current_state == IMGSHOW) {
                if (Serial.available() > 1) {
                        rgb565lo = Serial.read();
                        rgb565hi = Serial.read();
                        tft.spiwrite(rgb565hi);
                        tft.spiwrite(rgb565lo);
                        cntenable = 1;
                        sizecnt++;
                        imgDelay = 0;
                }
                else if (cntenable == 1) {
                        if ((sizecnt == imgsize) || (imgDelay > 100)) {
                                cntenable = 0;
                                sizecnt = 0;
                                tft.setcsbit();
                                switchstate(NOTSPECIAL);
                        }
                }
        }
        else {
                if (!(_rx_buffer_head == _rx_buffer_tail)) {
                        c = bufferRead();
			if (parsechar(c) > 0)
                        	tft.print((char)c);
                }
        }
}

uint8_t getReadAvailable(void)
{
	if (_rx_buffer_head >= _rx_buffer_tail)
		return _rx_buffer_head - _rx_buffer_tail;
	return SERIAL_RX_BUFFER_SIZE - _rx_buffer_tail + _rx_buffer_head;
}

uint8_t getBufferSize(void)
{
	if (_rx_buffer_head >= _rx_buffer_tail)
		return SERIAL_RX_BUFFER_SIZE - _rx_buffer_head + _rx_buffer_tail;
	return _rx_buffer_tail - _rx_buffer_head;
}

uint8_t bufferRead(void)
{
	uint8_t c = _rx_buffer[_rx_buffer_tail];
	_rx_buffer_tail = (_rx_buffer_tail + 1) % SERIAL_RX_BUFFER_SIZE;
	return c;
}

void bufferWrite(uint8_t c)
{
	_rx_buffer[_rx_buffer_head] = c;
	_rx_buffer_head = (_rx_buffer_head + 1) % SERIAL_RX_BUFFER_SIZE;
}

void switchstate(int newstate)
{
        previous_state = current_state;
        current_state = newstate;

}

void cursorDown()
{
        if (tft.cursor_y < bottom_edge0) {
                tft.cursor_y += textSize*8;
                tft.setCursor(tft.cursor_x, tft.cursor_y);
        }

}

int parsechar(unsigned char current_char)
{
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
		else if (current_char == 006) {		// ACK
			Serial.write(6);
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
                                //tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
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
                                        backgroundColor = change_mColor(tmpnum%40);
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
                                //tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                col = tmpnum;
                                startImage.row = row;
                                startImage.col = col;
                                tmpnum = 0;

                                return 0;
                                
                        case 'i':
                                //tmpnum = (tmpnum > 0) ? tmpnum - 1 : 0;
                                col = tmpnum;
                                endImage.row = row;
                                endImage.col = col;
                                imgsize = (endImage.row - startImage.row)*(endImage.col - startImage.col);    

                                tft.setAddrWindow(startImage.row, startImage.col, endImage.row-1, endImage.col-1);
                                tft.setdcbit();
                                tft.clearcsbit();
                                switchstate(IMGSHOW);
                                Serial.println("cat the raw data image");
                                return 0;

                        case 'X': // draw a point using current (text) foreground colour; "X marks the spot"
                                row = (row > right_edge0)     ? right_edge0  : row;
                                col = (tmpnum > bottom_edge0) ? bottom_edge0 : tmpnum;
                                Serial.print("Plotting row/col: ");
                                Serial.print(row);
                                Serial.print("/");
                                Serial.println(col);
                                tft.drawPixel(row, col, foregroundColor);
                                break;
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

uint16_t change_mColor(int opt)
{
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
