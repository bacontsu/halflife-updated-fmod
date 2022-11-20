#pragma once
#include "cbase.h"

#define FMOD_SAVE_LENGTH 64

class CFmodSaveManager : public CBaseEntity
{
public:
	void Spawn() override;
	bool Save(CSave& save) override;
	bool Restore(CRestore& restore) override;
	void EXPORT SendMsg(void);

	char m_fmodSaveName[FMOD_SAVE_LENGTH];

	static TYPEDESCRIPTION m_fmodSaveData[];
};