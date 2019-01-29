/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/**
 * @file      PulseAudio.cpp
 * @since     Dec 13, 2018
 * @author    Patricio A. Rossi (MeduZa)
 * @copyright Copyright © 2018 Patricio A. Rossi (MeduZa)
 */

#include "PulseAudio.hpp"

using namespace LEDSpicer::Animations;

pa_threaded_mainloop* PulseAudio::tml = nullptr;
pa_context* PulseAudio::context = nullptr;
pa_stream* PulseAudio::stream = nullptr;
uint8_t
	PulseAudio::instances     = 0,
	PulseAudio::totalChannels = 0;
string PulseAudio::source;
uint16_t PulseAudio::peak = MIN_MAX_PEAK;
std::mutex PulseAudio::mutex;

PulseAudio::Values PulseAudio::singleData;
vector<PulseAudio::Values> PulseAudio::vuData;

PulseAudio::PulseAudio(umap<string, string>& parameters, Group* const group) :
	Actor(parameters, group, REQUIRED_PARAM_ACTOR_PULSEAUDIO),
	userPref({
		Color(parameters["off"]),
		Color(parameters["low"]),
		Color(parameters["mid"]),
		Color(parameters["high"]),
		str2mode(parameters["mode"]),
		str2channel(parameters["channel"])
	}),
	total(userPref.channel != Channels::Both ? group->size() : group->size() / 2)
{

	vuData.resize(group->size(), Values());
	switch (userPref.mode) {
	case Modes::VuMeter:
		if (group->size() < VU_MIN_ELEMETS)
			throw Error("To use VU meter a minimum of " + to_string(VU_MIN_ELEMETS) + " elements is needed on the group.");
		break;
	default:
		affectAllElements(true);
	}

	// No bouncing here.
	if (direction == Directions::ForwardBouncing)
		direction = Directions::Forward;
	else if (direction == Directions::BackwardBouncing)
		direction = Directions::Backward;

	++instances;

	LogInfo("Connecting to pulseaudio");

	// If not connected to PA do it.
	if (tml) {
		LogInfo("Reusing Pulseaudio connection");
		return;
	}

	tml = pa_threaded_mainloop_new();
	if (not tml)
		throw Error("Failed create pulseaudio main loop");

	context = pa_context_new(pa_threaded_mainloop_get_api(tml), PACKAGE_STRING);
	if (not context)
		throw Error("Failed to create pulseaudio context");
	pa_context_set_state_callback(context, PulseAudio::onContextSetState, &userPref);
	pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
	pa_threaded_mainloop_start(tml);
	peak = 0;
	LogDebug("Pulseaudio connected");
}

PulseAudio::~PulseAudio() {
	std::lock_guard<std::mutex> lock(mutex);
	--instances;
	if (instances == 0)
		disconnect();
}

void PulseAudio::disconnect() {

	LogInfo("Closing pulseaudio connection");

	pa_signal_done();
	pa_threaded_mainloop_stop(tml);

	if (stream) {
		pa_stream_disconnect(stream);
		pa_stream_unref(stream);
		stream = nullptr;
	}

	if (context) {
		pa_context_disconnect(context);
		pa_context_unref(context);
		context = nullptr;
	}

	if (tml) {
		pa_threaded_mainloop_free(tml);
		tml = nullptr;
	}
}

void PulseAudio::drawConfig() {
	char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];
	cout <<
		"Actor Type: Pulseaudio, mode: " << mode2str(userPref.mode) <<
		", channel: " << channel2str(userPref.channel) <<
		", low: "     << userPref.c00.getName()        <<
		", mid: "     << userPref.c50.getName()        <<
		", high: "    << userPref.c75.getName()        << endl;
	Actor::drawConfig();
	cout << endl;
}

PulseAudio::Modes PulseAudio::str2mode(const string& mode) {
	if (mode == "Levels")
		return Modes::Levels;
	if (mode == "VuMeter")
		return Modes::VuMeter;
	if (mode == "Single")
		return Modes::Single;
	LogError("Invalid mode " + mode + " assuming Single");
	return Modes::Single;
}

