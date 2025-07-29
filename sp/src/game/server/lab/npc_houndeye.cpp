//=============== Copyright Valve Corporation, All rights reserved. ==============//
//							   Sonic dog / Eye creature
//
//  Ported and initially provided by Gmadador, further AI work by Comfort Jones,
//	  extra features and tweaks by Dana Cief. Made for the Map Labs template.
//===============================================================================//
#include "cbase.h"
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "ai_hull.h"
#include "ai_squadslot.h"
#include "ai_squad.h"
#include "ai_baseactor.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "npc_antlion.h"
#include "particle_parse.h"
#include "te_particlesystem.h"
#include "sceneentity.h"
#include "entitylist.h"
#include "activitylist.h"
#include "beam_flags.h"
#include "engine/IEngineSound.h"
#include "hl2_shareddefs.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar	sk_houndeye_health("sk_houndeye_health", "45");
ConVar	sk_houndeye_dmg_blast("sk_houndeye_dmg_blast", "25");

ConVar houndeye_attack_max_range("houndeye_attack_max_range", "280");
#define HOUNDEYE_TOP_MASS	 300.0f

int		HOUND_AE_THUMP;
int		HOUND_AE_FOOTSTEP;

int g_interactionHoundeyeSquadNewEnemy;
int	g_interactionHoundeyeGroupAttack = 0;
int	g_interactionHoundeyeGroupRetreat = 0;
int	g_interactionHoundeyeGroupRalley = 0;

int s_iSonicEffectTexture = -1;

//=========================================================
// Private activities
//=========================================================
//int	ACT_HOUNDEYEPUNTOBJECT = -1;

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

	SCHED_HOUNDEYE_GROUP_ATTACK,
	SCHED_HOUNDEYE_GROUP_RETREAT,
	SCHED_HOUNDEYE_GROUP_RALLEY
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
	COND_HOUNDEYE_GROUP_ATTACK = LAST_SHARED_CONDITION,
	COND_HOUNDEYE_GROUP_RETREAT,
	COND_HOUNDEYE_GROUP_RALLEY,
};

//=========================================================
// Squad Slots
//=========================================================

enum SquadSlot_T
{
	SQUAD_SLOT_HOUNDEYE_ATTACK1 = LAST_SHARED_SQUADSLOT,
	SQUAD_SLOT_HOUNDEYE_ATTACK2,
	SQUAD_SLOT_HOUNDEYE_ATTACK3,
	SQUAD_SLOT_HOUNDEYE_ATTACK4,

};


//=========================================================
//=========================================================
class CHoundeye : public CAI_BaseActor
{
	DECLARE_CLASS(CHoundeye, CAI_BaseActor);

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
	virtual void HuntSound();

	int OnTakeDamage_Alive(const CTakeDamageInfo& inputInfo);

	void SonicAttack(void);
	bool IsAnyoneInSquadAttacking(void);
	float GetNearbyAttackers(void);
		
	//INPUTS
	void InputSonicAttack(inputdata_t &inputdata);

	void Event_Killed(const CTakeDamageInfo &info);

	float	MaxYawSpeed(void);
	int		RangeAttack1Conditions(float flDot, float flDist);

	bool	OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval);
	bool    MovementCost(int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost);

	float		InnateRange1MinRange(void) { return 0.0f; }
	float		InnateRange1MaxRange(void) { return 192.0f; }

	void	PrescheduleThink(void);

	int		SelectSchedule(void);
	virtual	void	GatherConditions(void);
	bool			IsValidCover(const Vector &vecCoverLocation, CAI_Hint const *pHint);
	bool			IsValidShootPosition(const Vector &vecCoverLocation, CAI_Node *pNode, CAI_Hint const *pHint);
	int TranslateSchedule(int scheduleType);
	bool			HandleInteraction(int interactionType, void* data, CBaseCombatCharacter* pSourceEnt);
	void			BuildScheduleTestBits();

	float		GetMaxJumpSpeed() const { return 320.0f; }
	float			GetJumpGravity() const		{ return 3.0f; }

	virtual float	HearingSensitivity(void) { return 1.0; };
	virtual int				GetSoundInterests(void);

	bool			m_fDontBlink;// don't try to open/close eye if this bit is set!


	DECLARE_DATADESC();

	// This is a dummy field. In order to provide save/restore
	// code in this file, we must have at least one field
	// for the code to operate on. Delete this field when
	// you are ready to do your own save/restore for this
	// character.
	//int		m_iDeleteThisField;

	DEFINE_CUSTOM_AI;
