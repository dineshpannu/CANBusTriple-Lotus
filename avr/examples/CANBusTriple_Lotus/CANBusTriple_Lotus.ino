/*
*  CANBus Triple
*  The Car Hacking Platform
*  https://canb.us
*  https://github.com/CANBus-Triple
*/

#include <avr/wdt.h>
#include <SPI.h>
#include <EEPROM.h>
#include <CANBus.h>
#include <MessageQueue.h>


// #define SLEEP_ENABLE
#define INCLUDE_DEFAULT_EEPROM

#define BUILDNAME "CANBus Triple Lotus"
#ifdef HAS_AUTOMATIC_VERSIONING
    #include "_Version.h"
#else
    #define BUILD_VERSION "0.7.2"
#endif

#define READ_BUFFER_SIZE 20
#define WRITE_BUFFER_SIZE 10


CANBus busses[] = {
    CANBus(CAN1SELECT, CAN1RESET, 1, "Bus 1"),
    CANBus(CAN2SELECT, CAN2RESET, 2, "Bus 2"),
    CANBus(CAN3SELECT, CAN3RESET, 3, "Bus 3")
};

#include "Middleware.h"
#include "Settings.h"
#include "AutoBaud.h"
#include "SerialCommand.h"
#include "ServiceCall.h"
#include "ChannelSwap.h"
#include "Naptime.h"
#include "LotusDash.h"
#include "Haltech.h"

Message readBuffer[READ_BUFFER_SIZE];
Message writeBuffer[WRITE_BUFFER_SIZE];
MessageQueue readQueue(READ_BUFFER_SIZE, readBuffer);
MessageQueue writeQueue(WRITE_BUFFER_SIZE, writeBuffer);

/*
*  Middleware Setup
*  http://docs.canb.us/firmware/main.html
*/
SerialCommand *serialCommand = new SerialCommand( &writeQueue );
ServiceCall *serviceCall = new ServiceCall( &writeQueue );
#ifdef SLEEP_ENABLE
Naptime *naptime = new Naptime(0x0472);
#endif
Haltech *haltech = new Haltech(serialCommand);
LotusDash *lotusDash = new LotusDash( &writeQueue, serialCommand, haltech );

Middleware *activeMiddleware[] = {
  serialCommand,
  // new ChannelSwap(),
  // serviceCall,
#ifdef SLEEP_ENABLE
  naptime,
#endif
  lotusDash,
  haltech
};
int activeMiddlewareLength = (int)( sizeof(activeMiddleware) / sizeof(activeMiddleware[0]) );


void setup()
{
    Settings::init();
    delay(1);

    /*
    *  Middleware Settings
    */
#ifdef SLEEP_ENABLE
    // Set a command callback to enable disable sleep (4E01 on 4E00 off)
    serialCommand->registerCommand(0x4E, 1, naptime);
#endif
    serialCommand->registerCommand(0x09, 1, haltech);
    serialCommand->registerCommand(0x10, 1, lotusDash);
    serviceCall->setFilterPids();

    Serial.begin( 115200 ); // USB
    Serial1.begin( 57600 ); // UART

    /*
    *  Power LED
    */
    DDRE |= B00000100;
    PORTE |= B00000100;

    /*
    *  BLE112 Init
    */
    pinMode( BT_SLEEP, OUTPUT );
    digitalWrite( BT_SLEEP, HIGH ); // Keep BLE112 Awake

    /*
    *  Boot LED
    */
    pinMode( BOOT_LED, OUTPUT );

    pinMode( CAN1INT_D, INPUT );
    pinMode( CAN2INT_D, INPUT );
    pinMode( CAN3INT_D, INPUT );
    pinMode( CAN1RESET, OUTPUT );
    pinMode( CAN2RESET, OUTPUT );
    pinMode( CAN3RESET, OUTPUT );
    pinMode( CAN1SELECT, OUTPUT );
    pinMode( CAN2SELECT, OUTPUT );
    pinMode( CAN3SELECT, OUTPUT );

    digitalWrite(CAN1RESET, LOW);
    digitalWrite(CAN2RESET, LOW);
    digitalWrite(CAN3RESET, LOW);


    // Setup CAN Busses
    for (int b = 0; b < 3; b++) {
        busses[b].begin(); // Resets bus and puts it in CONTROL mode
        busses[b].setClkPre(1);
        busses[b].baudConfig(cbt_settings.busCfg[b].baud);
        busses[b].setRxInt(true);
        busses[b].bitModify(RXB0CTRL, 0x04, 0x04); // Set buffer rollover enabled
        busses[b].disableFilters();
        busses[b].setMode(cbt_settings.busCfg[b].mode);
    }

    for(int l = 0; l < 5; l++) {
        digitalWrite( BOOT_LED, HIGH );
        delay(50);
        digitalWrite( BOOT_LED, LOW );
        delay(50);
    }

    // wdt_enable(WDTO_1S);
}


/*
*  Main Loop
*/
void loop() 
{
    // Run all middleware ticks
    for(int i = 0; i <= activeMiddlewareLength - 1; i++) 
    {
        activeMiddleware[i]->tick();
    }

    if (digitalRead(CAN1INT_D) == 0) readBus(&busses[0]);
    if (digitalRead(CAN2INT_D) == 0) readBus(&busses[1]);
    if (digitalRead(CAN3INT_D) == 0) readBus(&busses[2]);

    // Process received CAN message through middleware
    if (!readQueue.isEmpty()) {
        Message msg = readQueue.pop();
        for(int i = 0; i <= activeMiddlewareLength - 1; i++) msg = activeMiddleware[i]->process(msg);
        if (msg.dispatch) writeQueue.push(msg);
    }

    bool error = false;
    while(!writeQueue.isEmpty() && !error) {
        Message msg = writeQueue.pop();
	if (msg.dispatch) {
	    error = !sendMessage(msg, &busses[msg.busId - 1]);
            // When TX Failure, add back to queue
            if (error) writeQueue.push(msg);
        }
    }

    // Pet the dog
    // wdt_reset();

} // End loop()


/*
*  Load CAN Controller buffer and set send flag
*/
bool sendMessage( Message msg, CANBus * bus )
{
    int ch = bus->getNextTxBuffer();
    if (ch < 0 || ch > 2) return false; // All TX buffers full

    digitalWrite(BOOT_LED, HIGH);
    bus->loadFullFrame(ch, msg.length, msg.frame_id, msg.frame_data );
    bus->transmitBuffer(ch);
    digitalWrite(BOOT_LED, LOW );

    return true;
}


/*
*  Read Can Controller Buffer
*/
void readBus( CANBus * bus )
{
    byte rx_status = 0x3;
    bool bufferAvailable = true;
    while((rx_status & 0x3) && bufferAvailable) {
        rx_status = bus->readStatus();
        if (rx_status & 0x1) 
            bufferAvailable = readMsgFromBuffer(bus, 0, rx_status);
        if ((rx_status & 0x2) && bufferAvailable) 
            bufferAvailable = readMsgFromBuffer(bus, 1, rx_status);
    }
}


bool readMsgFromBuffer(CANBus * bus, byte bufferId, byte rx_status)
{
    if (readQueue.isFull()) return false;
    Message msg;
    msg.busStatus = rx_status;
    msg.busId = bus->busId;
    msg.dispatch = false;
    bus->readFullFrame(bufferId, &msg.length, msg.frame_data, &msg.frame_id );  
    return readQueue.push(msg);
}
