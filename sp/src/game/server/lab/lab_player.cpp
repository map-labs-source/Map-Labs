#include "cbase.h"
#include "lab_player.h"

LINK_ENTITY_TO_CLASS(player, CLabPlayer);
PRECACHE_REGISTER(player);

BEGIN_DATADESC( CLabPlayer )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CLabPlayer, DT_LabPlayer )
END_SEND_TABLE()

CLabPlayer::CLabPlayer()
{
}


CLabPlayer::~CLabPlayer()
{
}


CLabPlayer *CLabPlayer::CreatePlayer( const char *className, edict_t *ed )
{
	CLabPlayer::s_PlayerEdict = ed;
	return (CLabPlayer*)CreateEntityByName( className );
}

void CLabPlayer::PreThink(void)
{
	BaseClass::PreThink();
}


void CLabPlayer::PostThink()
{
	BaseClass::PostThink();
}


void CLabPlayer::Precache()
{
	BaseClass::Precache();
}

void CLabPlayer::CreateHandModel(int viewmodelindex, int iOtherVm)
{
	Assert(viewmodelindex >= 0 && viewmodelindex < MAX_VIEWMODELS && iOtherVm >= 0 && iOtherVm < MAX_VIEWMODELS);

	if (GetViewModel(viewmodelindex))
		return;

	CBaseViewModel *vm = (CBaseViewModel *)CreateEntityByName("hand_viewmodel");
	if (vm)
	{
		vm->SetAbsOrigin(GetAbsOrigin());
		vm->SetOwner(this);
		vm->SetIndex(viewmodelindex);
		DispatchSpawn(vm);
		vm->FollowEntity(GetViewModel(iOtherVm), true);
		m_hViewModel.Set(viewmodelindex, vm);
	}
}

void CLabPlayer::Spawn()
{
	BaseClass::Spawn();
	GetLabPlayerProxy();
	CreateHandModel();
}

class CLogicLabPlayerProxy : public CLogicalEntity
{
	DECLARE_CLASS(CLogicLabPlayerProxy, CLogicalEntity);
private:
	DECLARE_DATADESC();
public:
	void InputSetHandModel(inputdata_t &inputdata);
	EHANDLE m_hPlayer;
};

LINK_ENTITY_TO_CLASS( logic_labplayerproxy, CLogicLabPlayerProxy );

BEGIN_DATADESC( CLogicLabPlayerProxy )
	DEFINE_INPUTFUNC( FIELD_STRING,	"SetHandModel", InputSetHandModel ),
	DEFINE_FIELD( m_hPlayer, FIELD_EHANDLE ),
END_DATADESC()

void CLogicLabPlayerProxy::InputSetHandModel(inputdata_t &inputdata)
{
	if (!m_hPlayer)
		return;

	string_t iszModel = MAKE_STRING(inputdata.value.String());

	if (iszModel != NULL_STRING)
	{
		CLabPlayer *pPlayer = ToLabPlayer(m_hPlayer.Get());
		CBaseViewModel *vm = pPlayer->GetViewModel(1);
		if(vm)
			vm->SetModel(iszModel.ToCStr());
	}
}

CLogicLabPlayerProxy *CLabPlayer::GetLabPlayerProxy()
{
	CLogicLabPlayerProxy *pProxy = dynamic_cast<CLogicLabPlayerProxy*> (m_hLabPlayerProxy.Get());

	if (!pProxy)
	{
		pProxy = (CLogicLabPlayerProxy*)gEntList.FindEntityByClassname(NULL, "logic_labplayerproxy");

		if (!pProxy)
			return NULL;

		pProxy->m_hPlayer = this;
		m_hLabPlayerProxy = pProxy;
	}

	return pProxy;
}