private: HSOUNDSCRIPTHANDLE	m_hFootstep;
};

LINK_ENTITY_TO_CLASS(npc_houndeye, CHoundeye);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CHoundeye)

DEFINE_INPUTFUNC(FIELD_VOID, "DoSonicAttack", InputSonicAttack),
DEFINE_FIELD(m_fDontBlink, FIELD_BOOLEAN),
//DEFINE_FIELD(m_iDeleteThisField, FIELD_INTEGER),

END_DATADESC()

void CHoundeye::Precache(void)
{
	PrecacheModel( DefaultOrCustomModel( "models/houndeye.mdl" ) );
	PrecacheScriptSound("NPC_Houndeye.Idle");
	PrecacheScriptSound("NPC_Houndeye.Sonic");
	PrecacheScriptSound("NPC_Houndeye.Alert");
	PrecacheScriptSound("NPC_Houndeye.Hunt");
	PrecacheScriptSound("NPC_Houndeye.Pain");
	PrecacheScriptSound("NPC_Houndeye.Die");
	s_iSonicEffectTexture = PrecacheModel("sprites/physbeam.vmt");
	m_hFootstep = PrecacheScriptSound("NPC_Houndeye.Footstep");

	PrecacheParticleSystem("houndeye_sonicattack_ring");
	PrecacheParticleSystem("houndeye_sonicattack_ring-squad");
	PrecacheParticleSystem("houndeye_sonicattack_ring-squadfull");

	PrecacheInstancedScene("scenes/npc/houndeye/houndeye_attack.vcd");
	PrecacheInstancedScene("scenes/npc/houndeye/houndeye_blink.vcd");
	PrecacheInstancedScene("scenes/npc/houndeye/houndeye_flinch.vcd");

	BaseClass::Precache();
}