string PulseAudio::mode2str(Modes mode) {
	switch (mode) {
	case Modes::Levels:
		return "Levels";
	case Modes::VuMeter:
		return "VuMeter";
	case Modes::Single:
		return "Single";
	}
	return "";
}

PulseAudio::Channels PulseAudio::str2channel(const string& channel) {
	if (channel == "Both")
		return Channels::Both;
	if (channel == "Right")
		return Channels::Right;
	if (channel == "Left")
		return Channels::Left;
	if (channel == "Mono")
		return Channels::Mono;
	LogError("Invalid mode " + channel + " assuming Both");
	return Channels::Both;
}

string PulseAudio::channel2str(Channels channel) {
	switch (channel) {
	case Channels::Both:
		return "Both";
	case Channels::Left:
		return "Left";
	case Channels::Right:
		return "Right";
	case Channels::Mono:
		return "Mono";
	}
	return "";
}

void PulseAudio::onContextSetState(pa_context* context, void* channels) {

	switch (pa_context_get_state(context)) {
	case PA_CONTEXT_CONNECTING:
#ifdef DEVELOP
		LogDebug("Connecting context.");
		break;
#endif
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;

	case PA_CONTEXT_READY:
		LogDebug("Connection to pulseaudio context established");
		pa_operation_unref(pa_context_get_server_info(
			context,
			[] (pa_context* context, const pa_server_info* info, void* userdata) {
				source = info->default_sink_name;
				pa_operation_unref(pa_context_get_sink_info_list(context, onSinkInfo, userdata));
			},
			channels
		));
		pa_context_set_subscribe_callback(
			context,
			[] (pa_context* context, pa_subscription_event_type_t type, uint32_t idx, void* userdata) {
				if (type == PA_SUBSCRIPTION_EVENT_CHANGE)
					pa_operation_unref(pa_context_get_sink_info_by_index(
						context,
						idx,
						[](pa_context* condex, const pa_sink_info* info, int eol, void* userPref) {
							LogDebug("context event");
							peak = MIN_MAX_PEAK;
						},
						nullptr
					));
			},
			nullptr
		);
		pa_context_subscribe(context, PA_SUBSCRIPTION_MASK_SINK, nullptr, nullptr);
		break;

	case PA_CONTEXT_TERMINATED:
		LogDebug("Pulseaudio context terminated");
		break;

	case PA_CONTEXT_FAILED:
	default:
		throw Error("Pulseaudio Context failed: " + string(pa_strerror(pa_context_errno(context))));
	}
}

void PulseAudio::onSinkInfo(pa_context* condex, const pa_sink_info* info, int eol, void* userPref) {

	if (not info)
		return;

	if (source.empty())
		return;

#ifdef DEVELOP
	LogDebug("Finding monitor for " + source);
#endif

	if (source.compare(info->name) != 0) {
#ifdef DEVELOP
	LogDebug("Not found");
#endif
		return;
	}
	source = "";

#ifdef DEVELOP
	LogDebug("Found: "       + string(info->monitor_source_name));
	LogDebug("Index: "       + to_string(info->index));
	LogDebug("Description: " + string(info->description));
	char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];
	LogDebug("Sample: "      + string(pa_sample_spec_snprint(sst, sizeof(sst), &info->sample_spec)));
	LogDebug("Channels: "    + string(pa_channel_map_snprint(cmt, sizeof(cmt), &info->channel_map)));
	LogDebug("Total bytes: " + to_string(pa_frame_size(&info->sample_spec)));
	LogDebug("Sample Size: " + to_string(pa_sample_size(&info->sample_spec)));
#endif

	totalChannels = info->channel_map.channels > 1 ? 2 : 1;

	pa_sample_spec ss = {SAMPLE_FORMAT, RATE, info->sample_spec.channels};

	if (not (stream = pa_stream_new(context, STREAM_NAME, &ss, &info->channel_map)))
		throw Error("Failed to create stream: " + string(pa_strerror(pa_context_errno(context))));

	pa_stream_set_state_callback(stream, onStreamSetState, userPref);

