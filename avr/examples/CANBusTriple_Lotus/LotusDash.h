#ifndef LotusDash_H
#define LotusDash_H

#include <Arduino.h>
#include <MessageQueue.h>
#include "Middleware.h"
#include "SerialCommand.h"
#include "Haltech.h"
#include "DashMessage.h"


class LotusDash : public Middleware
{
  static const unsigned int kBusId = 2; // CAN bus id for Lotus dash bus
  static const unsigned int kHalfSweepPeriod = 1500; // 2 seconds for gauge to go to maximum but not return

  public:
    LotusDash( MessageQueue *q, SerialCommand *serCom, Haltech *haltech );
    void tick();
    Message process( Message msg );
    void commandHandler(byte* bytes, int length);
    
  private:
    MessageQueue* mainQueue;
    SerialCommand* serialCommand;
    Haltech* haltech;
    unsigned long lastPacketSentTime;
    bool isStartup;
    unsigned long sweepStartTime;
    void doStartupSequence();
    void enableGaugeSweep();
    void printDash();
    DashMessage translateHaltechToLotus();
    void updateDash(DashMessage dashMessage);
};


//
// COnstructor
//
LotusDash::LotusDash( MessageQueue *q, SerialCommand *serCom, Haltech *haltechObj )
{
  mainQueue = q;
  serialCommand = serCom;
  haltech = haltechObj;
  lastPacketSentTime = 0;

  enableGaugeSweep();
}

//
// Public functions
//

void LotusDash::tick()
{ 
  // Update dash every ~100ms
  if (millis() >= lastPacketSentTime + 100)
  {
    if (isStartup) 
    {
      // Make the gauges do their dance
      doStartupSequence();
    }
    else
    {
      DashMessage dashMessage = translateHaltechToLotus();
      updateDash(dashMessage);
    }
  }
}


Message LotusDash::process( Message msg )
{
  return msg;
}


void LotusDash::commandHandler(byte* bytes, int length)
{
  if (length > 0)
  {
    if (0x01 == bytes[0])
    {
      // Reset the gauge sweep control variables
      //
      enableGaugeSweep();
    }
    else if (0x02 == bytes[0])
    {
      printDash();
    }
  }
}


//
// Private functions
//

void LotusDash::doStartupSequence()
{
  if (0 == sweepStartTime)
  {
    sweepStartTime = millis();
    
    Serial.write("LotusDash::doStartupSequence() Start sweep.");
    Serial.println();
  }
  
  // If needle is on it's way up, keep telling it to go up
  //
  unsigned long sweepMidPoint = sweepStartTime + kHalfSweepPeriod;
  if (millis() <= sweepMidPoint)
  {
//    Serial.write("LotusDash::doStartupSequence() Going up");
//    Serial.println();
    
    DashMessage dashMessage;
    dashMessage.speed = 0xFF; // adjusted speed ~= d(XXh)-11d (61h-->97-11=86 mph) -- FF should be 256kmh
    dashMessage.rpm_1 = 0x27; // tach rpms [d(CCh)*256]+d(DDh) 06 D2 = 1746 rpm -- 27h should be 10,000rpm
    dashMessage.rpm_2 = 0x00; //  
    dashMessage.fuel = 0x00; // fuel level (00=empty, FF=full) d[5] / 256 * 100 -- fuel %
    dashMessage.temperature = 0x00; // engine temperature ~= d(XXh)-14d (D0-->208-14=194F)
    dashMessage.mil = 0x06; // MIL 06-on, 04-crank, 00-running, 01-shift light

    updateDash(dashMessage);
  }
  else
  {
//    Serial.write("LotusDash::doStartupSequence() Going down");
//    Serial.println();

    DashMessage dashMessage;
    dashMessage.speed = 0x00; // adjusted speed ~= d(XXh)-11d (61h-->97-11=86 mph) -- FF should be 256kmh
    dashMessage.rpm_1 = 0x00; // tach rpms [d(CCh)*256]+d(DDh) 06 D2 = 1746 rpm -- 27h should be 10,000rpm
    dashMessage.rpm_2 = 0x00; //  
    dashMessage.fuel = 0x00; // fuel level (00=empty, FF=full) d[5] / 256 * 100 -- fuel %
    dashMessage.temperature = 0x00; // engine temperature ~= d(XXh)-14d (D0-->208-14=194F)
    dashMessage.mil = 0x06; // MIL 06-on, 04-crank, 00-running, 01-shift light

    updateDash(dashMessage); 
  }


  unsigned long sweepEndPoint = sweepStartTime + (kHalfSweepPeriod * 2);
  if (millis() >= sweepEndPoint)
  {
    // End the sweep
    isStartup = false;

    Serial.write("LotusDash::doStartupSequence() Sweep finished.");
    Serial.println();
  }
}


