/*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
Assignment 1 Part 1: Restaurants Finder
Names: Rutvik Patel, Kaden Dreger
ID: 1530012, 1528632
CCID: rutvik, kaden
CMPUT 275 Winter 2018

This program demonstrates how to raw read restaurant information from the
formatted SD card. It introduces a simple GUI on the tft display and makes use
of the touchscreen to run specific functions. It features an explorable map of
Edmonton via the joystick, and also implements the ability to click the joystick
to fetches and sorts the 30 closest restaurants to the cursor location. You can
then select a restaurant and the cursor will snap to the location of the
restaurant. It also features the ability to tap the screen to bring up small
rectangles on the display to indicate the location of the restaurants on your
screen.

NOTE: This program requires a correctly formatted
SD card with the neccessary files to run the program.
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*/

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

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define YEG_SIZE 2048

#define JOY_VERT  A1  // should connect A1 to pin VRx
#define JOY_HORIZ A0  // should connect A0 to pin VRy
#define JOY_SEL   2

#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin

#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940
#define MINPRESSURE   10
#define MAXPRESSURE 1000

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

#define  MAP_WIDTH  2048
#define  MAP_HEIGHT  2048
#define  LAT_NORTH  5361858l
#define  LAT_SOUTH  5340953l
#define  LON_WEST  -11368652l
#define  LON_EAST  -11333496l

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Sd2Card card;

// the cursor position on the display
int CURSORX = (DISPLAY_WIDTH - 48)/2;
int CURSORY = DISPLAY_HEIGHT/2;
int MAPX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
int MAPY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;

uint32_t nowBlock;
int squareSize = 8;

// The initial selected restraunt
uint16_t selectedRest = 0;

// forward declaration for redrawing the cursor and moving map.
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
        Serial.println("------------------------------------------------------");
    }

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
} restBlock[8];
restaurant r;


/* The following two functions are identical to the ones provided in the
assignment description. These functions take in the longitude and latitude
(respectively) and return the mapped location onto the screen*/
int16_t  lon_to_x(int32_t  lon) {
    return  map(lon , LON_WEST , LON_EAST , 0, MAP_WIDTH);
}


int16_t  lat_to_y(int32_t  lat) {
    return  map(lat , LAT_NORTH , LAT_SOUTH , 0, MAP_HEIGHT);
}


void getRestaurant(int restIndex, restaurant* restPtr) {
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
The getRestaurant function takes in the paramaters:
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
    uint32_t blockNum = REST_START_BLOCK + restIndex/8;
    if (nowBlock == blockNum) {
        *restPtr = restBlock[restIndex % 8];
    } else {
        nowBlock = blockNum;  //set the current block to the blockNum

        while (!card.readBlock(blockNum, (uint8_t*) restBlock)) {  // raw read
        Serial.println("Read block failed, trying again.");  // from the SD card
        }

        *restPtr = restBlock[restIndex % 8];
    }
}



