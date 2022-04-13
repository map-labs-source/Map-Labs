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

#ifndef HL2_PLAYER_TALKER_H
#define HL2_PLAYER_TALKER_H
#pragma once

#include "hl2_player.h"
#include "ai_speech.h"
#include "ai_playerally.h"
#include "ai_sensorydummy.h"
#include "GameEventListener.h"

class CAI_PlayerNPCDummy;
class CHL2_TalkingPlayer;

// 
// Player-specific concepts
// 
#define TLK_LAST_ENEMY_DEAD "TLK_LAST_ENEMY_DEAD" // Last enemy was killed after a long engagement (separate from TLK_ENEMY_DEAD to bypass respeak delays)
#define TLK_WOUND_REMARK "TLK_WOUND_REMARK" // Do a long remark about taking a certain type of damage
#define TLK_THROWGRENADE "TLK_THROWGRENADE" // Grenade was thrown
#define TLK_ALLY_KILLED_NPC "TLK_ALLY_KILLED_NPC" // Ally killed a NPC
#define TLK_KILLED_ALLY "TLK_KILLED_ALLY" // Player killed an ally (intention ambiguous)
#define TLK_SCANNER_FLASH "TLK_SCANNER_FLASH" // Player was flashed by a scanner
#define TLK_COMMAND_SEND	"TLK_COMMAND_SEND" // 1upD - issuing order to send squad
#define TLK_COMMAND_RECALL	"TLK_COMMAND_RECALL" // 1upD - issuing order to recall squad
#define TLK_COMMAND_ADD		"TLK_COMMAND_ADD" // 1upD - issuing order to join squad
#define TLK_COMMAND_REMOVE	"TLK_COMMAND_REMOVE" // 1upD - issuing order to leave squad

//=============================================================================
// >> HL2_TALKINGPLAYERMEMORY
// A special component mostly meant to contain memory-related variables for player speech.
// CHL2_TalkingPlayer is farther below.
//=============================================================================
class CHL2_TalkingPlayerMemory
{
	DECLARE_SIMPLE_DATADESC();
public:

	void InitLastDamage(const CTakeDamageInfo &info);
	void RecordEngagementStart();
	void RecordEngagementEnd();

	// Are we in combat?
	bool			InEngagement() { return m_bInEngagement; }
	float			GetEngagementTime() { return gpGlobals->curtime - m_flEngagementStartTime; }
	int				GetPrevHealth() { return m_iPrevHealth; }

	int				GetHistoricEnemies() { return m_iNumEnemiesHistoric; }
	void			IncrementHistoricEnemies() { m_iNumEnemiesHistoric++; }

	void			KilledEnemy();

	int				GetLastDamageType() { return m_iLastDamageType; }
	int				GetLastDamageAmount() { return m_iLastDamageAmount; }
	CBaseEntity		*GetLastDamageAttacker() { return m_hLastDamageAttacker.Get(); }

	// Criteria sets
	void			AppendLastDamageCriteria( AI_CriteriaSet& set );
	void			AppendKilledEnemyCriteria( AI_CriteriaSet& set );

	CHL2_TalkingPlayer		*GetOuter() { return m_hOuter.Get(); }
	void					SetOuter( CHL2_TalkingPlayer *pOuter ) { m_hOuter.Set( pOuter ); }

private:

	// Conditions before combat engagement
	bool	m_bInEngagement;
	float	m_flEngagementStartTime;
	int		m_iPrevHealth;

	int		m_iNumEnemiesHistoric;

	// Kill combo
	int		m_iComboEnemies;
	float	m_flLastEnemyDeadTime;

	// Last damage stuff (for "revenge")
	int		m_iLastDamageType;
	int		m_iLastDamageAmount;
	EHANDLE	m_hLastDamageAttacker;

	CHandle<CHL2_TalkingPlayer> m_hOuter;
};

//=============================================================================
// >> HL2_TALKING_PLAYER
// 
// This class uses advanced criterion and memory tracking to support advanced responses to the player's actions.
// It also includes a real, invisible dummy NPC to keep track of enemies, AI sounds, etc.
// This class was created by Blixibon and 1upD.
// 
//=============================================================================
class CHL2_TalkingPlayer : public CAI_ExpresserHost<CHL2_Player>
{
	DECLARE_CLASS( CHL2_TalkingPlayer, CAI_ExpresserHost<CHL2_Player> );
public:

	CHL2_TalkingPlayer();

	void			Precache( void );
	void			Spawn( void );
	void			UpdateOnRemove( void );

