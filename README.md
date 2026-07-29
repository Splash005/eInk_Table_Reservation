# eInk_Table_Reservation
A Demo-Project for a smart Table-Reservation Sign, which has an integrated Webserver and handles reservations.  
It periodically checks the time and when the reservation is upcoming the screen changes to show customers that the table is reserved. When the reservation starts, it changes again its screen to welcome the customer and display a QR code with a link to the menu card.
The Wifi can be reconfigured over a webportal or reset with a physical button at the back of the board.
  
It is based on an ESP32 S3 and a 2,9 inch 4-color eInk Display. The driver board can be battery powered, but the software is currently designed to run on USB.

### Following products have been used:
- Driver Board --> XIAO ePaper Display Board(ESP32-S3) - EE04 (SKU 100075670)
- eInk Display --> 2.9" Quadruple Color eInk / ePaper Display with 128x296 Pixels, SPI interface, Support XIAO/Arduino (SKU 104990855)

| component | spezification / type |
| --- | --- | 
| Microcontroller | ESeeed Studio XIAO ESP32S3 PLUS |
| Display | 2.9" E-Ink, 4-color (GDEY029F52) |
| Display-driver IC | JD79661 (resolution: 296 x 128 Pixel) |


    
## Used external libaries:
- **GxEPD2_4C** - for contolling the eInk display (custom integration designed from me for the above mentioned screen)
- **WiFiManager** - for the capitave Portal to configure the Wifi
- **QRCode** - custom integration for the generation of QR codes directly on the ESP
- **Standard ESP Libaries** - WiFi.h, WebServer.h, Preferences.h, time.h
  
## Pin wiring
| Function | ESP32 Pin | Description |
| --- | --- | --- |
| EPD_CS | D7 | Chip Select (SPI) |
| EPD_DC | 10 | Data/Command Control |
| EPD_RST | 38 | Reset Pin |
| EPD_BUSY | D3 | Busy-Signal of the display|
| SPI_SCK | D8 | SPI Clock |
| SPI_MOSI | D10 |SPI Master Out Slave In |
| SPI_MISO | 8 | SPI Master In Slave Out |
  
## State machine and display logic
| state | condition | display |
| --- | --- | --- |
| STATE_AVAILABLE | > 60 Min. before the starttime OR after endtime | big centered text: "Table available". |
| STATE_UPCOMING | less or exactly 60 minutes before the start time | yellow Warning background on the top. Text: "This table is reserved! From [Start] till [End] for: [Name]". Name in red. |
| STATE_ACTIVE | current time is between start and end time | "Welcome [Name]" (Name in red). QR-Code to the menu card. Endtime with yellow Font and outlined with black. |
  
## Logic Diagram
```
======================================================================
                         [ SETUP FUNCTION (setup) ]
======================================================================
                                   |
                                   v
             [ Init: Hardware, Display & load saved data ]
                                   |
                                   v
         +---------------- ( WiFi known? ) ------------------+
         |                                                   |
      [ Yes ]                                              [ No ]
         |                                                   |
         |                                                   v
         |                                [ start AP-Mode (Table_Reservation_AP) ]
         |                                [ display shows IP & QR-Code           ]
         |                                                   |
         |<--------------------------------------------------+ (After the setup from web portal)
         v
 [ get time (NTP)  ]
         |
 [ start webserver ]
         |
         v
======================================================================
                        [ MAIN FUNCTION (loop) ]
======================================================================
         |
         +
         |                                                       
         v                                                        
[ Check Web interfaces ] --> (New Data?) -> [ save and force Update]
         |                                                       
         v                                                       
[ Check Button ] ------> (3 Sec. pressed?) -> [ reset WiFi & Restart ]
         |                                                      
         v                                                       
( 10 seconds elapsed OR forced screen update? ) ------[ No ] -----+           
         |                                                        |
       [ Yes ]                                                    |
         |                                                        |
         v                                                        |
[ check time & State (updateDisplayLogik) ]                       |
         |                                                        |
         +---> (time < starttime AND > 60 Min) ---> [ AVAILABLE ] |
         +---> (time < starttime AND <= 60 Min) --> [ UPCOMING ]  |
         +---> (time >= starttime AND < endtime) -> [ ACTIVE ]    |
         +---> (time >= endtime) -----------------> [ AVAILABLE ] |
         |                                                        |
         v                                                        |
( state changed or display update is forced? ) -------[ No ] -----+  
         |                                                        |
       [ Yes ]                                                    |
         |                                                        |
         v                                                        |
[ update eInk display ]                                           |
         |                                                        |
[ set eInk in Hibernate mode ]                                    |
         |                                                        |
         +--------------------------------------------------------+
```
  
## Known Issues:
- currently the reservation is not deleted after it elapsed therefor it is still in memory and the table will be reserved the next day again.
  - Solution: After endtime elapsed delete reservation or create a flag if reservation has elapsed and check in the program for the flag. 

  
## Future Improvements:
- add an array of reservations and the ESP checks itself which reservation is next and after it elapsed delete it from the array
  - Advantages:
    - Multiple reservations can be entered for one table
        
- change the architecture from a webserver to a client which connects to a server and checks if reservations are available for this table (one server --> multiple Client architecture). The Reservation handling is done on the server and the client just checks it status and updates the display then goes to low power sleep.
  - Advantages:
    - Reservation handling is done on the server side (all reservations can be entered in one place with an table number and the clients check automatically if there is an upcoming reservation for their table number)
    - Client can be battery powered and goes to deepsleep until next check (currently no deepsleep for the ESP because it runs the webserver, the eink display is in sleep)


