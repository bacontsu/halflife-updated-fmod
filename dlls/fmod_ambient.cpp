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
	bool KeyValue(KeyValueData* pkvd) override;

	bool m_fLooping;

private:
	int min_atten;
	int max_atten;
	int pitch;
};

LINK_ENTITY_TO_CLASS(fmod_ambient, CFmodAmbient);

#define FMOD_AMBIENT_LOOPING 1

void CFmodAmbient::Spawn()
{
	// TODO: Throw error if pev->targetname is empty
	if (FBitSet(pev->spawnflags, FMOD_AMBIENT_LOOPING)) m_fLooping = true;

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;
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
	WRITE_BYTE(pev->health); // Volume (0-255). 100 = 100% volume
	WRITE_SHORT(min_atten);	 // Min Attenuation Distance (0-32767)
	WRITE_LONG(max_atten); // Max Attenuation Distance (0-2147483647)
	WRITE_BYTE(pitch); // Pitch (0-255). 100 = normal pitch, 200 = one octave up
	MESSAGE_END();

	// TODO: sanitize inputs
}

// Load key/value pairs
bool CFmodAmbient::KeyValue(KeyValueData* pkvd)
{
	// minatten
	if (FStrEq(pkvd->szKeyName, "minatten"))
	{
		min_atten = atoi(pkvd->szValue);
		return true;
	}

	// maxatten
	else if (FStrEq(pkvd->szKeyName, "maxatten"))
	{
		max_atten = atoi(pkvd->szValue);
		return true;
	}

	// pitch
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pitch = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}