	virtual void    ModifyOrAppendCriteria(AI_CriteriaSet& criteriaSet);
	virtual bool	IsAllowedToSpeak(AIConcept_t concept = NULL);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL);
	virtual bool    SpeakIfAllowed(AIConcept_t concept, AI_CriteriaSet& modifiers, char *pszOutResponseChosen = NULL, size_t bufsize = 0, IRecipientFilter *filter = NULL);
	bool			SelectSpeechResponse( AIConcept_t concept, AI_CriteriaSet *modifiers, CBaseEntity *pTarget, AISpeechSelection_t *pSelection );
	virtual void	PostConstructor(const char * szClassname);
	virtual CAI_Expresser * CreateExpresser(void);
	virtual CAI_Expresser * GetExpresser() { return m_pExpresser;  }

	bool			GetGameTextSpeechParams( hudtextparms_t &params );

	void			ModifyOrAppendDamageCriteria(AI_CriteriaSet & set, const CTakeDamageInfo & info, bool bPlayer = true);
	void			ModifyOrAppendEnemyCriteria(AI_CriteriaSet & set, CBaseEntity * pEnemy);
	void			ModifyOrAppendSquadCriteria(AI_CriteriaSet & set);
	void			ModifyOrAppendWeaponCriteria(AI_CriteriaSet & set, CBaseEntity * pWeapon = NULL);
	void			ModifyOrAppendSoundCriteria(AI_CriteriaSet & set, CSound *pSound, float flDist);

	void			ModifyOrAppendFinalEnemyCriteria(AI_CriteriaSet & set, CBaseEntity * pEnemy, const CTakeDamageInfo & info);
	void			ModifyOrAppendAICombatCriteria(AI_CriteriaSet & set);

	// "Speech target" is a thing from CAI_PlayerAlly mostly used for things like Q&A.
	// I'm using it here to refer to the player's allies in player dialogue. (shouldn't be used for enemies)
	void			ModifyOrAppendSpeechTargetCriteria(AI_CriteriaSet &set, CBaseEntity *pTarget);
	CBaseEntity		*FindSpeechTarget();
	void			SetSpeechTarget( CBaseEntity *pEntity ) { m_hSpeechTarget.Set( pEntity ); }
	CBaseEntity		*GetSpeechTarget() { return m_hSpeechTarget.Get(); }

	void			OnUseEntity( CBaseEntity *pEntity );
	bool			HandleInteraction( int interactionType, void *data, CBaseCombatCharacter* sourceEnt );

	Disposition_t	IRelationType( CBaseEntity *pTarget );

	// For more accurate representations of whether the player actually sees something
	// (3D dot calculations instead of 2D dot calculations)
	bool			FInTrueViewCone( const Vector &vecSpot );

	virtual int		OnTakeDamage_Alive(const CTakeDamageInfo &info);
	virtual int		TakeHealth( float flHealth, int bitsDamageType );
	void			TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr, CDmgAccumulator *pAccumulator );
	void			Event_KilledOther(CBaseEntity * pVictim, const CTakeDamageInfo & info);
	void			Event_KilledEnemy(CBaseCombatCharacter * pVictim, const CTakeDamageInfo & info);
	void			Event_Killed( const CTakeDamageInfo &info );
	bool			CommanderExecuteOne(CAI_BaseNPC *pNpc, const commandgoal_t &goal, CAI_BaseNPC **Allies, int numAllies);

	void			Event_NPCKilled( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );
	void			Event_NPCIgnited( CAI_BaseNPC *pVictim );
	void			AllyDied( CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );
	void			AllyKilledEnemy( CBaseEntity *pAlly, CAI_BaseNPC *pVictim, const CTakeDamageInfo &info );

	void			Event_SeeEnemy( CBaseEntity *pEnemy );
	bool			HandleAddToPlayerSquad( CAI_BaseNPC *pNPC );
	bool			HandleRemoveFromPlayerSquad( CAI_BaseNPC *pNPC );

	void			Event_FirstDrawWeapon( CBaseCombatWeapon *pWeapon );
	void			Event_ThrewGrenade( CBaseCombatWeapon *pWeapon );

	// Blixibon - StartScripting for gag replacement
	bool				IsInAScript( void ) { return m_bInAScript; }
	inline void			SetInAScript( bool bScript ) { m_bInAScript = bScript; }
	void				InputStartScripting( inputdata_t &inputdata );
	void				InputStopScripting( inputdata_t &inputdata );

	// Blixibon - Speech thinking implementation
	void				DoSpeechAI();
	bool				DoIdleSpeech();
	bool				DoCombatSpeech();

	void				MeasureEnemies(int &iVisibleEnemies, int &iCloseEnemies);

	bool				ReactToSound( CSound *pSound, float flDist );

	CBaseEntity*		GetStaringEntity() { return m_hStaringEntity.Get(); }
	void				SetStaringEntity(CBaseEntity *pEntity) { return m_hStaringEntity.Set(pEntity); }

	void				SetSpeechFilter( CAI_SpeechFilter *pFilter )	{ m_hSpeechFilter = pFilter; }
	CAI_SpeechFilter	*GetSpeechFilter( void )						{ return m_hSpeechFilter; }

	CAI_PlayerNPCDummy	*GetNPCComponent() { return m_hNPCComponent.Get(); }
	void				CreateNPCComponent();
	void				RemoveNPCComponent();

	CHL2_TalkingPlayerMemory	*GetMemoryComponent() { return &m_MemoryComponent; }

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();
	DECLARE_ENT_SCRIPTDESC();

