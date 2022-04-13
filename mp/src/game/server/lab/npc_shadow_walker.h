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
#include "ai_default.h"
#include "ai_task.h"
#include "ai_schedule.h"
#include "entitylist.h"
#include "activitylist.h"
#include "ai_basenpc.h"
#include "ai_blended_movement.h"
#include "ai_behavior_actbusy.h"

//=========================================================
// schedules
//=========================================================
enum
{
	SCHED_MELEE_ATTACK_NOINTERRUPT = LAST_SHARED_SCHEDULE,
	SCHED_HIDE,

	LAST_SHADOW_WALKER_SCHED
};

//=========================================================
//=========================================================
typedef CAI_BlendingHost< CAI_BehaviorHost<CAI_BaseNPC> > CAI_CustomNPCBase;

class CNPC_ShadowWalker : public CAI_CustomNPCBase
{
	DECLARE_CLASS(CNPC_ShadowWalker, CAI_CustomNPCBase);

public:
	void				Precache(void);
	void				Spawn(void);
	Class_T				Classify(void);
	virtual int			SelectFailSchedule(int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode);
	virtual int 		SelectScheduleRetrieveItem();
	virtual int 		SelectScheduleWander();
	virtual int 		SelectSchedule();
	virtual int			SelectIdleSchedule();
	virtual int			SelectAlertSchedule();
	virtual int			SelectCombatSchedule();
	virtual float		GetSequenceGroundSpeed(CStudioHdr *pStudioHdr, int iSequence);
	virtual Activity	NPC_TranslateActivity(Activity eNewActivity);
	virtual int			TranslateSchedule(int scheduleType);

	// Sounds
	virtual void		PlaySound(string_t soundname, bool optional);
	virtual void		DeathSound(const CTakeDamageInfo &info) { PlaySound(m_iszDeathSound, true); }
	virtual void		AlertSound(void) { PlaySound(m_iszAlertSound, false); };
	virtual void		IdleSound(void) { PlaySound(m_iszIdleSound, false); };
	virtual void		PainSound(const CTakeDamageInfo &info) { PlaySound(m_iszPainSound, true); };
	virtual void		FearSound(void) { PlaySound(m_iszFearSound, false); };
	virtual void		LostEnemySound(void) { PlaySound(m_iszLostEnemySound, false); };
	virtual void		FoundEnemySound(void);

	void				Activate();
	void				FixupWeapon();

	// Inputs
	virtual void		InputSetSpeedModifier(inputdata_t &inputdata);
	virtual void		InputEnableOpenDoors(inputdata_t &inputdata);
	virtual void		InputDisableOpenDoors(inputdata_t &inputdata);
	virtual void		InputEnablePickupWeapons(inputdata_t &inputdata);
	virtual void		InputDisablePickupWeapons(inputdata_t &inputdata);

	DECLARE_DATADESC();

	string_t			m_iszWeaponModelName;			// Path/filename of model to override weapon model.

	string_t			m_iszFearSound;			// Path/filename of WAV file to play.
	string_t			m_iszDeathSound;			// Path/filename of WAV file to play.
	string_t			m_iszIdleSound;			// Path/filename of WAV file to play.
	string_t			m_iszPainSound;			// Path/filename of WAV file to play.
	string_t			m_iszAlertSound;			// Path/filename of WAV file to play.
	string_t			m_iszLostEnemySound;		// Path/filename of WAV file to play.
	string_t			m_iszFoundEnemySound;		// Path/filename of WAV file to play.

	DEFINE_CUSTOM_AI;

private:
	bool				HasRangedWeapon();
	void				PrecacheNPCSoundScript(string_t* SoundName, string_t defaultSoundName);


	bool				m_bUseBothSquadSlots;	// If true use two squad slots, if false use one squad slot
	bool				m_bCannotOpenDoors;		// If true, this NPC cannot open doors. The condition is reversed because originally it could.
	bool				m_bCanPickupWeapons;			// If true, this NPC is able to pick up weapons off of the ground just like npc_citizen.
	bool				m_bWanderToggle;		// Boolean to toggle wandering / standing every think cycle
	float				m_flNextSoundTime;		// Next time at which this NPC is allowed to play an NPC sound
	float				m_flNextFoundEnemySoundTime;	// Next time at which this NPC is allowed to play an NPC sound
	float				m_flSpeedModifier;		// Modifier to apply to move distance
};


