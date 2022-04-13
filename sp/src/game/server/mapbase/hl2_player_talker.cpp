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

#include "hl2_player_talker.h"
#include "ai_squad.h"
#include "basegrenade_shared.h"
#include "in_buttons.h"
#include "eventqueue.h"
#include "iservervehicle.h"
#include "ai_interactions.h"
#include "world.h"
#include "sceneentity.h"
#include "fmtstr.h"
#include "combine_mine.h"
#include "weapon_physcannon.h"
#include "saverestore_utlvector.h"
#include "grenade_satchel.h"
#include "npc_citizen17.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar player_mute_responses( "player_mute_responses", "0", FCVAR_ARCHIVE, "Mutes the responsive player." );

ConVar player_dummy_in_squad( "player_dummy_in_squad", "0", FCVAR_ARCHIVE, "Puts the player dummy in the player's squad, which means squadmates will see enemies the player sees." );
ConVar player_dummy_free_knowledge_time( "player_dummy_free_knowledge_time", "3.0", FCVAR_ARCHIVE, "How long the player dummy should update the position of enemies they can't see (i.e. enemies passed to them or enemies that are eluding them)." );
ConVar player_dummy_memory_discard_time( "player_dummy_memory_discard_time", "120", FCVAR_ARCHIVE, "How long the player dummy should remember enemies." );
ConVar player_dummy( "player_dummy", "1", FCVAR_NONE, "Enables the player NPC dummy." );

ConVar player_use_instructor( "player_use_instructor", "1", FCVAR_NONE, "Enables game instructor hints instead of HL2 HUD hints" );

#if HL2_PLAYER_TALKER
LINK_ENTITY_TO_CLASS(player, CHL2_TalkingPlayer);
#endif //HL2_PLAYER_TALKER

BEGIN_DATADESC(CHL2_TalkingPlayer)
	DEFINE_FIELD(m_bInAScript, FIELD_BOOLEAN),

	DEFINE_FIELD(m_hStaringEntity, FIELD_EHANDLE),
	DEFINE_FIELD(m_flCurrentStaringTime, FIELD_TIME),

	DEFINE_FIELD( m_vecLastCommandGoal, FIELD_VECTOR ),

	DEFINE_FIELD(m_hNPCComponent, FIELD_EHANDLE),
	DEFINE_FIELD(m_flNextSpeechTime, FIELD_TIME),
	DEFINE_FIELD(m_hSpeechFilter, FIELD_EHANDLE),

	DEFINE_EMBEDDED( m_MemoryComponent ),

	DEFINE_FIELD(m_hSpeechTarget, FIELD_EHANDLE),

	// These don't need to be saved
	//DEFINE_FIELD(m_iVisibleEnemies, FIELD_INTEGER),
	//DEFINE_FIELD(m_iCloseEnemies, FIELD_INTEGER),
	//DEFINE_FIELD(m_iCriteriaAppended, FIELD_INTEGER),

	DEFINE_INPUTFUNC(FIELD_VOID, "StartScripting", InputStartScripting),
	DEFINE_INPUTFUNC(FIELD_VOID, "StopScripting", InputStopScripting),
END_DATADESC()

BEGIN_ENT_SCRIPTDESC( CHL2_TalkingPlayer, CHL2_Player, "Talking player entity." )

	DEFINE_SCRIPTFUNC_NAMED( ScriptGetNPCComponent, "GetNPCComponent", "Gets the player's NPC component." )
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetStaringEntity, "GetStaringEntity", "Gets the player's staring entity." )

	DEFINE_SCRIPTFUNC_NAMED( VScriptGetEnemy, "GetEnemy", "Gets the player's current enemy." )
	DEFINE_SCRIPTFUNC( GetVisibleEnemies, "Gets the player's visible enemies." )
	DEFINE_SCRIPTFUNC( GetCloseEnemies, "Gets the player's close enemies." )

	DEFINE_SCRIPTFUNC( IsInAScript, "Returns true if the player is in a script." )

END_SCRIPTDESC();

IMPLEMENT_SERVERCLASS_ST(CHL2_TalkingPlayer, DT_HL2_TalkingPlayer)

END_SEND_TABLE()

#define PLAYER_MIN_ENEMY_CONSIDER_DIST Square(4096)
#define PLAYER_MIN_MOB_DIST_SQR Square(192)

// How many close enemies there has to be before it's considered a "mob".
#define PLAYER_ENEMY_MOB_COUNT 4

#define SPEECH_AI_INTERVAL_IDLE 0.5f
#define SPEECH_AI_INTERVAL_ALERT 0.25f

//-----------------------------------------------------------------------------
// Purpose: Allow post-frame adjustments on the player
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::PostThink(void)
{
	if (m_flNextSpeechTime < gpGlobals->curtime) 
	{
		float flCooldown = SPEECH_AI_INTERVAL_IDLE;
		if (GetNPCComponent())
		{
			// Do some pre-speech setup based off of our state.
			switch (GetNPCComponent()->GetState())
			{
				// Speech AI runs more frequently if we're alert or in combat.
				case NPC_STATE_ALERT:
				{
					flCooldown = SPEECH_AI_INTERVAL_ALERT;
				} break;
				case NPC_STATE_COMBAT:
				{
					flCooldown = SPEECH_AI_INTERVAL_ALERT;

					// Measure enemies and cache them.
					// They're almost entirely used for speech anyway, so it makes sense to put them here.
					MeasureEnemies(m_iVisibleEnemies, m_iCloseEnemies);
				} break;
			}
		}

		// Some stuff in DoSpeechAI() relies on m_flNextSpeechTime.
		m_flNextSpeechTime = gpGlobals->curtime + flCooldown;

		DoSpeechAI();
	}

	BaseClass::PostThink();
}

