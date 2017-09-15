#include "adafruit/GFX.h"
#include "adafruit/SSD1306.h"
#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2


#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

template< class T >
constexpr const T& min( const T& a, const T& b ) {
    return (b < a) ? b : a;
}

long int random(long int max) {
    return random() % max + 1;
}

Adafruit_SSD1306 display(5);


void testdrawbitmap(const uint8_t *bitmap, uint8_t w, uint8_t h) {
    uint8_t icons[NUMFLAKES][3];

    // initialize
    for (uint8_t f=0; f< NUMFLAKES; f++) {
        icons[f][XPOS] = random(display.width());
        icons[f][YPOS] = 0;
        icons[f][DELTAY] = random(5) + 1;
    }

    while (1) {
        // draw each icon
        for (uint8_t f=0; f< NUMFLAKES; f++) {
            display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, WHITE);
        }
        display.display();
        _delay_ms(200);

        // then erase it + move it
        for (uint8_t f=0; f< NUMFLAKES; f++) {
            display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, BLACK);
            // move it
            icons[f][YPOS] += icons[f][DELTAY];
            // if its gone, reinit
            if (icons[f][YPOS] > display.height()) {
                icons[f][XPOS] = random(display.width());
                icons[f][YPOS] = 0;
                icons[f][DELTAY] = random(5) + 1;
            }
        }
    }
}


void testdrawchar(void) {
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);

    for (uint8_t i=0; i < 168; i++) {
        if (i == '\n') continue;
        display.write(i);
        if ((i > 0) && (i % 21 == 0))
        display.println();
    }
    display.display();
    _delay_ms(1);
}

void testdrawcircle(void) {
    for (int16_t i=0; i<display.height(); i+=2) {
        display.drawCircle(display.width()/2, display.height()/2, i, WHITE);
        display.display();
        _delay_ms(1);
    }
}

void testfillrect(void) {
    uint8_t color = 1;
    for (int16_t i=0; i<display.height()/2; i+=3) {
        // alternate colors
        display.fillRect(i, i, display.width()-i*2, display.height()-i*2, color%2);
        display.display();
        _delay_ms(1);
        color++;
    }
}

void testdrawtriangle(void) {
    for (int16_t i=0; i<min(display.width(),display.height())/2; i+=5) {
        display.drawTriangle(display.width()/2, display.height()/2-i,
                display.width()/2-i, display.height()/2+i,
                display.width()/2+i, display.height()/2+i, WHITE);
        display.display();
        _delay_ms(1);
    }
}

void testfilltriangle(void) {
    uint8_t color = WHITE;
    for (int16_t i=min(display.width(),display.height())/2; i>0; i-=5) {
        display.fillTriangle(display.width()/2, display.height()/2-i,
                display.width()/2-i, display.height()/2+i,
                display.width()/2+i, display.height()/2+i, WHITE);
        if (color == WHITE)
            color = BLACK;
        else
            color = WHITE;
        display.display();
        _delay_ms(1);
    }
}

void testdrawroundrect(void) {
    for (int16_t i=0; i<display.height()/2-2; i+=2) {
        display.drawRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, WHITE);
        display.display();
        _delay_ms(1);
    }
}

void testfillroundrect(void) {
    uint8_t color = WHITE;
    for (int16_t i=0; i<display.height()/2-2; i+=2) {
        display.fillRoundRect(i, i, display.width()-2*i, display.height()-2*i, display.height()/4, color);
        if (color == WHITE)
            color = BLACK;
        else
            color = WHITE;
        display.display();
        _delay_ms(1);
    }
}

void testdrawrect(void) {
    for (int16_t i=0; i<display.height()/2; i+=2) {
        display.drawRect(i, i, display.width()-2*i, display.height()-2*i, WHITE);
        display.display();
        _delay_ms(1);
    }
}

