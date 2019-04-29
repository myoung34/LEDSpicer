/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      Filler.cpp
 * @since     Oct 26, 2018
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

#include "Filler.hpp"

using namespace LEDSpicer::Animations;

Filler::Filler(umap<string, string>& parameters, Group* const group) :
	TimedActor(parameters, group, REQUIRED_PARAM_ACTOR_FILLER),
	color(parameters["color"])
{
	mode = str2mode(parameters["mode"]);
	totalActorFrames = group->size();
	restart();
	switch (mode) {
	case Modes::Random:
	case Modes::RandomSimple:
		cDirection = direction  = Directions::Forward;
	}
}

void Filler::advanceActorFrame() {

	if (willChange()) {
		changeFrame = 1;

		if (mode == Modes::Linear) {

			/*
			 this will generate an double run when the actor is working as a transition.
			 */
			if (isBouncer()) {
				if (cDirection == Directions::Forward and currentActorFrame == totalActorFrames) {
					if (filling) {
						filling = false;
						currentActorFrame = 1;
						return;
					}
					filling = true;
				}
				if (cDirection == Directions::Backward and currentActorFrame == 1) {
					if (filling) {
						filling = false;
						currentActorFrame = totalActorFrames;
						return;
					}
					filling = true;
				}
				Actor::advanceActorFrame();
				return;
			}

			if (isLastFrame()) {
				if (filling) {
					// restart forward.
					if (cDirection == Directions::Forward) {
						filling = false;
						currentActorFrame = 1;
						return;
					}
					// restart backward.
					if (cDirection == Directions::Backward) {
						filling = false;
						currentActorFrame = totalActorFrames;
						return;
					}
				}
				filling = true;
			}
		}
		Actor::advanceActorFrame();
		return;
	}
	changeFrame++;
}

const vector<bool> Filler::calculateElements() {

	switch (mode) {
	default:
		if (cDirection == Directions::Forward) {
			fillElementsLinear(
				filling ? 0                 : currentActorFrame - 1,
				filling ? currentActorFrame : totalActorFrames
			);
			break;
		}
		fillElementsLinear(
			filling ? currentActorFrame - 1 : 0,
			filling ? totalActorFrames  : currentActorFrame
		);
		break;

	// Forward or backward will work in the same way for random modes.
	case Modes::Random:
		fillElementsRandom(filling);
		break;
	case Modes::RandomSimple:
		if (isFirstFrame() and not isSameFrame())
			affectAllElements();
		fillElementsRandom(true);
		break;
	}
	return affectedElements;
}

void Filler::fillElementsLinear(uint8_t begin, uint8_t end) {
	affectAllElements();
	for (uint8_t c = begin; c < end; ++c) {
		changeElementColor(c, color, filter);
		affectedElements[c] = true;
	}
}

void Filler::fillElementsRandom(bool val) {

	// camniar aca: dibujar lo q esta on, el nuevo guardarlo en una property, y dibujarlo faded

	// Draw.
	if (isSameFrame()) {
		drawRandom();
		return;
	}

	// Extract candidates.
	vector<uint8_t> posibleElements;
	for (uint8_t e = 0; e < affectedElements.size(); ++e)
		if (affectedElements[e] != val)
			posibleElements.push_back(e);

	// Roll dice.
	uint8_t r = 0;
	if (posibleElements.size() > 1)
		r = std::rand() / ((RAND_MAX + 1u) / (posibleElements.size() - 1));

	// Set dice and draw.
	affectedElements[posibleElements[r]] = val;
	drawRandom();

	// Check direction and force last frame.
	if (posibleElements.size() == 1) {
		filling = not filling;
		currentActorFrame = totalActorFrames;
	}
}

void Filler::drawRandom() {
	for (uint8_t e = 0; e < affectedElements.size(); ++e)
		if (affectedElements[e])
			changeElementColor(e, color, filter);
}

void Filler::drawConfig() {
	cout << "Actor Type: Filler, Mode: " << mode2str(mode) << endl;
	Actor::drawConfig();
	cout << "Color: ";
	color.drawColor();
}

string Filler::mode2str(Modes mode) {
	switch (mode) {
	case Modes::Linear:
		return "Linear";
	case Modes::Random:
		return "Random";
	case Modes::LinearSimple:
		return "Linear Simple";
	case Modes::RandomSimple:
		return "Random Simple";
	}
	return "";
}

Filler::Modes Filler::str2mode(const string& mode) {
	if (mode == "Linear")
		return Modes::Linear;
	if (mode == "Random")
		return Modes::Random;
	if (mode == "LinearSimple")
		return Modes::LinearSimple;
	if (mode == "RandomSimple")
		return Modes::RandomSimple;
	throw Error("Invalid mode " + mode);
}