public:

	// VScript functions
	HSCRIPT				ScriptGetNPCComponent();
	HSCRIPT				ScriptGetStaringEntity();

	HSCRIPT				VScriptGetEnemy() { return ToHScript( GetEnemy() ); }
	int					GetVisibleEnemies() { return m_iVisibleEnemies; }
	int					GetCloseEnemies() { return m_iCloseEnemies; }

public:

	// Expresser shortcuts
	bool IsSpeaking()				{ return GetExpresser()->IsSpeaking(); }

	// NPC component shortcuts
	CBaseEntity*		GetEnemy();
	NPC_STATE			GetState();

protected:
	virtual	void	PostThink(void);

	// A lot of combat-related concepts modify categorized criteria directly for specific subjects.
	// 
	// Modifiers will always overwrite automatic criteria, so we don't have to worry about this overwriting the desired "enemy",
	// but we do have to worry about doing a bunch of useless measurements in general criteria that are just gonna be overwritten.
	// 
	// As a result, each relevant category marks itself as "used" until the next time the original ModifyOrAppendCriteria() is called.
	enum PlayerCriteria_t
	{
		PLAYERCRIT_DAMAGE,
		PLAYERCRIT_ENEMY,
		PLAYERCRIT_SQUAD,
		PLAYERCRIT_WEAPON,
		PLAYERCRIT_SPEECHTARGET,
		PLAYERCRIT_SOUND,
	};

	inline void			MarkCriteria(PlayerCriteria_t crit) { m_iCriteriaAppended |= (1 << crit); }
	inline bool			IsCriteriaModified(PlayerCriteria_t crit) { return (m_iCriteriaAppended & (1 << crit)) != 0; }
	inline void			ResetPlayerCriteria() { m_iCriteriaAppended = 0; }

private:
	CAI_Expresser * m_pExpresser;

	bool			m_bInAScript;

	EHANDLE			m_hStaringEntity;
	float			m_flCurrentStaringTime;
	QAngle			m_angLastStaringEyeAngles;

	// For speech purposes
	Vector			m_vecLastCommandGoal;

	CHandle<CAI_PlayerNPCDummy> m_hNPCComponent;
	float			m_flNextSpeechTime;
	CHandle<CAI_SpeechFilter>	m_hSpeechFilter;

	CHL2_TalkingPlayerMemory	m_MemoryComponent;

	EHANDLE			m_hSpeechTarget;

	int				m_iVisibleEnemies;
	int				m_iCloseEnemies;

	// See PlayerCriteria_t
	int				m_iCriteriaAppended;
};

//-----------------------------------------------------------------------------
// Purpose: Sensory dummy for player component
// 
// NPC created by Blixibon.
//-----------------------------------------------------------------------------
class CAI_PlayerNPCDummy : public CAI_SensingDummy<CAI_BaseNPC>
{
	DECLARE_CLASS(CAI_PlayerNPCDummy, CAI_SensingDummy<CAI_BaseNPC>);
	DECLARE_DATADESC();
public:

	// Don't waste CPU doing sensing code.
	// We now need hearing for state changes and stuff, but sight isn't necessary at the moment.
	//int		GetSensingFlags( void ) { return SENSING_FLAGS_DONT_LOOK /*| SENSING_FLAGS_DONT_LISTEN*/; }

	void	Spawn();

	void	ModifyOrAppendOuterCriteria( AI_CriteriaSet &set );
	void	ModifyOrAppendOuterEnemyCriteria( AI_CriteriaSet &set, CAI_BaseNPC *pNPC );

