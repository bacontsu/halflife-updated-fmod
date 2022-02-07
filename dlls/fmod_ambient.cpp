#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"
#include <string>
#include <iostream>

class CFmodAmbient : public CBaseEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value) override;

	bool m_fLooping;
};

LINK_ENTITY_TO_CLASS(fmod_ambient, CFmodAmbient);

#define FMOD_AMBIENT_LOOPING 1

void CFmodAmbient::Spawn()
{
	if (FBitSet(pev->spawnflags, FMOD_AMBIENT_LOOPING)) m_fLooping = true;
}

void CFmodAmbient::Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value)
{
	// Name of the channel and path of the sound
	std::string msg = STRING(pev->targetname) + std::string("\n") + STRING(pev->message);
	// TODO: Figure out if we can truly only write one string in a message or if I'm doing something wrong
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodAmb, NULL);
	WRITE_STRING(msg.c_str());
	WRITE_BYTE(m_fLooping);
	WRITE_COORD(pev->origin.x);
	WRITE_COORD(pev->origin.y);
	WRITE_COORD(pev->origin.z);
	MESSAGE_END();
}