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

#include "DualSourceMixer.h"

#if SSR_USE_PULSEAUDIO

#include "Logger.h"

void DualSourceMixer::ChannelSink::ReadAudioSamples(unsigned int channels, unsigned int sample_rate, AVSampleFormat format, unsigned int sample_count, const uint8_t* data, int64_t timestamp) {
	Q_UNUSED(sample_rate);
	Q_UNUSED(format);
	Q_UNUSED(timestamp);
	// We expect mono S16LE from each PulseAudioInput
	if(channels != 1 || sample_count == 0)
		return;
	std::lock_guard<std::mutex> lock(m_buffer.mutex);
	const int16_t *samples = (const int16_t*) data;
	m_buffer.samples.insert(m_buffer.samples.end(), samples, samples + sample_count);
	m_buffer.started = true;
}

void DualSourceMixer::ChannelSink::ReadAudioHole() {
	std::lock_guard<std::mutex> lock(m_buffer.mutex);
	m_buffer.hole = true;
}

DualSourceMixer::DualSourceMixer(const QString& source_left, const QString& source_right, unsigned int sample_rate)
	: m_source_left(source_left), m_source_right(source_right), m_sample_rate(sample_rate),
	  m_sink_left(m_buffer_left), m_sink_right(m_buffer_right) {

	Logger::LogInfo("[DualSourceMixer::DualSourceMixer] " + Logger::tr("Starting dual-source mixer (left: %1, right: %2) ...").arg(source_left).arg(source_right));

	try {
		// Create two mono PulseAudioInput instances
		m_input_left.reset(new PulseAudioInput(source_left, sample_rate, 1));
		m_input_right.reset(new PulseAudioInput(source_right, sample_rate, 1));

		// Connect each input to its channel sink
		m_sink_left.ConnectAudioSource(m_input_left.get());
		m_sink_right.ConnectAudioSource(m_input_right.get());

		// Start the mixer thread
		m_should_stop = false;
		m_thread = std::thread(&DualSourceMixer::MixerThread, this);

	} catch(...) {
		m_sink_left.ConnectAudioSource(NULL);
		m_sink_right.ConnectAudioSource(NULL);
		m_input_left.reset();
		m_input_right.reset();
		throw;
	}

	Logger::LogInfo("[DualSourceMixer::DualSourceMixer] " + Logger::tr("Started dual-source mixer."));
}

DualSourceMixer::~DualSourceMixer() {
	Logger::LogInfo("[DualSourceMixer::~DualSourceMixer] " + Logger::tr("Stopping dual-source mixer ..."));

	// Stop the mixer thread
	if(m_thread.joinable()) {
		m_should_stop = true;
		m_thread.join();
	}

	// Disconnect sinks before destroying inputs
	m_sink_left.ConnectAudioSource(NULL);
	m_sink_right.ConnectAudioSource(NULL);

	// Destroy inputs
	m_input_left.reset();
	m_input_right.reset();

	Logger::LogInfo("[DualSourceMixer::~DualSourceMixer] " + Logger::tr("Stopped dual-source mixer."));
}

