#ifndef MORPH_AUDIO_H
#define MORPH_AUDIO_H

#include "platform.h"

#define AUDIO_RATE 48000
#define DEMO_SECONDS 60

extern SDL_atomic_t demo_clock;

SDL_AudioDeviceID audio_open(void);
void audio_resume(SDL_AudioDeviceID);

#endif
