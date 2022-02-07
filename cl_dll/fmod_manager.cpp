#include "cl_dll.h"
#include "cl_util.h"
#include "extdll.h"
#include "hud.h"
#include "parsemsg.h"
#include "fmod_manager.h"
#include "FMOD/fmod_errors.h"

#include <fstream>
#include <iostream>

FMOD::System *fmod_system;
Fmod_Group fmod_mp3_group;
Fmod_Group fmod_sfx_group;

//Fmod_Sound fmod_main_menu_music;

std::unordered_map<std::string, FMOD::Sound*> fmod_cached_sounds;
std::unordered_map<std::string, FMOD::Channel*> fmod_channels;

bool Fmod_Init(void)
{
    FMOD_RESULT result;
    //fmod_main_menu_music = NULL;

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

void Fmod_Release_Sounds(void)
{
	auto sounds_it = fmod_cached_sounds.begin();

    while (sounds_it != fmod_cached_sounds.end())
	{
		sounds_it->second->release();
		sounds_it++;
    }

    fmod_cached_sounds.clear();
}

void Fmod_Release_Channels(void)
{
	auto channels_it = fmod_channels.begin();

	while (channels_it != fmod_channels.end())
	{
		channels_it->second->stop();
		channels_it++;
	}

	fmod_channels.clear();
}

FMOD::Sound* Fmod_CacheSound(const char* path, bool is_track)
{
    FMOD_RESULT result;
    FMOD::Sound *sound = NULL;

    std::string gamedir = gEngfuncs.pfnGetGameDirectory();
	std::string full_path = gamedir + "/" + path; 

    // Create the sound/stream from the file on disk
    if (!is_track)
        result = fmod_system->createSound(full_path.c_str(), FMOD_DEFAULT, NULL, &sound);
	else
		result = fmod_system->createStream(full_path.c_str(), FMOD_DEFAULT, NULL, &sound);

    // TODO: investigate if a failure here might create a memory leak
	if (!_Fmod_Result_OK(&result)) return NULL;

    // If all went okay, insert the sound into the cache
	fmod_cached_sounds.insert(std::pair(path, sound));

    return sound;
}

FMOD::Channel* Fmod_CreateChannel(FMOD::Sound* sound, const char* name, Fmod_Group group, bool loop, float volume)
{
	FMOD_RESULT result;
	FMOD::Channel* channel = NULL;

	// Create the sound channel
	// We "play" the sound to create the channel, but it's starting paused so we can set properties on it before it /actually/ plays.
	result = fmod_system->playSound(sound, group, true, &channel);
	if (!_Fmod_Result_OK(&result)) return NULL; // TODO: investigate if a failure here might create a memory leak

    // Set channel properties
	channel->setVolume(volume);
	if (loop)
	{
		channel->setMode(FMOD_LOOP_NORMAL);
	}

    // If all went okay, stick it in the channel list. Only looping sounds store a reference.
	fmod_channels.insert(std::pair(name, channel));

	return channel;
}

/* void Fmod_PlayMainMenuMusic(void)
{
    std::string gamedir = gEngfuncs.pfnGetGameDirectory();
	std::string cfg_path = gamedir + FMOD_PATH_SEP + "menu_music.cfg";

    std::ifstream cfg_file;
    cfg_file.open(cfg_path);

    if (cfg_file.fail())
	{
		std::string error_msg = "FMOD ERROR: Could not open menu_music.cfg\n";
		fprintf(stderr, error_msg.c_str());
		ConsolePrint(error_msg.c_str());
		return;
    }

    std::string music_file_path = "";
	cfg_file >> music_file_path;

    if (music_file_path.compare("") != 0)
    {
        // Get volume from cfg file
        std::string volume_str = "";
        float volume = 1.0f;
		cfg_file >> volume_str;

        if (volume_str.compare("") != 0)
			volume = std::stof(volume_str);
		else
			volume = 1.0f;
        
        fmod_main_menu_music = Fmod_LoadSound(music_file_path.c_str());
        Fmod_PlaySound(fmod_main_menu_music, fmod_mp3_group, true, volume);
    }

    cfg_file.close();
}*/

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

// Shortcut to send message to both stderr and console
void _Fmod_Report(std::string report_type, std::string info)
{
	std::string msg = "FMOD " + report_type +": " + info + "\n";
	fprintf(stderr, msg.c_str());
	ConsolePrint(msg.c_str());
}

DECLARE_MESSAGE(m_Fmod, FmodCache)
DECLARE_MESSAGE(m_Fmod, FmodAmb)
DECLARE_MESSAGE(m_Fmod, FmodTrk)

bool CHudFmodPlayer::Init()
{
	HOOK_MESSAGE(FmodCache);
	HOOK_MESSAGE(FmodAmb);
	HOOK_MESSAGE(FmodTrk);

	gHUD.AddHudElem(this);
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodCache(const char* pszName, int iSize, void* pbuf)
{
	std::string gamedir = gEngfuncs.pfnGetGameDirectory();
	std::string level_name = gEngfuncs.pfnGetLevelName();
	std::string soundcache_path = gamedir + "/" + level_name + "_soundcache.txt";

    Fmod_Release_Channels();
	Fmod_Release_Sounds(); // TODO: find elegant way to avoid unloading and reloading sounds. For now just release all.

    std::ifstream soundcache_file;
	soundcache_file.open(soundcache_path);

	if (!soundcache_file.is_open())
	{
		_Fmod_Report("WARNING", "Could not open soundcache file " + soundcache_path + ". No sounds were precached!");
		return true;
	}
	else _Fmod_Report("INFO", "Precaching from file: " + soundcache_path);

	std::string filename;
	while (std::getline(soundcache_file, filename))
	{
		FMOD::Sound *sound = Fmod_CacheSound(filename.c_str(), false);
		if (!sound)
		{
			_Fmod_Report("ERROR", "Error occured during precaching. Tried precaching: " + filename + ". Precaching stopped.");
			return false;
        }
    }

    if (!soundcache_file.eof())
        _Fmod_Report("WARNING", "Stopped reading soundcache file " + soundcache_path + " before the end of file due to error.");

	soundcache_file.close();

    return true;
}

bool CHudFmodPlayer::MsgFunc_FmodAmb(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string msg = std::string(READ_STRING());
	bool looping = READ_BYTE();

    std::string channel_name = msg.substr(0, msg.find('\n'));
	std::string sound_path = msg.substr(msg.find('\n')+1, std::string::npos);

    FMOD::Sound *sound = NULL;
	FMOD::Channel *channel = NULL;

    auto sound_iter = fmod_cached_sounds.find(sound_path);
	
    if (sound_iter == fmod_cached_sounds.end())
	{
		_Fmod_Report("WARNING", "Entity " + channel_name + " playing uncached sound " + sound_path + 
            ". Add the sound to your [MAPNAME].bsp_soundcache.txt file.");
		_Fmod_Report("WARNING", "Attempting to cache and play sound " + sound_path);
		sound = Fmod_CacheSound(sound_path.c_str(), false);
		if (!sound) return false;
    }
	else sound = sound_iter->second;

    // Non-looping sounds need a new channel every time they play
    if (!looping)
	{
		channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, false, 1.0f);
        if (!channel) return false;
		channel->setPaused(false);
    }

    // If looping, find the existing channel
    else
	{
        auto channel_iter = fmod_channels.find(channel_name);

        if (channel_iter == fmod_channels.end())
	    {
            // TODO: send looping and volume info from entity
		    channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, true, 1.0f);
		    if (!channel) return false;
        }
	    else channel = channel_iter->second;

        // When a looping fmod_ambient gets used, by default it'll flip the status of paused
		bool paused = false;
		channel->getPaused(&paused);
	    channel->setPaused(!paused);
    }

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodTrk(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const char* msg = READ_STRING();

	//Fmod_Sound_Container sound = Fmod_LoadSound(msg);
	//Fmod_PlaySound(sound, fmod_mp3_group, false, 1.0f);

	return true;
}

bool CHudFmodPlayer::VidInit() { return true; }
bool CHudFmodPlayer::Draw(float flTime) { return true; }
void CHudFmodPlayer::Reset() { return; }