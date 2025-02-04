/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      Device.hpp
 *
 * @since     Jun 7, 2018
 * @author    Patricio A. Rossi (MeduZa)
 *
 * @copyright Copyright © 2018 - 2019 Patricio A. Rossi (MeduZa)
 *
 * @copyright LEDSpicer is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * @copyright LEDSpicer is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ConnectionUSB.hpp"
#include "Group.hpp"

#ifndef DEVICE_HPP_
#define DEVICE_HPP_ 1

namespace LEDSpicer {
namespace Devices {

/**
 * LEDSpicer::Devices::Device
 * Generic Device settings and functionality.
 */
class Device : public ConnectionUSB {

public:

	using ConnectionUSB::ConnectionUSB;

	virtual ~Device() = default;

	/**
	 * Set a LED to an intensity
	 * @param led
	 * @param intensity (0-255)
	 * @return
	 */
	Device* setLed(uint8_t led, uint8_t intensity);

	/**
	 * Set all the LEDs to an specific intensity
	 * @param intensity (0-255)
	 * @return
	 */
	Device* setLeds(uint8_t intensity);

	/**
	 * Returns a pointer to a single LED.
	 * @param ledPos
	 * @return
	 */
	uint8_t* getLed(uint8_t ledPos);

	/**
	 * Register a new Element with a single LED.
	 * @param name
	 * @param led
	 */
	void registerElement(const string& name, uint8_t led);

	/**
	 * Register a new Element with three LEDs (RGB).
	 * @param name
	 * @param led1
	 * @param led2
	 * @param led3
	 */
	void registerElement(const string& name, uint8_t led1, uint8_t led2, uint8_t led3);

	Element* getElement(const string& name);

	/**
	 * Check if a LED (or pin) is valid.
	 * @param led
	 * @throw Error if not valid.
	 */
	void validateLed(uint8_t led) const;

	/**
	 * Returns the number of registered elements.
	 * @return
	 */
	uint8_t getNumberOfElements() const;

	umap<string, Element>* getElements();

	/**
	 * Sets all LEDs off.
	 */
	virtual void resetLeds() = 0;

protected:

	/// Maps elements by name.
	umap<string, Element> elementsByName;

	/// Basic
	virtual void afterClaimInterface();

};

// The functions to create and destroy devices.
#define deviceFactory(plugin) \
	extern "C" LEDSpicer::Devices::Device* createDevice(uint8_t boardId, umap<string, string>& options) { return new plugin(boardId, options); } \
	extern "C" void destroyDevice(LEDSpicer::Devices::Device* instance) { delete instance; }

}} /* namespace LEDSpicer */

#endif /* DEVICE_HPP_ */
