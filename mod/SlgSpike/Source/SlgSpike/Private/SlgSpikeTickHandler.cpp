#include "SlgSpikeTickHandler.h"

#include "SlgSpikeLog.h"
#include "Buildables/FGBuildableCircuitSwitch.h"
#include "FGCircuitSubsystem.h"
#include "FGPowerCircuit.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** Capacity of the in-memory ring of recent calls (about 17 s at 30 calls per second). */
	constexpr int32 RingCapacity = 512;

	/** Wall-clock length of one summary window. */
	constexpr double SummaryPeriodSeconds = 10.0;

	/** After a SetSwitchOn the switch is watched for this long, first on every call, then at a fixed period. */
	constexpr double WatchSeconds = 4.0;
	constexpr double WatchPeriodSeconds = 0.25;
	constexpr int32 WatchFirstCalls = 8;

	const TCHAR* ActionName(ESlgSwitchAction Action)
	{
		switch (Action)
		{
		case ESlgSwitchAction::List: return TEXT("list");
		case ESlgSwitchAction::Read: return TEXT("read");
		case ESlgSwitchAction::On: return TEXT("on");
		case ESlgSwitchAction::Off: return TEXT("off");
		case ESlgSwitchAction::Toggle: return TEXT("toggle");
		}
		return TEXT("unknown");
	}
}

