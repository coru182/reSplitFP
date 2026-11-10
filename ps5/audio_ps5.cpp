// reSplitFP: the runtime's mixer played through the PS5's audio out, 48 kHz 16-bit stereo. The port's clock paces
// the loop: sceAudioOutOutput returns once the previous 256 frames (5.3 ms) have been taken, as reUndr does.
// Replaces audio/xaudio2.cpp (Windows) and the harness's null_audio.cpp.
#include <pthread.h>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <vector>

#include <algorithm>

#include "app/app.h"
#include "audio/audio.h"
#include "core/log.h"

extern "C" {
int sceAudioOutInit(void);
int sceAudioOutOpen(int user, int type, int index, unsigned grain, unsigned rate, unsigned format);
int sceAudioOutOutput(int handle, const void* samples);
int sceAudioOutSetVolume(int handle, int channel_mask, const int* volumes);
}

namespace audio {

namespace {

constexpr unsigned kGrain = 256;      // frames per output call
constexpr unsigned kS16Stereo = 1;    // SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO
constexpr int kSystemUser = 255;      // SCE_USER_SERVICE_USER_ID_SYSTEM
constexpr int kMainPort = 0;          // SCE_AUDIO_OUT_PORT_TYPE_MAIN

struct Output {
    std::function<void(float*, uint32_t)> render;
    int handle = -1;
};

void* Run(void* p) {
    auto* o = static_cast<Output*>(p);
    std::vector<float> f(kGrain * 2);
    std::vector<int16_t> s(kGrain * 2);
    for (;;) {
        // held (the in-game menu is open): the mixer takes no time, so the game's sounds and music wait where they
        // were; muted, they play on unheard (as audio/xaudio2.cpp)
        if (app::Held()) std::fill(f.begin(), f.end(), 0.0f);
        else {
            o->render(f.data(), kGrain);
            if (app::Muted()) std::fill(f.begin(), f.end(), 0.0f);
        }
        for (size_t i = 0; i < f.size(); i++) {
            float x = f[i] < -1.f ? -1.f : f[i] > 1.f ? 1.f : f[i];
            s[i] = int16_t(x * 32767.f);
        }
        int rc = sceAudioOutOutput(o->handle, s.data());
        if (rc < 0) {
            LOGE("audio: output failed %08X, sound stops", unsigned(rc));
            return nullptr;
        }
    }
}

}  // namespace

void StartOutput(std::function<void(float*, uint32_t)> render) {
    static_assert(kRate == 48000, "the PS5 output runs at 48 kHz");
    sceAudioOutInit();
    auto* o = new Output{std::move(render)};
    o->handle = sceAudioOutOpen(kSystemUser, kMainPort, 0, kGrain, kRate, kS16Stereo);
    if (o->handle < 0) {
        LOGE("audio: sceAudioOutOpen failed %08X, the game runs without sound", unsigned(o->handle));
        delete o;
        return;
    }
    const int unity[2] = {32768, 32768};  // 0 dB on both channels
    sceAudioOutSetVolume(o->handle, 3, unity);
    LOGI("audio: PS5 audio out, 48000 Hz 16-bit stereo, port %d", o->handle);
    pthread_t t;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &a, Run, o);
    pthread_attr_destroy(&a);
}

}  // namespace audio
