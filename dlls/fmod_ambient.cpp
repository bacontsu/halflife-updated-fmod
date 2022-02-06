#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"

class CFmodAmbient : public CBaseEntity
{
public:
	void Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value) override;
};

LINK_ENTITY_TO_CLASS(fmod_ambient, CFmodAmbient);

void CFmodAmbient::Use(CBaseEntity* pActivator, CBaseEntity* pOther, USE_TYPE useType, float value)
{
	MESSAGE_BEGIN(MSG_ALL, gmsgFmodAmb, NULL);
	WRITE_STRING(STRING(pev->message));
	MESSAGE_END();
}