#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"
#include <string>
#include <iostream>

class CFmodTrack : public CBaseEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value) override;
	bool KeyValue(KeyValueData* pkvd) override;
	void EXPORT SendMsg(void);

	bool m_fLooping;
	bool m_fPlayOnStart;

private:
	int pitch;
};

LINK_ENTITY_TO_CLASS(fmod_track, CFmodTrack);

#define FMOD_TRACK_LOOPING 1
#define FMOD_TRACK_PLAYONSTART 2

void CFmodTrack::Spawn()
{
	if (FStringNull(pev->message))
	{
		ALERT(at_error, "EMPTY FMOD_TRACK AT: %f, %f, %f\n", pev->origin.x, pev->origin.y, pev->origin.z);
		REMOVE_ENTITY(ENT(pev));
		return;
	}

	if (FBitSet(pev->spawnflags, FMOD_TRACK_LOOPING))
		m_fLooping = true;
	if (FBitSet(pev->spawnflags, FMOD_TRACK_PLAYONSTART))
		m_fPlayOnStart = true;

	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_NONE;

	if (m_fPlayOnStart)
	{
		SetThink(&CFmodTrack::SendMsg);
		pev->nextthink = gpGlobals->time + 0.2f; // TODO: Make sure this is a long enough delay or the message might get dropped
	}
}

void CFmodTrack::Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value)
{
	SendMsg();
}

void CFmodTrack::SendMsg(void)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodTrk, NULL);
	WRITE_STRING(STRING(pev->message));
	WRITE_BYTE(m_fLooping);
	WRITE_BYTE(pev->health); // Volume (0-255). 100 = 100% volume
	WRITE_BYTE(pitch);		 // Pitch (0-255). 100 = normal pitch, 200 = one octave up
	MESSAGE_END();

	// TODO: sanitize inputs
}

// Load key/value pairs
bool CFmodTrack::KeyValue(KeyValueData* pkvd)
{
	// pitch
	if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pitch = atoi(pkvd->szValue);
		return true;
	}

	return CBaseEntity::KeyValue(pkvd);
}

class CFmodTrackPause : public CBaseEntity
{
public:
	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value) override;
};

LINK_ENTITY_TO_CLASS(fmod_track_pause, CFmodTrackPause);

void CFmodTrackPause::Spawn()
{
	if (FStringNull(pev->targetname))
	{
		ALERT(at_error, "EMPTY FMOD_TRACK_PAUSE AT: %f, %f, %f\n", pev->origin.x, pev->origin.y, pev->origin.z);
		REMOVE_ENTITY(ENT(pev));
		return;
	}
}

void CFmodTrackPause::Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodTrkPs, NULL);
	MESSAGE_END();
}