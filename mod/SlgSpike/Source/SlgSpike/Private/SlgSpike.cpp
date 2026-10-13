#include "SlgSpike.h"

#include "SlgSpikeLog.h"
#include "SlgSpikeRemoteCall.h"
#include "FGPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "UnrealEngine.h"

DEFINE_LOG_CATEGORY(LogSlgSpike);

double SlgSpikeUtil::WallSeconds()
{
	// Wall clock, not a monotonic one: a clock step shows up as a negative gap_ms and makes that summary window
	// invalid. Its resolution on the owner's platform is not documented in the SDK; the smallest non-zero gap_ms in
	// a log shows the real step. Frame and sub-step questions use the world time and the sub-step counters instead.
	static const FDateTime Start = FDateTime::UtcNow();
	return (FDateTime::UtcNow() - Start).GetTotalSeconds();
}

FString SlgSpikeUtil::NetModeToString(const UWorld* World)
{
	if (World == nullptr)
	{
		return TEXT("no_world");
	}
	const ENetMode Mode = World->GetNetMode();
	if (Mode == ENetMode::NM_DedicatedServer)
	{
		return TEXT("dedicated");
	}
	if (Mode == ENetMode::NM_Client)
	{
		return TEXT("client");
	}
	return FString::Printf(TEXT("other(%d)"), static_cast<int32>(Mode));
}

int32 SlgSpikeUtil::CountPlayers(UWorld* World)
{
	if (World == nullptr)
	{
		return 0;
	}
	int32 Count = 0;
	for (TPlayerControllerIterator<AFGPlayerController>::ServerAll It(World); It; ++It)
	{
		++Count;
	}
	return Count;
}

void FSlgSpikeModule::StartupModule()
{
	// S1 evidence: this line proves the module was loaded in this process. SML's own "Version check passed
	// successfully! Game Changelist: N" and "Discovered N world modules" lines are the second source for the CL.
	const uint32 Changelist = FEngineVersion::Current().GetChangelist();
	const bool bDedicated = IsRunningDedicatedServer();

#if UE_BUILD_SHIPPING
	const TCHAR* ConfigName = TEXT("shipping");
#else
	const TCHAR* ConfigName = TEXT("non_shipping");
#endif

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S1 event=StartupModule plugin_build=%s changelist=%u dedicated_server=%d config=%s"),
		SlgSpikeUtil::BuildTag(), Changelist, bDedicated ? 1 : 0, ConfigName);

	// A dedicated server has no local player, so the client chat hook is never attached there.
	if (!bDedicated)
	{
		ControllerBegunPlayHandle = AFGPlayerController::PlayerControllerBegunPlay.AddLambda([](AFGPlayerController* Controller)
		{
			SlgSpikeUtil::AttachClientChatHook(Controller);
		});
	}
}

void FSlgSpikeModule::ShutdownModule()
{
	if (ControllerBegunPlayHandle.IsValid())
	{
		AFGPlayerController::PlayerControllerBegunPlay.Remove(ControllerBegunPlayHandle);
		ControllerBegunPlayHandle.Reset();
	}
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S1 event=ShutdownModule"));
}

IMPLEMENT_MODULE(FSlgSpikeModule, SlgSpike)
