/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      Profile.cpp
 * @since     Jun 25, 2018
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

#include "Profile.hpp"

using namespace LEDSpicer::Devices;

void Profile::addAnimation(const vector<Actor*>& animation) {
	animations.insert(animations.begin(), animation.begin(), animation.end());
}

void Profile::drawConfig() {
	cout << "* Background color: " << backgroundColor.getName() << endl;
	if (start) {
		cout << endl << "* Start transition:" << endl;
		start->drawConfig();
	}
	if (end) {
		cout << endl << endl << "* Ending transition:" << endl;
		end->drawConfig();
	}

	if (animations.size()) {
		cout << endl << "* Animation Actors:" << endl;
		uint count = 1;
		for(auto actor : animations) {
			cout << "Actor " << count++ << ":" << endl;
			actor->drawConfig();
		}
	}

	if (alwaysOnGroups.size()) {
		cout << endl << "* Groups Overwrite Color: " << endl;
		for (auto& g : alwaysOnGroups) {
			cout << g.group->getName() << " ";
			g.color->drawColor();
			cout << endl;
		}
	}

	if (alwaysOnElements.size()) {
		cout << endl << "* Elements Overwrite Color: " << endl;
		for (auto& e : alwaysOnElements) {
			cout << e.element->getName() << " ";
			e.color->drawColor();
			cout << endl;
		}
	}

	if (inputs.size()) {
		cout << endl << endl << "* Input plugins:" << endl;
		for (auto i : inputs) {
			cout << "Plugin " << i.first << endl;
			i.second->drawConfig();
		}
	}
}

void Profile::runFrame() {

	for (auto i : inputs)
		i.second->process();

	if (actual and actual->draw()) {
		if (actual == end)
			running = false;
		actual = nullptr;
	}

	if (not actual and running)
		for (auto actor : animations)
			actor->draw();
}

void Profile::reset() {
	running = true;
	actual = nullptr;
	restartActors();
}

void Profile::restart() {
	running = true;
	restartActors();

	if (start) {
		actual = start;
		actual->restart();
	}
}

void Profile::terminate() {
	for (auto i : inputs)
		i.second->deactivate();

	if (end) {
		actual = end;
		actual->restart();
	}
	running = end != nullptr;
}

void Profile::restartActors() {
	for (auto i : inputs)
		i.second->activate();
	for (auto actor : animations)
		actor->restart();
}

bool Profile::isTransiting() const {
	return (actual == start or actual == end) and actual != nullptr;
}

bool Profile::isTerminating() const {
	return actual == end and actual != nullptr;
}

bool Profile::isStarting() const {
	return actual == start and actual != nullptr;
}

bool Profile::isRunning() const {
	return running;
}

const LEDSpicer::Color& Profile::getBackgroundColor() const {
	return backgroundColor;
}

uint8_t Profile::getAnimationsCount() const {
	return animations.size();
}

const string& Profile::getName() const {
	return name;
}

const vector<Element::Item>& Profile::getAlwaysOnElements() const {
	return alwaysOnElements;
}

void Profile::addAlwaysOnElement(Element* element ,const string& color) {
	alwaysOnElements.push_back(Element::Item{element, &Color::getColor(color), Color::Filters::Normal});
}

const vector<Group::Item> & Profile::getAlwaysOnGroups() const {
	return alwaysOnGroups;
}

void Profile::addAlwaysOnGroup(Group* group, const string& color) {
	alwaysOnGroups.push_back(Group::Item{group, &Color::getColor(color), Color::Filters::Normal});
}

void Profile::addInput(string name, Input* input) {
	if (inputs.count(name))
		throw Error(name + " already exist, only one input plugin of this type can be assigned");
	inputs.emplace(name, input);
}
