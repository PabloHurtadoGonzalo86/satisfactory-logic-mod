#include "SlgSpikeModules.h"

#include "SlgSpikeChatCommand.h"
#include "SlgSpikeLog.h"
#include "SlgSpikeRemoteCall.h"
#include "SlgSpikeSubsystem.h"
#include "Engine/World.h"

USlgSpikeWorldModule::USlgSpikeWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(ASlgSpikeSubsystem::StaticClass());
	mChatCommands.Add(ASlgSpikeChatCommand::StaticClass());
}

void USlgSpikeWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	// The base class registers the subsystem in CONSTRUCTION and the chat command in INITIALIZATION.
	Super::DispatchLifecycleEvent(Phase);

	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S1 event=ModuleLifecycle module=world phase=%s mod_reference=%s netmode=%s"),
		*UModModule::LifecyclePhaseToString(Phase), *GetOwnerModReference().ToString(),
		*SlgSpikeUtil::NetModeToString(GetWorld()));
}

USlgSpikeInstanceModule::USlgSpikeInstanceModule()
{
	bRootModule = true;
	RemoteCallObjects.Add(USlgSpikeRemoteCallObject::StaticClass());
}

void USlgSpikeInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	// The base class registers the remote call object in INITIALIZATION.
	Super::DispatchLifecycleEvent(Phase);

	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S1 event=ModuleLifecycle module=game_instance phase=%s mod_reference=%s netmode=%s"),
		*UModModule::LifecyclePhaseToString(Phase), *GetOwnerModReference().ToString(),
		*SlgSpikeUtil::NetModeToString(GetWorld()));
}
