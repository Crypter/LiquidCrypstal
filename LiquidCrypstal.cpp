#include "LiquidCrypstal.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
  #else
  #include "WProgram.h"
  #endif

// When the display powers up, it is configured as follows:
//
// 1. Display clear
// 2. Function set:
//    DL = 1; 8-bit interface data
//    N = 0; 1-line display
//    F = 0; 5x8 dot character font
// 3. Display on/off control:
//    D = 0; Display off
//    C = 0; Cursor off
//    B = 0; Blinking off
// 4. Entry mode set:
//    I/D = 1; Increment by 1
//    S = 0; No shift
//
// Note, however, that resetting the Arduino doesn't reset the LCD, so we
// can't assume that its in that state when a sketch starts (and the
// LiquidCrypstal constructor is called).

LiquidCrypstal::LiquidCrypstal(uint8_t data, uint8_t clock, uint8_t latch) {
  _data_pin = data;
  _clock_pin = clock;
  _latch_pin = latch;

  pinMode(_data_pin, OUTPUT);
  pinMode(_clock_pin, OUTPUT);
  pinMode(_latch_pin, OUTPUT);
  _bit_map=0x00;
  init(2, 1, 3, 4, 5, 6, 7); //set pin mappings on the shift register
}

void LiquidCrypstal::init( uint8_t led, uint8_t rs, uint8_t enable, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7) {
  _led_pin = led;
  _rs_pin = rs;
  _enable_pin = enable;

  _data_pins[0] = d4;
  _data_pins[1] = d5;
  _data_pins[2] = d6;
  _data_pins[3] = d7;

  _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
  _bit_map=0x00;

  begin(16, 1);

}

void LiquidCrypstal::begin(uint8_t cols, uint8_t lines, uint8_t dotsize) {
  if (lines > 1) {
    _displayfunction |= LCD_2LINE;
  }
  _numlines = lines;
  _currline = 0;

  // for some 1 line displays you can select a 10 pixel high font
  if ((dotsize != 0) && (lines == 1)) {
    _displayfunction |= LCD_5x10DOTS;
  }

  // SEE PAGE 45/46 FOR INITIALIZATION SPECIFICATION!
  // according to datasheet, we need at least 40ms after power rises above 2.7V
  // before sending commands. Arduino can turn on way befer 4.5V so we'll wait 50
  delay(100);
  // Now we pull both RS and R/W low to begin commands
  _bit_map=0x00;
  bitWrite(_bit_map, _rs_pin, 1);
  bitWrite(_bit_map, _enable_pin, 1);
  shift_out();

  bitWrite(_bit_map, _rs_pin, 0);
  bitWrite(_bit_map, _enable_pin, 0);
  shift_out();

  //put the LCD into 4 bit or 8 bit mode
  if (!(_displayfunction & LCD_8BITMODE)) {
    // this is according to the hitachi HD44780 datasheet
    // figure 24, pg 46
    // we start in 8bit mode, try to set 4 bit mode
    write4bits(0x03);
    delay(5); // wait min 4.1ms

    // second try
    write4bits(0x03);
    delay(5); // wait min 4.1ms

    // third go!
    write4bits(0x03);
    delay(1);

    // finally, set to 4-bit interface
    write4bits(0x02);
  } else {
    // this is according to the hitachi HD44780 datasheet
    // page 45 figure 23

    // Send function set command sequence
    command(LCD_FUNCTIONSET | _displayfunction);
    delay(5); // wait more than 4.1ms

    // second try
    command(LCD_FUNCTIONSET | _displayfunction);
    delay(1);

    // third go
    command(LCD_FUNCTIONSET | _displayfunction);
  }

  // finally, set # lines, font size, etc.
  command(LCD_FUNCTIONSET | _displayfunction);

  // turn the display on with no cursor or blinking default
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  display();

  // clear it off
  clear();

  // Initialize to default text direction (for romance languages)
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  // set the entry mode
  command(LCD_ENTRYMODESET | _displaymode);

}

/********** high level commands, for the user! */
void LiquidCrypstal::clear() {
  command(LCD_CLEARDISPLAY); // clear display, set cursor position to zero
  delay(2); // this command takes a long time!
}

void LiquidCrypstal::home() {
  command(LCD_RETURNHOME); // set cursor position to zero
  delay(2); // this command takes a long time!
}

