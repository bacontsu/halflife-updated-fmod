#pragma once

#include <string>
#include "FMOD/fmod.hpp"

typedef FMOD::Sound *Fmod_Sound;

extern FMOD::System *fmod_system;

bool Fmod_Init(void);
void Fmod_Think(void);
Fmod_Sound Fmod_LoadSound(const char *path);
void Fmod_PlaySound(Fmod_Sound sound);
void Fmod_Shutdown(void);