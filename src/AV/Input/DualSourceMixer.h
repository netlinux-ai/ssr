/*
Copyright (c) 2012-2020 Maarten Baert <maarten-baert@hotmail.com>

This file is part of SimpleScreenRecorder.

SimpleScreenRecorder is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SimpleScreenRecorder is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SimpleScreenRecorder.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include "Global.h"

#if SSR_USE_PULSEAUDIO

#include "SourceSink.h"
#include "PulseAudioInput.h"

class DualSourceMixer : public AudioSource {

private:
	struct ChannelBuffer {
		std::mutex mutex;
		std::vector<int16_t> samples;
		bool started;
		bool hole;
		inline ChannelBuffer() : started(false), hole(false) {}
	};

	class ChannelSink : public AudioSink {
	private:
		ChannelBuffer &m_buffer;
	public:
		inline ChannelSink(ChannelBuffer &buffer) : m_buffer(buffer) {}
		virtual void ReadAudioSamples(unsigned int channels, unsigned int sample_rate, AVSampleFormat format, unsigned int sample_count, const uint8_t* data, int64_t timestamp) override;
		virtual void ReadAudioHole() override;
	};

	QString m_source_left, m_source_right;
	unsigned int m_sample_rate;

	std::unique_ptr<PulseAudioInput> m_input_left, m_input_right;
	ChannelBuffer m_buffer_left, m_buffer_right;
	ChannelSink m_sink_left, m_sink_right;

	std::thread m_thread;
	std::atomic<bool> m_should_stop;

	void MixerThread();

public:
	DualSourceMixer(const QString& source_left, const QString& source_right,
					unsigned int sample_rate);
	~DualSourceMixer();

};

#endif