	void	RunAI( void );
	void	GatherEnemyConditions( CBaseEntity *pEnemy );
	int 	TranslateSchedule( int scheduleType );
	void 	OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	// Base class's sound interests include combat and danger, add relevant scents onto it
	int		GetSoundInterests( void ) { return BaseClass::GetSoundInterests() | SOUND_PHYSICS_DANGER | SOUND_CARCASS | SOUND_MEAT; }
	bool	QueryHearSound( CSound *pSound );
	bool	QuerySeeEntity( CBaseEntity *pEntity, bool bOnlyHateOrFearIfNPC = false );

	bool	UpdateEnemyMemory( CBaseEntity *pEnemy, const Vector &position, CBaseEntity *pInformer = NULL );

	void	DrawDebugGeometryOverlays( void );

	// Special handling for info_remarkable
	void	OnSeeEntity( CBaseEntity *pEntity );

	bool	IsValidEnemy( CBaseEntity *pEnemy );

	//---------------------------------------------------------------------------------------------
	// Override a bunch of stuff to redirect to our outer.
	//---------------------------------------------------------------------------------------------
	Vector				EyePosition() { return GetOuter()->EyePosition(); }
	const QAngle		&EyeAngles() { return GetOuter()->EyeAngles(); }
	void				EyePositionAndVectors( Vector *pPosition, Vector *pForward, Vector *pRight, Vector *pUp ) { GetOuter()->EyePositionAndVectors(pPosition, pForward, pRight, pUp); }
	const QAngle		&LocalEyeAngles() { return GetOuter()->LocalEyeAngles(); }
	void				EyeVectors( Vector *pForward, Vector *pRight = NULL, Vector *pUp = NULL ) { GetOuter()->EyeVectors(pForward, pRight, pUp); }

	Vector				HeadDirection2D( void ) { return GetOuter()->HeadDirection2D(); }
	Vector				HeadDirection3D( void ) { return GetOuter()->HeadDirection3D(); }
	Vector				EyeDirection2D( void ) { return GetOuter()->EyeDirection2D(); }
	Vector				EyeDirection3D( void ) { return GetOuter()->EyeDirection3D(); }

	Disposition_t		IRelationType( CBaseEntity *pTarget )		{ return GetOuter()->IRelationType(pTarget); }
	int					IRelationPriority( CBaseEntity *pTarget );	//{ return GetOuter()->IRelationPriority(pTarget); }

	// NPCs seem to be able to see the player inappropriately with these overrides to FVisible()
	//bool				FVisible ( CBaseEntity *pEntity, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL ) { return GetOuter()->FVisible(pEntity, traceMask, ppBlocker); }
	//bool				FVisible( const Vector &vecTarget, int traceMask = MASK_BLOCKLOS, CBaseEntity **ppBlocker = NULL )	{ return GetOuter()->FVisible( vecTarget, traceMask, ppBlocker ); }

	bool				FInViewCone( CBaseEntity *pEntity ) { return GetOuter()->FInViewCone(pEntity); }
	bool				FInViewCone( const Vector &vecSpot ) { return GetOuter()->FInViewCone(vecSpot); }

	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	bool		IsPlayerAlly(CBasePlayer *pPlayer = NULL) { return false; }
	bool		IsSilentSquadMember() const 	{ return true; }

	// The player's dummy can only sense, it isn't a real enemy
	bool		CanBeAnEnemyOf( CBaseEntity *pEnemy ) { return false; }
	bool		CanBeSeenBy( CAI_BaseNPC *pNPC ) { return false; }

	Class_T	Classify( void ) { return CLASS_NONE; }

	CHL2_TalkingPlayer		*GetOuter() { return m_hOuter.Get(); }
	void					SetOuter( CHL2_TalkingPlayer *pOuter) { m_hOuter.Set(pOuter); }

protected:
	//-----------------------------------------------------
	// Conditions, Schedules, Tasks
	//-----------------------------------------------------
	enum
	{
		//COND_EXAMPLE = BaseClass::NEXT_CONDITION,
		//NEXT_CONDITION,

		SCHED_PLAYERDUMMY_ALERT_STAND = BaseClass::NEXT_SCHEDULE,
		NEXT_SCHEDULE,

		//TASK_EXAMPLE = BaseClass::NEXT_TASK,
		//NEXT_TASK,

		//AE_EXAMPLE = LAST_SHARED_ANIMEVENT

	};

	CHandle<CHL2_TalkingPlayer> m_hOuter;

	DEFINE_CUSTOM_AI;
};

#endif	//HL2_PLAYER_TALKER_H
