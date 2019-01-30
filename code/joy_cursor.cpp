// Importing libraries
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include "lcd_image.h"
#include <TouchScreen.h>

// Defining some global variables
#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Sd2Card card;

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_VERT  A1  // should connect A1 to pin VRx
#define JOY_HORIZ A0  // should connect A0 to pin VRy
#define JOY_SEL   2

#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

// the cursor position on the display
int CURSORX = (DISPLAY_WIDTH - 48)/2;
int CURSORY = DISPLAY_HEIGHT/2;
int MAPX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
int MAPY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;



// forward declaration for redrawing the cursor
void redrawCursor(uint16_t colour);
void moveMap();

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

	Serial.println("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	} else {
        Serial.println("OK!");
    }
    Serial.println("Initializing SPI communication for raw reads...");
    if (!card.init(SPI_HALF_SPEED, SD_CS)) {
        Serial.println("failed! Is the card inserted properly?");
    while (true) {}
    } else {
    Serial.println("OK!");
    Serial.println("---------------------------------------------------------");
    }
	//Serial.println("OK!");

	tft.setRotation(3);  // Sets the proper orientation of the display

  tft.fillScreen(ILI9341_BLACK);

  // draws the centre of the Edmonton map
  // leaving the rightmost 48 columns black
  moveMap();

  redrawCursor(ILI9341_RED);  // Draws the cursor to the screen
}

struct restaurant {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The restaurant struct is responsible for holding all the data for the restaur-
ants such as latitude (lat), longitude (lon), name, and their rating.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
  int32_t lat;
  int32_t lon;
  uint8_t rating;  // from 0 to 10
  char name[55];
};


void getRestaurant(int restIndex, restaurant* restPtr) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The getRestaurantFast function takes in the paramaters:
        restIndex: the current index of the restaurant block
        restPtr  : a pointer to the restaurant block

It does not return any parameters.

This function is responsible for raw reading from the SD card in an efficient
manner, only reading from the SD card when we have exceeded a restIndex of 8,
indicating we have read in all 8 restaurants from the current block. 
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    /* The fast implementation consists of a simple if statement checking if
    the restIndex is has exceeded a "multiple of 8" meaning the pointer is now
    past the current block we are reading.
    */
    //if ((restIndex % 8) == 0) {
        uint32_t blockNum = REST_START_BLOCK + restIndex/8;
        restaurant restBlock[8];  // creating an array of structs

        while (!card.readBlock(blockNum, (uint8_t*) restBlock)) {  // raw read
        Serial.println("Read block failed, trying again.");  // from the SD card
        }

        *restPtr = restBlock[restIndex % 8];
    //ssss}
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

void moveMap() {
  lcd_image_draw(&yegImage, &tft, MAPX, MAPY,
                 0, 0, DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
  CURSORY = DISPLAY_HEIGHT/2;
  CURSORX =  (DISPLAY_WIDTH - 48)/2;
  //redrawCursor(ILI9341_RED);
}


void checkMap() {
  MAPX = constrain(MAPX, 0,
      YEG_SIZE - DISPLAY_WIDTH - 48);

  MAPY = constrain(MAPY, 0,
      YEG_SIZE - DISPLAY_HEIGHT);
}

struct  RestDist {
uint16_t  index; // index  of  restaurant  from 0 to  NUM_RESTAURANTS -1
uint16_t  dist;   //  Manhatten  distance  to  cursor  position
};
RestDist restDist[NUM_RESTAURANTS];

void fetchRests() {
    //tft.fillScreen (0);
    tft.setCursor(0, 0); //  where  the  characters  will be  displayed
    tft.setTextWrap(false);
    int selectedRest = 0; //  which  restaurant  is  selected?
    Serial.println("Restaurants read in...");
    for (int16_t i = 0; i < 30; i++) {
        /*Only reads the first 30 restaurants... using the slow read method,
        fast read gives copies of 8 of the same restaurant, not sure if it reads
        all of them or not...*/
        restaurant r;
        getRestaurant(i, &r);
        Serial.println(r.name);
        Serial.print("latitude: ");
        Serial.print(r.lat);
        Serial.print(" longitude: ");
        Serial.print(r.lon);
        Serial.println();
        Serial.println();
        if (i !=  selectedRest) { // not  highlighted
            //  white  characters  on  black  background
            tft.setTextColor (0xFFFF , 0x0000);
        } else { //  highlighted
            //  black  characters  on  white  background
            tft.setTextColor (0x0000 , 0xFFFF);
        }
        //Serial.println(restDist[i].index);
        //Serial.println(restDist[i].index);

        tft.print(r.name);
        tft.print("\n");
    }
    tft.print("\n");

}


void restaurantList() {
    tft.fillScreen (0);
    int joyClick;
    //tft.fillScreen(ILI9341_BLACK);
    delay(500);  // to allow the stick to become unpressed
    fetchRests();
    while (true) {
        joyClick = digitalRead(JOY_SEL);
        if (not joyClick) {
            break;
        }
        //fetchRests(0);
    }
    moveMap();
    redrawCursor(ILI9341_RED);
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
  int joyClick = digitalRead(JOY_SEL);

  //Serial.println(joyClick);

  if (not joyClick) {
    // run a function (function has to be in a while loop)
    restaurantList();
  }
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
    //Serial.println(CURSORX);

    if (CURSORX <= CURSOR_SIZE/2 && MAPX != 0) {
      MAPX -= DISPLAY_WIDTH - 48;
      checkMap();
      moveMap();
      redrawCursor(ILI9341_RED);
    } else if (CURSORX >= (DISPLAY_WIDTH - 48 - CURSOR_SIZE/2 - 1) &&
               MAPX != YEG_SIZE - DISPLAY_WIDTH - 48) {
      MAPX += DISPLAY_WIDTH - 48;
      checkMap();
      moveMap();
      redrawCursor(ILI9341_RED);
    } else if (CURSORY <= CURSOR_SIZE/2 && MAPY != 0) {
      MAPY -= DISPLAY_HEIGHT;
      checkMap();
      moveMap();
      redrawCursor(ILI9341_RED);
    } else if (CURSORY >= (DISPLAY_HEIGHT - CURSOR_SIZE/2) &&
               MAPY != YEG_SIZE - DISPLAY_HEIGHT) {
      MAPY += DISPLAY_HEIGHT;
      checkMap();
      moveMap();
      redrawCursor(ILI9341_RED);
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
