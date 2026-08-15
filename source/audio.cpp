#include "audio.h"
#include "soloud.h"
#include "soloud_wav.h"
#include "soloud_wavstream.h"
#include <mutex>
#include <map>
#include <unordered_map>
#include <stdio.h> 
#include "utils/logger.h"

static SoLoud::Soloud gSoloud;

static std::map<int, SoLoud::AudioSource*> gAudioCache;
static std::mutex gAudioMutex;
static SoLoud::handle gMusicHandle = 0;

static const std::unordered_map<int, const char*> soundFileTable = {
    {0,   "bc9.wav"},
    {1,   "bc8.wav"},
    {2,   "bc7.wav"},
    {3,   "bc6.wav"},
    {4,   "bc5.wav"},
    {5,   "bc4.wav"},
    {6,   "bc3.wav"},
    {7,   "bc2.wav"},
    {8,   "bc1.wav"},
    {9,   "bc.wav"},
    {10,  "Lz9.wav"},
    {11,  "Lz8.wav"},
    {12,  "Lz7.wav"},
    {13,  "Lz6.wav"},
    {14,  "Lz5.wav"},
    {15,  "Lz4.wav"},
    {16,  "Lz3.wav"},
    {17,  "Lz2.wav"},
    {18,  "Lz1.wav"},
    {19,  "Lz.wav"},
    {20,  "7T9.wav"},
    {21,  "7T8.wav"},
    {22,  "7T7.wav"},
    {23,  "7T6.wav"},
    {24,  "7T5.wav"},
    {25,  "7T4.wav"},
    {26,  "7T3.wav"},
    {27,  "7T2.wav"},
    {28,  "7T1.wav"},
    {29,  "7T.wav"},
    {30,  "r-7.wav"},
    {31,  "r-8.wav"},
    {32,  "r-7.wav"},
    {33,  "bc6.wav"},
    {34,  "r-5.wav"},
    {35,  "r-4.wav"},
    {36,  "r-3.wav"},
    {37,  "r-2.wav"},
    {38,  "r-1.wav"},
    {39,  "r-.wav"},
    {40,  "bf1.wav"},
    {41,  "bf.wav"},
    {100, "jh8.wav"},
    {101, "jh7.wav"},
    {102, "jh6.wav"},
    {103, "jh5.wav"},
    {104, "jh4.wav"},
    {105, "jh3.wav"},
    {106, "jh2.wav"},
    {107, "jh1.wav"},
    {108, "jh.wav"},
};

void audio_init() {
    gSoloud.init();
}

void audio_cleanup() {
    std::lock_guard<std::mutex> lock(gAudioMutex);

    gSoloud.stopAll();

    for (auto const& [id, source] : gAudioCache) {
        delete source;
    }
    gAudioCache.clear();

    gSoloud.deinit();
}

void audio_play_sound(int sndID, int vol, int isLoop) {
    
    std::lock_guard<std::mutex> lock(gAudioMutex);

    if (gAudioCache.find(sndID) == gAudioCache.end()) {
        auto soundFileName = soundFileTable.find(sndID);
        if (soundFileName == soundFileTable.end()) {
            l_debug("Unable to find sndID");
            return;
        }

        char filepath[256];
        snprintf(filepath, sizeof(filepath), DATA_PATH "/res/%s", soundFileName->second);

        l_debug("[OnSoundPlay] Attempting to play %s", filepath);

        SoLoud::AudioSource* newSource = nullptr;

        if (sndID < 100) {
            SoLoud::Wav* sfx = new SoLoud::Wav();
            sfx->load(filepath);
            newSource = sfx;
        } else {
            SoLoud::WavStream* music = new SoLoud::WavStream();
            music->load(filepath);
            newSource = music;
        }

        gAudioCache[sndID] = newSource;
    }

    SoLoud::AudioSource* sourceToPlay = gAudioCache[sndID];
    if (sourceToPlay != nullptr) {
        if (sndID >= 100) {
            if (gMusicHandle != 0) {
                gSoloud.stop(gMusicHandle);
            }
            gMusicHandle = gSoloud.play(*sourceToPlay);
            gSoloud.setVolume(gMusicHandle, (float)vol / 100.0f);
            gSoloud.setLooping(gMusicHandle, isLoop != 0);
        } else {
            SoLoud::handle handle = gSoloud.play(*sourceToPlay);
            gSoloud.setVolume(handle, (float)vol / 100.0f);
            gSoloud.setLooping(handle, isLoop != 0);
        }

    }
}

void audio_stop_sound() {
    std::lock_guard<std::mutex> lock(gAudioMutex);
    gSoloud.stopAll();
}