void CHL2_TalkingPlayer::Precache( void )
{
	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
CHL2_TalkingPlayer::CHL2_TalkingPlayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Spawn( void )
{
	if (player_dummy.GetBool() && gpGlobals->eLoadType != MapLoad_Background)
	{
		// Must do this here because PostConstructor() is called before save/restore,
		// which causes duplicates to be created.
		CreateNPCComponent();
	}

	BaseClass::Spawn();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::UpdateOnRemove( void )
{
	RemoveNPCComponent();

	BaseClass::UpdateOnRemove();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::OnUseEntity( CBaseEntity *pEntity )
{
	// Continuous use = turning valves, using chargers, etc.
	if (pEntity->ObjectCaps() & FCAP_CONTINUOUS_USE)
		return;

	AI_CriteriaSet modifiers;
	ModifyOrAppendSpeechTargetCriteria( modifiers, pEntity );

	SpeakIfAllowed( TLK_USE, modifiers );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt )
{
	if ( interactionType == g_interactionBarnacleVictimGrab )
	{
		AI_CriteriaSet modifiers;

		if (sourceEnt)
			ModifyOrAppendEnemyCriteria(modifiers, sourceEnt);

		SpeakIfAllowed(TLK_SELF_IN_BARNACLE, modifiers);

		// Fall in on base
		//return true;
	}
	else if ( interactionType == g_interactionScannerInspectBegin )
	{
		AI_CriteriaSet modifiers;

		if (sourceEnt)
			ModifyOrAppendEnemyCriteria(modifiers, sourceEnt);

		SpeakIfAllowed(TLK_SCANNER_FLASH, modifiers);

		return true;
	}

	return BaseClass::HandleInteraction( interactionType, data, sourceEnt );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Disposition_t CHL2_TalkingPlayer::IRelationType( CBaseEntity *pTarget )
{
	Disposition_t base = BaseClass::IRelationType( pTarget );

	return base;
}

//-----------------------------------------------------------------------------
// Purpose: For checking if the player is looking at something, but takes the pitch into account (more expensive)
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::FInTrueViewCone( const Vector &vecSpot )
{
	Vector los = ( vecSpot - EyePosition() );

	// Same as FInViewCone(), but in 2D
	VectorNormalize( los );

	Vector facingDir = EyeDirection3D();

	float flDot = DotProduct( los, facingDir );

	if ( flDot > m_flFieldOfView )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Event fired when a living player takes damage - used to emit damage sounds
//-----------------------------------------------------------------------------
int CHL2_TalkingPlayer::OnTakeDamage_Alive(const CTakeDamageInfo & info)
{
	// Record memory stuff
	if (info.GetAttacker() && info.GetAttacker() != GetWorldEntity() && info.GetAttacker() != this)
	{
		GetMemoryComponent()->RecordEngagementStart();
		GetMemoryComponent()->InitLastDamage(info);

		// If the attacker was an NPC or client update our memory
		if (GetNPCComponent() && info.GetAttacker()->GetFlags() & (FL_NPC | FL_CLIENT))
		{
			GetNPCComponent()->GetEnemies()->OnTookDamageFrom( info.GetAttacker() );
		}
	}

	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);

	if (IsAllowedToSpeak(TLK_WOUND))
	{
		SetSpeechTarget( FindSpeechTarget() );
		if (!GetSpeechTarget())
			SetSpeechTarget( info.GetAttacker() );

		// Complain about taking damage from an enemy.
		// If that doesn't work, just do a regular wound. (we know we're allowed to speak it)
		if (!SpeakIfAllowed(TLK_WOUND_REMARK, modifiers))
			Speak(TLK_WOUND, modifiers);
	}

	return BaseClass::OnTakeDamage_Alive(info);
}

//-----------------------------------------------------------------------------
// Purpose: give health. Returns the amount of health actually taken.
//-----------------------------------------------------------------------------
int CHL2_TalkingPlayer::TakeHealth( float flHealth, int bitsDamageType )
{
	// Cache the player's original health
	int beforeHealth = m_iHealth;

	// Give health
	int returnValue = BaseClass::TakeHealth( flHealth, bitsDamageType );

	AI_CriteriaSet modifiers;
	modifiers.AppendCriteria( "health", UTIL_VarArgs( "%i", beforeHealth ) );
	modifiers.AppendCriteria( "damage", UTIL_VarArgs( "%i", returnValue ) );
	modifiers.AppendCriteria( "damage_type", UTIL_VarArgs( "%i", bitsDamageType ) );

	SpeakIfAllowed( TLK_HEAL, modifiers );

	return returnValue;
}

//-----------------------------------------------------------------------------
// Purpose: Override and copy-paste of CBasePlayer::TraceAttack(), does fake hitgroup calculations
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator )
{
	if ( m_takedamage )
	{
		CTakeDamageInfo info = inputInfo;

		if ( info.GetAttacker() )
		{
			// --------------------------------------------------
			//  If an NPC check if friendly fire is disallowed
			// --------------------------------------------------
			CAI_BaseNPC *pNPC = info.GetAttacker()->MyNPCPointer();
			if ( pNPC && (pNPC->CapabilitiesGet() & bits_CAP_NO_HIT_PLAYER) && pNPC->IRelationType( this ) != D_HT )
				return;

			// Prevent team damage here so blood doesn't appear
			if ( info.GetAttacker()->IsPlayer() )
			{
				if ( !g_pGameRules->FPlayerCanTakeDamage( this, info.GetAttacker(), info ) )
					return;
			}
		}

		int hitgroup = ptr->hitgroup;

		if ( hitgroup == HITGROUP_GENERIC )
		{
			// Try and calculate a fake hitgroup since player might not have a model.
			Vector vPlayerMins = GetPlayerMins();
			Vector vPlayerMaxs = GetPlayerMaxs();
			Vector vecDamagePos = (inputInfo.GetDamagePosition() - GetAbsOrigin());

			if (vecDamagePos.z < (vPlayerMins[2] + vPlayerMaxs[2])*0.5)
			{
				// Legs (under waist)
				// We could do either leg with matrix calculations if we want, but we don't need that right now.
				hitgroup = HITGROUP_LEFTLEG;
			}
			else if (vecDamagePos.z >= (GetViewOffset()[2] - 1.0f))
			{
				// Head
				hitgroup = HITGROUP_HEAD;
			}
			else
			{
				// Torso
				// We could do arms with matrix calculations if we want, but we don't need that right now.
				hitgroup = HITGROUP_STOMACH;
			}
		}

		SetLastHitGroup( hitgroup );


		// If this damage type makes us bleed, then do so
		bool bShouldBleed = !g_pGameRules->Damage_ShouldNotBleed( info.GetDamageType() );
		if ( bShouldBleed )
		{
			SpawnBlood(ptr->endpos, vecDir, BloodColor(), info.GetDamage());// a little surface blood.
			TraceBleed( info.GetDamage(), vecDir, ptr, info.GetDamageType() );
		}

		AddMultiDamage( info, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Execute squad command
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::CommanderExecuteOne(CAI_BaseNPC * pNpc, const commandgoal_t & goal, CAI_BaseNPC ** Allies, int numAllies)
{
	// This is called for each NPC, so make sure we haven't already spoken yet
	if (GetExpresser()->GetTimeSpokeConcept(TLK_COMMAND_RECALL) != gpGlobals->curtime &&
		GetExpresser()->GetTimeSpokeConcept(TLK_COMMAND_SEND) != gpGlobals->curtime)
	{
		if (goal.m_pGoalEntity)
		{
			SpeakIfAllowed(TLK_COMMAND_RECALL);
			m_vecLastCommandGoal = vec3_invalid;
		}
		else if (pNpc->IsInPlayerSquad())
		{
			AI_CriteriaSet modifiers;

			modifiers.AppendCriteria("commandpoint_dist_to_player", UTIL_VarArgs("%.0f", (goal.m_vecGoalLocation - GetAbsOrigin()).Length()));
			modifiers.AppendCriteria("commandpoint_dist_to_npc", UTIL_VarArgs("%.0f", (goal.m_vecGoalLocation - pNpc->GetAbsOrigin()).Length()));
			modifiers.AppendCriteria("distancetoplayer", UTIL_VarArgs("%.0f", (GetAbsOrigin() - pNpc->GetAbsOrigin()).Length()));

			// This is the difference in between the new goal and the last goal, but it's mostly just
			// so we know whether we already had a goal before this command or not.
			if (m_vecLastCommandGoal != vec3_invalid)
				modifiers.AppendCriteria( "commandpoint_prev", UTIL_VarArgs( "%.0f", (goal.m_vecGoalLocation - m_vecLastCommandGoal).Length() ) );
			else
				modifiers.AppendCriteria( "commandpoint_prev", "-1" );

			m_vecLastCommandGoal = goal.m_vecGoalLocation;

			SpeakIfAllowed(TLK_COMMAND_SEND, modifiers);
		}
	}

	return BaseClass::CommanderExecuteOne(pNpc, goal, Allies, numAllies);
}

// Determines if this concept isn't worth saying "Shut up" over.
static inline bool WasUnremarkableConcept( AIConcept_t concept )
{
	return CompareConcepts( concept, TLK_WOUND ) ||
			CompareConcepts( concept, TLK_SHOT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendCriteria(AI_CriteriaSet& criteriaSet)
{
	ModifyOrAppendSquadCriteria(criteriaSet); // Add player squad criteria

	if (GetNPCComponent())
	{
		CAI_PlayerNPCDummy *pAI = GetNPCComponent();

		pAI->ModifyOrAppendOuterCriteria(criteriaSet);

		// Append enemy stuff
		if (pAI->GetState() == NPC_STATE_COMBAT)
		{
			// Append criteria for our general enemy if it hasn't been filled out already
			if (!IsCriteriaModified(PLAYERCRIT_ENEMY))
				ModifyOrAppendEnemyCriteria(criteriaSet, pAI->GetEnemy());

			ModifyOrAppendAICombatCriteria(criteriaSet);
		}
	}

	if (GetMemoryComponent()->InEngagement())
	{
		criteriaSet.AppendCriteria("combat_length", UTIL_VarArgs("%f", GetMemoryComponent()->GetEngagementTime()));
	}

	if (IsInAVehicle())
		criteriaSet.AppendCriteria("in_vehicle", GetVehicleEntity()->GetClassname());
	else
		criteriaSet.AppendCriteria("in_vehicle", "0");

	// Use our speech target's criteris if we should
	if (GetSpeechTarget() && !IsCriteriaModified(PLAYERCRIT_SPEECHTARGET))
		ModifyOrAppendSpeechTargetCriteria(criteriaSet, GetSpeechTarget());

	// Use criteria from our active weapon
	if (!IsCriteriaModified(PLAYERCRIT_WEAPON) && GetActiveWeapon())
		ModifyOrAppendWeaponCriteria(criteriaSet, GetActiveWeapon());

	// Reset this now that we're appending general criteria
	ResetPlayerCriteria();

	// Do we have a speech filter? If so, append its criteria too
	if ( GetSpeechFilter() )
	{
		GetSpeechFilter()->AppendContextToCriteria( criteriaSet );
	}

	// Find the last spoken concept.
	AIConcept_t lastSpokeConcept = GetExpresser()->GetLastSpokeConcept( "TLK_WOUND" );
	float lastSpokeTime = MAX( GetTimeSpokeConcept( lastSpokeConcept ), LastTimePlayerTalked() );
	criteriaSet.AppendCriteria( "last_spoke", UTIL_VarArgs( "%f", gpGlobals->curtime - lastSpokeTime ) );

	BaseClass::ModifyOrAppendCriteria(criteriaSet);
}

//-----------------------------------------------------------------------------
// Purpose: Appends damage criteria
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info, bool bPlayer)
{
	MarkCriteria(PLAYERCRIT_DAMAGE);

	set.AppendCriteria("damage", UTIL_VarArgs("%i", (int)info.GetDamage()));
	set.AppendCriteria("damage_type", UTIL_VarArgs("%i", info.GetDamageType()));

	if (info.GetInflictor())
	{
		CBaseEntity *pInflictor = info.GetInflictor();
		set.AppendCriteria("inflictor", pInflictor->GetClassname());
		set.AppendCriteria("inflictor_is_physics", pInflictor->GetMoveType() == MOVETYPE_VPHYSICS ? "1" : "0");
	}

	// Are we the one getting damaged?
	if (bPlayer)
	{
		// This technically doesn't need damage info, but whatever.
		set.AppendCriteria("hitgroup", UTIL_VarArgs("%i", LastHitGroup()));

		if (info.GetAttacker() != this)
		{
			if (!IsCriteriaModified(PLAYERCRIT_ENEMY))
				ModifyOrAppendEnemyCriteria(set, info.GetAttacker());
		}
		else
		{
			// We're our own enemy
			set.AppendCriteria("damaged_self", "1");
		}
	}
}
//-----------------------------------------------------------------------------
// Purpose: Appends enemy criteria
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendEnemyCriteria(AI_CriteriaSet& set, CBaseEntity *pEnemy)
{
	MarkCriteria(PLAYERCRIT_ENEMY);

	if (pEnemy)
	{
		set.AppendCriteria("enemy", pEnemy->GetClassname());
		set.AppendCriteria("enemyclass", g_pGameRules->AIClassText(pEnemy->Classify()));
		set.AppendCriteria("distancetoenemy", UTIL_VarArgs("%f", GetAbsOrigin().DistTo((pEnemy->GetAbsOrigin()))));
		set.AppendCriteria("enemy_visible", (FInViewCone(pEnemy) && FVisible(pEnemy)) ? "1" : "0");

		CAI_BaseNPC *pNPC = pEnemy->MyNPCPointer();
		if (pNPC)
		{
			set.AppendCriteria("enemy_is_npc", "1");

			if (pNPC->IsOnFire())
				set.AppendCriteria("enemy_on_fire", "1");

			// Bypass split-second reactions
			if (pNPC->GetEnemy() == this && (gpGlobals->curtime - pNPC->GetLastEnemyTime()) > 0.1f)
			{
				set.AppendCriteria( "enemy_attacking_me", "1" );
				set.AppendCriteria( "enemy_sees_me", pNPC->HasCondition( COND_SEE_ENEMY ) ? "1" : "0" );
			}
			else
			{
				// Some NPCs have tunnel vision and lose sight of their enemies often.
				// If they remember us and last saw us less than 4 seconds ago,
				// they probably know we're there.
				// Sure, this means the response system thinks a NPC "sees me" when they technically do not,
				// but it curbs false sneak attack cases.
				AI_EnemyInfo_t *EnemyInfo = pNPC->GetEnemies()->Find( this );
				if (EnemyInfo && (gpGlobals->curtime - EnemyInfo->timeLastSeen) < 4.0f)
				{
					set.AppendCriteria( "enemy_sees_me", "1" );
				}
				else
				{
					// Do a simple visibility check
					set.AppendCriteria( "enemy_sees_me", pNPC->FInViewCone( this ) && pNPC->FVisible( this ) ? "1" : "0" );
				}
			}

			if (pNPC->GetExpresser())
			{
				// That's enough outta you.
				// (IsSpeaking() accounts for the delay as well, so it lingers beyond actual speech time)
#ifdef NEW_RESPONSE_SYSTEM
				if (gpGlobals->curtime < pNPC->GetExpresser()->GetTimeSpeechCompleteWithoutDelay()
						&& !WasUnremarkableConcept(pNPC->GetExpresser()->GetLastSpokeConcept()))
#else
				if (gpGlobals->curtime < pNPC->GetExpresser()->GetRealTimeSpeechComplete()
						&& !WasUnremarkableConcept(pNPC->GetExpresser()->GetLastSpokeConcept()))
#endif
					set.AppendCriteria("enemy_is_speaking", "1");
			}

			set.AppendCriteria( "enemy_activity", CAI_BaseNPC::GetActivityName( pNPC->GetActivity() ) );

			set.AppendCriteria( "enemy_weapon", pNPC->GetActiveWeapon() ? pNPC->GetActiveWeapon()->GetClassname() : "0" );

			if (GetNPCComponent())
			{
				// Get the NPC component's memory of this enemy
				GetNPCComponent()->ModifyOrAppendOuterEnemyCriteria( set, pNPC );
			}
		}
		else
		{
			set.AppendCriteria("enemy_is_npc", "0");
		}

		// Append their contexts
		pEnemy->AppendContextToCriteria( set, "enemy_" );
	}
	else
	{
		set.AppendCriteria("distancetoenemy", "-1");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends squad criteria
//		1upD added this method, but the code is from Blixibon to add
//		squadmate criteria.
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendSquadCriteria(AI_CriteriaSet& set)
{
	MarkCriteria(PLAYERCRIT_SQUAD);

	CAI_BaseNPC *pRepresentative = GetSquadCommandRepresentative();
	if (pRepresentative != NULL)
	{
		if (pRepresentative->GetCommandGoal() != vec3_invalid)
			set.AppendCriteria("squad_mode", "1"); // Send
		else
			set.AppendCriteria("squad_mode", "0"); // Follow

		// Get criteria related to individual squad members
		int iNumSquadCommandables = 0;
		bool bSquadInPVS = false;
		AISquadIter_t iter;
		for (CAI_BaseNPC *pAllyNpc = GetPlayerSquad()->GetFirstMember( &iter ); pAllyNpc; pAllyNpc = GetPlayerSquad()->GetNextMember( &iter ))
		{
			if (pAllyNpc->IsCommandable())
			{
				if (pAllyNpc->HasCondition( COND_IN_PVS ))
					bSquadInPVS = true;

				iNumSquadCommandables++;
			}
		}

		set.AppendCriteria("squadmates", UTIL_VarArgs("%i", iNumSquadCommandables));

		if (bSquadInPVS)
			set.AppendCriteria("squad_in_pvs", "1");
	}
	else
	{
		set.AppendCriteria("squadmates", "0");
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends weapon criteria
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendWeaponCriteria(AI_CriteriaSet& set, CBaseEntity *pWeapon)
{
	MarkCriteria(PLAYERCRIT_WEAPON);

	set.AppendCriteria( "weapon", pWeapon->GetClassname() );

	// Append its contexts
	pWeapon->AppendContextToCriteria( set, "weapon_" );
}

//-----------------------------------------------------------------------------
// Purpose: Appends speech target criteria
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendSpeechTargetCriteria(AI_CriteriaSet &set, CBaseEntity *pTarget)
{
	MarkCriteria(PLAYERCRIT_SPEECHTARGET);

	Assert(pTarget);

	set.AppendCriteria( "speechtarget", pTarget->GetClassname() );
	set.AppendCriteria( "speechtargetname", STRING(pTarget->GetEntityName()) );

	set.AppendCriteria( "speechtarget_visible", (FInViewCone(pTarget) && FVisible(pTarget)) ? "1" : "0" );

	if (pTarget->IsNPC())
	{
		CAI_BaseNPC *pNPC = pTarget->MyNPCPointer();

		if (pNPC->GetActiveWeapon())
			set.AppendCriteria( "speechtarget_weapon", pNPC->GetActiveWeapon()->GetClassname() );

		if (pNPC->IsInPlayerSquad())
		{
			set.AppendCriteria( "speechtarget_inplayersquad", "1" );
		}
		else
		{
			set.AppendCriteria( "speechtarget_inplayersquad", "0" );
		}
	}

	// Append their contexts
	pTarget->AppendContextToCriteria( set, "speechtarget_" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CHL2_TalkingPlayer::FindSpeechTarget()
{
	const Vector &	vAbsOrigin = GetAbsOrigin();
	float 			closestDistSq = FLT_MAX;
	CBaseEntity *	pNearest = NULL;
	float			distSq;

	// Next, search our squad
	AISquadIter_t iter;
	for (CAI_BaseNPC *pAllyNpc = GetPlayerSquad()->GetFirstMember( &iter ); pAllyNpc; pAllyNpc = GetPlayerSquad()->GetNextMember( &iter ))
	{
		if (!pAllyNpc->HasCondition( COND_IN_PVS ))
			continue;

		distSq = (vAbsOrigin - pAllyNpc->GetAbsOrigin()).LengthSqr();
		if ( distSq > Square(TALKRANGE_MIN) || distSq > closestDistSq )
			continue;

		if (pAllyNpc->IsAlive() && pAllyNpc->CanBeUsedAsAFriend() && !pAllyNpc->IsInAScript() && !pAllyNpc->HasSpawnFlags(SF_NPC_GAG))
		{
			closestDistSq = distSq;
			pNearest = pAllyNpc;
		}
	}

	if (pNearest)
		return pNearest;

	// Finally, look for one of our enemies
	if (GetNPCComponent())
	{
		CAI_Enemies *pEnemies = GetNPCComponent()->GetEnemies();
		AIEnemiesIter_t iter;
		for ( AI_EnemyInfo_t *pEMemory = pEnemies->GetFirst(&iter); pEMemory != NULL; pEMemory = pEnemies->GetNext(&iter) )
		{
			distSq = (vAbsOrigin - pEMemory->hEnemy->GetAbsOrigin()).LengthSqr();
			if ( distSq > Square(TALKRANGE_MIN) || distSq > closestDistSq )
				continue;

			CAI_BaseNPC *pNPC = pEMemory->hEnemy->MyNPCPointer();
			if (!pNPC)
				continue;

			if (pNPC->IsAlive() && pNPC->CanBeUsedAsAFriend() && !pNPC->IsInAScript() && !pNPC->HasSpawnFlags( SF_NPC_GAG ))
			{
				closestDistSq = distSq;
				pNearest = pNPC;
			}
		}
	}

	return pNearest;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the given concept can be spoken
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::IsAllowedToSpeak(AIConcept_t concept)
{
	if (m_lifeState > LIFE_DYING)
		return false;

	bool bCanSpeak = GetExpresser()->CanSpeak();
	if (concept && !bCanSpeak)
	{
		ConceptInfo_t *pInfo = GetAllySpeechManager()->GetConceptInfo( concept );
		ConceptInfo_t *pPrevInfo = GetAllySpeechManager()->GetConceptInfo( GetExpresser()->GetLastSpokeConcept() );
		if (pInfo && pPrevInfo)
		{
			bCanSpeak = pInfo->category > pPrevInfo->category;
		}
	}

	if (!bCanSpeak)
		return false;

	if (!GetExpresser()->CanSpeakConcept(concept))
		return false;

	// OBSOLETE: Remove this once we've replaced gagging in all of the maps and talker files
	const char *szGag = GetContextValue( "gag" );
	if (szGag && FStrEq(szGag, "1"))
		return false;

	if (IsInAScript())
		return false;

	if (player_mute_responses.GetBool())
		return false;

	// Don't say anything if we're running a scene
	if ( IsTalkingInAScriptedScene( this, true ) )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Check if the given concept can be spoken and then speak it
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter)
{
	if (!IsAllowedToSpeak(concept))
		return false;

	return Speak(concept, modifiers, pszOutResponseChosen, bufsize, filter);
}

//-----------------------------------------------------------------------------
// Purpose: Alternate method signature for SpeakIfAllowed allowing no criteriaset parameter 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::SpeakIfAllowed(AIConcept_t concept, char *pszOutResponseChosen, size_t bufsize, IRecipientFilter *filter)
{
	AI_CriteriaSet set;
	return SpeakIfAllowed(concept, set, pszOutResponseChosen, bufsize, filter);
}

//-----------------------------------------------------------------------------
// Purpose: Find a response for the given concept
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::SelectSpeechResponse( AIConcept_t concept, AI_CriteriaSet *modifiers, CBaseEntity *pTarget, AISpeechSelection_t *pSelection )
{
	if ( IsAllowedToSpeak( concept ) )
	{
		// If we have modifiers, send them, otherwise create a new object
#ifdef NEW_RESPONSE_SYSTEM
		bool result = FindResponse( pSelection->Response, concept, modifiers );

		if ( result )
		{
			pSelection->concept = concept;
			pSelection->hSpeechTarget = pTarget;
			return true;
		}
#else
		AI_Response *pResponse = SpeakFindResponse( concept, (modifiers != NULL ? *modifiers : AI_CriteriaSet()) );

		if ( pResponse )
		{
			pSelection->Set( concept, pResponse, pTarget );
			return true;
		}
#endif
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::GetGameTextSpeechParams( hudtextparms_t &params )
{
	params.channel = 4;
	params.x = -1;
	params.y = 0.7;
	params.effect = 0;

	/*
	params.r1 = 255;
	params.g1 = 51;
	params.b1 = 0;
	*/

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CHL2_TalkingPlayer::CreateExpresser(void)
{
	m_pExpresser = new CAI_Expresser(this);
	if (!m_pExpresser)
		return NULL;

	m_pExpresser->Connect(this);
	return m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::PostConstructor(const char *szClassname)
{
	BaseClass::PostConstructor(szClassname);
	CreateExpresser();

	GetMemoryComponent()->SetOuter(this);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::CreateNPCComponent()
{
	// Create our NPC component
	if (!m_hNPCComponent)
	{
		CBaseEntity *pEnt = CBaseEntity::CreateNoSpawn("player_npc_dummy", EyePosition(), EyeAngles(), this);
		m_hNPCComponent.Set(static_cast<CAI_PlayerNPCDummy*>(pEnt));

		if (m_hNPCComponent)
		{
			m_hNPCComponent->SetParent(this);
			m_hNPCComponent->SetOuter(this);

			DispatchSpawn( m_hNPCComponent );
		}
	}
	else
	{
		// Their outer is saved now, but double-check
		m_hNPCComponent->SetOuter(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::RemoveNPCComponent()
{
	if ( m_hNPCComponent != NULL )
	{
		// Don't let friends call it out as a dead ally, as it overrides TLK_PLDEAD
		m_hNPCComponent->RemoveFromSquad();

		UTIL_Remove( m_hNPCComponent.Get() );
		m_hNPCComponent = NULL;
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_Killed(const CTakeDamageInfo &info)
{
	BaseClass::Event_Killed(info);

	AI_CriteriaSet modifiers;
	ModifyOrAppendDamageCriteria(modifiers, info);
	Speak(TLK_DEATH, modifiers);

	// No speaking anymore
	m_flNextSpeechTime = FLT_MAX;
	RemoveNPCComponent();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_KilledOther(CBaseEntity *pVictim, const CTakeDamageInfo &info)
{
	BaseClass::Event_KilledOther(pVictim, info);

	if (pVictim->IsCombatCharacter())
	{
		// Event_KilledOther is called later than Event_NPCKilled in the NPC dying process,
		// meaning some pre-death conditions are retained in Event_NPCKilled that are not retained in Event_KilledOther,
		// such as the activity they were playing when they died.
		// As a result, NPCs always call through to Event_KilledEnemy through Event_NPCKilled and Event_KilledOther is not used.
		if (!pVictim->IsNPC())
			Event_KilledEnemy(pVictim->MyCombatCharacterPointer(), info);
	}
	else
	{
		// Killed a non-NPC (we don't do anything for this yet)
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends criteria for when we're leaving an engagement
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendFinalEnemyCriteria(AI_CriteriaSet& set, CBaseEntity *pEnemy, const CTakeDamageInfo &info)
{
	// Append our pre-engagement conditions
	set.AppendCriteria( "prev_health_diff", UTIL_VarArgs( "%i", GetHealth() - GetMemoryComponent()->GetPrevHealth() ) );

	set.AppendCriteria( "num_enemies_historic", UTIL_VarArgs( "%i", GetMemoryComponent()->GetHistoricEnemies() ) );

	// Try to identify retaliatory attacks
	if (pEnemy == GetMemoryComponent()->GetLastDamageAttacker() && gpGlobals->curtime - GetLastDamageTime() < 5.0f)
	{
		set.AppendCriteria("enemy_revenge", "1");
		GetMemoryComponent()->AppendLastDamageCriteria( set );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends criteria from our AI
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendAICombatCriteria(AI_CriteriaSet& set)
{
	// Append cached enemy numbers.
	// "num_enemies" here is just "visible" enemies and not every enemy the player saw before and knows is there, but that's good enough.
	set.AppendCriteria("num_enemies", UTIL_VarArgs("%i", m_iVisibleEnemies));
	set.AppendCriteria("close_enemies", UTIL_VarArgs("%i", m_iCloseEnemies));
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_KilledEnemy(CBaseCombatCharacter *pVictim, const CTakeDamageInfo &info)
{
	if (!IsAlive())
		return;

	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, pVictim);

	GetMemoryComponent()->KilledEnemy();
	GetMemoryComponent()->AppendKilledEnemyCriteria(modifiers);

	Disposition_t disposition = pVictim->IRelationType(this);

	// This code used to check for CBaseCombatCharacter before,
	// but now the function specifically takes that kind of class.
	// Maybe an if statement related to optimization could be added here?
	{
		modifiers.AppendCriteria("hitgroup", UTIL_VarArgs("%i", pVictim->LastHitGroup()));

		modifiers.AppendCriteria("enemy_relationship", UTIL_VarArgs("%i", disposition));

		// Add our relationship to the victim as well
		modifiers.AppendCriteria("relationship", UTIL_VarArgs("%i", IRelationType(pVictim)));

		//if (CAI_BaseNPC *pNPC = pVictim->MyNPCPointer())
		//{
		//}

		// Bad Cop needs to differentiate between human-like opponents (rebels, zombies, vorts, etc.)
		// and non-human opponents (antlions, bullsquids, headcrabs, etc.) for some responses.
		if (pVictim->GetHullType() == HULL_HUMAN || pVictim->GetHullType() == HULL_WIDE_HUMAN)
			modifiers.AppendCriteria("enemy_humanoid", "1");
	}

	bool bSpoken = false;

	// Is this the last enemy we know about?
	if (m_iVisibleEnemies <= 1 && GetNPCComponent()->GetEnemy() == pVictim && GetMemoryComponent()->InEngagement())
	{
		ModifyOrAppendFinalEnemyCriteria( modifiers, pVictim, info );

		// If the enemy was targeting one of our allies, use said ally as the speech target.
		// Otherwise, just look for a random one.
		if (pVictim->GetEnemy() && pVictim->GetEnemy() != this && IRelationType(pVictim->GetEnemy()) > D_FR)
			SetSpeechTarget( pVictim->GetEnemy() );
		else
			SetSpeechTarget( FindSpeechTarget() );

		// Check if we should say something special in regards to the situation being apparently over.
		// Separate concepts are used to bypass respeak delays.
		bSpoken = SpeakIfAllowed(TLK_LAST_ENEMY_DEAD, modifiers);
	}
	else if (disposition == D_LI)
	{
		// Our "enemy" was actually our "friend"
		SetSpeechTarget( pVictim );
		bSpoken = SpeakIfAllowed(TLK_KILLED_ALLY, modifiers);
	}
	else
	{
		// Look for someone to talk to during murder
		SetSpeechTarget( FindSpeechTarget() );
	}

	if (!bSpoken)
		SpeakIfAllowed( TLK_ENEMY_DEAD, modifiers );
	else
		GetExpresser()->SetSpokeConcept( TLK_ENEMY_DEAD, NULL, false ); // Pretend we spoke TLK_ENEMY_DEAD too
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by all NPCs, intended for when allies are killed, enemies are killed by allies, etc.
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_NPCKilled(CAI_BaseNPC *pVictim, const CTakeDamageInfo &info)
{
	if (info.GetAttacker() == this)
	{
		// Event_NPCKilled is called before Event_KilledOther in the NPC dying process,
		// meaning some pre-death conditions are retained in Event_NPCKilled that are not retained in Event_KilledOther,
		// such as the activity they were playing when they died.
		// As a result, NPCs always call through to Event_KilledEnemy through Event_NPCKilled and Event_KilledOther is not used.
		Event_KilledEnemy(pVictim, info);
		return;
	}

	// For now, don't care about NPCs not in our PVS.
	if (!pVictim->HasCondition(COND_IN_PVS))
		return;

	// "Mourn" dead allies
	if (pVictim->IsPlayerAlly(this))
	{
		AllyDied(pVictim, info);
		return;
	}

	if (info.GetAttacker())
	{
		// Check to see if they were killed by an ally.
		if (info.GetAttacker()->IsNPC())
		{
			if (info.GetAttacker()->MyNPCPointer()->IsPlayerAlly(this))
			{
				// Cheer them on, maybe!
				AllyKilledEnemy(info.GetAttacker(), pVictim, info);
				return;
			}
		}

		// Check to see if they were killed by a hopper mine.
		else if (FClassnameIs(info.GetAttacker(), "combine_mine"))
		{
			CBounceBomb *pBomb = static_cast<CBounceBomb*>(info.GetAttacker());
			if (pBomb)
			{
				if (pBomb->IsPlayerPlaced())
				{
					// Pretend we killed it.
					Event_KilledEnemy(pVictim, info);
					return;
				}
				else if (pBomb->IsFriend(this))
				{
					// Cheer them on, maybe!
					AllyKilledEnemy(info.GetAttacker(), pVictim, info);
					return;
				}
			}
		}
	}

	// Finally, see if they were an ignited NPC we were attacking.
	// Entity flames don't credit their igniters, so we can only guess that immediate enemies
	// dying on fire were lit up by the player.
	if (pVictim->IsOnFire() && GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim)
	{
		// Pretend we killed it.
		Event_KilledEnemy(pVictim, info);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by killed allies
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::AllyDied( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, info.GetAttacker());
	SetSpeechTarget(pVictim);

	// Bad Cop needs to differentiate between human-like allies (soldiers, metrocops, etc.)
	// and non-human allies (hunters, manhacks, etc.) for some responses.
	if (pVictim->GetHullType() == HULL_HUMAN || pVictim->GetHullType() == HULL_WIDE_HUMAN)
		modifiers.AppendCriteria("speechtarget_humanoid", "1");

	SpeakIfAllowed(TLK_ALLY_KILLED, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by NPCs killed by allies
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::AllyKilledEnemy( CBaseEntity *pAlly, CAI_BaseNPC *pVictim, const CTakeDamageInfo &info )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendDamageCriteria(modifiers, info, false);
	ModifyOrAppendEnemyCriteria(modifiers, pVictim);
	SetSpeechTarget(pAlly);

	if (m_iVisibleEnemies <= 1 && GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim && GetMemoryComponent()->InEngagement())
	{
		ModifyOrAppendFinalEnemyCriteria( modifiers, pVictim, info );
	}

	SpeakIfAllowed(TLK_ALLY_KILLED_NPC, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: Event fired by NPCs when they're ignited
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_NPCIgnited(CAI_BaseNPC *pVictim)
{
	if ( GetNPCComponent() && GetNPCComponent()->GetEnemy() == pVictim && (FInTrueViewCone(pVictim->WorldSpaceCenter()) && FVisible(pVictim)) )
	{
		SpeakIfAllowed( TLK_ENEMY_BURNING );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_SeeEnemy( CBaseEntity *pEnemy )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendEnemyCriteria(modifiers, pEnemy);

	SpeakIfAllowed(TLK_STARTCOMBAT, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::HandleAddToPlayerSquad( CAI_BaseNPC *pNPC )
{
	SetSpeechTarget(pNPC);

	return SpeakIfAllowed(TLK_COMMAND_ADD);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::HandleRemoveFromPlayerSquad( CAI_BaseNPC *pNPC )
{
	SetSpeechTarget(pNPC);

	return SpeakIfAllowed(TLK_COMMAND_REMOVE);
}

//-----------------------------------------------------------------------------
// Purpose: Event fired upon picking up a new weapon
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_FirstDrawWeapon( CBaseCombatWeapon *pWeapon )
{
	AI_CriteriaSet modifiers;
	ModifyOrAppendWeaponCriteria( modifiers, pWeapon );
	SpeakIfAllowed( TLK_NEWWEAPON, modifiers );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::Event_ThrewGrenade( CBaseCombatWeapon *pWeapon )
{
	AI_CriteriaSet modifiers;

	ModifyOrAppendWeaponCriteria(modifiers, pWeapon);

	SpeakIfAllowed(TLK_THROWGRENADE, modifiers);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::InputStartScripting( inputdata_t &inputdata )
{
	m_bInAScript = true;

	// OBSOLETE: Remove this once we've replaced gagging in all of the maps and talker files
	AddContext("gag", "1");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::InputStopScripting( inputdata_t &inputdata )
{
	m_bInAScript = false;

	// OBSOLETE: Remove this once we've replaced gagging in all of the maps and talker files
	AddContext("gag", "0");
}

//=============================================================================
// Player Speech System
// By Blixibon
// 
// A special speech AI system inspired by what CAI_PlayerAlly uses.
// Right now, this runs every 0.5 seconds on idle (0.25 on alert or combat) and reads our NPC component for NPC state, sensing, etc.
// This allows the player to react to danger and comment on things while idle.
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::DoSpeechAI( void )
{
	// First off, make sure we should be doing this AI
	if (IsInAScript())
		return;

	// If we're in notarget, don't comment on anything.
	if (GetFlags() & FL_NOTARGET)
		return;

	// Access our NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	NPC_STATE iState = NPC_STATE_IDLE;
	if (pAI)
	{
		iState = pAI->GetState();

		// Has our NPC heard anything recently?
		AISoundIter_t iter;
		if (pAI->GetSenses()->GetFirstHeardSound( &iter ))
		{
			// Refresh sound conditions.
			pAI->OnListened();

			// First off, look for important sounds the player should react to immediately.
			// This is the "priority" version of sound sensing. Idle things like scents are handled in DoIdleSpeech().
			// Update CAI_PlayerNPCDummy::GetSoundInterests() if you want to add more.
			int iBestSound = SOUND_NONE;
			if (pAI->HasCondition(COND_HEAR_DANGER))
				iBestSound = SOUND_DANGER;
			else if (pAI->HasCondition(COND_HEAR_PHYSICS_DANGER))
				iBestSound = SOUND_PHYSICS_DANGER;

			if (iBestSound != SOUND_NONE)
			{
				CSound *pSound = pAI->GetBestSound(iBestSound);
				if (pSound)
				{
					if (ReactToSound(pSound, (GetAbsOrigin() - pSound->GetSoundReactOrigin()).Length()))
						return;
				}
			}
		}

		// Do other things if our NPC is idle
		switch (iState)
		{
			case NPC_STATE_IDLE:
			{
				if (DoIdleSpeech())
					return;
			} break;

			case NPC_STATE_COMBAT:
			{
				if (DoCombatSpeech())
					return;
			} break;
		}
	}

	float flRandomSpeechModifier = GetSpeechFilter() ? GetSpeechFilter()->GetIdleModifier() : 1.0f;

	if ( flRandomSpeechModifier > 0.0f )
	{
		int iChance = (int)(RandomFloat(0, 10) * flRandomSpeechModifier);

		// 2% chance by default
		if (iChance > RandomInt(0, 500))
		{
			// Find us a random speech target
			SetSpeechTarget( FindSpeechTarget() );

			switch (iState)
			{
				case NPC_STATE_IDLE:
				case NPC_STATE_ALERT:
					SpeakIfAllowed(TLK_IDLE); break;

				case NPC_STATE_COMBAT:
					SpeakIfAllowed(TLK_ATTACKING); break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::DoIdleSpeech()
{
	float flHealthPerc = ((float)m_iHealth / (float)m_iMaxHealth);
	if ( flHealthPerc < 0.5f )
	{
		// Find us a random speech target
		SetSpeechTarget( FindSpeechTarget() );

		// The player could be feeling pretty shit.
		if ( SpeakIfAllowed( TLK_PLHURT ) )
			return true;
	}

	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	// Has our NPC heard anything recently?
	AISoundIter_t iter;
	if (pAI->GetSenses()->GetFirstHeardSound( &iter ))
	{
		// Refresh sound conditions.
		pAI->OnListened();

		// React to the little things in life that the player cares about.
		// This is the idle version of sound sensing. Priority things like danger are handled in DoSpeechAI().
		// Update CAI_PlayerNPCDummy::GetSoundInterests() if you want to add more.
		int iBestSound = SOUND_NONE;
		if (pAI->HasCondition(COND_HEAR_SPOOKY))
			iBestSound = SOUND_COMBAT;
		else if (pAI->HasCondition(COND_SMELL))
			iBestSound = (SOUND_MEAT | SOUND_CARCASS);

		if (iBestSound != SOUND_NONE)
		{
			CSound *pSound = pAI->GetBestSound(iBestSound);
			if (pSound)
			{
				if (ReactToSound(pSound, (GetAbsOrigin() - pSound->GetSoundReactOrigin()).Length()))
					return true;
			}
		}
	}

	// Check if we're staring at something
	if (GetSmoothedVelocity().LengthSqr() < Square( 100 ) && !GetUseEntity())
	{
		// First, reset our staring entity
		CBaseEntity *pLastStaring = GetStaringEntity();
		SetStaringEntity( NULL );

		// If our eye angles haven't changed much, and we're not running a scripted scene, it could mean we're intentionally looking at something
		if (QAnglesAreEqual(EyeAngles(), m_angLastStaringEyeAngles, 5.0f) && !IsRunningScriptedSceneWithSpeechAndNotPaused( this, true ))
		{
			// Trace a line 96 units in front of ourselves to see what we're staring at
			trace_t tr;
			Vector vecEyeDirection = EyeDirection3D();
			UTIL_TraceLine(EyePosition(), EyePosition() + vecEyeDirection * 96.0f, MASK_BLOCKLOS_AND_NPCS, this, COLLISION_GROUP_NONE, &tr);

			if (tr.m_pEnt && !tr.m_pEnt->IsWorld())
			{
				// Make sure we're looking at its "face" more-so than its origin
				if (!tr.m_pEnt->IsCombatCharacter() || (tr.m_pEnt->EyePosition() - tr.endpos).LengthSqr() < (tr.m_pEnt->GetAbsOrigin() - tr.endpos).LengthSqr())
				{
					if (tr.m_pEnt != pLastStaring)
					{
						// We're staring at a new entity
						m_flCurrentStaringTime = gpGlobals->curtime;
					}

					SetStaringEntity(tr.m_pEnt);
				}
			}
		}

		m_angLastStaringEyeAngles = EyeAngles();

		if (GetStaringEntity())
		{
			// Check if we've been staring for longer than 3 seconds
			if (gpGlobals->curtime - m_flCurrentStaringTime > 3.0f)
			{
				// We're staring at something
				AI_CriteriaSet modifiers;

				SetSpeechTarget(GetStaringEntity());

				modifiers.AppendCriteria("staretime", UTIL_VarArgs("%f", gpGlobals->curtime - m_flCurrentStaringTime));

				if (SpeakIfAllowed(TLK_STARE, modifiers))
					return true;
			}
		}
		else
		{
			// Reset staring time
			m_flCurrentStaringTime = FLT_MAX;
		}
	}

	// TLK_IDLE is handled in DoSpeechAI(), so there's nothing else we could say.
	return false;

	// We could use something like this somewhere (separated since Speak() does all of this anyway)
	//AISpeechSelection_t selection;
	//if ( SelectSpeechResponse(TLK_IDLE, NULL, NULL, &selection) )
	//{
	//	if (SpeakDispatchResponse(selection.concept.c_str(), selection.pResponse))
	//		return true;
	//}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::DoCombatSpeech()
{
	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	// Comment on enemy counts
	if ( pAI->HasCondition( COND_MOBBED_BY_ENEMIES ) )
	{
		// Find us a random speech target
		SetSpeechTarget( FindSpeechTarget() );

		if (SpeakIfAllowed( TLK_MOBBED ))
			return true;
	}
	else if ( m_iVisibleEnemies >= 5 && m_iCloseEnemies < 3 )
	{
		if (GetNPCComponent()->GetEnemy() && GetNPCComponent()->GetEnemy()->GetEnemy() == this)
		{
			// Player sees 5+ enemies and they (or at least one) are targeting them in particular
			// (indicates they're not idle and not distracted)
			if (SpeakIfAllowed( TLK_MANY_ENEMIES ))
				return true;
		}
	}

	if ( GetActiveWeapon() )
	{
		if (GetActiveWeapon()->m_bInReload && (GetActiveWeapon()->Clip1() / GetActiveWeapon()->GetMaxClip1()) < 0.5f && !IsSpeaking())
		{
			// Player announces reloading
			if (SpeakIfAllowed( TLK_HIDEANDRELOAD ))
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Counts enemies from our NPC component, based off of Alyx's enemy counting/mobbing implementaton.
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::MeasureEnemies(int &iVisibleEnemies, int &iCloseEnemies)
{
	// We shouldn't be this far if we don't have a NPC component
	CAI_PlayerNPCDummy *pAI = GetNPCComponent();
	Assert( pAI );

	CAI_Enemies *pEnemies = pAI->GetEnemies();
	Assert( pEnemies );

	iVisibleEnemies = 0;
	iCloseEnemies = 0;

	// This is a simplified version of Alyx's mobbed AI found in CNPC_Alyx::DoMobbedCombatAI().
	// This isn't as expensive as it looks.
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pEMemory = pEnemies->GetFirst(&iter); pEMemory != NULL; pEMemory = pEnemies->GetNext(&iter) )
	{
		if ( IRelationType( pEMemory->hEnemy ) <= D_FR && pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= PLAYER_MIN_ENEMY_CONSIDER_DIST &&
			pEMemory->hEnemy->IsAlive() && gpGlobals->curtime - pEMemory->timeLastSeen <= 5.0f )
		{
			Class_T classify = pEMemory->hEnemy->Classify();
			if (classify == CLASS_BARNACLE || classify == CLASS_BULLSEYE)
				continue;

			iVisibleEnemies += 1;

			if( pEMemory->hEnemy->GetAbsOrigin().DistToSqr(GetAbsOrigin()) <= PLAYER_MIN_MOB_DIST_SQR )
			{
				iCloseEnemies += 1;
				pEMemory->bMobbedMe = true;
			}
		}
	}

	// Set the NPC component's mob condition here.
	if( iCloseEnemies >= PLAYER_ENEMY_MOB_COUNT )
	{
		pAI->SetCondition( COND_MOBBED_BY_ENEMIES );
	}
	else
	{
		pAI->ClearCondition( COND_MOBBED_BY_ENEMIES );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Appends sound criteria
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayer::ModifyOrAppendSoundCriteria(AI_CriteriaSet & set, CSound *pSound, float flDist)
{
	set.AppendCriteria( "sound_distance", UTIL_VarArgs("%f", flDist ) );

	set.AppendCriteria( "sound_type", UTIL_VarArgs("%i", pSound->SoundType()) );

	if (pSound->m_hOwner)
	{
		if (pSound->SoundChannel() == SOUNDENT_CHANNEL_ZOMBINE_GRENADE)
		{
			// Pretend the owner is a grenade (the zombine will be the enemy)
			set.AppendCriteria( "sound_owner", "npc_grenade_frag" );
		}
		else
		{
			set.AppendCriteria( "sound_owner", UTIL_VarArgs("%s", pSound->m_hOwner->GetClassname()) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHL2_TalkingPlayer::ReactToSound( CSound *pSound, float flDist )
{
	// Do not react to our own sounds, or sounds produced by the vehicle we're in
	if (pSound->m_hOwner == this || pSound->m_hOwner == GetVehicleEntity())
		return false;

	AI_CriteriaSet set;
	ModifyOrAppendSoundCriteria(set, pSound, flDist);

	if (pSound->m_iType & SOUND_DANGER)
	{
		CBaseEntity *pOwner = pSound->m_hOwner.Get();
		CBaseGrenade *pGrenade = dynamic_cast<CBaseGrenade*>(pOwner);
		if (pGrenade)
			pOwner = pGrenade->GetThrower();

		// Only danger sounds with no owner or an owner we don't like are counted
		// (no reacting to danger from self or allies)
		if (!pOwner || IRelationType(pOwner) <= D_FR)
		{
			if (pOwner)
				ModifyOrAppendEnemyCriteria(set, pOwner);

			return SpeakIfAllowed(TLK_DANGER, set);
		}
	}
	else if (pSound->m_iType & (SOUND_MEAT | SOUND_CARCASS))
	{
		return SpeakIfAllowed(TLK_SMELL, set);
	}
	else if (pSound->m_iType & SOUND_COMBAT && pSound->SoundChannel() == SOUNDENT_CHANNEL_SPOOKY_NOISE)
	{
		// Player is creeped out
		return SpeakIfAllowed( TLK_DARKNESS_HEARDSOUND, set );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HSCRIPT CHL2_TalkingPlayer::ScriptGetNPCComponent()
{
	return ToHScript( GetNPCComponent() );
}

HSCRIPT CHL2_TalkingPlayer::ScriptGetStaringEntity()
{
	return ToHScript( GetStaringEntity() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CHL2_TalkingPlayer::GetEnemy()
{
	return GetNPCComponent() ? GetNPCComponent()->GetEnemy() : NULL;
}

NPC_STATE CHL2_TalkingPlayer::GetState()
{
	return GetNPCComponent() ? GetNPCComponent()->GetState() : NPC_STATE_NONE;
}

//=============================================================================
// Talking Player Memory Component
// By Blixibon
// 
// Carries miscellaneous "memory" information that the talking player should remember later on for dialogue.
// 
// For example, the player's health is recorded before they begin combat.
// When combat ends, the response system subtracts the player's current health from the health recorded here,
// creating a "health difference" that indicates how much health the player lost in the engagement.
//=============================================================================
BEGIN_SIMPLE_DATADESC( CHL2_TalkingPlayerMemory )

	DEFINE_FIELD( m_bInEngagement, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flEngagementStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_iPrevHealth, FIELD_INTEGER ),

	DEFINE_FIELD( m_iNumEnemiesHistoric, FIELD_INTEGER ),

	DEFINE_FIELD( m_iComboEnemies, FIELD_INTEGER ),
	DEFINE_FIELD( m_flLastEnemyDeadTime, FIELD_TIME ),

	DEFINE_FIELD( m_iLastDamageType, FIELD_INTEGER ),
	DEFINE_FIELD( m_iLastDamageAmount, FIELD_INTEGER ),
	DEFINE_FIELD( m_hLastDamageAttacker, FIELD_EHANDLE ),

	DEFINE_FIELD( m_hOuter, FIELD_EHANDLE ),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayerMemory::InitLastDamage(const CTakeDamageInfo &info)
{
	m_iLastDamageType = info.GetDamageType();

	if (info.GetAttacker() == m_hLastDamageAttacker)
	{
		// Just add it on
		m_iLastDamageAmount += (int)(info.GetDamage());
	}
	else
	{
		m_iLastDamageAmount = (int)(info.GetDamage());
		m_hLastDamageAttacker = info.GetAttacker();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayerMemory::RecordEngagementStart()
{
	if (!InEngagement())
	{
		m_iPrevHealth = GetOuter()->GetHealth();
		m_bInEngagement = true;
		m_flEngagementStartTime = gpGlobals->curtime;
	}
}

void CHL2_TalkingPlayerMemory::RecordEngagementEnd()
{
	m_bInEngagement = false;
	m_iNumEnemiesHistoric = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayerMemory::KilledEnemy()
{
	if (gpGlobals->curtime - m_flLastEnemyDeadTime > 5.0f)
		m_iComboEnemies = 0;

	m_iComboEnemies++;
	m_flLastEnemyDeadTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayerMemory::AppendLastDamageCriteria( AI_CriteriaSet& set )
{
	set.AppendCriteria( "lasttaken_damagetype", UTIL_VarArgs( "%i", GetLastDamageType() ) );
	set.AppendCriteria( "lasttaken_damage", UTIL_VarArgs( "%i", GetLastDamageAmount() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHL2_TalkingPlayerMemory::AppendKilledEnemyCriteria( AI_CriteriaSet& set )
{
	set.AppendCriteria("killcombo", CNumStr(m_iComboEnemies));
}

//=============================================================================
// Player NPC "Dummy"
// By Blixibon
//
// This is a "dummy" NPC only meant to see enemies, hear sounds, and change states,
// intended to be an invisible component for the player.
//=============================================================================

BEGIN_DATADESC(CAI_PlayerNPCDummy)

DEFINE_FIELD(m_hOuter, FIELD_EHANDLE),

END_DATADESC()

LINK_ENTITY_TO_CLASS(player_npc_dummy, CAI_PlayerNPCDummy);

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::Spawn( void )
{
	BaseClass::Spawn();

	// This is a dummy model that is never used!
	UTIL_SetSize(this, Vector(-16,-16,-16), Vector(16,16,16));

	// What the player uses by default
	m_flFieldOfView = 0.766;

	SetMoveType( MOVETYPE_NONE );
	ClearEffects();
	SetGravity( 0.0 );

	AddEFlags( EFL_NO_DISSOLVE );

	SetSolid( SOLID_NONE );
	AddSolidFlags( FSOLID_NOT_SOLID );
	m_takedamage = DAMAGE_NO;

	AddEffects( EF_NODRAW );

	if (player_dummy_in_squad.GetBool())
	{
		// Put us in the player's squad
		CapabilitiesAdd(bits_CAP_SQUAD);
		AddToSquad( AllocPooledString(PLAYER_SQUADNAME) );
	}

	GetEnemies()->SetFreeKnowledgeDuration( player_dummy_free_knowledge_time.GetFloat() );
	GetEnemies()->SetEnemyDiscardTime( player_dummy_memory_discard_time.GetFloat() );
}

//-----------------------------------------------------------------------------
// Purpose: Higher priority for enemies the player is actually aiming at
//-----------------------------------------------------------------------------
int CAI_PlayerNPCDummy::IRelationPriority( CBaseEntity *pTarget )
{
	// Draw from our outer for the base priority
	int iPriority = GetOuter()->IRelationPriority(pTarget);

	Vector los = ( pTarget->WorldSpaceCenter() - EyePosition() );
	Vector facingDir = EyeDirection3D();
	float flDot = DotProduct( los, facingDir );

	if ( flDot > 0.75f )
		iPriority += 1;
	if ( flDot > 0.9f )
		iPriority += 1;

	return iPriority;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::ModifyOrAppendOuterCriteria( AI_CriteriaSet &set )
{
	// CAI_ExpresserHost uses its own complicated take on NPC state, but
	// considering the other response enums works fine, I think it might be from another era.
	set.AppendCriteria( "npcstate", UTIL_VarArgs( "%i", m_NPCState ) );

	if ( GetLastEnemyTime() == 0.0 )
		set.AppendCriteria( "timesincecombat", "999999.0" );
	else
		set.AppendCriteria( "timesincecombat", UTIL_VarArgs( "%f", gpGlobals->curtime - GetLastEnemyTime() ) );
}

static inline const char *GetAITimeVar( const float &var )
{
	return var <= 0.0f ? "-1" : UTIL_VarArgs( "%f", gpGlobals->curtime - var);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::ModifyOrAppendOuterEnemyCriteria( AI_CriteriaSet &set, CAI_BaseNPC *pNPC )
{
	AI_EnemyInfo_t *EnemyInfo = GetEnemies()->Find( pNPC );
	if (EnemyInfo)
	{
		// Record memory info
		// NOTE: timeAtFirstHand is normally equal to timeLastSeen and only differs if the dummy was notified before seeing the enemy (i.e. by player squad)
		set.AppendCriteria( "enemy_time_last_seen", GetAITimeVar( EnemyInfo->timeLastSeen ) );
		set.AppendCriteria( "enemy_time_first_seen", GetAITimeVar( EnemyInfo->timeFirstSeen ) );
		//set.AppendCriteria( "enemy_time_last_reacquired", GetAITimeVar( EnemyInfo->timeLastReacquired ) );
		//set.AppendCriteria( "enemy_time_valid_enemy", GetAITimeVar( EnemyInfo->timeValidEnemy ) );
		set.AppendCriteria( "enemy_time_last_received_damage_from", GetAITimeVar( EnemyInfo->timeLastReceivedDamageFrom ) );
		set.AppendCriteria( "enemy_time_at_firsthand", GetAITimeVar( EnemyInfo->timeAtFirstHand ) );
		//set.AppendCriteria( "enemy_danger_memory", EnemyInfo->bDangerMemory ? "1" : "0" );
		//set.AppendCriteria( "enemy_eluded_me", EnemyInfo->bEludedMe ? "1" : "0" );
		//set.AppendCriteria( "enemy_unforgettable", EnemyInfo->bUnforgettable ? "1" : "0" );
		set.AppendCriteria( "enemy_mobbed_me", EnemyInfo->bMobbedMe ? "1" : "0" );
	}
	else
	{
		set.AppendCriteria( "enemy_time_last_seen", "-1" );
		set.AppendCriteria( "enemy_time_first_seen", "-1" );
		//set.AppendCriteria( "enemy_time_last_reacquired", "-1" );
		//set.AppendCriteria( "enemy_time_valid_enemy", "-1" );
		set.AppendCriteria( "enemy_time_last_received_damage_from", "-1" );
		set.AppendCriteria( "enemy_time_at_firsthand", "-1" );
		//set.AppendCriteria( "enemy_danger_memory", EnemyInfo->bDangerMemory ? "1" : "0" );
		//set.AppendCriteria( "enemy_eluded_me", "0" );
		//set.AppendCriteria( "enemy_unforgettable", "0" );
		set.AppendCriteria( "enemy_mobbed_me", "0" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::RunAI( void )
{
	if (GetOuter()->GetFlags() & FL_NOTARGET)
	{
		SetActivity( ACT_IDLE );
		return;
	}

	BaseClass::RunAI();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::GatherEnemyConditions( CBaseEntity *pEnemy )
{
	BaseClass::GatherEnemyConditions( pEnemy );

	if ( GetLastEnemyTime() == 0 || gpGlobals->curtime - GetLastEnemyTime() > 30 )
	{
		if ( HasCondition( COND_SEE_ENEMY ) && pEnemy->Classify() != CLASS_BULLSEYE && !(GetOuter()->GetFlags() & FL_NOTARGET) )
		{
			GetOuter()->Event_SeeEnemy(pEnemy);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CAI_PlayerNPCDummy::TranslateSchedule( int scheduleType ) 
{
	switch( scheduleType )
	{
	case SCHED_ALERT_STAND:
		return SCHED_PLAYERDUMMY_ALERT_STAND;
		break;
	}

	return scheduleType;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );

	if (GetOuter())
	{
		if (NewState == NPC_STATE_COMBAT && OldState < NPC_STATE_COMBAT)
		{
			// The player is entering combat
			GetOuter()->GetMemoryComponent()->RecordEngagementStart();
		}
		else if (GetOuter()->GetMemoryComponent()->InEngagement() && (NewState == NPC_STATE_ALERT || NewState == NPC_STATE_IDLE))
		{
			// The player is probably exiting combat
			GetOuter()->GetMemoryComponent()->RecordEngagementEnd();
		}

		if ( OldState == NPC_STATE_IDLE )
		{
			// Remove the player from non-idle scenes
			RemoveActorFromScriptedScenes( GetOuter(), true, true );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can hear the specified sound
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::QueryHearSound( CSound *pSound )
{
	if ( pSound->m_hOwner != NULL )
	{
		// We can't hear sounds emitted directly by our player.
		if ( pSound->m_hOwner.Get() == GetOuter() )
			return false;

		// We can't hear sounds emitted directly by a vehicle driven by our player or by nobody.
		IServerVehicle *pVehicle = pSound->m_hOwner->GetServerVehicle();
		if ( pVehicle && (!pVehicle->GetPassenger(VEHICLE_ROLE_DRIVER) || pVehicle->GetPassenger(VEHICLE_ROLE_DRIVER) == GetOuter()) )
			return false;
	}

	return BaseClass::QueryHearSound( pSound );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this NPC can see the specified entity
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC )
{
	if ( pEntity->IsNPC() )
	{
		// Under regular circumstances, the player should pick up on all NPCs, regardless of relationship.
		// This is so the player dummy can see commandable soldiers for things like automated HUD hints.
		if ( bOnlyHateOrFearIfNPC && (pEntity->Classify() == CLASS_BULLSEYE) )
		{
			Disposition_t disposition = IRelationType( pEntity );
			return ( disposition == D_HT || disposition == D_FR );
		}
	}
	else if ( pEntity == GetOuter() )
	{
		// Do not see our player
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Update information on my enemy
// Input  :
// Output : Returns true is new enemy, false is known enemy
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer )
{
	if ( BaseClass::UpdateEnemyMemory(pEnemy, position, pInformer) )
	{
		// New enemy, tell memory component
		if (GetOuter())
			GetOuter()->GetMemoryComponent()->IncrementHistoricEnemies();

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::DrawDebugGeometryOverlays( void )
{
	BaseClass::DrawDebugGeometryOverlays();

	// Highlight enemies
	AIEnemiesIter_t iter;
	for ( AI_EnemyInfo_t *pEMemory = GetEnemies()->GetFirst(&iter); pEMemory != NULL; pEMemory = GetEnemies()->GetNext(&iter) )
	{
		if (!pEMemory->bEludedMe)
		{
			int iTextOffset = 0;

			if (pEMemory->hEnemy == GetEnemy())
			{
				NDebugOverlay::BoxAngles( pEMemory->vLastSeenLocation,
					pEMemory->hEnemy->CollisionProp()->OBBMins(), pEMemory->hEnemy->CollisionProp()->OBBMaxs(),
					pEMemory->hEnemy->CollisionProp()->GetCollisionAngles(),
					0, 255, 0, 15 * IRelationPriority( pEMemory->hEnemy ), 0 );
			}
			else
			{
				NDebugOverlay::BoxAngles( pEMemory->vLastSeenLocation,
					pEMemory->hEnemy->CollisionProp()->OBBMins(), pEMemory->hEnemy->CollisionProp()->OBBMaxs(),
					pEMemory->hEnemy->CollisionProp()->GetCollisionAngles(),
					0, 0, 255, 15 * IRelationPriority( pEMemory->hEnemy ), 0 );
			}

			if (GetOuter()->GetVisibleEnemies() <= 1)
			{
				if (pEMemory->hEnemy == GetOuter()->GetMemoryComponent()->GetLastDamageAttacker() && gpGlobals->curtime - GetOuter()->GetLastDamageTime() < 5.0f)
				{
					NDebugOverlay::EntityTextAtPosition( pEMemory->hEnemy->EyePosition(), iTextOffset, "Revenge Candidate", 0, 255, 255, 255, 255 );
					iTextOffset++;
				}
			}
			
			if (CAI_BaseNPC *pNPC = pEMemory->hEnemy->MyNPCPointer())
			{
				if (pNPC->GetExpresser())
				{
#ifdef NEW_RESPONSE_SYSTEM
					if (gpGlobals->curtime < pNPC->GetExpresser()->GetTimeSpeechCompleteWithoutDelay()
						&& !WasUnremarkableConcept( pNPC->GetExpresser()->GetLastSpokeConcept() ))
#else
					if (gpGlobals->curtime < pNPC->GetExpresser()->GetRealTimeSpeechComplete()
						&& !WasUnremarkableConcept( pNPC->GetExpresser()->GetLastSpokeConcept() ))
#endif
					{
						NDebugOverlay::EntityTextAtPosition( pEMemory->hEnemy->EyePosition(), iTextOffset, "Enemy Speaking", 0, 255, 255, 255, 255 );
						iTextOffset++;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAI_PlayerNPCDummy::OnSeeEntity( CBaseEntity * pEntity )
{
	BaseClass::OnSeeEntity( pEntity );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CAI_PlayerNPCDummy::IsValidEnemy( CBaseEntity *pEnemy )
{
	if (!BaseClass::IsValidEnemy( pEnemy ))
		return false;

	// Just completely ignore bullseyes for now
	// (fixes an issue where the NPC component always regards any bullseye as an enemy)
	if (pEnemy->Classify() == CLASS_BULLSEYE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC( player_npc_dummy, CAI_PlayerNPCDummy )

	DEFINE_SCHEDULE
	(
		SCHED_PLAYERDUMMY_ALERT_STAND,

		"	Tasks"
		"		TASK_STOP_MOVING			0"
		"		TASK_FACE_REASONABLE		0"
		"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE"
		"		TASK_WAIT					5" // Don't wait very long
		"		TASK_SUGGEST_STATE			STATE:IDLE"
		""
		"	Interrupts"
		"		COND_NEW_ENEMY"
		"		COND_SEE_ENEMY"
		"		COND_LIGHT_DAMAGE"
		"		COND_HEAVY_DAMAGE"
		"		COND_HEAR_COMBAT"		// sound flags
		"		COND_HEAR_DANGER"
		"		COND_HEAR_BULLET_IMPACT"
		"		COND_IDLE_INTERRUPT"
	);

AI_END_CUSTOM_NPC()

//-----------------------------------------------------------------------------
// Purpose: Debugging commands
//-----------------------------------------------------------------------------

CON_COMMAND( player_dummy_reset, "Resets the player NPC dummy. Needed to update dummy cvars without restarting level" )
{
	CHL2_TalkingPlayer *pPlayer = dynamic_cast<CHL2_TalkingPlayer*>( UTIL_GetLocalPlayer() );
	if (pPlayer)
	{
		pPlayer->RemoveNPCComponent();

		if (player_dummy.GetBool())
		{
			pPlayer->CreateNPCComponent();
		}
	}
}