void CHoundeye::Spawn(void)
{
	Precache();

	SetModel( DefaultOrCustomModel( "models/houndeye.mdl" ) );
	BaseClass::Spawn();
	SetHullType(HULL_MEDIUM); //TODO: Look into a custom 48x48x48 hull in the future
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetNavType(NAV_GROUND);
	SetMoveType(MOVETYPE_STEP);
	SetBloodColor(BLOOD_COLOR_YELLOW);

	m_iHealth = sk_houndeye_health.GetFloat();
	m_flFieldOfView = 0.3;
	m_NPCState = NPC_STATE_NONE;
	m_fDontBlink = false;

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_MOVE_JUMP | bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD);
	CapabilitiesAdd(bits_CAP_INNATE_RANGE_ATTACK1);
	CapabilitiesAdd(bits_CAP_SQUAD);

	//SetCollisionGroup(HL2COLLISION_GROUP_HOUNDEYE);

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
		ClearExpression();
		SonicAttack();
		return;
	}
	if (pEvent->event == HOUND_AE_FOOTSTEP)
	{
		MakeAIFootstepSound(240.0f);
		EmitSound("NPC_Houndeye.Footstep", m_hFootstep, pEvent->eventtime);
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

	//Damage and radius is boosted by a max of 3 squadmates attacking nearby -- CJ
	float NearbyAttackers = GetNearbyAttackers();
	NearbyAttackers = clamp(NearbyAttackers, 0.0f, 3.0f);
	float		flRadius = houndeye_attack_max_range.GetFloat();

	flRadius += ((houndeye_attack_max_range.GetFloat() * 0.5f) * NearbyAttackers);

	int red = 255 - (100 * NearbyAttackers);
	int green = 255 - (60 * NearbyAttackers);
	int blu = 255;

	EmitSound("NPC_Houndeye.Sonic");

	SetExpression("scenes/npc/houndeye/houndeye_attack.vcd");

	if (m_pSquad && m_pSquad->NumMembers() > 3)
	{
		DispatchParticleEffect("houndeye_sonicattack_ring-squadfull", PATTACH_ROOTBONE_FOLLOW, this);
	}
	if (m_pSquad && m_pSquad->NumMembers() > 1)
	{
		DispatchParticleEffect("houndeye_sonicattack_ring-squad", PATTACH_ROOTBONE_FOLLOW, this);
	}
	else
	{
		DispatchParticleEffect("houndeye_sonicattack_ring", PATTACH_ROOTBONE_FOLLOW, this);
	}
	

	// Old beam effect
	CBroadcastRecipientFilter filter;
	te->BeamRingPoint(filter, 0.0,
		GetAbsOrigin(),							//origin
		16,									//start radius
		flRadius,									//end radius
		s_iSonicEffectTexture,				//texture
		0,									//halo index
		0,									//start frame
		0,									//framerate
		0.2,								//life
		32,									//width
		16,									//spread
		0,									//amplitude
		red,									//r
		green,									//g
		blu,								//b
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

				flDist = (pEntity->WorldSpaceCenter() - GetAbsOrigin()).Length();

				// houndeyes do FULL damage if the ent in question is visible. Half damage otherwise.
				// This means that you must get out of the houndeye's attack range entirely to avoid damage.
				// Calculate full damage first

				if (m_pSquad && m_pSquad->NumMembers() > 1)
				{
					// squad gets attack bonus.

					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat() + ((sk_houndeye_dmg_blast.GetFloat() * 0.5f) * NearbyAttackers);
				}
				else
				{
					// solo
					flAdjustedDamage = sk_houndeye_dmg_blast.GetFloat();
				}

				flAdjustedDamage -= (flDist / houndeye_attack_max_range.GetFloat()) * flAdjustedDamage;

				// Cief: Flip over distant antlions.	
				trace_t tr;
				if (FClassnameIs(pEntity, "npc_antlion"))
				{
					// For antlions, we give only a third of the damage to balance squad damage.
					flAdjustedDamage *= 0.30f;

					CNPC_Antlion *pAntlion = static_cast<CNPC_Antlion *>(pEntity);
					// Attempt to trace a line to hit the target
					UTIL_TraceLine(GetAbsOrigin(), pAntlion->BodyTarget(GetAbsOrigin()), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
					if (tr.fraction < 1.0f && tr.m_pEnt != pAntlion)
						continue;

					Vector vecDir = (pAntlion->GetAbsOrigin() - GetAbsOrigin());
					vecDir[2] = 0.0f;
					float flDist = VectorNormalize(vecDir);

					float flFalloff = RemapValClamped(flDist, 0, flRadius*0.50f, 1.0f, 0.1f);

					vecDir *= (flRadius * 1.5f * flFalloff);
					vecDir[2] += (flRadius * 0.5f * flFalloff);

					pAntlion->ApplyAbsVelocityImpulse(vecDir);

					// gib nearby antlions, knock over distant ones. 
					if (flDist < 96)
					{
						// splat!
						vecDir[2] += 400.0f * flFalloff;
						CTakeDamageInfo dmgInfo(this, this, vecDir, pAntlion->GetAbsOrigin(), 100, DMG_SONIC);
						pAntlion->TakeDamage(dmgInfo);
					}
					else
					{
						// Turn them over
						pAntlion->Flip(true);
					}
				}

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
								UTIL_ScreenShake(pEntity->GetAbsOrigin(), 50, 150.0, 1.0, 512, SHAKE_START);

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
	const float MAX_JUMP_DISTANCE = 250.0f;
	const float MAX_JUMP_DROP = 128.0f;

	if (BaseClass::IsJumpLegal(startPos, apex, endPos, MAX_JUMP_RISE, MAX_JUMP_DROP, MAX_JUMP_DISTANCE))
	{
		// Hang onto the jump distance. The AI is going to want it.
		//m_flJumpDist = (startPos - endPos).Length();

		return true;
	}
	return false;
}
//------------------------------------------------------------------------------
// Purpose : Broadcast retreat when member of squad killed
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CHoundeye::Event_Killed(const CTakeDamageInfo &info)
{
	EmitSound("NPC_Houndeye.Retreat");
	m_flSoundWaitTime = gpGlobals->curtime + 1.0;

	if (m_pSquad)
	{
		m_pSquad->BroadcastInteraction(g_interactionHoundeyeGroupRetreat, NULL, this);
	}

	BaseClass::Event_Killed(info);
}

void CHoundeye::IdleSound()
{
	EmitSound("NPC_Houndeye.Idle");
}

void CHoundeye::HuntSound()
{
	EmitSound("NPC_Houndeye.Hunt");
}


void CHoundeye::AlertSound()
{
	// only first squad member makes ALERT sound.
	if (m_pSquad && !m_pSquad->IsLeader(this))
	{
		return;
	}
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

void CHoundeye::InputSonicAttack(inputdata_t &inputdata)
{
	SetSchedule(SCHED_HOUNDEYE_RANGEATTACK1);
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
	//Occasionally broadcast retreat to squad
	if (m_pSquad && random->RandomInt(0, 10) == 10)
	{
		EmitSound("NPC_Houndeye.Retreat");
		m_flSoundWaitTime = gpGlobals->curtime + 1.0;

		m_pSquad->BroadcastInteraction(g_interactionHoundeyeGroupRetreat, NULL, this);
	}

	SetExpression("scenes/npc/houndeye/houndeye_flinch.vcd");

	return BaseClass::OnTakeDamage_Alive(inputInfo);
}
bool CHoundeye::IsAnyoneInSquadAttacking(void)
{
	if (!m_pSquad)
	{
		return false;
	}
	//Checks if anyone in our squad is attacking already
	AISquadIter_t iter;
	for (CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember(&iter); pSquadMember; pSquadMember = m_pSquad->GetNextMember(&iter))
	{
		if (pSquadMember->IsCurSchedule(SCHED_HOUNDEYE_RANGEATTACK1))
		{
			return true;
		}
	}
	return false;
}

float CHoundeye::GetNearbyAttackers(void)
{
	if (!m_pSquad)
	{
		return 0;
	}

	float NumAttackers = 0;

	//Checks if anyone in our squad is attacking already
	AISquadIter_t iter;
	for (CAI_BaseNPC *pSquadMember = m_pSquad->GetFirstMember(&iter); pSquadMember; pSquadMember = m_pSquad->GetNextMember(&iter))
	{
		if (pSquadMember == this)
			continue;

		float SquadDist = (pSquadMember->GetAbsOrigin() - GetAbsOrigin()).Length();
		if (pSquadMember->IsCurSchedule(SCHED_HOUNDEYE_RANGEATTACK1) && SquadDist < houndeye_attack_max_range.GetFloat())
		{
			NumAttackers++;
		}
	}
	return NumAttackers;
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
	if (flDist >(flMaxHoundeyeAttackMaxRange))
	{
		return COND_TOO_FAR_TO_ATTACK;
	}

	trace_t tr;
	AI_TraceHull(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 1),
		GetHullMins(), GetHullMaxs(),
		MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);

	if (tr.startsolid)
	{
		return COND_WEAPON_SIGHT_OCCLUDED;
	}

	/*
	if (flDot < 0.6)
	{
	return COND_NOT_FACING_ATTACK;
	}
	*/
	return COND_CAN_RANGE_ATTACK1;
}

//-----------------------------------------------------------------------------
// Purpose: turn in the direction of movement
// Output :
//-----------------------------------------------------------------------------
bool CHoundeye::OverrideMoveFacing(const AILocalMoveGoal_t &move, float flInterval)
{
	if (GetEnemy())
	{
		if (IsCurSchedule(SCHED_HOUNDEYE_CHASE_ENEMY) && EnemyDistance(GetEnemy()) < houndeye_attack_max_range.GetFloat())
		{
			AddFacingTarget(GetEnemy()->GetAbsOrigin(), 1.0f, 0.2f);
		}
		else return false;
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

bool CHoundeye::MovementCost(int moveType, const Vector &vecStart, const Vector &vecEnd, float *pCost)
{
	float multiplier = 1;

	if (!OccupyStrategySlotRange(SQUAD_SLOT_HOUNDEYE_ATTACK1, SQUAD_SLOT_HOUNDEYE_ATTACK2))
	{
		Vector	moveDir = (vecEnd - vecStart);
		VectorNormalize(moveDir);
		if (m_pSquad)
		{
			AISquadIter_t iter;

			CAI_BaseNPC *pSquadmate = m_pSquad ? m_pSquad->GetFirstMember(&iter) : NULL;
			while (pSquadmate)
		{
				Vector	SquadmateDir = (pSquadmate->GetAbsOrigin() - vecStart);
				VectorNormalize(SquadmateDir);

				// If we're moving towards our enemy, then the cost is much higher than normal
				if (DotProduct(SquadmateDir, moveDir) > 0.5f)
				{
					multiplier += 50.0f;
				}

				pSquadmate = m_pSquad->GetNextMember(&iter);
			}
		}
	}

	*pCost *= multiplier;

	return (multiplier != 1);
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
		// dead enemy

		if (HasCondition(COND_ENEMY_DEAD))
		{
			// call base class, all code to handle dead enemies is centralized there.
			return BaseClass::SelectSchedule();
		}

		// If a group attack was requested attack even if attack conditions not met
		if (HasCondition(COND_HOUNDEYE_GROUP_ATTACK))
		{
			// Check that I'm not standing in another hound eye 
			// before attacking
			trace_t tr;
			AI_TraceHull(GetAbsOrigin(), GetAbsOrigin() + Vector(0, 0, 1),
				GetHullMins(), GetHullMaxs(),
				MASK_NPCSOLID, this, COLLISION_GROUP_NONE, &tr);
			if (!tr.startsolid)
			{
				return SCHED_HOUNDEYE_GROUP_ATTACK;
			}

			// Otherwise attack as soon as I can
			else
			{
				m_flNextAttack = gpGlobals->curtime;
				SCHED_CHASE_ENEMY;
			}
		}

		// If a group retread was requested 
		if (HasCondition(COND_HOUNDEYE_GROUP_RETREAT))
		{
			return SCHED_HOUNDEYE_GROUP_RETREAT;
		}

		if (HasCondition(COND_LIGHT_DAMAGE))
		{
			if (random->RandomInt(0, 3) > 2)
			{
				//DevMsg("ow!\n");
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
		// If a group rally was requested 
		if (HasCondition(COND_HOUNDEYE_GROUP_RALLEY))
		{
			return SCHED_HOUNDEYE_GROUP_RALLEY;
		}

		if (HasCondition(COND_CAN_RANGE_ATTACK1))
		{
			if (OccupyStrategySlotRange(SQUAD_SLOT_HOUNDEYE_ATTACK1, SQUAD_SLOT_HOUNDEYE_ATTACK3))
				return SCHED_HOUNDEYE_RANGEATTACK1;
			return SCHED_COMBAT_FACE;
		}
		if (HasCondition(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY))
		{
			// A squadmate found an enemy. Respond to their call.
			//	DevMsg("Squadmate has found enemy, setting my schedule to chase enemy!\n");
			return SCHED_HOUNDEYE_CHASE_ENEMY;
		}

		if (HasCondition(COND_TOO_FAR_TO_ATTACK) || !HasCondition(COND_SEE_ENEMY) || HasCondition(COND_WEAPON_SIGHT_OCCLUDED))
		{

			return SCHED_HOUNDEYE_CHASE_ENEMY;

		}
		else
		{
			if (m_pSquad && random->RandomInt(0, 5) == 0)
			{
				if (!IsAnyoneInSquadAttacking())
				{
					EmitSound("NPC_Houndeye.Hunt");

					m_flSoundWaitTime = gpGlobals->curtime + 1.0;

					m_pSquad->BroadcastInteraction(g_interactionHoundeyeGroupRalley, NULL, this);
					return SCHED_CHASE_ENEMY;
				}
			}
			return SCHED_MOVE_AWAY;
		}
		break;
	}
	case NPC_STATE_IDLE:
	{
		if (!GetEnemy())
		{
			return SCHED_HOUNDEYE_WANDER;
		}
		break;
	}

	case NPC_STATE_ALERT:
	{
		CSound *pSound;
		pSound = GetBestSound();

		Assert(pSound != NULL);
		if (pSound && !FInViewCone(pSound->GetSoundOrigin()))
		{
			HuntSound();
			return SCHED_HOUND_INVESTIGATE_SOUND;
		}
		if (HasCondition(COND_LIGHT_DAMAGE) ||
			HasCondition(COND_HEAVY_DAMAGE))
		{
			return SCHED_TAKE_COVER_FROM_ORIGIN;
		}
		break;
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
		float SquadDistLimit = 64.0f;
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

bool CHoundeye::IsValidShootPosition(const Vector &vecCoverLocation, CAI_Node *pNode, CAI_Hint const *pHint)
{

	if (!BaseClass::IsValidShootPosition(vecCoverLocation, pNode, pHint))
		return false;

	if (m_pSquad)
	{
		AISquadIter_t iter;
		CAI_BaseNPC *pSquadmate = GetSquad() ? GetSquad()->GetFirstMember(&iter) : NULL;
		while (pSquadmate)
		{
			if (pSquadmate != this)
			{
				Vector vecPos = pSquadmate->GetAbsOrigin();
				float SquadDist = (vecPos - vecCoverLocation).Length();

				if (pSquadmate->GetNavigator()->IsGoalActive())
				{
					vecPos = pSquadmate->GetNavigator()->GetGoalPos();

					SquadDist = (vecPos - vecCoverLocation).Length();
				}

				if (SquadDist <= 96.0f)
					return false;
			}

			pSquadmate = GetSquad()->GetNextMember(&iter);
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Purpose :
// Input   :
// Output  :
//------------------------------------------------------------------------------
void CHoundeye::PrescheduleThink(void)
{
	BaseClass::PrescheduleThink();

	//Cief: HACK! Check if houndeye is attacking to clear expressions, and allow blinking if not.
	if (!IsCurSchedule(SCHED_HOUNDEYE_RANGEATTACK1))
	{
		m_fDontBlink = 0;
		ClearExpression();
	}
	else
	{
		m_fDontBlink = 1;
	}
	// if the hound is mad and is running, make hunt noises.
	if (m_NPCState == NPC_STATE_COMBAT && (GetActivity() == ACT_RUN) && random->RandomFloat(0, 1) < 0.2)
	{
		HuntSound();
	}

	// Cief: at random, initiate a blink if not already blinking. 
	//Adapted from previous code since default blinking looks awful on them.
	if (!m_fDontBlink)
	{
		if (random->RandomInt(0, 12) == 0)
		{// do a blink!
			SetExpression("scenes/npc/houndeye/houndeye_blink.vcd");
		}
	}
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

	case SCHED_HOUNDEYE_CHASE_ENEMY:
	{
		//If we're not one of the first attackers, let's just do a los schedule so we can try to get a better flanking position etc. --CJ
		if (!OccupyStrategySlotRange(SQUAD_SLOT_HOUNDEYE_ATTACK1, SQUAD_SLOT_HOUNDEYE_ATTACK2))
			return SCHED_HOUNDEYE_GROUP_RALLEY;
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

	if (IsCurSchedule(SCHED_HOUNDEYE_CHASE_ENEMY))
	{
		if (GetEnemy())
		{
			if (EnemyDistance(GetEnemy()) <= houndeye_attack_max_range.GetFloat() * .6)
			{
				SetCustomInterruptCondition(COND_CAN_RANGE_ATTACK1);
			}
		}
	}
}

AI_BEGIN_CUSTOM_NPC(npc_houndeye, CHoundeye)

DECLARE_INTERACTION(g_interactionHoundeyeSquadNewEnemy);

DECLARE_CONDITION(COND_HOUNDEYE_SQUADMATE_FOUND_ENEMY)
DECLARE_CONDITION(COND_HOUNDEYE_GROUP_ATTACK)
DECLARE_CONDITION(COND_HOUNDEYE_GROUP_RETREAT)

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
//"		TASK_STOP_MOVING				0"
//"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_RUN_RANDOM"
"		TASK_SET_TOLERANCE_DISTANCE		64"
"		TASK_GET_CHASE_PATH_TO_ENEMY	0"
"		TASK_RUN_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
"		TASK_FACE_ENEMY			0"
""
"	Interrupts"
"		COND_NEW_ENEMY"
"		COND_ENEMY_DEAD"
"		COND_ENEMY_UNREACHABLE"
//"		COND_CAN_RANGE_ATTACK1"
"		COND_TOO_CLOSE_TO_ATTACK"
"		COND_TASK_FAILED"
"		COND_LOST_ENEMY"
"		COND_HEAR_DANGER"
"		COND_HOUNDEYE_GROUP_ATTACK"
"		COND_HOUNDEYE_GROUP_RETREAT"
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
"		COND_HOUNDEYE_GROUP_ATTACK"
"		COND_HOUNDEYE_GROUP_RETREAT"

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
"		COND_HOUNDEYE_GROUP_ATTACK"
"		COND_HOUNDEYE_GROUP_RETREAT"
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
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_GROUP_RALLEY,

"	Tasks"
"		TASK_SET_TOLERANCE_DISTANCE		30"
"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_CHASE_ENEMY"
//"		TASK_GET_PATH_TO_TARGET			0"
"		TASK_GET_PATH_TO_ENEMY_LKP		0"
"		TASK_RUN_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
""
"	Interrupts"
"		COND_NEW_ENEMY"
"		COND_ENEMY_DEAD"
"		COND_HEAVY_DAMAGE"
"		COND_HOUNDEYE_GROUP_ATTACK"
"		COND_HOUNDEYE_GROUP_RETREAT"
);
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_GROUP_RETREAT,

"	Tasks"
"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_MOVE_AWAY"
"		TASK_STOP_MOVING				0"
"		TASK_WAIT						0.2"
"		TASK_SET_TOLERANCE_DISTANCE		24"
"		TASK_FIND_COVER_FROM_ENEMY		0"
"		TASK_RUN_PATH					0"
"		TASK_WAIT_FOR_MOVEMENT			0"
"		TASK_REMEMBER					MEMORY:INCOVER"
"		TASK_FACE_ENEMY					0"
"		TASK_SET_ACTIVITY				ACTIVITY:ACT_IDLE"	// Translated to cover
"		TASK_SET_SCHEDULE				SCHEDULE:SCHED_HOUNDEYE_TAKE_COVER_FROM_ENEMY"
""
"	Interrupts"
"		COND_NEW_ENEMY"
);
DEFINE_SCHEDULE
(
SCHED_HOUNDEYE_GROUP_ATTACK,

"	Tasks "
"		TASK_STOP_MOVING			0"
"		TASK_FACE_ENEMY				0"
"		TASK_SET_ACTIVITY			ACTIVITY:ACT_IDLE_ANGRY"
"		TASK_SPEAK_SENTENCE			0"
"		TASK_WAIT					1"
"		TASK_SET_SCHEDULE			SCHEDULE:SCHED_HOUND_RANGE_ATTACK1"
""
"	Interrupts "
"		COND_NEW_ENEMY"
"		COND_ENEMY_DEAD"
"		COND_HEAVY_DAMAGE"
);
AI_END_CUSTOM_NPC()