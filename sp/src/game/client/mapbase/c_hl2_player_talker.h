//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: A version of CHL2_Player which is capable of using the Response System.
//			The player code triggers various NPC-like responses and supports special
//			NPC and memory components for realistic reaction/memory purposes.
// 
//			The vast majority of this code was originally created for Bad Cop, the protagonist of Entropy : Zero 2.
//			It has been migrated to a clean fork of Mapbase for general use in singleplayer Half-Life 2 mods.
//
// Author: Blixibon, 1upD
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_HL2_TALKING_PLAYER_H
#define C_HL2_TALKING_PLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "c_basehlplayer.h"

class C_HL2_TalkingPlayer : public C_BaseHLPlayer
{
public:
	DECLARE_CLASS( C_HL2_TalkingPlayer, C_BaseHLPlayer );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_HL2_TalkingPlayer();
	~C_HL2_TalkingPlayer();

};


inline C_HL2_TalkingPlayer* ToHL2TalkingPlayer( CBaseEntity *pPlayer )
{
	Assert( dynamic_cast<C_HL2_TalkingPlayer*>( pPlayer ) != NULL );
	return static_cast<C_HL2_TalkingPlayer*>( pPlayer );
}


#endif // C_HL2_TALKING_PLAYER_H
