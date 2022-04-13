//========= Copyright Valve Corporation, All rights reserved. ============//
// little fucking bastard
//=============================================================================//
#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "entitylist.h"
#include "activitylist.h"
#include "ai_basenpc.h"
#include "beam_flags.h"
#include "engine/IEngineSound.h"
#include "hl2_shareddefs.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_houndeye_health("sk_houndeye_health", "45");
ConVar	sk_houndeye_dmg_blast("sk_houndeye_dmg_blast", "25");

ConVar houndeye_attack_max_range("houndeye_attack_max_range", "384");
#define HOUNDEYE_TOP_MASS	 300.0f

int		HOUND_AE_THUMP;
int		HOUND_AE_FOOTSTEP;

int g_interactionHoundeyeSquadNewEnemy;

int s_iSonicEffectTexture = -1;

//=========================================================
// Private activities
//=========================================================
int	ACT_HOUNDEYEPUNTOBJECT = -1;

//=========================================================
// Custom schedules
//=========================================================
enum
{
	SCHED_HOUNDEYE_RANGEATTACK1 = LAST_SHARED_SCHEDULE,
	SCHED_HOUNDEYE_CHASE_ENEMY,
	SCHED_HOUNDEYE_WANDER,
	SCHED_HOUND_INVESTIGATE_SOUND,
	SCHED_HOUND_INVESTIGATE_SCENT,
	SCHED_HOUNDEYE_TAKE_COVER_FROM_ENEMY,
};

//=========================================================
// Custom tasks
//=========================================================
enum
{
	TASK_HOUNDEYETASK = LAST_SHARED_TASK,
};


//=========================================================
// Custom Conditions
//=========================================================
enum
{
	COND_HOUNDEYECONDITION = LAST_SHARED_CONDITION,
	COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY,
};


//=========================================================
//=========================================================
class CHoundeye : public CAI_BaseNPC
{
	DECLARE_CLASS(CHoundeye, CAI_BaseNPC);

public:
	void	Precache(void);
	void	Spawn(void);
	Class_T Classify(void);
	void	HandleAnimEvent(animevent_t* pEvent);

	bool IsJumpLegal(const Vector& startPos, const Vector& apex, const Vector& endPos) const;

	virtual void PainSound(const CTakeDamageInfo& info);
	virtual void DeathSound(const CTakeDamageInfo& info);
	virtual void IdleSound();
	virtual void AlertSound();

	int OnTakeDamage_Alive(const CTakeDamageInfo& inputInfo);

	void SonicAttack(void);

	float	MaxYawSpeed(void);
	int		RangeAttack1Conditions(float flDot, float flDist);
	int		SelectSchedule(void);
	virtual	void	GatherConditions(void);
	bool			IsValidCover(const Vector &vecCoverLocation, CAI_Hint const *pHint);
	int TranslateSchedule(int scheduleType);
	bool			HandleInteraction(int interactionType, void* data, CBaseCombatCharacter* pSourceEnt);
	void			BuildScheduleTestBits();

	float		GetMaxJumpSpeed() const { return 320.0f; }
	float			GetJumpGravity() const		{ return 3.0f; }

	virtual float	HearingSensitivity(void) { return 1.0; };
	virtual int				GetSoundInterests(void);

	DECLARE_DATADESC();

	// This is a dummy field. In order to provide save/restore
	// code in this file, we must have at least one field
	// for the code to operate on. Delete this field when
	// you are ready to do your own save/restore for this
	// character.
	int		m_iDeleteThisField;

	DEFINE_CUSTOM_AI;
private: HSOUNDSCRIPTHANDLE	m_hFootstep;
};

LINK_ENTITY_TO_CLASS(npc_houndeye, CHoundeye);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CHoundeye)

DEFINE_FIELD(m_iDeleteThisField, FIELD_INTEGER),

END_DATADESC()

void CHoundeye::Precache(void)
{
	PrecacheModel("models/houndeye.mdl");
	PrecacheScriptSound("NPC_Houndeye.Idle");
	PrecacheScriptSound("NPC_Houndeye.Sonic");
	PrecacheScriptSound("NPC_Houndeye.Alert");
	PrecacheScriptSound("NPC_Houndeye.Pain");
	PrecacheScriptSound("NPC_Houndeye.Die");
	s_iSonicEffectTexture = PrecacheModel("sprites/physbeam.vmt");
	m_hFootstep = PrecacheScriptSound("NPC_Antlion.Footstep");

	BaseClass::Precache();
}

