void updateDisplayLogik() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return; // return if no time is available
  }

  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = getMinutesFromMidnight(startTime);
  int endMinutes = getMinutesFromMidnight(endTime);

  // calculate the minutes to start time in minutes
  int minutesUntilStart = startMinutes - currentMinutes;

  DisplayState newState;

  if (currentMinutes < startMinutes && minutesUntilStart <= 60) {
    newState = STATE_UPCOMING;
  } else if (currentMinutes >= startMinutes && currentMinutes < endMinutes) {
    newState = STATE_ACTIVE;
  } else {
    newState = STATE_AVAILABLE;
  }

  // ONLY refresh the display if the state is changed
  // OR the forceDisplayUpdate is requested (webinterface changed data)

  if (newState != currentDisplayState || forceDisplayUpdate) {
    Serial.println("Display updating...");
    
    //display.setFullWindow();
    display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();

    display.fillScreen(GxEPD_WHITE);
    
    do {

      if (newState == STATE_UPCOMING) {
        display.fillRect(0, 0, 296, 40, GxEPD_YELLOW);
        display.fillRect(0, 40, 296, 2, GxEPD_BLACK);

        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(6, 30);
        display.print("This table is reserved !");

        display.setFont(&FreeSans12pt7b);
        display.setCursor(6, 70);
        display.print("From ");
        display.print(startTime);
        display.print(" till ");
        display.print(endTime);
        display.print(" for:");
      
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(26, 105);
        display.setTextColor(GxEPD_RED);
        //drawCenteredText(resName, 105);     
        display.print(resName);

     } else if (newState == STATE_ACTIVE) {
        display.setFont(&FreeSansBold18pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(6, 35);
        display.print("Welcome");
      
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_RED);
        display.setCursor(6, 65);
        display.print(resName);
      
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(6, 120);
        display.print("Your table is reserved till:");
        drawOutlinedText(endTime, 230, 120, GxEPD_YELLOW);

        drawQRCode("https://your-restaurant.com/Food-Menu", 200, 6, 3); //90x90 Pixel

      } else if (newState == STATE_AVAILABLE) {
        display.setFont(&FreeSansBold24pt7b);
        display.setTextColor(GxEPD_BLACK);
        drawCenteredText("Table", 50);
        drawCenteredText("available", 110);
      }
    } while (display.nextPage());
    display.hibernate();

    currentDisplayState = newState;
    forceDisplayUpdate = false;
  }
}