#ifndef LAB_PLAYER_H
#define LAB_PLAYER_H
#pragma once

#include "hl2_player.h"

class CLogicLabPlayerProxy;

class CLabPlayer : public CHL2_Player
{
public:
	DECLARE_CLASS(CLabPlayer, CHL2_Player);
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CLabPlayer();
	~CLabPlayer();

	static CLabPlayer *CreatePlayer( const char *className, edict_t *ed );

	virtual void PreThink();
	virtual void PostThink();
	virtual void Spawn();
	virtual void Precache();
	virtual void CreateHandModel(int viewmodelindex = 1, int iOtherVm = 0);
	CLogicLabPlayerProxy *GetLabPlayerProxy();
private:
	EHANDLE				m_hLabPlayerProxy;

};


inline CLabPlayer *ToLabPlayer( CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsPlayer() )
		return NULL;

#ifdef _DEBUG
	Assert( dynamic_cast<CLabPlayer*>( pEntity ) != 0 );
#endif
	return static_cast<CLabPlayer*>( pEntity );
}


#endif	// SDK_PLAYER_H