void LiquidCrypstal::setCursor(uint8_t col, uint8_t row) {
  int row_offsets[] = {
    0x00,
    0x40,
    0x14,
    0x54
  };
  if (row > _numlines) {
    row = _numlines - 1; // we count rows starting w/0
  }

  command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

// Turn the display on/off (quickly)
void LiquidCrypstal::noDisplay() {
  _displaycontrol &= ~LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrypstal::display() {
  _displaycontrol |= LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turns the underline cursor on/off
void LiquidCrypstal::noCursor() {
  _displaycontrol &= ~LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrypstal::cursor() {
  _displaycontrol |= LCD_CURSORON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// Turn on and off the blinking cursor
void LiquidCrypstal::noBlink() {
  _displaycontrol &= ~LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}
void LiquidCrypstal::blink() {
  _displaycontrol |= LCD_BLINKON;
  command(LCD_DISPLAYCONTROL | _displaycontrol);
}

// These commands scroll the display without changing the RAM
void LiquidCrypstal::scrollDisplayLeft(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}
void LiquidCrypstal::scrollDisplayRight(void) {
  command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

// This is for text that flows Left to Right
void LiquidCrypstal::leftToRight(void) {
  _displaymode |= LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This is for text that flows Right to Left
void LiquidCrypstal::rightToLeft(void) {
  _displaymode &= ~LCD_ENTRYLEFT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'right justify' text from the cursor
void LiquidCrypstal::autoscroll(void) {
  _displaymode |= LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// This will 'left justify' text from the cursor
void LiquidCrypstal::noAutoscroll(void) {
  _displaymode &= ~LCD_ENTRYSHIFTINCREMENT;
  command(LCD_ENTRYMODESET | _displaymode);
}

// Allows us to fill the first 8 CGRAM locations
// with custom characters
void LiquidCrypstal::createChar(uint8_t location, uint8_t charmap[]) {
  location &= 0x7; // we only have 8 locations 0-7
  command(LCD_SETCGRAMADDR | (location << 3));
  for (int i = 0; i < 8; i++) {
    write(charmap[i]);
  }
}

void LiquidCrypstal::ledOn()
{
    bitWrite(_bit_map, _led_pin, 0x00);
    shift_out();
}

void LiquidCrypstal::ledOff()
{
    bitWrite(_bit_map, _led_pin, 0x01);
    shift_out();
}


/*********** mid level commands, for sending data/cmds */

inline void LiquidCrypstal::command(uint8_t value) {
  send(value, LOW);
}

inline size_t LiquidCrypstal::write(uint8_t value) {
  send(value, HIGH);
  return 1; // assume sucess
}

/************ low level data pushing commands **********/

// write either command or data, with automatic 4/8-bit selection
void LiquidCrypstal::send(uint8_t value, uint8_t mode) {
  bitWrite(_bit_map, _rs_pin, mode);
  shift_out();
  write4bits(value >> 4);
  write4bits(value);
}

void LiquidCrypstal::pulseEnable(void) {
  bitWrite(_bit_map, _enable_pin, LOW);
  shift_out();
  delayMicroseconds(1);
  bitWrite(_bit_map, _enable_pin, HIGH);
  shift_out();
  delayMicroseconds(1); // enable pulse must be >450ns
  bitWrite(_bit_map, _enable_pin, LOW);
  shift_out();
  delayMicroseconds(50); // commands need > 37us to settle
}

void LiquidCrypstal::shift_out() {
  for (int i=0; i<8; i++) {
    digitalWrite(_data_pin, (_bit_map>>(7-i))&0x01);
    delayMicroseconds(1);
    digitalWrite(_clock_pin, HIGH);
    delayMicroseconds(1);
    digitalWrite(_clock_pin, LOW);
    delayMicroseconds(1);
  }
  digitalWrite(_latch_pin, HIGH);
  delayMicroseconds(1);
  digitalWrite(_latch_pin, LOW);
  delayMicroseconds(1);
  digitalWrite(_data_pin, LOW);
}

void LiquidCrypstal::write4bits(uint8_t value) {
  for (int i=0; i<4; i++){
    bitWrite(_bit_map, _data_pins[i], (value>>i)&0x01);
  }
  shift_out();
  pulseEnable();
}