// Console trigger for S4. SML declares its own console variables this way (TAutoConsoleVariable and
// GetValueOnGameThread). The handler only exists in the process that runs the server side, so the variables only take
// effect when typed in that process: the console of a host-and-play session, or the interactive console of the
// dedicated server (documented for console commands in general, untested here). A remote client's console cannot
// change them. Set slgspike.SwitchIndex first, then slgspike.SwitchAction 5 toggles that switch; set the action back
// to 0 before repeating the same action. The chat command /slgspike switch is the supported route for a dedicated server.
static TAutoConsoleVariable<int32> CVarSwitchAction(
	TEXT("slgspike.SwitchAction"),
	0,
	TEXT("SlgSpike S4: 1 list, 2 read, 3 on, 4 off, 5 toggle. Runs on the next factory tick."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarSwitchIndex(
	TEXT("slgspike.SwitchIndex"),
	0,
	TEXT("SlgSpike S4: which switch slgspike.SwitchAction acts on (the idx printed by the list action)."),
	ECVF_Default);

// ---------------------------------------------------------------------------------------------------------------------
// S3: call statistics
// ---------------------------------------------------------------------------------------------------------------------

void USlgSpikeTickHandler::PreFactoryTick(AFGBuildableSubsystem* subsystem, float deltaTime)
{
	const double Wall = SlgSpikeUtil::WallSeconds();
	UWorld* World = subsystem != nullptr ? subsystem->GetWorld() : nullptr;

	if (PreCalls == 0)
	{
		FirstPreWall = Wall;
		ResetWindow(Wall);
	}

	FSlgCallRecord Record;
	Record.Seq = PreCalls + 1;
	Record.Wall = Wall;
	Record.GapMs = PreCalls > 0 ? (Wall - LastPreWall) * 1000.0 : 0.0;
	Record.Dt = deltaTime;
	Record.Sub = static_cast<int32>(AFGBuildableSubsystem::GetCurrentSubStepIteration());
	Record.SubMax = static_cast<int32>(AFGBuildableSubsystem::GetLastSubStepIteration());
	Record.bLast = AFGBuildableSubsystem::IsLastFactorySubStep();
	Record.SimTime = subsystem != nullptr ? static_cast<double>(subsystem->GetFactorySimulationTime()) : -1.0;
	Record.WorldTime = World != nullptr ? static_cast<double>(World->GetTimeSeconds()) : -1.0;
	Record.bGameThread = IsInGameThread();

	++PreCalls;
	++TotalPreCalls;
	LastPreWall = Wall;

	// Calls that share one world time belong to the same engine frame. This is how the data tells "once per frame"
	// from "once per factory sub-step": several calls with an identical world value are sub-steps of one frame.
	if (Record.WorldTime != LastWorldTime)
	{
		++FrameCount;
		++Window.Frames;
		CallsInThisFrame = 1;
		LastWorldTime = Record.WorldTime;
	}
	else
	{
		++CallsInThisFrame;
	}
	MaxCallsInFrame = FMath::Max(MaxCallsInFrame, CallsInThisFrame);
	Window.MaxCallsInFrame = FMath::Max(Window.MaxCallsInFrame, CallsInThisFrame);

	++Window.Calls;
	const double Dt = static_cast<double>(deltaTime);
	Window.DtSum += Dt;
	Window.DtMin = Window.Calls == 1 ? Dt : FMath::Min(Window.DtMin, Dt);
	Window.DtMax = Window.Calls == 1 ? Dt : FMath::Max(Window.DtMax, Dt);
	if (Record.Seq > 1)
	{
		Window.GapMsMin = Window.GapCount == 0 ? Record.GapMs : FMath::Min(Window.GapMsMin, Record.GapMs);
		Window.GapMsMax = Window.GapCount == 0 ? Record.GapMs : FMath::Max(Window.GapMsMax, Record.GapMs);
		Window.GapMsSum += Record.GapMs;
		++Window.GapCount;
	}
	if (Record.bLast)
	{
		++Window.LastSubStepCalls;
	}
	if (!Record.bGameThread)
	{
		++Window.NonGameThreadCalls;
	}
	++Window.SubHist[FMath::Clamp(Record.Sub, 0, 15)];

	if (Ring.Num() < RingCapacity)
	{
		Ring.Add(Record);
	}
	else
	{
		Ring[RingNext] = Record;
		RingNext = (RingNext + 1) % RingCapacity;
	}

	if (BurstRemaining > 0)
	{
		--BurstRemaining;
		UE_LOG(LogSlgSpike, Display,
			TEXT("[SLGSPIKE] S3 phase=pre seq=%lld t=%.6f gap_ms=%.3f dt=%.6f sub=%d/%d last=%d sim=%.4f world=%.4f gt=%d"),
			static_cast<int64>(Record.Seq), Record.Wall - FirstPreWall, Record.GapMs, Dt, Record.Sub, Record.SubMax,
			Record.bLast ? 1 : 0, Record.SimTime, Record.WorldTime, Record.bGameThread ? 1 : 0);
	}

	if (Wall - Window.StartWall >= SummaryPeriodSeconds)
	{
		EmitSummary(World, Wall);
		ResetWindow(Wall);
	}

	// S4 work happens here, inside the documented-safe callback, after the statistics so a slow request never
	// distorts the timing of the call that triggered it.
	PollConsoleTrigger();
	if (subsystem != nullptr && World != nullptr)
	{
		ProcessSwitchRequests(subsystem, World, Wall);
		ProcessSwitchWatches(subsystem, World, Wall);
	}
}

void USlgSpikeTickHandler::PostFactoryTick(AFGBuildableSubsystem* subsystem, float deltaTime)
{
	const double Wall = SlgSpikeUtil::WallSeconds();
	++PostCalls;
	++Window.PostCalls;

	// Time since the most recent PreFactoryTick: how long the dispatch of the factory tick took, as far as this
	// pair of callbacks can tell. If Pre and Post counts differ in the summary, the pairing is not one to one.
	const double SpanMs = PreCalls > 0 ? (Wall - LastPreWall) * 1000.0 : 0.0;
	Window.SpanMsSum += SpanMs;
	Window.SpanMsMax = FMath::Max(Window.SpanMsMax, SpanMs);

	if (PostBurstRemaining > 0)
	{
		--PostBurstRemaining;
		UE_LOG(LogSlgSpike, Display,
			TEXT("[SLGSPIKE] S3 phase=post post_seq=%lld t=%.6f span_ms=%.3f dt=%.6f sub=%d/%d sim=%.4f gt=%d"),
			static_cast<int64>(PostCalls), Wall - FirstPreWall, SpanMs, static_cast<double>(deltaTime),
			static_cast<int32>(AFGBuildableSubsystem::GetCurrentSubStepIteration()),
			static_cast<int32>(AFGBuildableSubsystem::GetLastSubStepIteration()),
			subsystem != nullptr ? static_cast<double>(subsystem->GetFactorySimulationTime()) : -1.0,
			IsInGameThread() ? 1 : 0);
	}
}

void USlgSpikeTickHandler::StartBurst(int32 Calls)
{
	BurstRemaining = FMath::Max(0, Calls);
	PostBurstRemaining = BurstRemaining;
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=BurstStarted calls=%d"), BurstRemaining);
}

void USlgSpikeTickHandler::ResetStats()
{
	PreCalls = 0;
	PostCalls = 0;
	FrameCount = 0;
	CallsInThisFrame = 0;
	MaxCallsInFrame = 0;
	FirstPreWall = 0.0;
	LastPreWall = 0.0;
	LastWorldTime = -1.0;
	Ring.Reset();
	RingNext = 0;
	Window = FSlgWindowStats();
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=StatsReset"));
}

void USlgSpikeTickHandler::ResetWindow(double Wall)
{
	Window = FSlgWindowStats();
	Window.StartWall = Wall;
}

const FSlgCallRecord& USlgSpikeTickHandler::GetChrono(int32 Index) const
{
	if (Ring.Num() < RingCapacity)
	{
		return Ring[Index];
	}
	return Ring[(RingNext + Index) % RingCapacity];
}

void USlgSpikeTickHandler::EmitSummary(UWorld* World, double Wall)
{
	const double Elapsed = Wall - Window.StartWall;
	if (Window.Calls <= 0 || Elapsed <= 0.0)
	{
		return;
	}

	FString SubHist;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		if (Window.SubHist[Index] > 0)
		{
			SubHist += FString::Printf(TEXT("%d:%d,"), Index, Window.SubHist[Index]);
		}
	}

	const double DtMean = Window.DtSum / static_cast<double>(Window.Calls);
	const double GapMean = Window.GapCount > 0 ? Window.GapMsSum / static_cast<double>(Window.GapCount) : 0.0;
	const double SpanMean = Window.PostCalls > 0 ? Window.SpanMsSum / static_cast<double>(Window.PostCalls) : 0.0;

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S3 event=Summary window_s=%.3f pre_calls=%d post_calls=%d pre_per_s=%.3f frames=%d frames_per_s=%.3f max_calls_in_frame=%d dt_min=%.6f dt_mean=%.6f dt_max=%.6f gap_ms_min=%.3f gap_ms_mean=%.3f gap_ms_max=%.3f last_substep_calls=%d sub_hist=%s non_game_thread_calls=%d factory_span_ms_mean=%.3f factory_span_ms_max=%.3f players=%d total_pre_calls=%lld"),
		Elapsed, Window.Calls, Window.PostCalls, static_cast<double>(Window.Calls) / Elapsed, Window.Frames,
		static_cast<double>(Window.Frames) / Elapsed, Window.MaxCallsInFrame, Window.DtMin, DtMean, Window.DtMax,
		Window.GapMsMin, GapMean, Window.GapMsMax, Window.LastSubStepCalls, *SubHist, Window.NonGameThreadCalls,
		SpanMean, Window.SpanMsMax, SlgSpikeUtil::CountPlayers(World), static_cast<int64>(PreCalls));
}

