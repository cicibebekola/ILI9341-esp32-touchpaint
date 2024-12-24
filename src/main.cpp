#include <Arduino.h>   // Core Arduino functionality
#include <FS.h>        // File system library
#include <SPIFFS.h>    // SPIFFS file system support
#include <TFT_eSPI.h>  // TFT display library

#define CALIBRATION_FILE "/TouchCalData"
#define REPEAT_CAL false  // Set to true to run calibration every time

#define BOXSIZE 30  // Reduced button size
int PENRADIUS = 5;  // Brush size and line thickness
int oldcolor, currentcolor;

TFT_eSPI tft = TFT_eSPI();  // Declare the display object
TFT_eSPI_Button colorButtons[6];  // Declare button objects for color selection
TFT_eSPI_Button clearButton;      // Declare button object for clear button
TFT_eSPI_Button calibrateButton;  // Declare button object for calibrate button
TFT_eSPI_Button saveButton;       // Declare button for saving image
TFT_eSPI_Button loadButton;       // Declare button for loading image

uint16_t last_x = 0, last_y = 0;
bool first_touch = true;
unsigned long last_touch_time = 0;
const unsigned long touch_delay = 200;  // Delay threshold in milliseconds

void drawUI() {
  // Ensure background is black before drawing UI
 

  // Set up color buttons horizontally
  int buttonGap = 5;  // Gap between buttons
  int startX = 5;     // Starting X position for color buttons
  int startY = 5;     // Starting Y position for color buttons
  
  for (int i = 0; i < 6; i++) {
    int x = startX + i * (BOXSIZE + buttonGap);  // Place buttons horizontally
    colorButtons[i].initButton(&tft, x + BOXSIZE / 2, startY + BOXSIZE / 2, BOXSIZE, BOXSIZE, TFT_WHITE, 
                               (i == 0 ? TFT_RED : i == 1 ? TFT_YELLOW : i == 2 ? TFT_GREEN 
                               : i == 3 ? TFT_CYAN : i == 4 ? TFT_BLUE : TFT_MAGENTA), 
                               TFT_WHITE, "", 0);
    colorButtons[i].drawButton(false);
  }

  // Set up clear, calibrate, save, and load buttons vertically, aligned to the right side of the screen
  int secondaryStartX = tft.width() - BOXSIZE / 2 - buttonGap;  // Start next to the right edge of the screen
  int secondaryStartY = startY;  // Align the top of secondary buttons with the top of color buttons

  clearButton.initButton(&tft, secondaryStartX, secondaryStartY + BOXSIZE / 2, BOXSIZE, BOXSIZE, TFT_WHITE, TFT_BLACK, TFT_WHITE, "CLR", 1);
  calibrateButton.initButton(&tft, secondaryStartX, secondaryStartY + (BOXSIZE + buttonGap) * 1 + BOXSIZE / 2, BOXSIZE, BOXSIZE, TFT_WHITE, TFT_BLUE, TFT_WHITE, "CAL", 1);
  saveButton.initButton(&tft, secondaryStartX, secondaryStartY + (BOXSIZE + buttonGap) * 2 + BOXSIZE / 2, BOXSIZE, BOXSIZE, TFT_WHITE, TFT_GREEN, TFT_WHITE, "SAVE", 1);
  loadButton.initButton(&tft, secondaryStartX, secondaryStartY + (BOXSIZE + buttonGap) * 3 + BOXSIZE / 2, BOXSIZE, BOXSIZE, TFT_WHITE, TFT_ORANGE, TFT_WHITE, "LOAD", 1);

  // Draw the secondary buttons
  clearButton.drawButton(false);
  calibrateButton.drawButton(false);
  saveButton.drawButton(false);
  loadButton.drawButton(false);

  // Set initial color
  colorButtons[0].drawButton(true);  // Default selected color is red
  currentcolor = TFT_RED;
}