#ifdef DEVELOP
	// underflows
	pa_stream_set_underflow_callback(
		stream,
		[](pa_stream* stream, void* userdata) {
			LogDebug("Underflow");
			peak = MIN_MAX_PEAK;
		},
		nullptr
	);
	// overflows
	pa_stream_set_overflow_callback(
		stream,
		[](pa_stream* stream, void* userdata) {
			LogDebug("overflow");
			peak = MIN_MAX_PEAK;
		},
		nullptr
	);
#endif

	pa_buffer_attr ba;
	ba.maxlength = (uint32_t) -1;
	ba.fragsize  = vuData.size() * 10;
	if (pa_stream_connect_record(stream, info->monitor_source_name, &ba, PA_STREAM_PEAK_DETECT) < 0)
		throw Error("Failed to connect to output stream: " + string(pa_strerror(pa_context_errno(context))));
}

void PulseAudio::onStreamSetState(pa_stream* stream, void* userPref) {

	switch (pa_stream_get_state(stream)) {

	case PA_STREAM_CREATING:
#ifdef DEVELOP
		LogDebug("Creating stream.");
#endif
		break;

	case PA_STREAM_TERMINATED:
#ifdef DEVELOP
		LogDebug("Stream terminated.");
#endif
		break;

	case PA_STREAM_READY:
		LogInfo("Stream successfully created.");
		pa_stream_set_read_callback(stream, onStreamRead, userPref);
		break;

	case PA_STREAM_FAILED:
	default:
		throw Error("Connection to stream failed: " + string(pa_strerror(pa_context_errno(context))));
	}
}

void PulseAudio::onStreamRead(pa_stream* stream, size_t length, void* userdata) {

	// userdata keeps a pointer to userPref
	UserPref* userPref = static_cast<UserPref*>(userdata);

	// Function to reset the data.
	auto reseter = [userPref] () {
		switch (userPref->mode) {
		default:
			singleData = {0, 0};
		break;
		case Modes::Levels:
			vuData.assign(vuData.size(), Values());
		}
	};

	if (not length) {
		reseter();
		return;
	}

	// Read peaks.
	const void *data;
	if (pa_stream_peek(stream, &data, &length) < 0)
		throw Error("Stream peek failed: " + string(pa_strerror(pa_context_errno(context))));

	// Hole detected (no idea what a hole is but is on the docs).
	if (not data and length > 0) {
		reseter();
		pa_stream_drop(stream);
		return;
	}
	// Parse data.
	std::lock_guard<std::mutex> lock(mutex);
	const int16_t* buffer = static_cast<const int16_t*>(data);
	vector<int16_t> rawData(buffer, buffer + (length / 2));
	singleData.l = singleData.r = 0;
	vuData.assign(vuData.size(), Values());
	if (rawData.empty())
		return;
	switch (userPref->mode) {
	default:
		processRawData(rawData, userPref);
	}
	pa_stream_drop(stream);
}

const vector<bool> PulseAudio::calculateElements() {
	std::lock_guard<std::mutex> lock(mutex);
	switch (userPref.mode) {
	case Modes::VuMeter:
		vuMeters();
		break;
	case Modes::Single:
		single();
		break;
	case Modes::Levels:
		levels();
	}
	return affectedElements;
}

void PulseAudio::processRawData(vector<int16_t>& rawData, UserPref* userPref) {

	uint16_t
		m   = 0,
		v   = 0,
		rec = 0;

	for (size_t c = 0; c < rawData.size(); ++c) {

		v = abs(rawData[c]);

		// Even for left, odd for right.
		if (c % 2) {
			if (v > singleData.r)
				singleData.r = v;
			if (v > vuData[rec].r)
				vuData[rec].r = v;
		}
		else {
			if (v > singleData.l)
				singleData.l = v;
			if (v > vuData[rec].l)
				vuData[rec].l = v;
			++rec;
			if (rec >= vuData.size() / 2)
				rec = 0;
		}
	}

	m = (peak + singleData.l + singleData.r) / (totalChannels + 1);
	if (m > peak)
		peak = m;
	else
		peak -= MIN_MAX_PEAK;

#ifdef DEVELOP
		cout << "L:" << std::left << std::setfill(' ') << std::setw(5) << to_string(singleData.l) << " R:" << std::setw(5) << to_string(singleData.r) << " Peak:" << std::setw(5) << to_string(peak) << endl;
#endif
}

