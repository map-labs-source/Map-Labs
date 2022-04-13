//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Map Labs ConVars
//
//=============================================================================

#include "cbase.h"
#include "convar.h"
#include "steam/steam_api.h"

void OpenAchievements_f()
{
	if (steamapicontext && steamapicontext->SteamFriends())
		steamapicontext->SteamFriends()->ActivateGameOverlay("Achievements");
	else
		DevMsg("Missing Steam API context, failed to execute: maplab_openachievements\n");
}
ConCommand maplab_openachievements("maplab_openachievements", OpenAchievements_f, "Open Map labs Achievements in Steam Overlay.", 0);


void OpenInvite_f()
{
	if (steamapicontext && steamapicontext->SteamFriends())
		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage("https://discord.gg/GHCj4P5");
	IGameEvent *event = gameeventmanager->CreateEvent("joined_ml");
	if (event)
		gameeventmanager->FireEvent(event);
	
}
ConCommand maplab_openinvite("maplab_openinvite", OpenInvite_f, "Open the Map Labs Discord invite link.", 0);

//reset all achievements & stats for testing purposes
void ResetAchievements_f()
{
	if (steamapicontext && steamapicontext->SteamUserStats())
		steamapicontext->SteamUserStats()->ResetAllStats( true );
	else
		DevMsg("Missing Steam API context, failed to execute: maplab_resetachievement\n");
}

ConCommand maplab_resetachievement("maplab_resetachievement", ResetAchievements_f, "Reset all achievements for testing purposes.", 0);