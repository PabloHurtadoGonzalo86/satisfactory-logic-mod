// S2 and S6: the mod subsystem. It is spawned on the server by SML (ReplicationPolicy SpawnOnServer), saved with the
// world through IFGSaveInterface, owns the factory tick handler, and runs a heartbeat that keeps ticking while the
// world is paused so the server log shows what happens during Auto Pause.

#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Subsystem/ModSubsystem.h"
#include "SlgSpikeSubsystem.generated.h"

class AFGBuildableSubsystem;
class USlgSpikeTickHandler;

UCLASS()
class SLGSPIKE_API ASlgSpikeSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()
public:
	ASlgSpikeSubsystem();

	/** The spawned subsystem of this world, or nullptr (clients never have one). */
	static ASlgSpikeSubsystem* Get(UWorld* World);

	//~ Begin IFGSaveInterface
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
	virtual bool NeedTransform_Implementation() override;
	virtual bool ShouldSave_Implementation() const override;
	//~ End IFGSaveInterface

	//~ Begin AActor interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	USlgSpikeTickHandler* GetTickHandler() const;

	/** One line with the S2 counters, the handler state and the heartbeat state, for /slgspike status. */
	FString BuildStatusLine() const;

	/** Turn the 1 Hz S6 heartbeat lines on or off. */
	void SetHeartbeatEnabled(bool bEnabled);

protected:
	//~ Begin AModSubsystem interface
	virtual void Init() override;
	virtual void BeginPlay() override;
	//~ End AModSubsystem interface

private:
	/** Creates the handler and registers it with the buildable subsystem. Retries on the next tick until it exists. */
	void TryRegisterHandler();
	void UnregisterHandler();

	/**
	 * One-shot S3 diagnostic. If a player has been connected for a few seconds and the handler was never called, the
	 * registration may have been lost (the buildable subsystem could rebuild its list after our BeginPlay). It logs
	 * HandlerSilent, removes and adds the handler once, and starts a new burst, so the log tells "list was cleared"
	 * from "the game never calls mod handlers".
	 */
	void CheckHandlerSilence(double Wall);

	/** Writes one S2 line. EventName says which callback fired; ord shows the order the callbacks fire in. */
	void LogSaveEvent(const TCHAR* EventName, int32 SaveVersion, int32 GameVersion);

	/** Version of this plugin's own saved data. The game's saveVersion and gameVersion belong to the game. */
	UPROPERTY(SaveGame)
	int32 SpikeSaveVersion;

	/** Random id chosen the first time the subsystem exists in a world. Equal across reloads means the same actor came back. */
	UPROPERTY(SaveGame)
	int32 SpikeId;

	/** Number of BeginPlay calls over the whole life of the save, loads included. */
	UPROPERTY(SaveGame)
	int32 BeginPlayCount;

	/** Incremented in PostLoadGame_Implementation. The S2 pass criterion is that it grows by one per reload. */
	UPROPERTY(SaveGame)
	int32 LoadCount;

	/** Incremented in PreSaveGame_Implementation, so the saved value includes the save that wrote it. */
	UPROPERTY(SaveGame)
	int32 SaveCount;

	/** The handler object. Kept in a UPROPERTY so the garbage collector does not free what the game calls. */
	UPROPERTY()
	TObjectPtr<USlgSpikeTickHandler> TickHandler;

	/** The buildable subsystem the handler is registered with. Weak: the world may be tearing down. */
	TWeakObjectPtr<AFGBuildableSubsystem> RegisteredOn;

	/** True between a successful AddFactoryTickHandler and the matching RemoveFactoryTickHandler. */
	bool bHandlerRegistered = false;

	double HandlerRegisteredWall = 0.0;
	double PlayersPresentSinceWall = -1.0;
	double NextSilenceCheckWall = 0.0;
	bool bSilenceCheckDone = false;
	bool bHandlerReRegistered = false;

	bool bLoadedFromSave = false;
	int32 EventOrdinal = 0;
	int32 RegisterAttempts = 0;

	bool bHeartbeatEnabled = true;
	double HeartbeatLastWall = 0.0;
	int64 HeartbeatTicks = 0;
	int64 HeartbeatTicksSinceLast = 0;
};