#define TO_PERC(x) round(x * 100.00 / peak)

void PulseAudio::vuMeters() {

	uint16_t
		leftVal,
		rightVal;
	/*
	 * Function to draw the elements.
	 */
	auto tintFn = [&] (uint8_t elems, uint8_t start, bool reverse) {
		for (uint8_t e = 0; e <= elems; ++e) {
			uint8_t s = reverse ? start - e : start + e;
#ifdef DEVELOP
			cout << std::setw(3) << to_string(s);detectColor(round(e * 100.00 / total));
#else
			changeElementColor(s, detectColor(round((e + 1) * 100.00 / total)), filter);
#endif
			affectedElements[s] = true;
		}
	};

	// Clean.
	affectAllElements();

	// Convert to mono and them to elements.
	if (userPref.channel == Channels::Mono) {
		leftVal  = total * TO_PERC((singleData.l + singleData.r) / 2) / 100;
		if (leftVal > total)
			leftVal = total;
		if (leftVal) {
			--leftVal;
			if (leftVal) {
				if (direction == Directions::Forward)
					tintFn(leftVal, 0, false);
				else
					tintFn(leftVal, getNumberOfElements() - 1, true);
			}
		}
		return;
	}

	// Convert left channel peak to elements.
	if (userPref.channel & Channels::Left) {
		leftVal  = total * TO_PERC(singleData.l) / 100;
		if (leftVal > total)
			leftVal = total;
		if (leftVal) {
			--leftVal;
			if (leftVal) {
				if (direction == Directions::Forward)
					tintFn(leftVal, 0, false);
				else
					tintFn(leftVal, total, true);
			}
		}
	}

	// Convert right channel peak to elements.
	if (userPref.channel & Channels::Right) {
		rightVal = total * TO_PERC(singleData.r) / 100;
		if (rightVal > total)
			rightVal = total;
		if (rightVal) {
			--rightVal;
			if (rightVal) {
				if (direction == Directions::Forward)
						tintFn(rightVal, getNumberOfElements() - 1, true);
					else
						tintFn(rightVal, total, false);
				}
		}
	}
}

void PulseAudio::levels() {
	uint8_t e = userPref.channel == Channels::Both ? total : 0;
	for (uint8_t c = 0; c < total; ++c) {
		if (userPref.channel & Channels::Mono)
			changeElementColor(c, detectColor(TO_PERC(vuData[c].l > vuData[c].r ? vuData[c].l : vuData[c].r)), filter);
		if (userPref.channel & Channels::Left)
			changeElementColor(c, detectColor(TO_PERC(vuData[c].l)), filter);
		if (userPref.channel & Channels::Right)
			changeElementColor(e + c, detectColor(TO_PERC(vuData[c].r)), filter);
	}
}

void PulseAudio::single() {
	uint8_t e = userPref.channel == Channels::Both ? total : 0;
	for (uint8_t c = 0; c < total; ++c) {
		if (userPref.channel & Channels::Mono)
			changeElementColor(c, detectColor(TO_PERC(singleData.l > singleData.r ? singleData.l : singleData.r)), filter);
		if (userPref.channel & Channels::Left)
			changeElementColor(c, detectColor(TO_PERC(singleData.l)), filter);
		if (userPref.channel & Channels::Right)
			changeElementColor(e + c, detectColor(TO_PERC(singleData.r)), filter);
	}
}

LEDSpicer::Color PulseAudio::detectColor(uint8_t percent) {

	// Off.
	if (not percent)
		return userPref.off;

	#define CALC_PERC(t, b) round((percent - b) * 100.00 / (t - b))

	// Mid -> High.
	if (percent > MID_POINT)
		return userPref.c50.transition(userPref.c75, CALC_PERC(100, MID_POINT));

	// Low -> Mid.
	if (percent > LOW_POINT)
		return userPref.c00.transition(userPref.c50, CALC_PERC(MID_POINT, LOW_POINT));

	// 0 -> low.
	return userPref.c00;

}
