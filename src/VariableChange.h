#ifndef VARIABLE_CHANGE_H
#define VARIABLE_CHANGE_H

#if defined(LUMOS_ARDUINO_MEGA)

# include <Arduino.h>
# include "Light.h"

class VariableChange
{
public:
	VariableChange();
	void init ();
	void check ();
	void sendInfo ();

private:
	bool changeOn;
	uint8_t changePower[LIGHT_MODE_N];
	uint16_t changeSpeed[LIGHT_MODE_N];
	uint32_t changeRgb[LIGHT_MODE_N];
	uint8_t changeLightMod;
	uint8_t changeSoundMod;
	uint16_t changeDawnTime;
	bool initialized;
};

extern VariableChange variableChange;

#endif // if defined(LUMOS_ARDUINO_MEGA)

#endif // ifndef VARIABLE_CHANGE_H