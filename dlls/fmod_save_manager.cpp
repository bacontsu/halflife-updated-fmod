#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"
#include "fmod_save_manager.h"
#include <string>
#include <iostream>
#include <ctime>
#include <sstream>

LINK_ENTITY_TO_CLASS(fmod_save_manager, CFmodSaveManager);

// Global Savedata for fmod_save_manager
TYPEDESCRIPTION CFmodSaveManager::m_fmodSaveData[] =
	{
		DEFINE_ARRAY(CFmodSaveManager, m_fmodSaveName, FIELD_CHARACTER, FMOD_SAVE_LENGTH),
};

void CFmodSaveManager::Spawn()
{
	ALERT(at_error, "FMOD SAVE MANAGER SPAWNED\n");
}

bool CFmodSaveManager::Save(CSave& save)
{
	// Get string representation of UNIX time
	std::time_t time = std::time(0);
	std::stringstream ss;
	ss << time;
	std::string ts = ss.str();

	// Get game directory
	char gamedir[64];
	GET_GAME_DIR(gamedir);

	// Create the save name string and copy it to the member field
	std::string fmod_save_name = std::string(gamedir) + std::string("/SAVE/") + std::string("fmod") + ts + ".fsv";
	strncpy(this->m_fmodSaveName, fmod_save_name.c_str(), FMOD_SAVE_LENGTH - 1);

	// Tell fmod on the clientside to save
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodSave, NULL);
	WRITE_STRING(this->m_fmodSaveName);
	MESSAGE_END();

	return save.WriteFields("FMODSAVEMANAGER", this, m_fmodSaveData, ARRAYSIZE(m_fmodSaveData));
}

bool CFmodSaveManager::Restore(CRestore& restore)
{
	ALERT(at_error, "FMOD SAVE MANAGER RESTORED\n");

	bool status = restore.ReadFields("FMODSAVEMANAGER", this, m_fmodSaveData, ARRAYSIZE(m_fmodSaveData));

	SetThink(&CFmodSaveManager::SendMsg);
	pev->nextthink = gpGlobals->time + 0.01f;

	return status;
}

void CFmodSaveManager::SendMsg(void)
{
	// Tell fmod on the clientside to load
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodLoad, NULL);
	WRITE_STRING(this->m_fmodSaveName);
	MESSAGE_END();
}