void LotusDash::enableGaugeSweep()
{
  isStartup = true;
  sweepStartTime = 0;
}


void LotusDash::printDash()
{
  DashMessage dashMessage = translateHaltechToLotus();

  Serial.write("LotusDash::printStatus() Hi, I'm a status.");
  Serial.println();

  Serial.print("RPM 1        ");
  Serial.println(dashMessage.rpm_1, DEC);
  Serial.print("RPM 2        ");
  Serial.println(dashMessage.rpm_2, DEC);
  Serial.print("Speed        ");
  Serial.println(dashMessage.speed, DEC);
  Serial.print("Coolant Temp ");
  Serial.println(dashMessage.temperature, DEC);
  Serial.print("Haltech Fuel Level   ");
  Serial.println(haltech->getFuelLevel());
  //Serial.print("Fuel Level   ");
  //Serial.println(dashMessage.fuel, DEC);
  Serial.print("MIL          ");
  Serial.println(dashMessage.mil, HEX);
}


DashMessage LotusDash::translateHaltechToLotus()
{
  int rpm = haltech->getRpm();
  byte rpm_1 = rpm / 256;
  byte rpm_2 = rpm % 256;
  
  DashMessage dashMessage;
  dashMessage.speed = haltech->getWheelspeed(); // adjusted speed ~= d(XXh)-11d (61h-->97-11=86 mph) -- FF should be 256kmh
  dashMessage.rpm_1 = rpm_1; // tach rpms [d(CCh)*256]+d(DDh) 06 D2 = 1746 rpm -- 27h should be 10,000rpm
  dashMessage.rpm_2 = rpm_2; //  
  dashMessage.fuel = (haltech->getFuelLevel() / 100) * 255; // fuel level (00=empty, FF=full) d[5] / 256 * 100 -- fuel %
  dashMessage.temperature = haltech->getCoolantTemp(); // engine temperature ~= d(XXh)-14d (D0-->208-14=194F)
  dashMessage.mil = haltech->getMil(); // MIL 06-on, 04-crank, 00-running, 01-shift light

  //updateDash(dashMessage);

  return dashMessage;
}


void LotusDash::updateDash(DashMessage dashMessage)
{
  Message msg;
  msg.busId = kBusId;
  msg.frame_id = 1024; // unsigned short - 400h

  msg.frame_data[0] = dashMessage.speed; // adjusted speed ~= d(XXh)-11d (61h-->97-11=86 mph)
  msg.frame_data[1] = 0x00; // unused (always 00) 
  msg.frame_data[2] = dashMessage.rpm_1; // tach rpms [d(CCh)*256]+d(DDh) 06 D2 = 1746 rpm
  msg.frame_data[3] = dashMessage.rpm_2; // 
  msg.frame_data[4] = dashMessage.fuel; // fuel level (00=empty, FF=full)
  msg.frame_data[5] = dashMessage.temperature; // engine temperature ~= d(XXh)-14d (D0-->208-14=194F) -- (degC * 9.0 / 5.0)+32)+14
  msg.frame_data[6] = dashMessage.mil; // 00=off, 01=shift light, 02=MIL, 03=Mil&Shift, 04=oil, 05=oil&shift, 06=oil&mil, 07=oil&mil&shift, 08=tc, 09=tc&shift, 0a=tc&mil,
                                      // 0b = tc&mil&shift, 0c=tc&oil, 0d=tc&oil&shift, 0e=tc&oil&mil, 0f=tc&oil&mil&shift
  msg.frame_data[7] = 0x00; // unused (always 00)

  msg.length = 8;
  msg.dispatch = true;

  //Serial.write("LotusDash::updateDash()");
  //Serial.println();
  //serialCommand->printMessageToSerial(msg);
  
  mainQueue->push(msg);
  lastPacketSentTime = millis();
}
#endif
