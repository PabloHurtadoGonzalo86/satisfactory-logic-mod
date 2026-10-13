// S3 and S4: the factory tick handler. It records how often and with which parameters the game calls it, and it is
// the only place that touches vanilla buildables (reads and the SetSwitchOn write), because the AFGBuildableSubsystem
// header documents PreFactoryTick as the safe place to read and write any buildable.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FGBuildableSubsystem.h"
#include "SlgSpikeTickHandler.generated.h"

class AFGBuildableCircuitSwitch;
class AFGCircuitSubsystem;

/** What a chat command asks the handler to do with a vanilla circuit switch. */
enum class ESlgSwitchAction : uint8
{
	List,
	Read,
	On,
	Off,
	Toggle
};

struct FSlgSwitchRequest
{
	ESlgSwitchAction Action = ESlgSwitchAction::List;
	FString Target;
};

/** A switch that is logged on every call for a few calls, then every quarter second, after a SetSwitchOn. */
struct FSlgSwitchWatch
{
	FString Name;
	double StartWall = 0.0;
	double NextWall = 0.0;
	int32 Logged = 0;
};

/** One PreFactoryTick call. */
struct FSlgCallRecord
{
	int64 Seq = 0;
	double Wall = 0.0;
	double GapMs = 0.0;
	float Dt = 0.0f;
	int32 Sub = 0;
	int32 SubMax = 0;
	bool bLast = false;
	double SimTime = 0.0;
	double WorldTime = 0.0;
	bool bGameThread = false;
};

/** Counters for the current summary window (10 s of wall time). */
struct FSlgWindowStats
{
	double StartWall = 0.0;
	int32 Calls = 0;
	int32 PostCalls = 0;
	double DtMin = 0.0;
	double DtMax = 0.0;
	double DtSum = 0.0;
	int32 GapCount = 0;
	double GapMsMin = 0.0;
	double GapMsMax = 0.0;
	double GapMsSum = 0.0;
	int32 LastSubStepCalls = 0;
	int32 NonGameThreadCalls = 0;
	int32 SubHist[16] = {};
	int32 Frames = 0;
	int32 MaxCallsInFrame = 0;
	double SpanMsSum = 0.0;
	double SpanMsMax = 0.0;
};

UCLASS()
class SLGSPIKE_API USlgSpikeTickHandler : public UObject, public IFGFactoryTickHandlerInterface
{
	GENERATED_BODY()
public:
	//~ Begin IFGFactoryTickHandlerInterface
	virtual void PreFactoryTick(AFGBuildableSubsystem* subsystem, float deltaTime) override;
	virtual void PostFactoryTick(AFGBuildableSubsystem* subsystem, float deltaTime) override;
	//~ End IFGFactoryTickHandlerInterface

	/** Log every Pre and Post call (not only the summary) for the next Calls calls. */
	void StartBurst(int32 Calls);

	/** Forget all counters, the ring buffer and the summary window. Pending switch requests are kept. */
	void ResetStats();

	/** Write the last MaxRecords calls from the ring buffer to the log, oldest first. */
	void DumpRing(int32 MaxRecords) const;

	/**
	 * Queue a switch request. It runs at the start of the next PreFactoryTick, never from the caller's context.
	 * Target is an actor name, a unique part of one, or "#n" for the n-th entry of the last list (counting from 0).
	 */
	void EnqueueSwitchRequest(ESlgSwitchAction Action, const FString& Target);

	int64 GetPreCalls() const { return PreCalls; }

	/** Like GetPreCalls, but ResetStats never clears it: the subsystem's silence check must not be fooled by /slgspike reset. */
	int64 GetTotalPreCalls() const { return TotalPreCalls; }
	double GetLastPreWall() const { return LastPreWall; }

	FString BuildStatusLine() const;

private:
	void EmitSummary(UWorld* World, double Wall);
	void ResetWindow(double Wall);
	const FSlgCallRecord& GetChrono(int32 Index) const;

	/** Turns a change of the console variable slgspike.SwitchAction into a queued switch request. */
	void PollConsoleTrigger();

	void ProcessSwitchRequests(AFGBuildableSubsystem* Subsystem, UWorld* World, double Wall);
	void ProcessSwitchWatches(AFGBuildableSubsystem* Subsystem, UWorld* World, double Wall);
	void LogSwitchState(const TCHAR* Phase, UWorld* World, AFGBuildableCircuitSwitch* Switch, double ElapsedSeconds) const;
	void LogPowerCircuit(const TCHAR* Label, AFGCircuitSubsystem* CircuitSubsystem, int32 CircuitId) const;

	int64 PreCalls = 0;
	int64 TotalPreCalls = 0;
	int64 PostCalls = 0;
	int64 FrameCount = 0;
	int32 CallsInThisFrame = 0;
	int32 MaxCallsInFrame = 0;
	double FirstPreWall = 0.0;
	double LastPreWall = 0.0;
	double LastWorldTime = -1.0;

	int32 BurstRemaining = 0;
	int32 PostBurstRemaining = 0;

	FSlgWindowStats Window;

	TArray<FSlgCallRecord> Ring;
	int32 RingNext = 0;

	int32 LastCvarAction = 0;

	TArray<FSlgSwitchRequest> PendingRequests;
	TArray<FSlgSwitchWatch> Watches;
};