void testdrawline() {
    for (int16_t i=0; i<display.width(); i+=4) {
        display.drawLine(0, 0, i, display.height()-1, WHITE);
        display.display();
        _delay_ms(1);
    }
    for (int16_t i=0; i<display.height(); i+=4) {
        display.drawLine(0, 0, display.width()-1, i, WHITE);
        display.display();
        _delay_ms(1);
    }
    _delay_ms(250);

    display.clearDisplay();
    for (int16_t i=0; i<display.width(); i+=4) {
        display.drawLine(0, display.height()-1, i, 0, WHITE);
        display.display();
        _delay_ms(1);
    }
    for (int16_t i=display.height()-1; i>=0; i-=4) {
        display.drawLine(0, display.height()-1, display.width()-1, i, WHITE);
        display.display();
        _delay_ms(1);
    }
    _delay_ms(250);

    display.clearDisplay();
    for (int16_t i=display.width()-1; i>=0; i-=4) {
        display.drawLine(display.width()-1, display.height()-1, i, 0, WHITE);
        display.display();
        _delay_ms(1);
    }
    for (int16_t i=display.height()-1; i>=0; i-=4) {
        display.drawLine(display.width()-1, display.height()-1, 0, i, WHITE);
        display.display();
        _delay_ms(1);
    }
    _delay_ms(250);

    display.clearDisplay();
    for (int16_t i=0; i<display.height(); i+=4) {
        display.drawLine(display.width()-1, 0, 0, i, WHITE);
        display.display();
        _delay_ms(1);
    }
    for (int16_t i=0; i<display.width(); i+=4) {
        display.drawLine(display.width()-1, 0, i, display.height()-1, WHITE);
        display.display();
        _delay_ms(1);
    }
    _delay_ms(250);
}

void testscrolltext(void) {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10,0);
    display.clearDisplay();
    display.println("scroll");
    display.display();
    _delay_ms(1);

    display.startscrollright(0x00, 0x0F);
    _delay_ms(2000);
    display.stopscroll();
    _delay_ms(1000);
    display.startscrollleft(0x00, 0x0F);
    _delay_ms(2000);
    display.stopscroll();
    _delay_ms(1000);
    display.startscrolldiagright(0x00, 0x07);
    _delay_ms(2000);
    display.startscrolldiagleft(0x00, 0x07);
    _delay_ms(2000);
    display.stopscroll();
}

void run_example() {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3D); // initialize with the I2C addr 0x3D (for the 128x64)
    display.display();


    _delay_ms(2000);


    // Clear the buffer.
    display.clearDisplay();

    // draw a single pixel
    display.drawPixel(10, 10, WHITE);
    // Show the display buffer on the hardware.
    // NOTE: You _must_ call display after making any drawing commands
    // to make them visible on the display hardware!
    display.display();
    _delay_ms(1000);
    display.clearDisplay();

    // draw many lines
    testdrawline();
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    // draw rectangles
    testdrawrect();
    display.display();
    _delay_ms(2000);
    //   while(1);
    display.clearDisplay();

    // draw multiple rectangles
    testfillrect();
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    // draw mulitple circles
    testdrawcircle();
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    // draw a white circle, 10 pixel radius
    display.fillCircle(display.width()/2, display.height()/2, 10, WHITE);
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    testdrawroundrect();
    _delay_ms(2000);
    display.clearDisplay();

    testfillroundrect();
    _delay_ms(2000);
    display.clearDisplay();

    testdrawtriangle();
    _delay_ms(2000);
    display.clearDisplay();

    testfilltriangle();
    _delay_ms(2000);
    display.clearDisplay();

    // draw the first ~12 characters in the font
    testdrawchar();
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    // draw scrolling text
    testscrolltext();
    _delay_ms(2000);
    display.clearDisplay();

    // text display tests
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
    display.println("Hello, world!");
    display.setTextColor(BLACK, WHITE); // 'inverted' text
    display.println(3.141592);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.print("0x"); display.println(0xDEADBEEF, HEX);
    display.display();
    _delay_ms(2000);
    display.clearDisplay();

    // miniature bitmap display
    display.drawBitmap(30, 16,  logo16_glcd_bmp, 16, 16, 1);
    display.display();
    _delay_ms(1);

    // invert the display
    display.invertDisplay(true);
    _delay_ms(1000);
    display.invertDisplay(false);
    _delay_ms(1000);
    display.clearDisplay();

    // draw a bitmap icon and 'animate' movement
    testdrawbitmap(logo16_glcd_bmp, LOGO16_GLCD_HEIGHT, LOGO16_GLCD_WIDTH);
}



int main(void) {
    sei();

    run_example();
}
