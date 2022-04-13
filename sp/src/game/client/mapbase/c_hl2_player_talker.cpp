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

#include "cbase.h"
#include "c_hl2_player_talker.h"

#if defined( CHL2_TalkingPlayer )
	#undef CHL2_TalkingPlayer
#endif


IMPLEMENT_CLIENTCLASS_DT( C_HL2_TalkingPlayer, DT_HL2_TalkingPlayer, CHL2_TalkingPlayer )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_HL2_TalkingPlayer )
END_PREDICTION_DATA()

C_HL2_TalkingPlayer::C_HL2_TalkingPlayer()
{
}


C_HL2_TalkingPlayer::~C_HL2_TalkingPlayer()
{
}