void USlgSpikeTickHandler::DumpRing(int32 MaxRecords) const
{
	const int32 Total = Ring.Num();
	const int32 Count = FMath::Clamp(MaxRecords, 0, Total);

	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=DumpBegin records=%d ring_holds=%d ring_capacity=%d"),
		Count, Total, RingCapacity);

	for (int32 Index = Total - Count; Index < Total; ++Index)
	{
		const FSlgCallRecord& Record = GetChrono(Index);
		UE_LOG(LogSlgSpike, Display,
			TEXT("[SLGSPIKE] S3 event=Dump seq=%lld t=%.6f gap_ms=%.3f dt=%.6f sub=%d/%d last=%d sim=%.4f world=%.4f gt=%d"),
			static_cast<int64>(Record.Seq), Record.Wall - FirstPreWall, Record.GapMs, static_cast<double>(Record.Dt),
			Record.Sub, Record.SubMax, Record.bLast ? 1 : 0, Record.SimTime, Record.WorldTime,
			Record.bGameThread ? 1 : 0);
	}

	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S3 event=DumpEnd"));
}

FString USlgSpikeTickHandler::BuildStatusLine() const
{
	return FString::Printf(
		TEXT("pre_calls=%lld post_calls=%lld frames=%lld max_calls_in_frame=%d burst_left=%d pending_switch_requests=%d watches=%d ring=%d"),
		static_cast<int64>(PreCalls), static_cast<int64>(PostCalls), static_cast<int64>(FrameCount), MaxCallsInFrame,
		BurstRemaining, PendingRequests.Num(), Watches.Num(), Ring.Num());
}

// ---------------------------------------------------------------------------------------------------------------------
// S4: vanilla circuit switch and power circuit
// ---------------------------------------------------------------------------------------------------------------------

void USlgSpikeTickHandler::PollConsoleTrigger()
{
	const int32 Value = CVarSwitchAction.GetValueOnGameThread();
	if (Value == LastCvarAction)
	{
		return;
	}
	LastCvarAction = Value;

	ESlgSwitchAction Action = ESlgSwitchAction::List;
	switch (Value)
	{
	case 1: Action = ESlgSwitchAction::List; break;
	case 2: Action = ESlgSwitchAction::Read; break;
	case 3: Action = ESlgSwitchAction::On; break;
	case 4: Action = ESlgSwitchAction::Off; break;
	case 5: Action = ESlgSwitchAction::Toggle; break;
	default: return;
	}

	const FString Target = Action == ESlgSwitchAction::List
		? FString()
		: FString::Printf(TEXT("#%d"), CVarSwitchIndex.GetValueOnGameThread());
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S4 event=ConsoleTrigger value=%d target=%s"), Value, *Target);
	EnqueueSwitchRequest(Action, Target);
}

