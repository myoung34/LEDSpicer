/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      FF00SharedCode.cpp
 * @since     Apr 7, 2019
 * @author    Patricio A. Rossi (MeduZa)
 * @copyright Copyright © 2019 Patricio A. Rossi (MeduZa)
 */

#include "FF00SharedCode.hpp"

using namespace LEDSpicer::Devices::Ultimarc;


void FF00SharedCode::resetLeds() {

	// Set Off Ramp Speed.
	vector<uint8_t> data FF00_MSG(0xC0, 0);
	transferData(data);

	// Turn off all LEDs and internal buffer.
	setLeds(0);
	data[0] = 0x80; //FIXME this may be wrong.
	transferData(data);
}

void FF00SharedCode::transfer() {

	// Send FE00 command.
	vector<uint8_t> data FF00_MSG(0xFE, 0);
	transferData(data);

	// Send pairs.
	for (uint8_t c = 0; c < LEDs.size(); c+=2) {
		data.clear();
		data.push_back(LEDs[c]);
		data.push_back(LEDs[c + 1]);
		transferData(data);
	}
}
