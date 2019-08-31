#ifndef Haltech_H
#define Haltech_H

#include <MessageQueue.h>
#include "Middleware.h"
#include "SerialCommand.h"


class Haltech : public Middleware
{
  static const unsigned int kBusId = 1; // CAN bus id for Haltech

  public:
    Haltech(SerialCommand* serCom);
    void tick();
    Message process(Message msg);
    void commandHandler(byte* bytes, int length);
    int getRpm();
    int getWheelspeed();
    int getCoolantTemp();
    int getFuelLevel();
    byte getMil();

  private:
    SerialCommand* serialCommand;
    int rpm;
    int wheelspeed;
    int coolantTemp;
    int fuelLevel;
    byte mil;
    void extractRPM(Message msg);
    void extractWheelspeed(Message msg);
    void extractCooltantTemp(Message msg);
    void extractFuelLevel(Message msg);
    void extractMIL(Message msg);
    void printStatus();
};

//
// COnstructor
//
Haltech::Haltech(SerialCommand* serCom)
{
  rpm = 0;
  wheelspeed = 0;
  coolantTemp = 0;
  fuelLevel = 0;
  mil = 0x00;
  serialCommand = serCom;
}


//
// Public functions
//

void Haltech::tick()
{
}


Message Haltech::process(Message msg)
{
	if (kBusId == msg.busId)
	{
		switch (msg.frame_id)
		{
		case 864: // 360 RPM
			extractRPM(msg);
			break;
		case 880: // 370 Wheelspeed
			extractWheelspeed(msg);
			break;
		case 992: // 3E0 Coolant temp
			extractCooltantTemp(msg);
			break;
		case 994: // 3E2 Fuel level
			extractFuelLevel(msg);
			break;
		case 996: // 3E4 MIL
			extractMIL(msg);
			break;
		}
	}

	return msg;
}

// 09 01 - Print status
void Haltech::commandHandler(byte* bytes, int length)
{
	if (length > 0)
	{
		if (0x01 == bytes[0])
		{
			printStatus();
		}
	}
}

int Haltech::getRpm()
{
	return rpm;
}

int Haltech::getWheelspeed()
{
	return wheelspeed;
}

int Haltech::getCoolantTemp()
{
	return coolantTemp;
}

int Haltech::getFuelLevel()
{
  return this->fuelLevel;
}

byte Haltech::getMil()
{
	return mil;
}


//
// Private functions
//

// 360
void Haltech::extractRPM(Message msg)
{
	// Bytes 0-1 are RPM
	rpm = (int)msg.frame_data[0] << 8;
	rpm |= msg.frame_data[1];
}

// 370
void Haltech::extractWheelspeed(Message msg)
{
	// Bytes 0-1 are Wheelspeed in 0.1 kmh
	wheelspeed = (int)msg.frame_data[0] << 8;
	wheelspeed |= msg.frame_data[1];

	// Conver to kmh
	wheelspeed = wheelspeed / 10;
}

// 3E0
void Haltech::extractCooltantTemp(Message msg)
{
	// Bytes 0-1 are Coolant Temp in 0.1 Kelvin
	coolantTemp = (int)msg.frame_data[0] << 8;
	coolantTemp |= msg.frame_data[1];

	// Convert to K
	coolantTemp = coolantTemp / 10;

	// Convert to C
	coolantTemp = coolantTemp - 273;
}

// 3E2
void Haltech::extractFuelLevel(Message msg)
{

}

// 3E4
void Haltech::extractMIL(Message msg)
{
	// 1st bit in byte 8 indicates MIL
	if (bitRead(msg.frame_data[7], 0))
	{
		mil = 0x06;
	}
	// 3rd bit in byte 8 indicates limp mode
	else if (bitRead(msg.frame_data[7], 2))
	{
		mil = 0x04; // I hope this blinks
	}
	else
	{
		mil = 0x00;
	}
}

void Haltech::printStatus()
{
  Serial.write("Haltech::printStatus() Hi, I'm a status.");
  Serial.println();

  Serial.print("RPM          ");
  Serial.println(rpm);
  Serial.print("Speed        ");
  Serial.println(wheelspeed);
  Serial.print("Coolant Temp ");
  Serial.println(coolantTemp);
  Serial.print("Fuel Level   ");
  Serial.println(fuelLevel);
  Serial.print("MIL          ");
  Serial.println(this->mil, HEX);
}

#endif
