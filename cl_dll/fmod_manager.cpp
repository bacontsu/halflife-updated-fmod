#include "extdll.h"
#include "util.h"
#include "fmod_manager.h"
#include "FMOD/fmod_errors.h"

FMOD::System *fmod_system;

bool Fmod_Init(void)
{
    FMOD_RESULT result;

    result = FMOD::System_Create(&fmod_system);      // Create the main system object.
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        return false;
    }

    result = fmod_system->init(512, FMOD_INIT_NORMAL, 0);    // Initialize FMOD.
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        return false;
    }

    return true;
}

void Fmod_Think(void)
{
    FMOD_RESULT result;

    result = fmod_system->update();
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
    }
}

Fmod_Sound Fmod_LoadSound(const char *path)
{
    FMOD_RESULT result;
    Fmod_Sound sound;

    result = fmod_system->createSound(path, FMOD_DEFAULT, NULL, &sound);
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
    }

    return sound;
}

void Fmod_PlaySound(Fmod_Sound sound)
{
    FMOD_RESULT result;

    result = fmod_system->playSound(sound, NULL, false, NULL);
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
    }
}

void Fmod_Shutdown(void)
{
    FMOD_RESULT result;

    result = fmod_system->release();
    if (result != FMOD_OK)
    {
        ALERT(at_console, "FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
    }
}