void CHoundeye::Spawn(void)
{
	Precache();

	SetModel("models/houndeye.mdl");
	BaseClass::Spawn();
	SetHullType(HULL_TINY);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetNavType(NAV_GROUND);
	SetMoveType(MOVETYPE_STEP);
	SetBloodColor(BLOOD_COLOR_YELLOW);
	//SetCollisionGroup(HL2COLLISION_GROUP_HOUNDEYE);
	m_iHealth = sk_houndeye_health.GetFloat();
	m_flFieldOfView = 0.3;
	m_NPCState = NPC_STATE_NONE;

	//AddSpawnFlags(SF_NPC_LONG_RANGE);

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP);
	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	CapabilitiesAdd(bits_CAP_SQUAD);

	NPCInit();
}

Class_T	CHoundeye::Classify(void)
{
	return CLASS_HOUNDEYE;
}

int CHoundeye::GetSoundInterests(void)
{
	return SOUND_WORLD | SOUND_COMBAT | SOUND_PLAYER | SOUND_PLAYER_VEHICLE |
		SOUND_BULLET_IMPACT;
}

void CHoundeye::HandleAnimEvent(animevent_t* pEvent)
{
	if (pEvent->event == HOUND_AE_THUMP)
	{
		SonicAttack();
		return;
	}
	if (pEvent->event == HOUND_AE_FOOTSTEP)
	{
		MakeAIFootstepSound(240.0f);
		EmitSound("NPC_Antlion.Footstep", m_hFootstep, pEvent->eventtime);
		return;
	}
	BaseClass::HandleAnimEvent(pEvent);
}
//-----------------------------------------------------------------------------
// Purpose: Creates the houndeye's shockwave attack
//-----------------------------------------------------------------------------

void CHoundeye::SonicAttack(void)
{
	float		flAdjustedDamage;
	float		flDist;

	EmitSound("NPC_Houndeye.Sonic");

	CBroadcastRecipientFilter filter;
	te->BeamRingPoint(filter, 0.0,
		GetAbsOrigin(),							//origin
		16,									//start radius
		houndeye_attack_max_range.GetFloat(),									//end radius
		s_iSonicEffectTexture,				//texture
		0,									//halo index
		0,									//start frame
		0,									//framerate
		0.2,								//life
		24,									//width
		16,									//spread
		0,									//amplitude
		255,								//r
		255,								//g
		255,								//b
		192,								//a
		0,									//speed
		FBEAM_FADEOUT
		);

	CBaseEntity* pEntity = NULL;
	// iterate on all entities in the vicinity.
	while ((pEntity = gEntList.FindEntityInSphere(pEntity, GetAbsOrigin(), houndeye_attack_max_range.GetFloat())) != NULL)
	{
		if (pEntity->m_takedamage != DAMAGE_NO)
		{
			if (!FClassnameIs(pEntity, "npc_houndeye"))
			{// houndeyes don't hurt other houndeyes with their attack

				// houndeyes do FULL damage if the ent in question is visible. Half damage otherwise.
				// This means that you must get out of the houndeye's attack range entirely to avoid damage.
				// Calculate full damage first

				if (m_pSquad && m_pSquad->NumMembers() > 1)
				{
					// squad gets attack bonus.
					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat() + sk_houndeye_dmg_blast.GetFloat() * (1.1 * (m_pSquad->NumMembers() - 1));
				}
				else
				{
					// solo
					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat();
				}

				flDist = (pEntity->WorldSpaceCenter() - GetAbsOrigin()).Length();

				flAdjustedDamage -= (flDist / houndeye_attack_max_range.GetFloat()) * flAdjustedDamage;

				if (!FVisible(pEntity))
				{
					if (pEntity->IsPlayer())
					{
						// if this entity is a client, and is not in full view, inflict half damage. We do this so that players still 
						// take the residual damage if they don't totally leave the houndeye's effective radius. We restrict it to clients
						// so that monsters in other parts of the level don't take the damage and get pissed.
						flAdjustedDamage *= 0.5;
					}
					else if (!FClassnameIs(pEntity, "func_breakable") && !FClassnameIs(pEntity, "func_pushable"))
					{
						// do not hurt nonclients through walls, but allow damage to be done to breakables
						flAdjustedDamage = 0;
					}
				}

				//ALERT ( at_aiconsole, "Damage: %f\n", flAdjustedDamage );

				if (flAdjustedDamage > 0)
				{
					CTakeDamageInfo info(this, this, flAdjustedDamage, DMG_SONIC | DMG_ALWAYSGIB);
					CalculateExplosiveDamageForce(&info, (pEntity->GetAbsOrigin() - GetAbsOrigin()), pEntity->GetAbsOrigin());

					pEntity->TakeDamage(info);

					//UTIL_ViewPunch(EyePosition(), QAngle(random->RandomFloat(-5, 5), 0, random->RandomFloat(10, -10)), houndeye_attack_max_range.GetFloat(), false);
					//UTIL_ScreenShake(EyePosition(), 10, 150.0, 1.0, houndeye_attack_max_range.GetFloat(), SHAKE_START);


					if ((pEntity->GetAbsOrigin() - GetAbsOrigin()).Length2D() <= houndeye_attack_max_range.GetFloat())
					{
						if (pEntity->GetMoveType() == MOVETYPE_VPHYSICS || (pEntity->VPhysicsGetObject() && !pEntity->IsPlayer()))
						{
							IPhysicsObject* pPhysObject = pEntity->VPhysicsGetObject();

							if (pPhysObject)
							{
								float flMass = pPhysObject->GetMass();

								if (flMass <= HOUNDEYE_TOP_MASS)
								{
									// Increase the vertical lift of the force
									Vector vecForce = info.GetDamageForce();
									vecForce.z *= 2.0f;
									info.SetDamageForce(vecForce);

									pEntity->VPhysicsTakeDamage(info);
								}
							}
						}
						else if (pEntity->IsPlayer())
						{
							CBasePlayer* pPlayer = ToBasePlayer(pEntity);

							if (pPlayer != NULL)
							{
								pEntity->ViewPunch(QAngle(random->RandomFloat(-5, 5), 0, random->RandomFloat(10, -10)));

								//Shake the screen
								UTIL_ScreenShake(pEntity->GetAbsOrigin(), 50, 150.0, 1.0, 1024, SHAKE_START);

								//Red damage indicator
								color32 red = { 128, 0, 0, 128 };
								UTIL_ScreenFade(pEntity, red, 1.0f, 0.1f, FFADE_IN);
								Vector forward, up;
								if (pEntity->GetGroundEntity() != NULL)
								{
									pEntity->SetGroundEntity(NULL);


									AngleVectors(GetLocalAngles(), &forward, NULL, &up);
									pEntity->ApplyAbsVelocityImpulse(forward * 80 + up * 225);
								}
								else
								{
									AngleVectors(GetLocalAngles(), &forward, NULL, &up);
									pEntity->ApplyAbsVelocityImpulse(forward * 20 + up * 60);
								}
							}

						}
					}
				}
			}
		}
	}
}