void DualSourceMixer::MixerThread() {
	try {

		Logger::LogInfo("[DualSourceMixer::MixerThread] " + Logger::tr("Mixer thread started."));

		// Maximum samples for one channel before we output silence for the missing channel
		// 100ms at the configured sample rate
		const unsigned int silence_threshold = m_sample_rate / 10;

		std::vector<int16_t> output;

		while(!m_should_stop) {

			usleep(5000); // 5ms

			unsigned int left_count = 0, right_count = 0;
			bool left_hole = false, right_hole = false;
			bool left_started = false, right_started = false;

			{
				std::lock_guard<std::mutex> lock_l(m_buffer_left.mutex);
				std::lock_guard<std::mutex> lock_r(m_buffer_right.mutex);

				left_count = m_buffer_left.samples.size();
				right_count = m_buffer_right.samples.size();
				left_hole = m_buffer_left.hole;
				right_hole = m_buffer_right.hole;
				left_started = m_buffer_left.started;
				right_started = m_buffer_right.started;
			}

			// Handle holes: if either side has a hole, propagate it
			if(left_hole || right_hole) {
				{
					std::lock_guard<std::mutex> lock_l(m_buffer_left.mutex);
					std::lock_guard<std::mutex> lock_r(m_buffer_right.mutex);
					m_buffer_left.samples.clear();
					m_buffer_right.samples.clear();
					m_buffer_left.hole = false;
					m_buffer_right.hole = false;
					m_buffer_left.started = false;
					m_buffer_right.started = false;
				}
				PushAudioHole();
				continue;
			}

			// Wait until both sides have started
			if(!left_started || !right_started) {
				// Safety: if one side has started and accumulated too much, output with silence for the other
				if(left_started && left_count > silence_threshold) {
					// Output left with silence on right
					std::lock_guard<std::mutex> lock_l(m_buffer_left.mutex);
					unsigned int count = m_buffer_left.samples.size();
					if(count > 0) {
						output.resize(count * 2);
						for(unsigned int i = 0; i < count; ++i) {
							output[i * 2 + 0] = m_buffer_left.samples[i]; // L = mic
							output[i * 2 + 1] = 0;                         // R = silence
						}
						m_buffer_left.samples.clear();
						int64_t timestamp = hrt_time_micro();
						PushAudioSamples(2, m_sample_rate, AV_SAMPLE_FMT_S16, count, (const uint8_t*) output.data(), timestamp);
					}
				} else if(right_started && right_count > silence_threshold) {
					// Output right with silence on left
					std::lock_guard<std::mutex> lock_r(m_buffer_right.mutex);
					unsigned int count = m_buffer_right.samples.size();
					if(count > 0) {
						output.resize(count * 2);
						for(unsigned int i = 0; i < count; ++i) {
							output[i * 2 + 0] = 0;                          // L = silence
							output[i * 2 + 1] = m_buffer_right.samples[i];  // R = app audio
						}
						m_buffer_right.samples.clear();
						int64_t timestamp = hrt_time_micro();
						PushAudioSamples(2, m_sample_rate, AV_SAMPLE_FMT_S16, count, (const uint8_t*) output.data(), timestamp);
					}
				}
				continue;
			}

			// Both sides have started - interleave the minimum of both
			unsigned int min_samples = std::min(left_count, right_count);

			if(min_samples == 0) {
				// Safety: if one buffer has exceeded the threshold, output with silence for the other
				if(left_count > silence_threshold) {
					std::lock_guard<std::mutex> lock_l(m_buffer_left.mutex);
					unsigned int count = m_buffer_left.samples.size();
					if(count > 0) {
						output.resize(count * 2);
						for(unsigned int i = 0; i < count; ++i) {
							output[i * 2 + 0] = m_buffer_left.samples[i];
							output[i * 2 + 1] = 0;
						}
						m_buffer_left.samples.clear();
						int64_t timestamp = hrt_time_micro();
						PushAudioSamples(2, m_sample_rate, AV_SAMPLE_FMT_S16, count, (const uint8_t*) output.data(), timestamp);
					}
				} else if(right_count > silence_threshold) {
					std::lock_guard<std::mutex> lock_r(m_buffer_right.mutex);
					unsigned int count = m_buffer_right.samples.size();
					if(count > 0) {
						output.resize(count * 2);
						for(unsigned int i = 0; i < count; ++i) {
							output[i * 2 + 0] = 0;
							output[i * 2 + 1] = m_buffer_right.samples[i];
						}
						m_buffer_right.samples.clear();
						int64_t timestamp = hrt_time_micro();
						PushAudioSamples(2, m_sample_rate, AV_SAMPLE_FMT_S16, count, (const uint8_t*) output.data(), timestamp);
					}
				}
				continue;
			}

			// Interleave left and right into stereo
			{
				std::lock_guard<std::mutex> lock_l(m_buffer_left.mutex);
				std::lock_guard<std::mutex> lock_r(m_buffer_right.mutex);

				// Recalculate min_samples under lock
				min_samples = std::min((unsigned int) m_buffer_left.samples.size(), (unsigned int) m_buffer_right.samples.size());
				if(min_samples == 0)
					continue;

				output.resize(min_samples * 2);
				for(unsigned int i = 0; i < min_samples; ++i) {
					output[i * 2 + 0] = m_buffer_left.samples[i];   // L channel = mic
					output[i * 2 + 1] = m_buffer_right.samples[i];  // R channel = app audio
				}

				// Erase consumed samples
				m_buffer_left.samples.erase(m_buffer_left.samples.begin(), m_buffer_left.samples.begin() + min_samples);
				m_buffer_right.samples.erase(m_buffer_right.samples.begin(), m_buffer_right.samples.begin() + min_samples);
			}

			int64_t timestamp = hrt_time_micro();
			PushAudioSamples(2, m_sample_rate, AV_SAMPLE_FMT_S16, min_samples, (const uint8_t*) output.data(), timestamp);

		}

		Logger::LogInfo("[DualSourceMixer::MixerThread] " + Logger::tr("Mixer thread stopped."));

	} catch(const std::exception& e) {
		Logger::LogError("[DualSourceMixer::MixerThread] " + Logger::tr("Exception '%1' in mixer thread.").arg(e.what()));
	} catch(...) {
		Logger::LogError("[DualSourceMixer::MixerThread] " + Logger::tr("Unknown exception in mixer thread."));
	}
}

#endif
