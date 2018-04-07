#include "Bluetooth.h"
#include "Logger.h"
#include "Request.h"
#include <Arduino.h>
#include "WString.h"

Bluetooth::Bluetooth() : module (&Serial2)
{
	connectionState = 0;
}

void Bluetooth::init ()
{
	Log.info (F("Initializing Bluetooth module... "));

	module.reset();
 
	while (module.getStatus() != BM70_STATUS_IDLE)
	{
		module.reset();
		delay (500);
	}

	module.removePaired (0xFF);

	Log.infonp ("Done" dendl);
}

void Bluetooth::action ()
{
	module.action();

	makeConnection();

	if (module.transparentAvailable())
	{
		char data[14];

		module.transparentRead (data);

		data[5] = '\0';

		Log.verbose (F("Received data from bluetooth: %s" dendl), data);

		request.decode (data);
	}
}

void Bluetooth::makeConnection ()
{
	if (module.getStatus() == BM70_STATUS_IDLE)
		connectionState = 0;

	switch (connectionState)
	{
		case 0:
			connectionState = 1;
			Log.trace (F("Configure auto-connect as master... "));
			module.configureAutoConnect (true, 0x3481F42F22A7, (char *) "123456");
			Log.tracenp (F("Done." dendl));

			Log.trace (F("Waiting for connection..." dendl));
			counter = millis();
			break;

		case 1:
			if (module.getStatus() == BM70_STATUS_CONNECTED)
			{
				connectionState = 2;
				Log.trace (F("Connected!" dendl));
				Log.trace (F("Waiting for pairing..." dendl));
				counter = millis();
			}
			else if (millis() - counter >= 50000)
			{
				connectionState = 0;
				Log.trace (F("No connection after 50s, resetting" dendl));
				module.reset();
			}
			break;

		case 2:
			if (module.isPaired())
			{
				connectionState = 3;
				Log.trace (F("Pairing succeeded." dendl));
			}
			else if (millis() - counter >= 30000)
			{
				connectionState = 0;
				Log.warning (F("Pairing timed out, restarting" dendl));
				module.reset();
			}
			break;

		case 3:
			connectionState = 4;
			Log.trace (F("Enabling remote transparent... "));
			module.enableTransparent();
			Log.tracenp (F("Done." dendl));

			Log.trace (F("Waiting for local transparent to be enabled..." dendl));
			counter = millis();
			break;

		case 4:
			if (module.getStatus() == BM70_STATUS_TRANSCOM)
			{
				connectionState = 5;
				Log.trace (F("Transparent is enabled, connection completed!" dendl));
			}
			else if (millis() - counter >= 50000)
			{
				connectionState = 0;
				Log.warning (F("Enabling local transparent timed out, restarting" dendl));
				module.reset();
			}
			break;

		case 5:
			break;
	}
} // Bluetooth::makeConnection

bool Bluetooth::isEnabled ()
{
	return connectionState == 5;
}

Bluetooth bluetooth = Bluetooth();
