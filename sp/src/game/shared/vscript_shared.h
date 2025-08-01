//========== Copyright � 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef VSCRIPT_SHARED_H
#define VSCRIPT_SHARED_H

#include "vscript/ivscript.h"

#if defined( _WIN32 )
#pragma once
#endif

extern IScriptVM * g_pScriptVM;

HSCRIPT VScriptCompileScript( const char *pszScriptName, bool bWarnMissing = false );
bool VScriptRunScript( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing = false );
inline bool VScriptRunScript( const char *pszScriptName, bool bWarnMissing = false ) { return VScriptRunScript( pszScriptName, NULL, bWarnMissing ); }

#define DECLARE_ENT_SCRIPTDESC()													ALLOW_SCRIPT_ACCESS(); virtual ScriptClassDesc_t *GetScriptDesc()

#define BEGIN_ENT_SCRIPTDESC( className, baseClass, description )										_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC( className, baseClass, description )
#define BEGIN_ENT_SCRIPTDESC_WITH_HELPER( className, baseClass, description, helper )					_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_WITH_HELPER( className, baseClass, description, helper )
#define BEGIN_ENT_SCRIPTDESC_ROOT( className, description )												_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_ROOT( className, description )
#define BEGIN_ENT_SCRIPTDESC_ROOT_WITH_HELPER( className, description, helper )							_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_ROOT_WITH_HELPER( className, description, helper )
#define BEGIN_ENT_SCRIPTDESC_NAMED( className, baseClass, scriptName, description )						_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_NAMED( className, baseClass, scriptName, description )
#define BEGIN_ENT_SCRIPTDESC_NAMED_WITH_HELPER( className, baseClass, scriptName, description, helper )	_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_NAMED_WITH_HELPER( className, baseClass, scriptName, description, helper )
#define BEGIN_ENT_SCRIPTDESC_ROOT_NAMED( className, scriptName, description )							_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_ROOT_NAMED( className, scriptName, description )
#define BEGIN_ENT_SCRIPTDESC_ROOT_NAMED_WITH_HELPER( className, scriptName, description, helper )		_IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className ); BEGIN_SCRIPTDESC_ROOT_NAMED_WITH_HELPER( className, scriptName, description, helper )

#define _IMPLEMENT_ENT_SCRIPTDESC_ACCESSOR( className )					template <> ScriptClassDesc_t * GetScriptDesc<className>( className *, bool ); ScriptClassDesc_t *className::GetScriptDesc()  { return ::GetScriptDesc( this ); }		

// Only allow scripts to create entities during map initialization
bool IsEntityCreationAllowedInScripts( void );

class ISaveRestoreBlockHandler;
ISaveRestoreBlockHandler *GetVScriptSaveRestoreBlockHandler();

class CBaseEntityScriptInstanceHelper : public IScriptInstanceHelper
{
	bool ToString( void *p, char *pBuf, int bufSize );
	void *BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId );
};

extern CBaseEntityScriptInstanceHelper g_BaseEntityScriptInstanceHelper;

#ifdef MAPBASE_VSCRIPT
#ifdef GAME_DLL
const int vscript_debugger_port = 1212;
#else
const int vscript_debugger_port = 1213;
#endif

void RegisterSharedScriptConstants();
void RegisterSharedScriptFunctions();

void RunAddonScripts();
#endif

#endif // VSCRIPT_SHARED_H