void touch_calibrate() {
  uint16_t calData[5];  // Array to store calibration data
  bool calibrationNeeded = true;  // Assume calibration is needed

  // Check if SPIFFS is mounted properly
  if (!SPIFFS.begin(true)) {  // Use 'true' to format SPIFFS if mount fails
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  // Check if the calibration file exists
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    // Open the calibration file
    fs::File f = SPIFFS.open(CALIBRATION_FILE, "r");
    if (f) {
      // Read the calibration data
      if (f.read((uint8_t *)calData, 14) == 14) {  // Read 14 bytes of data
        tft.setTouch(calData);  // Set the calibration data to the touch screen
        calibrationNeeded = false;  // Calibration data is valid, no need to recalibrate
        Serial.println("Calibration data loaded from SPIFFS.");
      }
      f.close();
    }
  }

  if (calibrationNeeded) {
    // Calibrate the screen if no valid data is found
    Serial.println("Calibration needed, starting calibration...");

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    // Store calibration data in SPIFFS
    fs::File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);  // Write 14 bytes of data
      f.close();
      Serial.println("Calibration data saved to SPIFFS.");
    }
    tft.fillScreen(TFT_BLACK);  // Clear the screen after calibration
  }
}


void saveImage() {
  fs::File file = SPIFFS.open("/image.bin", FILE_WRITE);

  if (file) {
    Serial.println("Saving image to SPIFFS...");
    for (int y = 0; y < tft.height(); y++) {
      for (int x = 0; x < tft.width(); x++) {
        uint16_t color = tft.readPixel(x, y);
        file.write((uint8_t*)&color, sizeof(color));  // Write pixel data
      }
    }
    file.close();
    Serial.println("Image saved!");
  } else {
    Serial.println("Error opening file for writing.");
  }
}



void loadImage() {
  fs::File file = SPIFFS.open("/image.bin", FILE_READ);

  if (file) {
    Serial.println("Loading image from SPIFFS...");
    for (int y = 0; y < tft.height(); y++) {
      for (int x = 0; x < tft.width(); x++) {
        uint16_t color;
        if (file.read((uint8_t*)&color, sizeof(color)) > 0) {
          tft.drawPixel(x, y, color);  // Draw pixel data
        }
      }
    }
    file.close();
    Serial.println("Image loaded!");
  } else {
    Serial.println("Error opening file for reading.");
  }
  drawUI();
}

void setup(void) {
  Serial.begin(115200);
  Serial.println(F("Touch Paint with Image Storage (SPIFFS)!"));
  

  tft.init();
  tft.setRotation(1);  // Adjust rotation as needed
  tft.fillScreen(TFT_BLACK);

  // Calibrate the touch screen
  touch_calibrate();

  // Draw the UI
  drawUI();
}

void drawThickLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    tft.fillCircle(x0, y0, PENRADIUS, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void loop() {
  uint16_t x, y;
  bool pressed = tft.getTouch(&x, &y);
  unsigned long current_time = millis();

  if (pressed) {
    // Handle color buttons
    for (int i = 0; i < 6; i++) {
      if (colorButtons[i].contains(x, y)) {
        oldcolor = currentcolor;
        currentcolor = (i == 0) ? TFT_RED : (i == 1) ? TFT_YELLOW : (i == 2) ? TFT_GREEN : (i == 3) ? TFT_CYAN : (i == 4) ? TFT_BLUE : TFT_MAGENTA;
        colorButtons[i].drawButton(true);
        if (oldcolor != currentcolor) {
          for (int j = 0; j < 6; j++) {
            if (j != i) {
              colorButtons[j].drawButton(false);
            }
          }
        }
        first_touch = true;
        return;
      }
    }

    // Handle other buttons
    if (clearButton.contains(x, y)) {
      tft.fillScreen(TFT_BLACK);
      for (int i = 0; i < 6; i++) {
        colorButtons[i].drawButton(false);
      }
      clearButton.drawButton(false);
      calibrateButton.drawButton(false);
      saveButton.drawButton(false);
      loadButton.drawButton(false);
      return;
    }

    if (calibrateButton.contains(x, y)) {
      touch_calibrate();
      return;
    }

    if (saveButton.contains(x, y)) {
      saveImage();
      return;
    }

    if (loadButton.contains(x, y)) {
      loadImage();
      return;
    }

    // Draw on the screen if outside button area
    if (y > BOXSIZE * 2) {
      if (first_touch || (current_time - last_touch_time > touch_delay)) {
        first_touch = false;
        last_x = x;
        last_y = y;
        last_touch_time = current_time;
      } else {
        drawThickLine(last_x, last_y, x, y, currentcolor);
        last_x = x;
        last_y = y;
        last_touch_time = current_time;
      }
      tft.fillCircle(x, y, PENRADIUS, currentcolor);
    }
  } else {
    first_touch = true;
  }
}