bool CHoundeye::IsJumpLegal(const Vector& startPos, const Vector& apex, const Vector& endPos) const
{
	const float MAX_JUMP_RISE = 96.0f;
	const float MAX_JUMP_DISTANCE = 296.0f;
	const float MAX_JUMP_DROP = 256.0f;

	if (BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE))
	{
		// Hang onto the jump distance. The AI is going to want it.
		//m_flJumpDist = (startPos - endPos).Length();

		return true;
	}
	return false;
}

void CHoundeye::IdleSound()
{
	EmitSound("NPC_Houndeye.Idle");
}


void CHoundeye::AlertSound()
{
	EmitSound("NPC_Houndeye.Alert");
}


void CHoundeye::PainSound(const CTakeDamageInfo& info)
{
	EmitSound("NPC_Houndeye.Pain");
}


void CHoundeye::DeathSound(const CTakeDamageInfo& info)
{
	EmitSound("NPC_Houndeye.Die");
}

int CHoundeye::OnTakeDamage_Alive(const CTakeDamageInfo& inputInfo)
{
	// add pain to the conditions
	if (IsLightDamage(inputInfo))
	{
		AddGesture(ACT_GESTURE_SMALL_FLINCH);
	}
	if (IsHeavyDamage(inputInfo))
	{
		AddGesture(ACT_GESTURE_BIG_FLINCH);
	}

	return BaseClass::OnTakeDamage_Alive(inputInfo);
}

