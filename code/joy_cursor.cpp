/*
Kaden Dreger
1528632
cmput275
Weekly Assignment 1
Display and Joystick

  This file is used to create and move a cursor on the screen
  using a joystick that is wired to the arduino.
*/
// Importing libraries
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include "lcd_image.h"

// Defining some global variables
#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_VERT  A1  // should connect A1 to pin VRx
#define JOY_HORIZ A0  // should connect A0 to pin VRy
#define JOY_SEL   2

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

// the cursor position on the display
int CURSORX, CURSORY;
int MAPX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
int MAPY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;


void moveMap() {
  lcd_image_draw(&yegImage, &tft, MAPX, MAPY,
                 0, 0, DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
}


// forward declaration for redrawing the cursor
void redrawCursor(uint16_t colour);

void setup() {
  /*  The point of this function is to initialize the arduino and set the
        modes for the various pins. This function also sets up the joy stick
        and initializes the SD card. Finally this function draws the map 
        and the cursor at the middle of the screen.

    Arguments:
        This function takes in no parameters.

    Returns:
        This function returns nothing.
    */
    // This initializes the arduino.
  init();

  Serial.begin(9600);

	pinMode(JOY_SEL, INPUT_PULLUP);

	tft.begin();

	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

	tft.setRotation(3);  // Sets the proper orientation of the display

  tft.fillScreen(ILI9341_BLACK);

  // draws the centre of the Edmonton map
  // leaving the rightmost 48 columns black
  moveMap();

  // initial cursor position is the middle of the screen
  CURSORX = (DISPLAY_WIDTH - 48)/2;
  CURSORY = DISPLAY_HEIGHT/2;

  redrawCursor(ILI9341_RED);  // Draws the cursor to the screen
}

void redrawMap()  {
  /*  The point of this function is to redraw only the part of
  the map where the cursor was before it moved.

    Arguments:
        This function takes in no parameters.

    Returns:
        This function returns nothing.
  */
  // Setting the middle of the map to the top left of the screen.
  //int yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
  //int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;
  // Drawing the map at the last location of the cursor.
  lcd_image_draw(&yegImage, &tft, MAPX + (CURSORX - CURSOR_SIZE/2),
    MAPY + (CURSORY - CURSOR_SIZE/2), CURSORX - CURSOR_SIZE/2,
    CURSORY - CURSOR_SIZE/2, CURSOR_SIZE, CURSOR_SIZE);
}

void redrawCursor(uint16_t colour) {
  /*  The point of this function is to redraw the cursor at its current location
  with a given colour.

    Arguments:
        uint17_t colour: This takes in a colour for the cursor.

    Returns:
        This function returns nothing.
  */
  // Drawing the cursor
  tft.fillRect(CURSORX - CURSOR_SIZE/2, CURSORY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}

void processJoystick() {
  /*  The point of this function is to use the joystick to move the cursor without
  having the cursor leave a black trail, go off screen, not flicker will not moving,
  and have a variable movement speed depending on the joystick movement.

    Arguments:
        This function takes in no parameters.

    Returns:
        This function returns nothing.
  */
  // This takes in the analog values of the joystick
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);
  int buttonVal = digitalRead(JOY_SEL);

  // This check if the joystick has been moved
  if (yVal < JOY_CENTER - JOY_DEADZONE || yVal > JOY_CENTER + JOY_DEADZONE
    || xVal < JOY_CENTER - JOY_DEADZONE || xVal > JOY_CENTER + JOY_DEADZONE) {
    // The distance from the centre of the joystick is measured and reduced
    // by a factor of 100 so that it can be used as a variable
    int deltaX = abs(JOY_CENTER - xVal)/100 + 1;
    int deltaY = abs(JOY_CENTER - yVal)/100 + 1;


    // map updates here
    redrawMap();

    // The cursor moves at a rate proportional with how far the joystick
    // is pressed
    if (yVal < JOY_CENTER - JOY_DEADZONE) {
      CURSORY -= deltaY;  // decrease the y coordinate of the cursor
    } else if (yVal > JOY_CENTER + JOY_DEADZONE) {
      CURSORY += deltaY;
    }

    // remember the x-reading increases as we push left
    if (xVal > JOY_CENTER + JOY_DEADZONE) {
      CURSORX -= deltaX;
    } else if (xVal < JOY_CENTER - JOY_DEADZONE) {
      CURSORX += deltaX;
    }

    // The cursor is restricted to the bounds of the screen and 48
    // pixels from the right.
    CURSORX = constrain(CURSORX, 0 + (CURSOR_SIZE/2),
      DISPLAY_WIDTH - 49 - (CURSOR_SIZE/2));

    CURSORY = constrain(CURSORY, 0 + (CURSOR_SIZE/2),
      DISPLAY_HEIGHT - (CURSOR_SIZE/2));
    // Draw a red square at the new position
    redrawCursor(ILI9341_RED);
    Serial.println(CURSORX);

    if (CURSORX <= CURSOR_SIZE/2) {
      MAPX -= DISPLAY_WIDTH - 48;
      CURSORX =  DISPLAY_WIDTH - 48 - CURSOR_SIZE/2 - 1;
      moveMap();
    } else if (CURSORX >= (DISPLAY_WIDTH - 48 - CURSOR_SIZE/2 - 1)) {
      MAPX += DISPLAY_WIDTH - 48;
      CURSORX = CURSOR_SIZE/2;
      moveMap();
    }
  }


  delay(20);
}

int main() {
  /*  This is the main function from which all other functions are called.

    Arguments:
        This function takes in no parameters.

    Returns:
        This function returns nothing.
  */
	setup();

  while (true) {
    processJoystick();
  }

	Serial.end();
	return 0;
}
