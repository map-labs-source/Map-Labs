#include "cbase.h"
#include "lab_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS( CLabGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CLabGameRules, DT_LabGameRules )
#ifdef CLIENT_DLL
	RecvPropBool(RECVINFO(m_bMegaPhysgun)),
#else
	SendPropBool(SENDINFO(m_bMegaPhysgun)),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( lab_gamerules, CLabGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( LabGameRulesProxy, DT_LabGameRulesProxy )


#ifdef CLIENT_DLL
	void RecvProxy_LabGameRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CLabGameRules *pRules = LabGameRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CLabGameRulesProxy, DT_LabGameRulesProxy )
		RecvPropDataTable( "lab_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_LabGameRules ), RecvProxy_LabGameRules )
	END_RECV_TABLE()
#else
	void *SendProxy_LabGameRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CLabGameRules *pRules = LabGameRules();
		Assert( pRules );
		pRecipients->SetAllRecipients();
		return pRules;
	}

	BEGIN_SEND_TABLE( CLabGameRulesProxy, DT_LabGameRulesProxy )
		SendPropDataTable( "lab_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_LabGameRules ), SendProxy_LabGameRules )
	END_SEND_TABLE()
#endif

CLabGameRules::CLabGameRules()
{
}
	
CLabGameRules::~CLabGameRules()
{
}

#ifdef CLIENT_DLL

#else
void CLabGameRules::Think()
{
	BaseClass::Think();
}
#endif