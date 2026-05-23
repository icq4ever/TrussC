// =============================================================================
// tcSound module implementation
// - Embeds the implementations of stb_vorbis, dr_wav, dr_mp3
// - Defines the file/memory decoder methods of SoundBuffer (declared in tcSound.h)
// =============================================================================

#define TC_SOUND_IMPL

#include <cstdio>
#include <cstring>

// stb_vorbis - OGG Vorbis decoder
extern "C" {
#include "stb_vorbis.c"
}

// dr_wav - WAV decoder
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

// dr_mp3 - MP3 decoder
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "tc/sound/tcSound.h"

namespace trussc {

bool SoundBuffer::loadOgg(const std::string& path) {
    int error = 0;
    stb_vorbis* vorbis = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
    if (!vorbis) {
        printf("SoundBuffer: failed to open %s (error=%d)\n", path.c_str(), error);
        return false;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis);
    channels = info.channels;
    sampleRate = info.sample_rate;
    numSamples = stb_vorbis_stream_length_in_samples(vorbis);

    samples.resize(numSamples * channels);

    int decoded = stb_vorbis_get_samples_float_interleaved(
        vorbis, channels, samples.data(), static_cast<int>(samples.size()));

    stb_vorbis_close(vorbis);

    printf("SoundBuffer: loaded %s (%d ch, %d Hz, %zu samples)\n",
           path.c_str(), channels, sampleRate, numSamples);

    return decoded > 0;
}

bool SoundBuffer::loadWav(const std::string& path) {
    unsigned int ch, sr;
    drwav_uint64 frameCount;

    float* data = drwav_open_file_and_read_pcm_frames_f32(
        path.c_str(), &ch, &sr, &frameCount, nullptr);

    if (!data) {
        printf("SoundBuffer: failed to open WAV %s\n", path.c_str());
        return false;
    }

    channels = ch;
    sampleRate = sr;
    numSamples = frameCount;

    samples.resize(numSamples * channels);
    std::memcpy(samples.data(), data, samples.size() * sizeof(float));

    drwav_free(data, nullptr);

    printf("SoundBuffer: loaded WAV %s (%d ch, %d Hz, %zu samples)\n",
           path.c_str(), channels, sampleRate, numSamples);

    return true;
}

bool SoundBuffer::loadMp3(const std::string& path) {
    drmp3_config config = {0, 0};
    drmp3_uint64 frameCount = 0;

    float* data = drmp3_open_file_and_read_pcm_frames_f32(
        path.c_str(), &config, &frameCount, nullptr);

    if (!data) {
        printf("SoundBuffer: failed to open MP3 %s\n", path.c_str());
        return false;
    }

    channels = config.channels;
    sampleRate = config.sampleRate;
    numSamples = frameCount;

    samples.resize(numSamples * channels);
    std::memcpy(samples.data(), data, samples.size() * sizeof(float));

    drmp3_free(data, nullptr);

    printf("SoundBuffer: loaded MP3 %s (%d ch, %d Hz, %zu samples)\n",
           path.c_str(), channels, sampleRate, numSamples);

    return true;
}

bool SoundBuffer::loadMp3FromMemory(const void* data, size_t dataSize) {
    drmp3_config config = {0, 0};
    drmp3_uint64 frameCount = 0;

    float* decoded = drmp3_open_memory_and_read_pcm_frames_f32(
        data, dataSize, &config, &frameCount, nullptr);

    if (!decoded) {
        printf("SoundBuffer: failed to decode MP3 from memory\n");
        return false;
    }

    channels = config.channels;
    sampleRate = config.sampleRate;
    numSamples = frameCount;

    samples.resize(numSamples * channels);
    std::memcpy(samples.data(), decoded, samples.size() * sizeof(float));

    drmp3_free(decoded, nullptr);

    printf("SoundBuffer: decoded MP3 from memory (%d ch, %d Hz, %zu samples)\n",
           channels, sampleRate, numSamples);

    return true;
}

} // namespace trussc
