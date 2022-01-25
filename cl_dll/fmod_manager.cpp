#include "cl_dll.h"
#include "cl_util.h"
#include "extdll.h"
//#include "util.h"
#include "fmod_manager.h"
#include "FMOD/fmod_errors.h"

#include <iostream>

FMOD::System *fmod_system;
Fmod_Group fmod_mp3_group;
Fmod_Group fmod_sfx_group;

bool Fmod_Init(void)
{
    FMOD_RESULT result;

    // Create the main system object.
    result = FMOD::System_Create(&fmod_system);
    if (!_Fmod_Result_OK(&result)) return false;

    // Initialize FMOD.
    result = fmod_system->init(512, FMOD_INIT_NORMAL, 0);
    if (!_Fmod_Result_OK(&result)) return false;

    // Create mp3 channel group
    result = fmod_system->createChannelGroup("MP3", &fmod_mp3_group);
    if (!_Fmod_Result_OK(&result)) return false;
    fmod_mp3_group->setVolume(1.0f);

    // Create sfx channel group
    result = fmod_system->createChannelGroup("SFX", &fmod_sfx_group);
    if (!_Fmod_Result_OK(&result)) return false;
    fmod_sfx_group->setVolume(1.0f);

    _Fmod_Update_Volume();

    return true;
}

void Fmod_Think(void)
{
    FMOD_RESULT result;

    // Update FMOD
    result = fmod_system->update();
    _Fmod_Result_OK(&result);

    _Fmod_Update_Volume();
}

Fmod_Sound Fmod_LoadSound(const char *path)
{
    FMOD_RESULT result;
    Fmod_Sound sound;

    std::string gamedir = gEngfuncs.pfnGetGameDirectory();
    std::string full_path = gamedir + "/" + path; 

    result = fmod_system->createSound(full_path.c_str(), FMOD_DEFAULT, NULL, &sound);
    _Fmod_Result_OK(&result);

    return sound;
}

void Fmod_PlaySound(Fmod_Sound sound, Fmod_Group group, float volume)
{
    FMOD_RESULT result;
    FMOD::Channel *sound_channel;

    // Play sound paused so we can apply the volume first
    result = fmod_system->playSound(sound, group, true, &sound_channel);
    _Fmod_Result_OK(&result);

    sound_channel->setVolume(volume);
    sound_channel->setPaused(false);
}

void Fmod_Shutdown(void)
{
    FMOD_RESULT result;

    result = fmod_system->release();
    _Fmod_Result_OK(&result);
}

void _Fmod_Update_Volume(void)
{
    // Check if user has changed volumes and update accordingly
    // TODO: See if this can be done in an event-based way instead
    float new_mp3_vol = CVAR_GET_FLOAT("MP3Volume"); // TODO: Get volume slider value somehow
    float old_mp3_vol = 1.0f;
    fmod_mp3_group->getVolume(&old_mp3_vol);
    if (new_mp3_vol != old_mp3_vol)
        fmod_mp3_group->setVolume(new_mp3_vol);

    float new_sfx_vol = CVAR_GET_FLOAT("volume"); // TODO: Get volume slider value somehow
    float old_sfx_vol = 1.0f;
    fmod_sfx_group->getVolume(&old_sfx_vol);
    if (new_sfx_vol != old_sfx_vol)
        fmod_sfx_group->setVolume(new_sfx_vol);
}

// Check result and return true if OK or false if not OK
// Sometimes return value is ignored and this is used for throwing warnings only
bool _Fmod_Result_OK (FMOD_RESULT *result)
{
    if (*result != FMOD_OK)
    {
        std::string fmod_error_str = FMOD_ErrorString(*result);
        std::string error_msg = "FMOD ERROR: " + *result + fmod_error_str + "\n";
        fprintf(stderr, error_msg.c_str());
        ConsolePrint(error_msg.c_str());
        return false;
    }

    return true;
}