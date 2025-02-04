/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      Inpulse.cpp
 * @since     May 23, 2019
 * @author    Patricio A. Rossi (MeduZa)
 *
 * @copyright Copyright © 2019 Patricio A. Rossi (MeduZa)
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

#include "Impulse.hpp"

using namespace LEDSpicer::Inputs;

void Impulse::process() {

	readAll();

	if (not events.size())
		return;

	for (auto& event : events) {
		string codeName = std::to_string(event.code);
		if (elementMap.count(codeName)) {
			string fullname = codeName + elementMap[codeName].element->getName();
			if (controlledElements->count(fullname)) {
				// released
				if (not event.value)
					controlledElements->erase(fullname);
			}
			else
				// activated
				controlledElements->emplace(fullname, &elementMap[codeName]);
		}
		if (groupMap.count(codeName)) {
			string fullname = codeName + groupMap[codeName].group->getName();
			if (controlledGroups->count(fullname)) {
				// released
				if (not event.value)
					controlledGroups->erase(fullname);
			}
			else
				// activated
				controlledGroups->emplace(fullname, &groupMap[codeName]);
		}
	}
}