//=========================================================
// RangeAttack1Conditions
//=========================================================
int CHoundeye::RangeAttack1Conditions(float flDot, float flDist)
{
	float flMaxHoundeyeAttackMaxRange = houndeye_attack_max_range.GetFloat();
	// If I'm really close to my enemy allow me to attack if 
	// I'm facing regardless of next attack time
	if (flDist < 100 && flDot >= 0.3)
	{
		return COND_CAN_RANGE_ATTACK1;
	}
	if (flDist >(flMaxHoundeyeAttackMaxRange * 0.3))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	if (flDot < 0.6)
	{
		return COND_NOT_FACING_ATTACK;
	}
	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CHoundeye::MaxYawSpeed(void)
{
	switch (GetActivity())
	{
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		return 120;
		break;

	case ACT_RUN:
		return 70;
		break;

	case ACT_WALK:
		return 40;
		break;
	case ACT_IDLE:
		return 25;
		break;

	default:
		return 20;
		break;
	}
}

int CHoundeye::SelectSchedule(void)
{
	switch (GetState())
	{
	case NPC_STATE_COMBAT:
	{
		if (HasCondition(COND_LIGHT_DAMAGE))
		{
			if (random->RandomInt(0, 3) > 2)
			{
				DevMsg("ow!\n");
				return SCHED_HOUNDEYE_TAKE_COVER_FROM_ENEMY;
			}
		}
		if (HasCondition(COND_HEAVY_DAMAGE))
		{
			return SCHED_HOUNDEYE_TAKE_COVER_FROM_ENEMY;
		}
		if (HasCondition(COND_NEW_ENEMY))
		{
			CBaseEntity* pEnemy = GetEnemy();

			AI_EnemyInfo_t* pEnemyInfo = GetEnemies()->Find(pEnemy);

			if (GetSquad() && pEnemyInfo && (pEnemyInfo->timeFirstSeen == pEnemyInfo->timeAtFirstHand))
			{
				GetSquad()->BroadcastInteraction(g_interactionHoundeyeSquadNewEnemy, NULL, this);
				//DevMsg("I found an enemy! Notifiying rest of my squad!\n");
				// First contact for my squad.
				return SCHED_HOUNDEYE_CHASE_ENEMY;
			}

		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			if (OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2))
				return SCHED_HOUNDEYE_RANGEATTACK1;
			return SCHED_COMBAT_FACE;
			//ClearCondition(COND_CAN_RANGE_ATTACK1);
		}
		if (HasCondition(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY))
		{
			// A squadmate found an enemy. Respond to their call.
			//	DevMsg("Squadmate has found enemy, setting my schedule to chase enemy!\n");
			return SCHED_HOUNDEYE_CHASE_ENEMY;
		}

		if (HasCondition(COND_TOO_FAR_TO_ATTACK))
		{

			return SCHED_HOUNDEYE_CHASE_ENEMY;

		}
	}
	case NPC_STATE_IDLE:
	{
		if (!GetEnemy())
		{
			return SCHED_HOUNDEYE_WANDER;
		}
	}

	case NPC_STATE_ALERT:
	{
		CSound *pSound;
		pSound = GetBestSound();

		Assert(pSound != NULL);
		if (pSound)
		{
			return SCHED_HOUND_INVESTIGATE_SOUND;
		}
	}

	}
	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Gather conditions specific to this NPC
//-----------------------------------------------------------------------------
void CHoundeye::GatherConditions(void)
{
	// Call our base
	BaseClass::GatherConditions();

}

bool CHoundeye::IsValidCover(const Vector &vecCoverLocation, const CAI_Hint *pHint)
{
	if (!BaseClass::IsValidCover(vecCoverLocation, pHint))
		return false;


	if (m_pSquad)
	{
		AISquadIter_t iter;
		CAI_BaseNPC *pSquadmate = GetSquad() ? GetSquad()->GetFirstMember(&iter) : NULL;
		float SquadDistLimit = 192.0f;
		while (pSquadmate)
		{
			float SquadDist = (pSquadmate->GetAbsOrigin() - vecCoverLocation).Length();
			if (pSquadmate != this)
			{

				if (pSquadmate->GetNavigator()->IsGoalActive())
				{
					Vector vecPos = pSquadmate->GetNavigator()->GetGoalPos();

					SquadDist = (vecPos - vecCoverLocation).Length();
				}

				if (SquadDist < SquadDistLimit)
					return false;

			}



			pSquadmate = GetSquad()->GetNextMember(&iter);
		}

	}

	return true;
}
int CHoundeye::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_RANGE_ATTACK1:
	{
		return SCHED_HOUNDEYE_RANGEATTACK1;
		break;
	}
	}
	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CHoundeye::HandleInteraction(int interactionType, void* data, CBaseCombatCharacter* pSourceEnt)
{
	if ((pSourceEnt != this) && (interactionType == g_interactionHoundeyeSquadNewEnemy))
	{
		SetCondition(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY);
		//	DevMsg("squadmate found enemy condition\n");
		SetState(NPC_STATE_COMBAT);
		return true;
	}

	return BaseClass::HandleInteraction(interactionType, data, pSourceEnt);
}