void USlgSpikeTickHandler::EnqueueSwitchRequest(ESlgSwitchAction Action, const FString& Target)
{
	FSlgSwitchRequest Request;
	Request.Action = Action;
	Request.Target = Target;
	PendingRequests.Add(Request);

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S4 event=RequestQueued action=%s target=%s pending=%d handler_calls_so_far=%lld"),
		ActionName(Action), *Target, PendingRequests.Num(), static_cast<int64>(PreCalls));
}

void USlgSpikeTickHandler::LogPowerCircuit(const TCHAR* Label, AFGCircuitSubsystem* CircuitSubsystem, int32 CircuitId) const
{
	// FindCircuit documents that the returned pointer is valid only for this tick, so it is resolved here, used and dropped.
	UFGPowerCircuit* Circuit = (CircuitSubsystem != nullptr && CircuitId >= 0)
		? CircuitSubsystem->FindCircuit<UFGPowerCircuit>(CircuitId)
		: nullptr;

	if (Circuit == nullptr)
	{
		UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S4 circuit=%s id=%d found=0"), Label, CircuitId);
		return;
	}

	FPowerCircuitStats Stats;
	Circuit->GetStats(Stats);

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S4 circuit=%s id=%d found=1 has_power=%d fuse_triggered=%d capacity_mw=%.2f produced_mw=%.2f consumed_mw=%.2f max_consumption_mw=%.2f"),
		Label, CircuitId, Circuit->HasPower() ? 1 : 0, Circuit->IsFuseTriggered() ? 1 : 0,
		static_cast<double>(Stats.PowerProductionCapacity), static_cast<double>(Stats.PowerProduced),
		static_cast<double>(Stats.PowerConsumed), static_cast<double>(Stats.MaximumPowerConsumption));
}

void USlgSpikeTickHandler::LogSwitchState(const TCHAR* Phase, UWorld* World, AFGBuildableCircuitSwitch* Switch, double ElapsedSeconds) const
{
	const int32 CircuitId0 = Switch->GetCircuitID0();
	const int32 CircuitId1 = Switch->GetCircuitID1();
	const FVector Location = Switch->GetActorLocation();

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S4 phase=%s switch=%s on=%d connected=%d circuit_id0=%d circuit_id1=%d loc=%.0f,%.0f,%.0f elapsed_s=%.3f authority=%d"),
		Phase, *Switch->GetName(), Switch->IsSwitchOn() ? 1 : 0, Switch->IsSwitchConnected() ? 1 : 0, CircuitId0,
		CircuitId1, static_cast<double>(Location.X), static_cast<double>(Location.Y), static_cast<double>(Location.Z),
		ElapsedSeconds, Switch->HasAuthority() ? 1 : 0);

	AFGCircuitSubsystem* CircuitSubsystem = AFGCircuitSubsystem::Get(World);
	LogPowerCircuit(TEXT("side0"), CircuitSubsystem, CircuitId0);
	LogPowerCircuit(TEXT("side1"), CircuitSubsystem, CircuitId1);
}

