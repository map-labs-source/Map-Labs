//=//=============================================================================//
//
// Purpose: A malevolent being from a parallel universe which at one point
//		may have been human.		
//
//		npc_shadow_walker is designed to be reusable as a generic horror 
//		game style npc. Its model and sound files may be configured through
//		the hammer editor using keyfields.
//
//	Author: 1upD
//
//=============================================================================//
#include "cbase.h"
#include "npc_shadow_walker.h"
#include "ai_hull.h"
#include "soundent.h"
#include "game.h"
#include "npcevent.h"
#include "engine/IEngineSound.h"
#include "basehlcombatweapon_shared.h"
#include "ai_squadslot.h"
#include "weapon_custom_melee.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//---------------------------------------------------------
// Constants
//---------------------------------------------------------
const float MIN_TIME_NEXT_SOUND = 0.5f;
const float MAX_TIME_NEXT_SOUND = 1.0f;
const float MIN_TIME_NEXT_FOUNDENEMY_SOUND = 2.0f;
const float MAX_TIME_NEXT_FOUNDENEMY_SOUND = 5.0f;

//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::Precache( void )
{
	// If no model name is supplied, use the default Shadow Walker model
	if (!GetModelName())
	{
		SetModelName(MAKE_STRING("models/monster/subject.mdl"));
	}

	if (&m_iszWeaponModelName && m_iszWeaponModelName != MAKE_STRING("")) {
		PrecacheModel(STRING(m_iszWeaponModelName));
	}
	else {
		PrecacheModel("models/props_canal/mattpipe.mdl");
	}

	PrecacheModel(STRING(GetModelName()));
	PrecacheNPCSoundScript(&m_iszFearSound, MAKE_STRING("NPC_ShadowWalker.Fear"));
	PrecacheNPCSoundScript(&m_iszIdleSound, MAKE_STRING("NPC_ShadowWalker.Idle"));
	PrecacheNPCSoundScript(&m_iszAlertSound, MAKE_STRING("NPC_ShadowWalker.Alert"));
	PrecacheNPCSoundScript(&m_iszPainSound, MAKE_STRING("NPC_ShadowWalker.Pain"));
	PrecacheNPCSoundScript(&m_iszLostEnemySound, MAKE_STRING("NPC_ShadowWalker.LostEnemy"));
	PrecacheNPCSoundScript(&m_iszFoundEnemySound, MAKE_STRING("NPC_ShadowWalker.FoundEnemy"));
	PrecacheNPCSoundScript(&m_iszDeathSound, MAKE_STRING("NPC_ShadowWalker.Death"));

	m_bWanderToggle = false;

	BaseClass::Precache();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
//
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::Spawn( void )
{
	Precache();

	SetModel(STRING(GetModelName()));
	SetHullType(HULL_HUMAN);
	SetHullSizeNormal();

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetBloodColor( BLOOD_COLOR_RED );
	
	// If the health has not been set through Hammer, use a default health value of 75
	if (m_iHealth < 1) 
	{
		m_iHealth = 75;
	}

	m_flFieldOfView		= 0.5;
	m_flNextSoundTime = gpGlobals->curtime;
	m_flNextFoundEnemySoundTime = gpGlobals->curtime;
	m_NPCState			= NPC_STATE_NONE;
	m_flSpeedModifier = 1.0f;

	CapabilitiesClear();

	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesAdd(bits_CAP_ANIMATEDFACE | bits_CAP_TURN_HEAD); // The default model has no face animations, but a custom model might
		CapabilitiesAdd(bits_CAP_SQUAD);
		CapabilitiesAdd(bits_CAP_USE_WEAPONS | bits_CAP_AIM_GUN | bits_CAP_MOVE_SHOOT);
		CapabilitiesAdd(bits_CAP_WEAPON_MELEE_ATTACK1 || bits_CAP_WEAPON_MELEE_ATTACK2);
		CapabilitiesAdd(bits_CAP_DUCK);
		CapabilitiesAdd(bits_CAP_USE_SHOT_REGULATOR);

		if (!m_bCannotOpenDoors) {
			CapabilitiesAdd(bits_CAP_DOORS_GROUP);
		}
	}

	CapabilitiesAdd(bits_CAP_MOVE_GROUND);
	SetMoveType(MOVETYPE_STEP);

	NPCInit();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::FixupWeapon()
{
	// If no weapons supplied, give a crowbar
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if (pWeapon == NULL) {
		CWeaponCustomMelee *pMeleeWeapon = (CWeaponCustomMelee*)CreateEntityByName("weapon_custommelee");

		// Apply weapon model override
		if (&m_iszWeaponModelName && m_iszWeaponModelName != MAKE_STRING("")) {
			pMeleeWeapon->m_iszWeaponModelName = this->m_iszWeaponModelName;
		}
		// Default custom weapon model
		else {
			pMeleeWeapon->m_iszWeaponModelName = MAKE_STRING("models/props_canal/mattpipe.mdl");
		}

		pWeapon = (CBaseCombatWeapon *)pMeleeWeapon;

		DispatchSpawn(pWeapon);
		Weapon_Equip(pWeapon);
	}

}

void CNPC_ShadowWalker::Activate()
{
	BaseClass::Activate();
	FixupWeapon();
}


//-----------------------------------------------------------------------------
// Purpose: Choose a schedule after schedule failed
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode)
{
	switch (failedSchedule)
	{
	case SCHED_NEW_WEAPON:
		// If failed trying to pick up a weapon, try again in one second. This is because other AI code
		// has put this off for 10 seconds under the assumption that the citizen would be able to 
		// pick up the weapon that they found. 
		m_flNextWeaponSearchTime = gpGlobals->curtime + 1.0f;
		break;
	case SCHED_TAKE_COVER_FROM_ENEMY:
		// I can't take cover, so I need to run away!
		return SCHED_RUN_FROM_ENEMY;
	case SCHED_CHASE_ENEMY:
		// I can't run towards the enemy, so I will just run randomly!
		return SCHED_CHASE_ENEMY_FAILED;
	case SCHED_RUN_FROM_ENEMY:
		// I can't run away, so I will just run randomly!
		return SCHED_RUN_RANDOM;
	case SCHED_INVESTIGATE_SOUND:
		// I can't investigate a sound I heard.
		return SCHED_IDLE_WANDER;
	}

	return BaseClass::SelectFailSchedule(failedSchedule, failedTask, taskFailCode);
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to retrieve better weapons if they are available.
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectScheduleRetrieveItem()
{
	if (m_bCanPickupWeapons && HasCondition(COND_BETTER_WEAPON_AVAILABLE))
	{
		CBaseHLCombatWeapon *pWeapon = dynamic_cast<CBaseHLCombatWeapon *>(Weapon_FindUsable(WEAPON_SEARCH_DELTA));
		if (pWeapon)
		{
			m_flNextWeaponSearchTime = gpGlobals->curtime + 10.0;
			// Now lock the weapon for several seconds while we go to pick it up.
			pWeapon->Lock(10.0, this);
			SetTarget(pWeapon);
			return SCHED_NEW_WEAPON;
		}
	}

	return SCHED_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to retrieve better weapons if they are available.
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectScheduleWander()
{
	m_bWanderToggle = !m_bWanderToggle;
	if (m_bWanderToggle) {
		return SCHED_IDLE_WANDER;
	}
	else {
		return SCHED_NONE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Select a schedule to execute based on conditions. 
// This is the most critical AI method.
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectSchedule()
{
	switch (m_NPCState)
	{
	case NPC_STATE_IDLE:
		AssertMsgOnce(GetEnemy() == NULL, "NPC has enemy but is not in combat state?");
		return SelectIdleSchedule();

	case NPC_STATE_ALERT:
		AssertMsgOnce(GetEnemy() == NULL, "NPC has enemy but is not in combat state?");
		return SelectAlertSchedule();

	case NPC_STATE_COMBAT:
		return SelectCombatSchedule();

	default:
		return BaseClass::SelectSchedule();
	}
}

//-----------------------------------------------------------------------------
// Idle schedule selection
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectIdleSchedule()
{
	int nSched = SelectFlinchSchedule();
	if (nSched != SCHED_NONE)
		return nSched;

	if (HasCondition(COND_HEAR_DANGER) ||
		HasCondition(COND_HEAR_COMBAT) ||
		HasCondition(COND_HEAR_WORLD) ||
		HasCondition(COND_HEAR_BULLET_IMPACT) ||
		HasCondition(COND_HEAR_PLAYER))
	{
		// Investigate sound source
		return SCHED_INVESTIGATE_SOUND;
	}

	nSched = SelectScheduleRetrieveItem();
	if (nSched != SCHED_NONE)
		return nSched;

	// no valid route! Wander instead
	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE) {
		nSched = SelectScheduleWander();
		if (nSched != SCHED_NONE)
			return nSched;
		return SCHED_IDLE_STAND;
	}

	// valid route. Get moving
	return SCHED_IDLE_WALK;
}

//-----------------------------------------------------------------------------
// Alert schedule selection
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectAlertSchedule()
{
	// Per default base NPC, check flinch schedule first
	int nSched = SelectFlinchSchedule();
	if (nSched != SCHED_NONE)
		return nSched;

	// Scan around for new enemies
	if (HasCondition(COND_ENEMY_DEAD) && SelectWeightedSequence(ACT_VICTORY_DANCE) != ACTIVITY_NOT_AVAILABLE)
		return SCHED_ALERT_SCAN;

	if (HasCondition(COND_HEAR_DANGER) ||
		HasCondition(COND_HEAR_PLAYER) ||
		HasCondition(COND_HEAR_WORLD) ||
		HasCondition(COND_HEAR_BULLET_IMPACT) ||
		HasCondition(COND_HEAR_COMBAT))
	{
		// Investigate sound source
		AlertSound();
		return SCHED_INVESTIGATE_SOUND;
	}

	nSched = SelectScheduleRetrieveItem();
	if (nSched != SCHED_NONE)
		return nSched;

	// no valid route! Wander instead
	if (GetNavigator()->GetGoalType() == GOALTYPE_NONE) {
		nSched = SelectScheduleWander();
		if (nSched != SCHED_NONE)
			return nSched;
		return SCHED_IDLE_STAND;
	}

	// valid route. Get moving
	return SCHED_ALERT_WALK;
}

//-----------------------------------------------------------------------------
// Combat schedule selection
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::SelectCombatSchedule()
{
	// Check flinch first
	int nSched = SelectFlinchSchedule();
	if (nSched != SCHED_NONE)
		return nSched;

	// Check enemy death
	if (HasCondition(COND_ENEMY_DEAD))
	{
		// clear the current (dead) enemy and try to find another.
		SetEnemy(NULL);

		if (ChooseEnemy())
		{
			FoundEnemySound();

			ClearCondition(COND_ENEMY_DEAD);
			return SelectSchedule();
		}

		SetState(NPC_STATE_ALERT);
		return SelectSchedule();
	}

	// Can any enemies see me?
	bool bEnemyCanSeeMe = HasCondition(COND_SEE_ENEMY) && HasCondition(COND_ENEMY_FACING_ME) && HasCondition(COND_HAVE_ENEMY_LOS);


	// If I'm scared of this enemy and he's looking at me, run away
	if ((IRelationType(GetEnemy()) == D_FR) && bEnemyCanSeeMe)
	{
			FearSound();
			return SCHED_RUN_FROM_ENEMY;
	}

	// If in a squad, only one or two shadow walkers can chase the player. This is configurable through Hammer.
	bool bCanChase = true;
	if (bEnemyCanSeeMe && m_bUseBothSquadSlots) {
		bCanChase = OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2);
	}
	else if (bEnemyCanSeeMe){
		bCanChase = OccupyStrategySlot(SQUAD_SLOT_ATTACK1);
	}

	// If I'm not allowed to chase this enemy of this enemy and he's looking at me, set up an ambush
	if (!bCanChase)
	{
		FearSound();
		return SCHED_HIDE;
		
	}

	// Reloading conditions are necessary just in case for some reason somebody gives the Shadow Walker a gun
	if (HasRangedWeapon() && (HasCondition(COND_LOW_PRIMARY_AMMO) || HasCondition(COND_NO_PRIMARY_AMMO)))
	{
		return SCHED_HIDE_AND_RELOAD;
	}

	// Can we see the enemy?
	if (!HasCondition(COND_SEE_ENEMY))
	{
		// Chase!
		return SCHED_CHASE_ENEMY;
	}

	if (HasCondition(COND_TOO_CLOSE_TO_ATTACK))
		return SCHED_BACK_AWAY_FROM_ENEMY;


	// we can see the enemy
	if (HasCondition(COND_CAN_MELEE_ATTACK1)) {
			return SCHED_MELEE_ATTACK1;
	}

	if (HasCondition(COND_CAN_MELEE_ATTACK2)) {
			return SCHED_MELEE_ATTACK2;
	}

	if (HasRangedWeapon() && GetShotRegulator()->IsInRestInterval())
	{
		if (HasCondition(COND_CAN_RANGE_ATTACK1))
			return SCHED_COMBAT_FACE;
	}

	if (HasRangedWeapon() && HasCondition(COND_CAN_RANGE_ATTACK1))
	{
		if (OccupyStrategySlotRange(SQUAD_SLOT_ATTACK1, SQUAD_SLOT_ATTACK2))
			return SCHED_RANGE_ATTACK1;
		return SCHED_RUN_FROM_ENEMY;
	}

	if (HasRangedWeapon() && HasCondition(COND_CAN_RANGE_ATTACK2))
		return SCHED_RANGE_ATTACK2;


	if (HasCondition(COND_NOT_FACING_ATTACK))
		return SCHED_COMBAT_FACE;

	if (!HasCondition(COND_CAN_RANGE_ATTACK1) && !HasCondition(COND_CAN_MELEE_ATTACK1))
	{
		// if we can see enemy but can't use either attack type, we must need to get closer to enemy
		if (HasRangedWeapon())
			return SCHED_MOVE_TO_WEAPON_RANGE;

		return SCHED_CHASE_ENEMY;
	}

	DevWarning(2, "No suitable combat schedule!\n");
	return SCHED_FAIL;
}

bool CNPC_ShadowWalker::HasRangedWeapon()
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();

	if (pWeapon)
		return !(FClassnameIs(pWeapon, "weapon_crowbar") || FClassnameIs(pWeapon, "weapon_stunstick") || FClassnameIs(pWeapon, "weapon_custommelee"));

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Override base class schedules
//-----------------------------------------------------------------------------
int CNPC_ShadowWalker::TranslateSchedule(int scheduleType)
{
	switch (scheduleType)
	{
	case SCHED_MELEE_ATTACK1:
		return SCHED_MELEE_ATTACK_NOINTERRUPT;
	case SCHED_IDLE_WANDER: // We want idle wandering to be interruptible - patrol walk is a better schedule
		return SCHED_PATROL_WALK;
	}

	return BaseClass::TranslateSchedule(scheduleType);
}

//-----------------------------------------------------------------------------
// Purpose: Override base class activiites
//-----------------------------------------------------------------------------
Activity CNPC_ShadowWalker::NPC_TranslateActivity(Activity activity)
{
	switch (activity) {
	case ACT_RUN_AIM_SHOTGUN:
		return ACT_RUN_AIM_RIFLE;
	case ACT_WALK_AIM_SHOTGUN:
		return ACT_WALK_AIM_RIFLE;
	case ACT_IDLE_ANGRY_SHOTGUN:
		return ACT_IDLE_ANGRY_SMG1;
	case ACT_RANGE_ATTACK_SHOTGUN_LOW:
		return ACT_RANGE_ATTACK_SMG1_LOW;
	case ACT_IDLE_MELEE:
	case ACT_IDLE_ANGRY_MELEE:  // If the walker has a melee weapon but is in an idle state, don't raise the weapon
		if (m_NPCState == NPC_STATE_IDLE)
			return ACT_IDLE_SUITCASE;
	default:
		return BaseClass::NPC_TranslateActivity(activity);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play sound when an enemy is spotted. This sound has a separate
//	timer from other sounds to prevent looping if the NPC gets caught
//	in a 'found enemy' condition.
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::FoundEnemySound(void)
{
	if (gpGlobals->curtime > m_flNextFoundEnemySoundTime)
	{
		m_flNextFoundEnemySoundTime = gpGlobals->curtime + random->RandomFloat(MIN_TIME_NEXT_FOUNDENEMY_SOUND, MAX_TIME_NEXT_FOUNDENEMY_SOUND);
		PlaySound(m_iszFoundEnemySound, true);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Play NPC soundscript
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::PlaySound(string_t soundname, bool required /*= false */)
{
	// TODO: Check if silent
	if (required || gpGlobals->curtime > m_flNextSoundTime)
	{
		m_flNextSoundTime = gpGlobals->curtime + random->RandomFloat(MIN_TIME_NEXT_SOUND, MAX_TIME_NEXT_SOUND);
		//CPASAttenuationFilter filter2(this, STRING(soundname));
		EmitSound(STRING(soundname));
	}
}

//-----------------------------------------------------------------------------
// Purpose: Assign a default soundscript if none is provided, then precache
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::PrecacheNPCSoundScript(string_t * SoundName, string_t defaultSoundName) 
{
	if (!SoundName) {
		*SoundName = defaultSoundName;
	}
	PrecacheScriptSound(STRING(*SoundName));
}

//-----------------------------------------------------------------------------
// Purpose: Get movement speed, multipled by modifier
//-----------------------------------------------------------------------------
float CNPC_ShadowWalker::GetSequenceGroundSpeed(CStudioHdr *pStudioHdr, int iSequence)
{
	float t = SequenceDuration(pStudioHdr, iSequence);

	if (t > 0)
	{
		return (GetSequenceMoveDist(pStudioHdr, iSequence) * m_flSpeedModifier / t);
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to change the speed of the NPC
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::InputSetSpeedModifier(inputdata_t &inputdata)
{
	this->m_flSpeedModifier = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable opening doors
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::InputEnableOpenDoors(inputdata_t &inputdata)
{
	m_bCannotOpenDoors = false;
	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesAdd(bits_CAP_DOORS_GROUP);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable opening doors
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::InputDisableOpenDoors(inputdata_t &inputdata)
{
	m_bCannotOpenDoors = true;
	if (!HasSpawnFlags(SF_NPC_START_EFFICIENT))
	{
		CapabilitiesRemove(bits_CAP_DOORS_GROUP);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable weapon pickup behavior
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::InputEnablePickupWeapons(inputdata_t &inputdata)
{
	m_bCanPickupWeapons = true;
}

//-----------------------------------------------------------------------------
// Purpose: Hammer input to enable weapon pickup behavior
//-----------------------------------------------------------------------------
void CNPC_ShadowWalker::InputDisablePickupWeapons(inputdata_t &inputdata)
{
	m_bCanPickupWeapons = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//
//
// Output : 
//-----------------------------------------------------------------------------
Class_T	CNPC_ShadowWalker::Classify( void )
{
	return	CLASS_ZOMBIE;
}