void CHoundeye::BuildScheduleTestBits()
{
	BaseClass::BuildScheduleTestBits();
	// If we're not too busy, allow ourselves to ACK found enemy signals.
	if (!GetEnemy())
	{
		SetCustomInterruptCondition(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY);
	}
}

AI_BEGIN_CUSTOM_NPC(npc_houndeye, CHoundeye)

DECLARE_INTERACTION(g_interactionHoundeyeSquadNewEnemy);

DECLARE_CONDITION(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY)

DECLARE_ANIMEVENT(HOUND_AE_THUMP)
DECLARE_ANIMEVENT(HOUND_AE_FOOTSTEP)

DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_RANGEATTACK1,

"	Tasks"
"		TASK_STOP_MOVING		0"
"		TASK_FACE_ENEMY			0"
"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
"		TASK_RANGE_ATTACK1		0"
""
"	Interrupts"
"		COND_HEAVY_DAMAGE"
)
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_CHASE_ENEMY,

"	Tasks"
"		TASK_STOP_MOVING				0"
"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RUN_RANDOM"
//	"		TASK_SET_TOLERANCE_DISTANCE		24"
"		TASK_GET_CHASE_PATH_TO_ENEMY	300"
"		TASK_RUN_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
"		TASK_FACE_ENEMY			0"
""
"	Interrupts"
"		COND_NEW_ENEMY"
"		COND_ENEMY_DEAD"
"		COND_ENEMY_UNREACHABLE"
"		COND_CAN_RANGE_ATTACK1"
"		COND_TOO_CLOSE_TO_ATTACK"
"		COND_TASK_FAILED"
"		COND_LOST_ENEMY"
"		COND_HEAR_DANGER"
)
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_WANDER,

"	Tasks"
//	"		TASK_SET_TOLERANCE_DISTANCE		48"
"		TASK_SET_ROUTE_SEARCH_TIME		5"	// Spend 5 seconds trying to build a path if stuck
"		TASK_GET_PATH_TO_RANDOM_NODE	100"
"		TASK_WALK_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			7"
"		TASK_WAIT						8"
"		TASK_WAIT_PVS					0"
""
"	Interrupts"
"		COND_GIVE_WAY"
"		COND_HEAR_COMBAT"
"		COND_HEAR_DANGER"
"		COND_NEW_ENEMY"
"		COND_SEE_ENEMY"
"		COND_SEE_FEAR"
"		COND_SMELL"
"		COND_LIGHT_DAMAGE"
"		COND_HEAVY_DAMAGE"
"		COND_IDLE_INTERRUPT"
"		COND_CAN_RANGE_ATTACK1"
"		COND_CAN_MELEE_ATTACK1"
"		COND_TOO_CLOSE_TO_ATTACK"

)
DEFINE_SCHEDULE
(
SCHED_HOUND_INVESTIGATE_SOUND,

"	Tasks"
"		TASK_STOP_MOVING				0"
"		TASK_STORE_LASTPOSITION			0"
//	"		TASK_SET_TOLERANCE_DISTANCE		32"
"		TASK_GET_PATH_TO_BESTSOUND		0"
"		TASK_FACE_IDEAL					0"
"		TASK_RUN_PATH					0"
""
"	Interrupts"
"		COND_NEW_ENEMY"
"		COND_SEE_FEAR"
"		COND_SEE_ENEMY"
"		COND_LIGHT_DAMAGE"
"		COND_HEAVY_DAMAGE"
"		COND_HEAR_DANGER"
"		COND_CAN_RANGE_ATTACK1"
"		COND_CAN_MELEE_ATTACK1"
)
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_TAKE_COVER_FROM_ENEMY,

"	Tasks"
"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RUN_FROM_ENEMY"
"		TASK_STOP_MOVING				0"
//	"		TASK_WAIT						0.2"
//	"		TASK_SET_TOLERANCE_DISTANCE		24"
"		TASK_FIND_COVER_FROM_ENEMY		0"
"		TASK_RUN_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
"		TASK_REMEMBER					MEMORY:INCOVER"
"		TASK_FACE_ENEMY					0"
"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Translated to cover
//	"		TASK_WAIT						1"
""
"	Interrupts"
"		COND_NEW_ENEMY"
"		COND_HEAR_DANGER"
)
AI_END_CUSTOM_NPC()