void USlgSpikeTickHandler::ProcessSwitchRequests(AFGBuildableSubsystem* Subsystem, UWorld* World, double Wall)
{
	if (PendingRequests.Num() == 0)
	{
		return;
	}

	const TArray<FSlgSwitchRequest> Requests = PendingRequests;
	PendingRequests.Reset();

	TArray<AFGBuildableCircuitSwitch*> Switches;
	Subsystem->GetTypedBuildable<AFGBuildableCircuitSwitch>(Switches);

	for (const FSlgSwitchRequest& Request : Requests)
	{
		if (Request.Action == ESlgSwitchAction::List)
		{
			UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S4 event=List switches=%d handler_call=%lld"),
				Switches.Num(), static_cast<int64>(PreCalls));
			for (int32 SwitchIndex = 0; SwitchIndex < Switches.Num(); ++SwitchIndex)
			{
				UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S4 event=ListEntry idx=%d switch=%s"), SwitchIndex,
					*Switches[SwitchIndex]->GetName());
				LogSwitchState(TEXT("list"), World, Switches[SwitchIndex], 0.0);
			}
			continue;
		}

		// "#n" picks the n-th switch of the current list. Otherwise match by exact actor name first, then by a
		// case-insensitive substring that must be unique.
		AFGBuildableCircuitSwitch* Found = nullptr;
		int32 MatchCount = 0;
		if (Request.Target.StartsWith(TEXT("#")))
		{
			const int32 WantedIndex = FCString::Atoi(*Request.Target.RightChop(1));
			if (WantedIndex >= 0 && WantedIndex < Switches.Num())
			{
				Found = Switches[WantedIndex];
				MatchCount = 1;
			}
		}
		for (AFGBuildableCircuitSwitch* Candidate : Switches)
		{
			if (Candidate->GetName() == Request.Target)
			{
				Found = Candidate;
				MatchCount = 1;
				break;
			}
		}
		if (Found == nullptr && !Request.Target.IsEmpty())
		{
			const FString Needle = Request.Target.ToLower();
			for (AFGBuildableCircuitSwitch* Candidate : Switches)
			{
				if (Candidate->GetName().ToLower().Contains(Needle))
				{
					Found = Candidate;
					++MatchCount;
				}
			}
		}

		if (MatchCount != 1 || Found == nullptr)
		{
			UE_LOG(LogSlgSpike, Warning,
				TEXT("[SLGSPIKE] S4 event=Request result=FAIL reason=%s target=%s matches=%d switches_known=%d"),
				MatchCount == 0 ? TEXT("not_found") : TEXT("ambiguous"), *Request.Target, MatchCount, Switches.Num());
			continue;
		}

		const bool bBefore = Found->IsSwitchOn();
		LogSwitchState(TEXT("before"), World, Found, 0.0);

		if (Request.Action == ESlgSwitchAction::Read)
		{
			continue;
		}

		const bool bWanted = Request.Action == ESlgSwitchAction::On
			? true
			: (Request.Action == ESlgSwitchAction::Off ? false : !bBefore);

		if (!Found->HasAuthority())
		{
			UE_LOG(LogSlgSpike, Warning,
				TEXT("[SLGSPIKE] S4 event=SetSwitchOn result=SKIPPED reason=no_authority switch=%s"), *Found->GetName());
			continue;
		}

		UE_LOG(LogSlgSpike, Display,
			TEXT("[SLGSPIKE] S4 event=SetSwitchOn switch=%s action=%s before=%d wanted=%d handler_call=%lld"),
			*Found->GetName(), ActionName(Request.Action), bBefore ? 1 : 0, bWanted ? 1 : 0,
			static_cast<int64>(PreCalls));

		Found->SetSwitchOn(bWanted);

		LogSwitchState(TEXT("after_same_call"), World, Found, 0.0);

		FSlgSwitchWatch Watch;
		Watch.Name = Found->GetName();
		Watch.StartWall = Wall;
		Watch.NextWall = Wall;
		Watches.Add(Watch);
	}
}

void USlgSpikeTickHandler::ProcessSwitchWatches(AFGBuildableSubsystem* Subsystem, UWorld* World, double Wall)
{
	if (Watches.Num() == 0)
	{
		return;
	}

	// Rescans on every call instead of caching a pointer, so a switch dismantled during the watch is never touched.
	// On a big factory that scan costs time: do not run S4 commands inside an S3 measurement window.
	TArray<AFGBuildableCircuitSwitch*> Switches;
	Subsystem->GetTypedBuildable<AFGBuildableCircuitSwitch>(Switches);

	for (int32 Index = Watches.Num() - 1; Index >= 0; --Index)
	{
		FSlgSwitchWatch& Watch = Watches[Index];
		const double Elapsed = Wall - Watch.StartWall;

		if (Watch.Logged < WatchFirstCalls || Wall >= Watch.NextWall)
		{
			AFGBuildableCircuitSwitch* Found = nullptr;
			for (AFGBuildableCircuitSwitch* Candidate : Switches)
			{
				if (Candidate->GetName() == Watch.Name)
				{
					Found = Candidate;
					break;
				}
			}

			if (Found != nullptr)
			{
				LogSwitchState(TEXT("watch"), World, Found, Elapsed);
			}
			else
			{
				UE_LOG(LogSlgSpike, Warning, TEXT("[SLGSPIKE] S4 phase=watch switch=%s found=0 elapsed_s=%.3f"),
					*Watch.Name, Elapsed);
			}

			++Watch.Logged;
			Watch.NextWall = Wall + WatchPeriodSeconds;
		}

		if (Elapsed >= WatchSeconds)
		{
			Watches.RemoveAt(Index);
		}
	}
}