void redrawMap()  {
  /*  The point of this function is to redraw only the part of
  the map where the cursor was before it moved.

    Arguments:
        This function takes in no parameters.

    Returns:
        This function returns nothing.
  */
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
    // NOT WORKING YET
    lcd_image_draw(&yegImage, &tft, MAPX, MAPY,
                 0, 0, DISPLAY_WIDTH - 48, DISPLAY_HEIGHT);
    Serial.println(CURSORX);
    if ((CURSORX > YEG_SIZE- DISPLAY_WIDTH/2 || CURSORX < 0 + DISPLAY_WIDTH/2) &&
        (CURSORY > YEG_SIZE - DISPLAY_HEIGHT/2 || CURSORY < 0 + DISPLAY_HEIGHT/2)) {
        CURSORY = constrain(CURSORY, 0 + CURSOR_SIZE/2,
            DISPLAY_HEIGHT - CURSOR_SIZE/2);
        CURSORX = constrain(CURSORX, 0 + CURSOR_SIZE/2,
            DISPLAY_WIDTH-48 - CURSOR_SIZE/2);
    } else if (CURSORX > YEG_SIZE- DISPLAY_WIDTH/2 || CURSORX < 0 + DISPLAY_WIDTH/2) {
        CURSORX = constrain(CURSORX, 0 + CURSOR_SIZE/2,
            DISPLAY_WIDTH-48 - CURSOR_SIZE/2);
        CURSORY = DISPLAY_HEIGHT/2;
    } else if (CURSORY > YEG_SIZE - DISPLAY_HEIGHT/2 || CURSORY < 0 + DISPLAY_HEIGHT/2) {
        CURSORY = constrain(CURSORY, 0 + CURSOR_SIZE/2,
            DISPLAY_HEIGHT - CURSOR_SIZE/2);
        CURSORX = (DISPLAY_WIDTH - 48)/2;
    }
    else {
        CURSORY = DISPLAY_HEIGHT/2;
        CURSORX = (DISPLAY_WIDTH - 48)/2;
    }
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


void insertionSort(RestDist *array) {
    int i = 1;
    int j;
    RestDist temp;
    while (i < NUM_RESTAURANTS) {
        j = i;
        while (j > 0 && array[j - 1].dist > array[j].dist) {
            //find the citation for the temp swap...
            temp = array[j];
            array[j] = array[j - 1];
            array[j-1] = temp;
            j--;
        }
        i++;
    } 

}

void fetchRests() {
    //tft.fillScreen (0);
    tft.setCursor(0, 0); //  where  the  characters  will be  displayed
    tft.setTextWrap(false);
    int selectedRest = 0;
    Serial.println("Restaurants read in...");
    for (int16_t i = 0; i < NUM_RESTAURANTS; i++) {
      restDist[i].index = i;
        /*Only reads the first 30 restaurants... using the slow read method,
        fast read gives copies of 8 of the same restaurant, not sure if it reads
        all of them or not...*/
        getRestaurant(i, &r);
        int16_t restY = lat_to_y(r.lat);
        int16_t restX = lon_to_x(r.lon);
        restDist[i].dist = abs((MAPX + CURSORX)-restX) + abs((MAPY + CURSORY) - restY);
    }
    // Insertion sort
    insertionSort(&restDist[0]);

    for (int16_t j = 0; j < 30; j++) {
        getRestaurant(restDist[j].index, &r);
        if (j !=  selectedRest) { // not  highlighted
            //  white  characters  on  black  background
            tft.setTextColor (0xFFFF , 0x0000);
        } else { //  highlighted
            //  black  characters  on  white  background
            tft.setTextColor (0x0000 , 0xFFFF);
        }
        tft.print(r.name);
        tft.print("\n");

        //Serial.println(r.name);
        Serial.print("latitude: ");
        Serial.print(r.lat);
        Serial.print(" longitude: ");
        Serial.print(r.lon);
        Serial.println();
        Serial.println();
    }
    tft.print("\n");
}

void drawCircles() {
    for (int16_t i = 0; i < NUM_RESTAURANTS; i++) {
      restDist[i].index = i;
        /*Only reads the first 30 restaurants... using the slow read method,
        fast read gives copies of 8 of the same restaurant, not sure if it reads
        all of them or not...*/
        getRestaurant(i, &r);
        int16_t restY = lat_to_y(r.lat);
        int16_t restX = lon_to_x(r.lon);
        // remember to subtract shapesize/2 from our boundaries...
        if ((restX > MAPX + squareSize && restX < MAPX + DISPLAY_WIDTH - 48 -
             squareSize) && (restY > MAPY + squareSize && restY < MAPY +
              DISPLAY_HEIGHT - squareSize)) {
            tft.fillRect(restX - MAPX, restY - MAPY, squareSize, squareSize, ILI9341_BLUE);
        }
    }
}
// This is from displayNames
// draws the name at the given index to the display
// assumes the text size is already 2, that text
// is not wrapping, and 0 <= index < number of names in the list
void drawName(uint16_t index) {
    restaurant rest;
    getRestaurant(restDist[index].index, &rest);
  tft.setCursor(0, index*8);
  tft.fillRect(0, index*8, DISPLAY_WIDTH, 8, tft.color565(0, 0, 0));
  if (index == selectedRest) {
    tft.setTextColor(ILI9341_BLACK, ILI9341_WHITE);
  }
  else {
    tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  }
  tft.println(rest.name);
}


void restaurantList() {
    tft.fillScreen (0);
    int joyClick, xVal, yVal;
    delay(100);  // to allow the stick to become unpressed
    fetchRests();
    selectedRest = 0;
    while (true) {
      xVal = analogRead(JOY_HORIZ);
      yVal = analogRead(JOY_VERT);
      joyClick = digitalRead(JOY_SEL);
      delay(50);
      uint16_t prevHighlight = selectedRest;
      
      // Working time:
      //  - Add more names than can be displayed on one screen, and
      //    go to the "next page" of names if you select far enough down

      if (yVal < JOY_CENTER - JOY_DEADZONE) {
        selectedRest -= 1;  // decrease the y coordinate of the cursor
        //if (selectedRest < 0) {
            //selectedRest = 0;
        //}
        selectedRest = constrain(selectedRest, 0, 29);  
        drawName(prevHighlight);
        drawName(selectedRest);
      } else if (yVal > JOY_CENTER + JOY_DEADZONE) {
        selectedRest += 1;
        selectedRest = constrain(selectedRest, 0, 29);  
        drawName(prevHighlight);
        drawName(selectedRest);
      }
      if (not joyClick) {
          restaurant rest;
          getRestaurant(restDist[selectedRest].index, &rest);
          CURSORY = lat_to_y(rest.lat) + CURSOR_SIZE/2;
          CURSORX = lon_to_x(rest.lon) + CURSOR_SIZE/2;
          MAPX = CURSORX - (DISPLAY_WIDTH - 48)/2;
          MAPY = CURSORY - DISPLAY_HEIGHT/2;
          checkMap();
          redrawMap();
          redrawCursor(ILI9341_RED);
          break;
      }
    }
      /*issue where it loops back if we go off the screen on the top...*/
    moveMap();
    redrawCursor(ILI9341_RED);
}


void getTouch(){
      TSPoint touch = ts.getPoint();
    if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE) {
        return;
    }
    // mapping to the screen, same implementation as we did in class
    int16_t touched_x = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH - 48, 0);
    //int16_t touched_y = map(touch.x, TS_MINX, TS_MAXX, 0, DISPLAY_HEIGHT - 1);
        if (touched_x < DISPLAY_WIDTH - 48) {
            Serial.println("Screen touched!");
            drawCircles();
        }
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

  getTouch();

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