LINK_ENTITY_TO_CLASS(npc_shadow_walker, CNPC_ShadowWalker);

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CNPC_ShadowWalker)
DEFINE_KEYFIELD(m_iszWeaponModelName, FIELD_STRING, "WeaponModel"),
DEFINE_KEYFIELD(m_iHealth, FIELD_INTEGER, "Health"),
DEFINE_KEYFIELD(m_iszFearSound, FIELD_SOUNDNAME, "FearSound"),
DEFINE_KEYFIELD(m_iszDeathSound, FIELD_SOUNDNAME, "DeathSound"),
DEFINE_KEYFIELD(m_iszIdleSound, FIELD_SOUNDNAME, "IdleSound"),
DEFINE_KEYFIELD(m_iszPainSound, FIELD_SOUNDNAME, "PainSound"),
DEFINE_KEYFIELD(m_iszAlertSound, FIELD_SOUNDNAME, "AlertSound"),
DEFINE_KEYFIELD(m_iszLostEnemySound, FIELD_SOUNDNAME, "LostEnemySound"),
DEFINE_KEYFIELD(m_iszFoundEnemySound, FIELD_SOUNDNAME, "FoundEnemySound"),
DEFINE_KEYFIELD(m_bUseBothSquadSlots, FIELD_BOOLEAN, "UseBothSquadSlots"),
DEFINE_KEYFIELD(m_bCannotOpenDoors, FIELD_BOOLEAN, "CannotOpenDoors"),
DEFINE_KEYFIELD(m_bCanPickupWeapons, FIELD_BOOLEAN, "CanPickupWeapons"),

DEFINE_FIELD(m_bWanderToggle, FIELD_BOOLEAN),
DEFINE_FIELD(m_flNextSoundTime, FIELD_TIME),
DEFINE_FIELD(m_flNextFoundEnemySoundTime, FIELD_TIME),
DEFINE_FIELD(m_flSpeedModifier, FIELD_TIME),

DEFINE_INPUTFUNC(FIELD_FLOAT, "SetSpeedModifier", InputSetSpeedModifier),
DEFINE_INPUTFUNC(FIELD_VOID, "EnableOpenDoors", InputEnableOpenDoors),
DEFINE_INPUTFUNC(FIELD_VOID, "DisableOpenDoors", InputDisableOpenDoors),
DEFINE_INPUTFUNC(FIELD_VOID, "EnablePickupWeapons", InputEnablePickupWeapons),
DEFINE_INPUTFUNC(FIELD_VOID, "DisablePickupWeapons", InputDisablePickupWeapons)
END_DATADESC()


AI_BEGIN_CUSTOM_NPC(npc_shadow_walker, CNPC_ShadowWalker)
//=========================================================
// > Melee_Attack_NoInterrupt
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_MELEE_ATTACK_NOINTERRUPT,

	"	Tasks"
	"		TASK_STOP_MOVING		0"
	"		TASK_FACE_ENEMY			0"
	"		TASK_ANNOUNCE_ATTACK	1"	// 1 = primary attack
	"		TASK_MELEE_ATTACK1		0"
	""
	"	Interrupts"
	"		COND_ENEMY_DEAD"
	"		COND_ENEMY_OCCLUDED"
);

//=========================================================
// 	SCHED_HIDE
//=========================================================
DEFINE_SCHEDULE
(
	SCHED_HIDE,

	"	Tasks"
	"		TASK_SET_FAIL_SCHEDULE		SCHEDULE:SCHED_COMBAT_FACE"
	"		TASK_STOP_MOVING			0"
	"		TASK_FIND_COVER_FROM_ENEMY	0"
	"		TASK_RUN_PATH				0"
	"		TASK_WAIT_FOR_MOVEMENT		0"
	"		TASK_REMEMBER				MEMORY:INCOVER"
	"		TASK_FACE_ENEMY				0"
	""
	"	Interrupts"
	"		COND_HEAR_DANGER"
	"		COND_NEW_ENEMY"
	"		COND_ENEMY_DEAD"
);
AI_END_CUSTOM_NPC()

