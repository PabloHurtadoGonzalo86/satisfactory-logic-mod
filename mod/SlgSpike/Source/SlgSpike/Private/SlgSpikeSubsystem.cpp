#include "SlgSpikeSubsystem.h"

#include "SlgSpikeLog.h"
#include "SlgSpikeTickHandler.h"
#include "FGBuildableSubsystem.h"
#include "Engine/World.h"
#include "Subsystem/SubsystemActorManager.h"
#include "TimerManager.h"

namespace
{
	/** About 10 s at 60 frames per second. After that the S3 check is marked as failed in the log. */
	constexpr int32 MaxRegisterAttempts = 600;

	/** Per-call logging for the first calls after registration, so a log exists even if nobody types a command. */
	constexpr int32 AutoBurstCalls = 120;

	constexpr double HeartbeatPeriodSeconds = 1.0;

	/** How long a player must be connected, and the handler registered, before a silent handler is reported. */
	constexpr double SilenceWindowSeconds = 5.0;
	constexpr double SilenceCheckPeriodSeconds = 0.5;
}

ASlgSpikeSubsystem::ASlgSpikeSubsystem()
{
	// SML decides on the server whether to spawn the actor; a remote client never gets one. Written out so the
	// intent survives a change of the enum's default.
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer;

	// The heartbeat must keep running while the world is paused, to show what Auto Pause does to the other clocks.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;

	SpikeSaveVersion = 1;
	SpikeId = 0;
	BeginPlayCount = 0;
	LoadCount = 0;
	SaveCount = 0;
}

ASlgSpikeSubsystem* ASlgSpikeSubsystem::Get(UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	USubsystemActorManager* Manager = World->GetSubsystem<USubsystemActorManager>();
	return Manager != nullptr ? Manager->GetSubsystemActor<ASlgSpikeSubsystem>() : nullptr;
}

USlgSpikeTickHandler* ASlgSpikeSubsystem::GetTickHandler() const
{
	return TickHandler;
}

// ---------------------------------------------------------------------------------------------------------------------
// S2: save and load
// ---------------------------------------------------------------------------------------------------------------------

void ASlgSpikeSubsystem::LogSaveEvent(const TCHAR* EventName, int32 SaveVersion, int32 GameVersion)
{
	++EventOrdinal;
	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S2 event=%s ord=%d name=%s class=%s loaded_from_save=%d spike_id=%d spike_save_version=%d begin_play_count=%d load_count=%d save_count=%d game_save_version=%d game_version=%d netmode=%s authority=%d"),
		EventName, EventOrdinal, *GetName(), *GetClass()->GetName(), bLoadedFromSave ? 1 : 0, SpikeId, SpikeSaveVersion,
		BeginPlayCount, LoadCount, SaveCount, SaveVersion, GameVersion, *SlgSpikeUtil::NetModeToString(GetWorld()),
		HasAuthority() ? 1 : 0);
}

void ASlgSpikeSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	++SaveCount;
	LogSaveEvent(TEXT("PreSaveGame"), saveVersion, gameVersion);
}

void ASlgSpikeSubsystem::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	LogSaveEvent(TEXT("PostSaveGame"), saveVersion, gameVersion);
}

void ASlgSpikeSubsystem::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	LogSaveEvent(TEXT("PreLoadGame"), saveVersion, gameVersion);
}

void ASlgSpikeSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	++LoadCount;
	bLoadedFromSave = true;
	LogSaveEvent(TEXT("PostLoadGame"), saveVersion, gameVersion);
}

void ASlgSpikeSubsystem::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
	// Nothing to add. SAVE_EMPTY_IMPLEMENTATION is not used because its version of this function is check(false).
}

bool ASlgSpikeSubsystem::NeedTransform_Implementation()
{
	return false;
}

bool ASlgSpikeSubsystem::ShouldSave_Implementation() const
{
	return true;
}

void ASlgSpikeSubsystem::Init()
{
	Super::Init();
	LogSaveEvent(TEXT("Init"), 0, 0);
}

void ASlgSpikeSubsystem::BeginPlay()
{
	// AModSubsystem documents that BeginPlay and EndPlay overrides must call their Super.
	Super::BeginPlay();

	++BeginPlayCount;
	if (SpikeId == 0)
	{
		SpikeId = FMath::RandRange(1, 2000000000);
	}
	LogSaveEvent(TEXT("BeginPlay"), 0, 0);

	if (HasAuthority())
	{
		TryRegisterHandler();
	}
}

void ASlgSpikeSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=SubsystemEndPlay reason=%d handler_registered=%d"),
		static_cast<int32>(EndPlayReason), bHandlerRegistered ? 1 : 0);

	UnregisterHandler();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------------------------------
// S3: handler registration
// ---------------------------------------------------------------------------------------------------------------------

void ASlgSpikeSubsystem::TryRegisterHandler()
{
	if (bHandlerRegistered)
	{
		return;
	}

	UWorld* World = GetWorld();
	AFGBuildableSubsystem* BuildableSubsystem = World != nullptr ? AFGBuildableSubsystem::Get(World) : nullptr;

	if (BuildableSubsystem == nullptr)
	{
		++RegisterAttempts;
		if (RegisterAttempts == 1 || RegisterAttempts == MaxRegisterAttempts)
		{
			UE_LOG(LogSlgSpike, Warning,
				TEXT("[SLGSPIKE] S3 event=HandlerRegistration result=WAITING reason=no_buildable_subsystem attempts=%d"),
				RegisterAttempts);
		}

		if (World != nullptr && RegisterAttempts <= MaxRegisterAttempts)
		{
			World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ASlgSpikeSubsystem::TryRegisterHandler));
		}
		else
		{
			UE_LOG(LogSlgSpike, Error, TEXT("[SLGSPIKE] S3 event=HandlerRegistration result=FAIL reason=gave_up attempts=%d"),
				RegisterAttempts);
		}
		return;
	}

	// The world is the outer, as in the one public mod that registers a handler. An outer that is a saved actor, like
	// this subsystem, has no precedent, and the registration belongs to the world anyway.
	TickHandler = NewObject<USlgSpikeTickHandler>(World);
	USlgSpikeTickHandler* Handler = TickHandler;
	IFGFactoryTickHandlerInterface* HandlerInterface = Handler;
	BuildableSubsystem->AddFactoryTickHandler(HandlerInterface);
	RegisteredOn = TWeakObjectPtr<AFGBuildableSubsystem>(BuildableSubsystem);
	bHandlerRegistered = true;
	HandlerRegisteredWall = SlgSpikeUtil::WallSeconds();

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S3 event=HandlerRegistered result=OK attempts=%d buildable_subsystem=%s netmode=%s"),
		RegisterAttempts, *BuildableSubsystem->GetName(), *SlgSpikeUtil::NetModeToString(World));

	Handler->StartBurst(AutoBurstCalls);
}

void ASlgSpikeSubsystem::UnregisterHandler()
{
	AFGBuildableSubsystem* BuildableSubsystem = RegisteredOn.Get();
	USlgSpikeTickHandler* Handler = TickHandler;
	if (BuildableSubsystem != nullptr && Handler != nullptr)
	{
		IFGFactoryTickHandlerInterface* HandlerInterface = Handler;
		BuildableSubsystem->RemoveFactoryTickHandler(HandlerInterface);
		UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=HandlerUnregistered result=OK pre_calls=%lld"),
			static_cast<int64>(Handler->GetPreCalls()));
	}
	RegisteredOn = TWeakObjectPtr<AFGBuildableSubsystem>();
	bHandlerRegistered = false;
}

