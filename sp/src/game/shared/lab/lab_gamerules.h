#ifndef LAB_GAMERULES_H
#define LAB_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "hl2_gamerules.h"

#ifdef CLIENT_DLL
	#define CLabGameRules C_LabGameRules
	#define CLabGameRulesProxy C_LabGameRulesProxy
#endif

class CLabGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CLabGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class CLabGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CLabGameRules, CHalfLife2 );
	DECLARE_NETWORKCLASS_NOBASE();

	CLabGameRules();
	virtual ~CLabGameRules();

#ifdef CLIENT_DLL

#else
	virtual void Think();
#endif
};

inline CLabGameRules* LabGameRules()
{
	return static_cast<CLabGameRules*>(g_pGameRules);
}

#endif // LAB_GAMERULES_H