void ASlgSpikeSubsystem::CheckHandlerSilence(double Wall)
{
	if (!bHandlerRegistered || bSilenceCheckDone || Wall < NextSilenceCheckWall)
	{
		return;
	}
	NextSilenceCheckWall = Wall + SilenceCheckPeriodSeconds;

	USlgSpikeTickHandler* Handler = TickHandler;
	if (Handler == nullptr)
	{
		return;
	}
	if (Handler->GetTotalPreCalls() > 0)
	{
		bSilenceCheckDone = true;
		return;
	}

	// An empty server may be paused by Auto Pause, which is what S6 measures and not a registration problem, so the
	// clock only runs while a player is connected.
	const int32 Players = SlgSpikeUtil::CountPlayers(GetWorld());
	if (Players <= 0)
	{
		PlayersPresentSinceWall = -1.0;
		return;
	}
	if (PlayersPresentSinceWall < 0.0)
	{
		PlayersPresentSinceWall = Wall;
	}
	if (Wall - PlayersPresentSinceWall < SilenceWindowSeconds || Wall - HandlerRegisteredWall < SilenceWindowSeconds)
	{
		return;
	}

	// Exactly once, whatever happens below, so /slgspike reset or a second server start cannot repeat it.
	bSilenceCheckDone = true;

	UE_LOG(LogSlgSpike, Warning,
		TEXT("[SLGSPIKE] S3 event=HandlerSilent seconds_since_registered=%.1f players=%d seconds_with_players=%.1f total_pre_calls=%lld"),
		Wall - HandlerRegisteredWall, Players, Wall - PlayersPresentSinceWall, static_cast<int64>(Handler->GetTotalPreCalls()));

	AFGBuildableSubsystem* BuildableSubsystem = RegisteredOn.Get();
	if (BuildableSubsystem == nullptr)
	{
		UE_LOG(LogSlgSpike, Warning, TEXT("[SLGSPIKE] S3 event=HandlerReRegistered result=SKIPPED reason=buildable_subsystem_gone"));
		return;
	}

	// Remove before Add: if the list was never cleared, a second Add alone would make the game call the handler
	// twice per frame, which looks exactly like "one call per factory sub-step" in the max_calls_in_frame figure.
	IFGFactoryTickHandlerInterface* HandlerInterface = Handler;
	BuildableSubsystem->RemoveFactoryTickHandler(HandlerInterface);
	BuildableSubsystem->AddFactoryTickHandler(HandlerInterface);
	bHandlerReRegistered = true;
	HandlerRegisteredWall = Wall;
	Handler->StartBurst(AutoBurstCalls);

	UE_LOG(LogSlgSpike, Warning,
		TEXT("[SLGSPIKE] S3 event=HandlerReRegistered result=DONE note=calls_after_this_line_mean_the_registration_was_lost"));
}

// ---------------------------------------------------------------------------------------------------------------------
// S6: heartbeat
// ---------------------------------------------------------------------------------------------------------------------

void ASlgSpikeSubsystem::SetHeartbeatEnabled(bool bEnabled)
{
	bHeartbeatEnabled = bEnabled;
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S6 event=HeartbeatEnabled value=%d"), bEnabled ? 1 : 0);
}

void ASlgSpikeSubsystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	++HeartbeatTicks;
	++HeartbeatTicksSinceLast;

	const double Wall = SlgSpikeUtil::WallSeconds();

	// Before the heartbeat switch: turning the heartbeat off must not turn the S3 diagnostic off.
	CheckHandlerSilence(Wall);

	if (!bHeartbeatEnabled)
	{
		return;
	}

	if (HeartbeatLastWall == 0.0)
	{
		HeartbeatLastWall = Wall;
		return;
	}
	if (Wall - HeartbeatLastWall < HeartbeatPeriodSeconds)
	{
		return;
	}

	UWorld* World = GetWorld();
	USlgSpikeTickHandler* Handler = TickHandler;
	const int64 PreCalls = Handler != nullptr ? Handler->GetPreCalls() : 0;
	const double LastCallAge = (Handler != nullptr && PreCalls > 0) ? (Wall - Handler->GetLastPreWall()) : -1.0;

	// If handler_last_call_age_s keeps growing while these lines keep coming, the actor still ticks but the factory
	// tick handler does not: that is what a paused factory looks like from here.
	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S6 event=Heartbeat gap_s=%.3f actor_ticks=%lld tick_dt=%.6f world_time=%.3f handler_pre_calls=%lld handler_last_call_age_s=%.3f players=%d"),
		Wall - HeartbeatLastWall, static_cast<int64>(HeartbeatTicksSinceLast), static_cast<double>(DeltaSeconds),
		World != nullptr ? static_cast<double>(World->GetTimeSeconds()) : -1.0, static_cast<int64>(PreCalls),
		LastCallAge, SlgSpikeUtil::CountPlayers(World));

	HeartbeatLastWall = Wall;
	HeartbeatTicksSinceLast = 0;
}

FString ASlgSpikeSubsystem::BuildStatusLine() const
{
	return FString::Printf(
		TEXT("name=%s spike_id=%d begin_play_count=%d load_count=%d save_count=%d loaded_from_save=%d handler_registered=%d handler_reregistered=%d heartbeat=%d"),
		*GetName(), SpikeId, BeginPlayCount, LoadCount, SaveCount, bLoadedFromSave ? 1 : 0, bHandlerRegistered ? 1 : 0,
		bHandlerReRegistered ? 1 : 0, bHeartbeatEnabled ? 1 : 0);
}
