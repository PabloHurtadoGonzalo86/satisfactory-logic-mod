# Sensors, actuators and runtime

_Generated from `data/runtime-api.json` by `tools/render_research.py`; do not edit by hand. Research run of 2026-10-13. Claims are the researcher's; verdicts are from independent skeptics told to refute them. Wording is kept verbatim (English)._

## Summary

Evidence base: SDK headers at SatisfactoryModLoader d2162c99, which is current master, 4 commits after the v3.12.0 tag. The SDK .cpp files are mostly generated stubs, so only declarations and header comments count as evidence; setter side effects and authority checks are unverified.

READ (all public C++ API, no SML hooks needed): power circuits (UFGPowerCircuit::HasPower, IsFuseTriggered, GetStats, which refreshes about once per second), machines (AFGBuildableFactory::IsProducing, IsProductionPaused, state delegates), inventories (UFGInventoryComponent::GetNumItems, IsEmpty), belt throughput (AFGBuildableConveyorMonitor::GetCalculatedAverage, only where a vanilla monitor is placed), reservoirs (GetFluidContent, GetFluidContentMax), trains and signals. Group-level power totals are private; FRM reads them with AccessTransformer Friend entries.

CONTROL: AFGBuildableCircuitSwitch::SetSwitchOn, AFGBuildableFactory::SetIsProductionPaused (standby), SetLightEnabled on lights and the light panel, AFGBuildableSplitterSmart::SetSortRules (the programmable splitter's parent class is the same), priority switch SetPriority, train SetSelfDrivingEnabled. Doors have no public setter. FRM 1.5.3, already on the owner's server, calls SetSwitchOn and SetIsProductionPaused: a working precedent, but secondary evidence.

RUNTIME: Run everything on the server. Inventories replicate only while a UI is open, and power totals are NotReplicated. Evaluate logic in IFGFactoryTickHandlerInterface::PreFactoryTick/PostFactoryTick (sequential, game thread, safe for any buildable), not in the multithreaded Factory_Tick. Host the netlist in an AModSubsystem plus IFGSaveInterface, a pattern FicsIt-Networks uses. Client requests go through an RCO (server RPC, dummy replicated property, never multicast) or IFGUseableInterface::OnUse, which runs on both sides.

SAVE: Only SaveGame properties persist. Removing the mod drops its actors and data, but vanilla state it changed persists. Keep your own version int, because saveVersion and gameVersion belong to the game.

TIME: Factory time is clamped and is not game time. The server target is about 30 ticks/s (secondary), and auto-pause freezes the whole simulation.

PERFORMANCE: No official or measured per-gate cost exists; I found none. Build the gate engine as UE-free C++ that can be benchmarked here with g++.

## Claims at a glance

| Id | Confidence | Decision-driving | Skeptic verdicts |
| --- | --- | --- | --- |
| C1 | verified-primary | yes | partially-correct |
| C2 | verified-primary | no | not verified (not decision-driving) |
| C3 | verified-primary | yes | confirmed |
| C4 | verified-primary | no | not verified (not decision-driving) |
| C5 | verified-primary | no | not verified (not decision-driving) |
| C6 | secondary | no | not verified (not decision-driving) |
| C7 | verified-primary | yes | partially-correct |
| C8 | verified-primary | no | not verified (not decision-driving) |
| C9 | verified-primary | no | not verified (not decision-driving) |
| C10 | verified-primary | no | not verified (not decision-driving) |
| C11 | verified-primary | yes | confirmed |
| C12 | verified-primary | no | not verified (not decision-driving) |
| C13 | verified-primary | yes | confirmed |
| C14 | verified-primary | no | not verified (not decision-driving) |
| C15 | verified-primary | yes | partially-correct |
| C16 | verified-primary | yes | confirmed |
| C17 | verified-primary | no | not verified (not decision-driving) |
| C18 | secondary | yes | partially-correct |
| C19 | secondary | no | not verified (not decision-driving) |
| C20 | verified-primary | no | not verified (not decision-driving) |
| C21 | unverified | no | not verified (not decision-driving) |
| C22 | unverified | no | not verified (not decision-driving) |
| C23 | secondary | no | not verified (not decision-driving) |
| C24 | verified-primary | no | not verified (not decision-driving) |
| C25 | verified-primary | no | not verified (not decision-driving) |

## Claims in detail

### C1 (verified-primary, decision-driving)

The safe place to evaluate a cross-building logic network is IFGFactoryTickHandlerInterface (PreFactoryTick/PostFactoryTick), registered with AFGBuildableSubsystem::AddFactoryTickHandler (public). Handlers run sequentially on the game thread and may read and write any buildable. Per-building Factory_Tick runs on worker threads and is unsafe for cross-buildable access. The header comment names the registration function 'RegisterFactoryTickHandler', but the declared method is AddFactoryTickHandler. The interface is UINTERFACE(MinimalAPI, CannotImplementInterfaceInBlueprint), so it is C++ only. Whether a mod can link against it is untested.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  Called before FactoryTick is dispatched on the build subsystem. Handlers are executed sequentially on the game thread, and as such, it is safe to read and write data for any buildable during this function
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  which is normally unsafe to do from FactoryTick because other buildables can be modified at the same time from other threads.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  void AddFactoryTickHandler( IFGFactoryTickHandlerInterface* tickHandler );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  Implement this interface and register it using RegisterFactoryTickHandler on the buildable subsystem to receive sequential factory tick callback.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/FactoryTick.html>

  ```text
  Some of the ticks (the load balancer decides which buildings tick on which thread) happen on the main thread.
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Evaluate the cross-building logic network from IFGFactoryTickHandlerInterface::PreFactoryTick, which the header says is safe for read/write of any buildable on the game thread. PostFactoryTick is documented only as sequential on the game thread. Register with the public AFGBuildableSubsystem::AddFactoryTickHandler (the header comment wrongly says RegisterFactoryTickHandler). The handler must be a UObject implementing a C++-only interface, and the registration is transient, so it must be repeated per world. Per-building FactoryTick may run on several threads, so cross-buildable access there is unsafe. A public mod (StructuralPower) attempts exactly this, but linking and runtime on the owner's build are not verified.

Note: Verified in the header (pinned commit d2162c99 equals master HEAD, so main is identical): the four quotes match; AddFactoryTickHandler is public; the comment says RegisterFactoryTickHandler; the UINTERFACE macro is `UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))`. The docs quote 'Some of the ticks ... happen on the main thread.' matches the page text (curl). Problems: (a) the 'safe to read and write data for any buildable' guarantee is attached only to PreFactoryTick; the PostFactoryTick comment says only 'executed sequentially on the game thread'. (b) 'Untested' is out of date. TheHaliax/SatisfactoryMods StructuralPower (commit beac1c5, 2026-08-14) has a class deriving from IFGFactoryTickHandlerInterface, overrides PostFactoryTick, and calls `BuildableSubsystem->AddFactoryTickHandler(Handler);` after `NewObject<...>(World)`. That is source-level evidence only. It does not show that it links or runs on CL 502094. (c) The header shows `mFactoryTickHandlers` as `UPROPERTY(Transient) TArray<TScriptInterface<IFGFactoryTickHandlerInterface>>`. So the handler should be a UObject, and registration is not saved, so it must be redone each world (the precedent unregisters on OnWorldCleanup). The SDK .cpp for AddFactoryTickHandler is an empty stub, which is normal for the SDK. (d) The docs say factory ticks run on multiple threads and some on the main thread. They do not say 'always worker threads'.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

### C2 (verified-primary)

Factory simulation time is not game time: it is clamped, so it can lag behind. The buildable subsystem exposes GetFactorySimulationTime() and static sub-step queries (IsLastFactorySubStep). A logic clock should advance from the delta handed to the tick handler or from GetFactorySimulationTime(), with its own fixed step. It should not use AFGTimeOfDaySubsystem (the file is FGTimeSubsystem.h), which is the in-game day clock with a speed multiplier.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/FactoryTick.html>

  ```text
  factory time is clamped, which means we don't get extreme values.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  FORCEINLINE float GetFactorySimulationTime() const { return mAccumulatedFactorySimulationTime; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableSubsystem.h>

  ```text
  static bool IsLastFactorySubStep()
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGTimeSubsystem.h>

  ```text
  void SetTimeSpeedMultiplier( float multiplier );
  ```

### C3 (verified-primary, decision-driving)

Every vanilla actuator the idea needs has a public C++ setter, so no SML hook is required to drive it. Power switch: AFGBuildableCircuitSwitch::SetSwitchOn(bool). Standby: AFGBuildableFactory::SetIsProductionPaused(bool). Lights: AFGBuildableLightSource::SetLightEnabled and AFGBuildableLightsControlPanel::SetLightEnabled. Smart and programmable splitter: AFGBuildableSplitterSmart::SetSortRules, AddSortRule, SetSortRuleAt. The programmable splitter blueprint's parent is the same C++ class, found by string inspection of the .uasset. Priority switch: SetPriority. Train autopilot: SetSelfDrivingEnabled. The vanilla trigger for production-pause is the 'standby' toggle (inferred from the delegate comment, not proven).

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableCircuitSwitch.h>

  ```text
  void SetSwitchOn( bool isSwitchOn );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  virtual void SetIsProductionPaused( bool isPaused );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  So that you can listen for when standby is toggled. True == Standby is active False == Standby was disabled
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableLightSource.h>

  ```text
  void SetLightEnabled( bool isEnabled );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableSplitterSmart.h>

  ```text
  void SetSortRules( TArray< FSplitterSortRule > newSortRules );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildablePriorityPowerSwitch.h>

  ```text
  void SetPriority( int32 priority );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGTrain.h>

  ```text
  Enable/disable the autopilot on a train, does nothing if enabled/disabled twice, must be called server.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Content/FactoryGame/Buildable/Factory/CA_SplitterProgrammable/Build_ConveyorAttachmentSplitterProgrammable.uasset>

  ```text
  /Script/FactoryGame.FGBuildableSplitterSmart
  ```

**Skeptic (source-accuracy): confirmed**

Note: Every setter was seen in the pinned headers (same as master) and sits in a public section of a FACTORYGAME_API class: AFGBuildableCircuitSwitch::SetSwitchOn, AFGBuildableFactory::SetIsProductionPaused (virtual), AFGBuildableLightSource::SetLightEnabled, AFGBuildableLightsControlPanel::SetLightEnabled, AFGBuildableSplitterSmart::AddSortRule/SetSortRuleAt/SetSortRules, AFGBuildablePriorityPowerSwitch::SetPriority, AFGTrain::SetSelfDrivingEnabled. Programmable splitter .uasset strings (extracted by me) include `/Script/FactoryGame.FGBuildableSplitterSmart` and `Default__FGBuildableSplitterSmart`, with no other native FGBuildable* class. Add: both SetLightEnabled overloads are BlueprintAuthorityOnly and the train setter says 'must be called server', so all actuators are server-side only. The standby link is still inferred: the header only has a 'standby is toggled' comment on mOnHasStandbyChanged and does not tie it to SetIsProductionPaused.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableSplitterSmart.h>

### C4 (verified-primary)

Doors have getters only: AFGBuildableDoor::GetCurrentDoorConfiguration() is public, but mDoorConfiguration (EDoorConfiguration: Auto, Closed, Open) and OnRep_DoorConfiguration are private or protected with no public setter. Driving doors needs an AccessTransformer (Accessor or Friend) plus manual replication handling. Classify doors as 'hard / later'.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableDoor.h>

  ```text
  EDoorConfiguration GetCurrentDoorConfiguration() const { return mDoorConfiguration; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGBuildableDoor.h>

  ```text
  EDoorConfiguration mDoorConfiguration;
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/AccessTransformers.html>

  ```text
  Accessor=(Class="AFGSomeClass", Property="mSomeProperty")
  ```

### C5 (verified-primary)

The SDK's Private/*.cpp files are auto-generated empty stubs, so the side effects, authority checks and threading rules of setters such as SetSwitchOn cannot be verified from source. Only declarations and header comments are evidence. Behaviour must be proven in a real LinuxServer + client test. (Exception: a few files contain real code, e.g. FGBuildable.cpp.)

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Private/Buildables/FGBuildableCircuitSwitch.cpp>

  ```text
  // This file has been automatically generated by the Unreal Header Implementation tool
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Private/Buildables/FGBuildableCircuitSwitch.cpp>

  ```text
  void AFGBuildableCircuitSwitch::SetSwitchOn(bool isSwitchOn){ }
  ```

### C6 (secondary)

BlueprintAuthorityOnly (used on SetLightEnabled and SetLightControlData) only gates Blueprint callers. C++ callers must check HasAuthority() themselves. Actuator calls should therefore be made only from the server-side subsystem.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableLightSource.h>

  ```text
  UFUNCTION( BlueprintCallable, BlueprintAuthorityOnly, Category = "FactoryGame|Buildable|Light" )
  ```
- <https://unrealcommunity.wiki/a-quick-reference-to-the-ufunction-macro-227ae2>

  ```text
  This function will only execute from Blueprint code if running on a machine with network authority (a server, dedicated server, or single-player game).
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

  ```text
  To check if your code has authority, use Actor 'Has Authority'.
  ```

### C7 (verified-primary, decision-driving)

Sensing must happen on the server. Inventory contents replicate to a client only while the building's UI is open. Circuit production, consumption and capacity totals in FPowerCircuitStats are NotReplicated, while HasPower and the fuse flag are replicated. Client-side sensing is therefore unreliable.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/ConditionalPropertyReplication.html>

  ```text
  the inventory will ONLY be replicated to the client if it has the buildable UI open
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/ConditionalPropertyReplication.html>

  ```text
  The inventory is always available now on the client as an object, but the contents are only replicated when needed.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerCircuit.h>

  ```text
  /** How much power is produced. */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerCircuit.h>

  ```text
  /** true if the power production is this circuit is above zero, false otherwise. */
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Do sensing on the server. UFGPowerCircuit unconditionally replicates HasPower, the fuse flag, maximum consumption, boost and battery sums, but not the per-circuit produced, consumed and capacity values. The FPowerCircuitStats struct (mPowerStats) is conditionally replicated (FGReplicated, with custom NetSerialize). Inventories of buildings that call SetReplicationRelevancyOwner replicate their contents only while the UI is open.

Note: The conclusion (sense on the server) holds, but the stated mechanism is misleading. The comments quoted in the claim exist and the per-member `NotReplicated` specifiers on FPowerCircuitStats are real. However, FPowerCircuitStats has `WithNetSerializer = true` (custom NetSerialize, body stubbed in the SDK). `UFGPowerCircuit::mPowerStats` is declared `UPROPERTY(meta = (FGReplicated))`, which is conditional replication, so the stats struct IS replicated under some condition and the NotReplicated flags do not decide what goes over the wire. What is provably not replicated: UFGPowerCircuit's mPowerProductionCapacity, mPowerProduced and mPowerConsumed (plain UPROPERTY(), absent from the DOREPLIFETIME list in the SDK FGPowerCircuit.cpp). Always replicated: mBoostProduced, mMaximumPowerConsumption, mHasPower, battery sums and times, mIsFuseTriggered, mProductionBoostFactor. The docs quotes match the curl text, but the docs say the inventory is replicated only on UI open 'if you follow' SetReplicationRelevancyOwner; without that call it is ALWAYS replicated. This is a pattern, not a guarantee for every vanilla building. The page does not mention power circuits.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerCircuit.h>

### C8 (verified-primary)

Machine status is readable through public const API: AFGBuildableFactory::IsProducing(), IsProductionPaused(), GetProductionIndicatorStatus() (EProductionStatus: IS_NONE, IS_PRODUCING, IS_PRODUCING_WITH_CRYSTAL, IS_STANDBY, IS_ERROR; valid on server and client), HasPower(), GetProductivity(). Public delegates allow event-driven sensing: mOnHasPowerChanged, mOnHasProductionChanged and mOnHasStandbyChanged (FBuildingStateChanged(bool)). The mapping of IS_STANDBY versus the manual pause flag in the vanilla UI is inferred, not proven.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  bool IsProducing() const
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  /** @return The status to display on the production indicator, valid on server and client. */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  FBuildingStateChanged mOnHasStandbyChanged;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableFactory.h>

  ```text
  /** The player is able to toggle if production should be paused or not */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  IS_PRODUCING_WITH_CRYSTAL, //We have a crystal in the potential slot and are producing
  ```

### C9 (verified-primary)

Storage contents and fullness come from UFGInventoryComponent, reached via AFGBuildableStorage::GetStorageInventory(). Public queries are GetNumItems(itemClass), HasItems, IsEmpty(), GetSizeLinear(), GetInventoryStacks and HasEnoughSpaceForStack(s). There is no single 'IsFull'; fullness must be derived (inference). HasEnoughSpaceForStacks is documented as slow, so poll it sparingly. Native delegates (OnItemAddedDelegate_Native / OnItemRemovedDelegate_Native) allow event-driven sensing. Remove() is 'Server only'.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableStorage.h>

  ```text
  FORCEINLINE class UFGInventoryComponent* GetStorageInventory() { return mStorageInventory; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGInventoryComponent.h>

  ```text
  int32 GetNumItems( TSubclassOf< UFGItemDescriptor > itemClass ) const;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGInventoryComponent.h>

  ```text
  @note This can be slow for large inventories and item quantities as it need to check every grid location, try stacking and consider stacking limits etc.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGInventoryComponent.h>

  ```text
  FOnItemAdded_Native OnItemAddedDelegate_Native;
  ```

### C10 (verified-primary)

Belt throughput is exposed by the vanilla AFGBuildableConveyorMonitor (GetCalculatedAverage() in items/min, GetConfidence()), but only for belts where the player has placed a monitor. AFGBuildableConveyorBase also exposes GetSpeed(), GetLength() and IsStalled(), all public. Counting items on a belt without a monitor would need a custom inline buildable or a hook (untested).

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableConveyorMonitor.h>

  ```text
  Attaches to conveyors to monitor their effective throughput
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableConveyorMonitor.h>

  ```text
  int32 GetCalculatedAverage() const { return mCalculatedItemsPerMinute; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableConveyorBase.h>

  ```text
  FORCEINLINE bool IsStalled() const { return mCanEverStall && mIsStalled && mItems.Num() != 0; }
  ```

### C11 (verified-primary, decision-driving)

Power sensing: per-circuit UFGPowerCircuit has public HasPower(), IsFuseTriggered(), GetPowerProductionCapacity(), GetMaximumPowerConsumption(), battery getters and GetStats(); delegates OnHasPowerChanged and OnFuseTriggeredDelegate are public. FPowerCircuitStats is refreshed only once per second, so stats-based sensors have about 1 s latency. FindCircuit() pointers are valid only for the current tick, and circuit IDs can change, so neither should be cached. Group-level totals (mConsumption, mBaseProduction) are private. FRM reads them via an AccessTransformer Friend entry. UFGPowerInfoComponent::SetTargetConsumption lets a mod building act as a load, with a one-frame delay.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerCircuit.h>

  ```text
  Stats for a power circuit, refreshed once every second. Implemented as a ring buffer, so that we don't have to re-replicate all the time
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerCircuit.h>

  ```text
  void GetStats( FPowerCircuitStats& out_stats ) const { out_stats = mPowerStats; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGCircuitSubsystem.h>

  ```text
  @note The returned pointer is only valid this tick or until a change is made to the circuitry by calling CreateCircuit, RemoveCircuit, ConnectComponents.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPowerInfoComponent.h>

  ```text
  @note This call is asynchronous and the result is available next frame.
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/Config/AccessTransformers.ini>

  ```text
  Friend=(Class="UFGPowerCircuitGroup", FriendClass="UPower")
  ```

**Skeptic (source-accuracy): confirmed**

Note: Seen in the pinned FGPowerCircuit.h (public section of UFGPowerCircuit): HasPower(), IsFuseTriggered() (non-const), GetPowerProductionCapacity(), GetMaximumPowerConsumption(), GetBatterySum* getters, GetStats(), and the public OnHasPowerChanged and OnFuseTriggeredDelegate. The stats comment says 'refreshed once every second' (StatIntervalTime is also a property). FindCircuit says 'only valid this tick or until a change is made to the circuitry'. SetTargetConsumption says 'asynchronous and the result is available next frame'. mConsumption and mBaseProduction are private in UFGPowerCircuitGroup. The FRM AccessTransformers.ini at 32fe64e (the latest commit on 2026-08-12) has the exact Friend line, and FRM Power.cpp reads PowerGroup->mBaseProduction and mConsumption. Caveats: 'circuit IDs can change' is only indirectly supported, by the MergeCircuits and RebuildCircuit declarations and the comment 'might split, remove or otherwise change the circuit'. mPowerProductionCapacity, the field behind GetPowerProductionCapacity(), is not replicated, so that getter is server-only in practice.

Evidence: <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/Config/AccessTransformers.ini>

### C12 (verified-primary)

Fluids, trains and signals are readable through public const API. Tanks: AFGBuildablePipeReservoir::GetFluidContent(), GetFluidContentMax() (m^3), GetFlowFill() and GetFlowDrain(). Trains: AFGTrain::GetTrainStatus() (parked, manual, self-driving, derailed), IsDocked() and GetDockingState(). Rail signals: AFGBuildableRailroadSignal::GetAspect().

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildablePipeReservoir.h>

  ```text
  float GetFluidContent() const;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGTrain.h>

  ```text
  FORCEINLINE ETrainStatus GetTrainStatus() const { return mTrainStatus; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableRailroadSignal.h>

  ```text
  ERailroadSignalAspect GetAspect() const { return mAspect; }
  ```

### C13 (verified-primary, decision-driving)

A mod's runtime state belongs in an AModSubsystem registered in UGameWorldModule::ModSubsystems. Lifecycle: spawned in the module's Construction phase. The actor is named '<ModRef>_<ClassName>', and an actor loaded from the save under that name is reused instead of spawning a new one. That is the save mechanism, provided the class also implements IFGSaveInterface and its ShouldSave returns true, since AFGSubsystem itself is only an AInfo. Replication: the AModSubsystem constructor sets bReplicates=false and bNetLoadOnClient=false. The default policy is SpawnOnServer (not spawned on clients); SpawnOnServer_Replicate calls SetReplicates(true) on the server; AFGSubsystem sets bAlwaysRelevant=true. Use subsystems from the game thread only. Open-source C++ precedents combine AModSubsystem with IFGSaveInterface: FicsIt-Networks (AFINSignalSubsystem) and Load Balancers.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp>

  ```text
  //Spawn new actor if we don't have one loaded from the game save
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Subsystem/ModSubsystem.cpp>

  ```text
  this->bReplicates = false;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp>

  ```text
  SpawnedSubsystem->SetReplicates(true);
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Private/FGSubsystem.cpp>

  ```text
  this->bAlwaysRelevant = true;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Subsystem/ModSubsystem.h>

  ```text
  Subsystem will be spawned on server and replicated to remote clients
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/Subsystems.html>

  ```text
  Make sure to only use subsystems from the game thread.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/Subsystems.html>

  ```text
  Subsystems allow you to implement custom game state information, for example, that you want to have saved to the save file
  ```
- <https://github.com/mklierman/SatisfactoryMods/blob/c154da4e9b7f74b0e03a4633aa4de42af452cc4d/LoadBalancers/Source/LoadBalancers/Public/LoadBalancers_Subsystem_CPP.h>

  ```text
  class LOADBALANCERS_API ALoadBalancers_Subsystem_CPP : public AModSubsystem, public IFGSaveInterface
  ```
- <https://github.com/Panakotta00/FicsIt-Networks/blob/789c32fdc857fc9971f7f73fbbf3789c64f637bc/Source/FicsItNetworksCircuit/Public/Signals/FINSignalSubsystem.h>

  ```text
  class FICSITNETWORKSCIRCUIT_API AFINSignalSubsystem : public AModSubsystem, public IFGSaveInterface {
  ```

**Skeptic (source-accuracy): confirmed**

Note: All the SML and SDK quotes match (pinned equals master): the name is built as '%s_%s' from OwnerModReference and class name; FindSubsystemActorByName is reused if found, else SpawnActor; SetReplicates(true) only for SpawnOnServer_Replicate; ShouldSpawnSubsystemWithPolicy returns NetMode != Client for both server policies; AModSubsystem() sets bNetLoadOnClient=false and bReplicates=false; AFGSubsystem() sets bAlwaysRelevant=true (and bReplicates=true, which the mod subsystem overrides); AFGSubsystem is `: public AInfo`. FGSaveInterface.h says 'If you want your actor to be saved, implement this!'. UGameWorldModule::ModSubsystems says 'registered automatically during construction phase'. Docs quotes confirmed verbatim in curl text: 'Make sure to only use subsystems from the game thread.', 'spawned early in the Construction phase', 'SpawnOnServer replication policy by default'. FicsIt-Networks AFINSignalSubsystem uses AModSubsystem + IFGSaveInterface with UPROPERTY(SaveGame) fields and ShouldSave/PreSave/PostLoad overrides, so it is a real data-saving precedent. The LoadBalancers class has the same inheritance and a ShouldSave override returning true, but its SaveGame properties are commented out, so it is a weak precedent for saved data. The `<ModRef>_<Class>` naming is from SML code, not the docs.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Private/Subsystem/SubsystemActorManager.cpp>

### C14 (verified-primary)

A C++-only mod (no .uasset) can have a root module: a C++ UGameWorldModule subclass with bRootModule=true set in its constructor. UGameWorldModule::ModSubsystems is declared there. The docs page ModModules lists 'Subsystems' under the Game Instance module, while the Subsystems page and the SDK say Game World Module, so the SDK header is authoritative. Buildings, meshes, recipes and schematics still need editor-authored assets (inference).

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/ModModules.html>

  ```text
  To make a C++-defined module a root module, set bRootModule = true; in its constructor.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Module/GameWorldModule.h>

  ```text
  /** Mod subsystem actors to be registered automatically during construction phase */
  ```

### C15 (verified-primary, decision-driving)

Client-to-server requests use a Remote Call Object (UFGRemoteCallObject subclass). It needs a Server RPC and a dummy replicated UPROPERTY. It is registered through UGameInstanceModule::RemoteCallObjects and fetched with AFGPlayerController::GetRemoteCallObjectOfClass; the SDK declares '...OfClass' while the docs text sometimes says '...ByClass', so trust the SDK. RCOs must not use Multicast. Server-to-all-clients broadcasts belong in a replicated mod subsystem. Vanilla itself uses Server/Reliable RPCs on RCOs (e.g. the light clipboard RCO). IFGUseableInterface::OnUse is documented as called on both server and client, so a lever or button can act directly with a HasAuthority() branch.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

  ```text
  Do Not Use Multicast in RCOs!
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

  ```text
  Replicated subsystems are a good place to implement multicast RPCs since they will be present on all connections.
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGPlayerController.h>

  ```text
  class UFGRemoteCallObject* GetRemoteCallObjectOfClass( TSubclassOf< UFGRemoteCallObject > inClass );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/Source/SML/Public/Module/GameInstanceModule.h>

  ```text
  /** Mod Remote Call Objects to be registered automatically during construction phase */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildableLightSource.h>

  ```text
  void Server_PasteSettingsBuildableLight( class AFGBuildableLightSource* buildableLight, FLightSourceControlData lightSourceControlData );
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGUseableInterface.h>

  ```text
  @note Called on both server and client.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

  ```text
  Keep in mind that dedicated servers have no player controller,
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: Client-to-server requests use an RCO (Server RPC plus a dummy replicated property, registered via UGameInstanceModule::RemoteCallObjects, fetched with GetRemoteCallObjectOfClass; the docs text says ...ByClass). RCOs must not use Multicast. Broadcasts to clients belong in a replicated mod subsystem. IFGUseableInterface::OnUse is documented as called on both server and client. Whether a HasAuthority() branch inside OnUse is enough for a modded lever in multiplayer is untested and needs a test.

Note: Verified: docs text says 'Do Not Use Multicast in RCOs!' and 'triggering one will softlock and crash client players'; replicated subsystems are the place for multicast; a dummy replicated UPROPERTY is required with GetLifetimeReplicatedProps; the docs text says 'GetRemoteCallObjectByClass' while the SDK declares 'GetRemoteCallObjectOfClass' (pinned FGPlayerController.h), so the claim's naming remark is right; UGameInstanceModule::RemoteCallObjects exists ('registered automatically during construction phase'); UFGLightSourceClipboardRCO has UFUNCTION(Server, Reliable) Server_PasteSettings* functions; FGUseableInterface.h has '@note Called on both server and client.' on OnUse; the docs say dedicated servers have no player controller. Not verified: the final inference that a lever or button 'can act directly with a HasAuthority() branch'. The header documents only where OnUse is called, and no source shows a client-initiated OnUse also runs on the server for a modded buildable.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html>

### C16 (verified-primary, decision-driving)

If the mod is removed from a save, its modded content simply vanishes; the save loads as base game. Saving again in that state makes the loss permanent. Changes the mod made to base-game objects (switch states, paused machines, splitter rules, lights) persist, because they are plain SaveGame properties of vanilla actors. A broken mod of this kind can in rare cases damage a save. So the mod's actuators leave vanilla state behind. Wires or poles that connect to removed modded buildings are not addressed by the docs.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/faq.html>

  ```text
  If you were to load up a save file without mods, all of the content added by those mods will simply vanish from the save.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/faq.html>

  ```text
  Note that if you create another save after loading without mods, the modded content will be lost from that point on.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/faq.html>

  ```text
  These effects can persist after the mod is uninstalled.
  ```

**Skeptic (source-accuracy): confirmed**

Note: The FAQ quotes match the curl text: 'all of the content added by those mods will simply vanish from the save', 'if you create another save after loading without mods, the modded content will be lost from that point on', 'These effects can persist after the mod is uninstalled', 'In exceedingly rare cases, a broken mod of this type could create a damaged save file'. The FAQ sentence about persistence concerns mods that configure base-game content in ways not normally possible and does not list switches, lights and so on. The better evidence is the SDK, where the vanilla state properties are SaveGame: mIsSwitchOn, mIsProductionPaused, mIsEnabled (light), mSortRules, mPriority, mIsSelfDrivingEnabled. The FAQ page has no mention of wires or poles (grep), so 'not addressed' is true for that page only. I did not search the other docs pages.

Evidence: <https://docs.ficsit.app/satisfactory-modding/latest/faq.html>

### C17 (verified-primary)

Save mechanics: only UPROPERTYs flagged SaveGame (and not SkipSerialization) are serialized, and soft references need manual conversion. IFGSaveInterface callbacks get saveVersion and gameVersion, which are the game's save and CL numbers, not the mod's, so the mod needs its own SaveGame version int. Unreal's tagged serialization leaves a newly added property at its default when loading an old save. The docs claim FGBuildable does not return true from ShouldSave, but the SDK's AFGBuildable::ShouldSave_Implementation returns !mIsLightweightTemporary && !mIsDismantled, so the doc is probably stale. Custom buildables still need their own SaveGame properties.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Savegame.html>

  ```text
  Not all attributes of a object get serialized, only UProperties marked with the SaveGame-Flag get actually serialized.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Savegame.html>

  ```text
  FGBuildable implements IFGSaveInterface already, but it doesn't return true in ShouldSave
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Private/Buildables/FGBuildable.cpp>

  ```text
  bool AFGBuildable::ShouldSave_Implementation() const { return !mIsLightweightTemporary && !mIsDismantled; }
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGSaveInterface.h>

  ```text
  @param gameVersion - the version of the game, is the CL of our internal Perforce server, will be a large number > 40k
  ```
- <https://dev.epicgames.com/documentation/unreal-engine/versioning-of-assets-and-packages-in-unreal-engine>

  ```text
  a newly-added field will be missing from the serialized data, but will hold its default value.
  ```

### C18 (secondary, decision-driving)

On a dedicated server the game-mode component overrides IsPaused() and carries an auto-pause flag. The official wiki describes the Auto Pause server setting as pausing the game when no players are connected. While paused, a mod's subsystem tick and the factory simulation do not advance, so clocks, latches and timers freeze. Whether the owner's server has Auto Pause on is unknown. The pause mechanism's details are inferred, because the SDK implementation is a stub.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryDedicatedServer/Public/FGGameModeDSComponent.h>

  ```text
  /** True if we want to auto pause when there are no players on the server */
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryDedicatedServer/Public/FGGameModeDSComponent.h>

  ```text
  virtual bool IsPaused(bool bGameModePaused) const override;
  ```
- <https://satisfactory.wiki.gg/wiki/Dedicated_servers>

  ```text
  Whether the game should automatically pause when no players are connected.
  ```

**Skeptic (source-accuracy): partially-correct**

Corrected statement: The dedicated server has an Auto Pause option (pause when no players are connected), implemented via UFGGameModeDSComponent::IsPaused (primary: SDK header, game localization, official wiki). That clocks, latches and timers freeze while paused is plausible but unverified. Test it on the actual server before relying on it.

Note: Primary evidence for the setting's existence: the SDK header has `uint8 mAutoPause : 1;` with the comment 'True if we want to auto pause when there are no players on the server', `mAllowedToAutoPause`, and `IsPaused(bool bGameModePaused) const override`. FGGameMode.h says 'Allows the dedicated server to determine whenever the game mode in question should currently be paused' and AFGGameMode overrides IsPaused(). The SDK Content localization Options_Data.csv has 'When the Server should automatically be paused when no players are connected.' The wiki page satisfactory.wiki.gg/wiki/Dedicated_servers is titled 'Official Satisfactory Wiki' and has the quoted sentence. The .cpp implementations are stubs, so the mechanism is unknown. NOT verified by any primary source: that a mod subsystem's tick and the factory simulation stop while paused. I looked for it and found only hosting guides, which are secondary. I could not retrieve Epic's bTickEvenWhenPaused semantics. 'FG.DSAutoPause' appeared only in third-party API clients, so treat it as secondary. The owner's Auto Pause value is not known.

Evidence: <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryDedicatedServer/Public/FGGameModeDSComponent.h>

### C19 (secondary)

The dedicated server's simulation tick rate targets about 30 per second, which caps the useful logic-step rate. The wolveix image documents MAXTICKRATE default 30. The dashboard code reads the live value as 'averageTickRate' from QueryServerState, and this repo's the owner's notes records 29.93 for a server (belonging to another user, not the owner's). The owner's own live value should be read from the existing poller before fixing a logic step.

Evidence:

- <https://github.com/wolveix/satisfactory-server>

  ```text
  set the maximum sim tick rate for your server
  ```
- <[local file of the owner, not published]>

  ```text
  'tickRate' => (float) ($state['averageTickRate'] ?? 0.0),
  ```
- <[local file of the owner, not published]>

  ```text
  tick rate 29.93, `player_count=0`
  ```

### C20 (verified-primary)

Significance-based tick throttling applies only to local players and is not run on dedicated servers. On a host-and-play host, building PrimaryActorTick can be throttled by distance, while the factory tick stays enabled. Logic must therefore not depend on AActor::Tick of buildables. It should live in a subsystem or tick handler.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FGSignificanceInterface.h>

  ```text
  Significance is handled only for local players and does not run on dedicated servers
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  FactoryTick is always enabled (as long as bCanEverTick is true) so that the factory part of buildings always can simulate.
  ```

### C21 (unverified)

Setting a per-building Factory tick interval is probably not usable. AFGBuildable::mFactoryTickFunction is an FFactoryTickFunction (an FTickFunction, which has a TickInterval) whose Target is the AFGBuildableSubsystem, so scheduling appears to be driven by the subsystem rather than per building. Whether TickInterval is honoured is unverified. For AActor ticks, Unreal documents SetActorTickInterval (does not enable a disabled tick, takes effect next tick; an interval <= 0 means every frame). For gates, a mod subsystem's own fixed-step accumulator is the safer design.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FactoryTick.h>

  ```text
  struct FACTORYGAME_API FFactoryTickFunction : public FTickFunction
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/FactoryTick.h>

  ```text
  class AFGBuildableSubsystem* Target;
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Source/FactoryGame/Public/Buildables/FGBuildable.h>

  ```text
  Controls if we should receive Factory_Tick and how frequent.
  ```
- <https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/AActor/SetActorTickInterval>

  ```text
  Sets the tick interval of this actor's primary tick function.
  ```
- <https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/ActorTickFunction>

  ```text
  The time in seconds between executions of this tick function. If <= 0 then it will tick every frame.
  ```

### C22 (unverified)

No official or measured cost figure exists for simulating thousands of gate updates per second in Satisfactory or per ticking Unreal actor. I searched the Satisfactory docs, the Coffee Stain wiki and Epic's docs, and an Epic performance blog post (returned HTTP 403). What exists is qualitative: Unreal ticks every actor once per frame unless an interval is set, and the Satisfactory simulation's worker threads run on ticks while the game loop is single-threaded. Any budget must be measured. Unreal-free gate-engine code can be benchmarked here; the Unreal glue only in an Unreal build with profiling.

Evidence:

- <https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-ticking-in-unreal-engine>

  ```text
  Actors and components are ticked once per frame, unless a minimum ticking interval is specified.
  ```
- <https://satisfactory.wiki.gg/wiki/Dedicated_servers>

  ```text
  The game loop is still fundamentally single threaded though and all multi threaded work is happening on ticks.
  ```

### C23 (secondary)

A working precedent for calling the actuators from C++ on a dedicated server: Ficsit Remote Monitoring (installed on the owner's server as 1.5.3) calls AFGBuildableCircuitSwitch::SetSwitchOn and AFGBuildableFactory::SetIsProductionPaused in its write endpoints. Treat this as secondary evidence of behaviour because it is third-party code and its call context (thread, authority) is not shown.

Evidence:

- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/Source/FicsitRemoteMonitoring/Private/Endpoints/Factory/Power.cpp>

  ```text
  PowerSwitch->SetSwitchOn(bStatus);
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/Source/FicsitRemoteMonitoring/Private/Endpoints/World/Communication.cpp>

  ```text
  FactoryBuilding->SetIsProductionPaused(!PowerState);
  ```
- <https://github.com/porisius/FicsitRemoteMonitoring/blob/32fe64e0c22389a944c27222ef6c881f5e207072/docs/modules/ROOT/pages/json/Write/setEnabled.adoc>

  ```text
  Allows to enable or disable a Constructor, Assembler, Manufacturer, Power Generators, (Priority) Power Switch.
  ```

### C24 (verified-primary)

Pin check: SML master HEAD is exactly the owner's pinned SDK commit d2162c99 ('Update headers to CL502094'), so 'compare with main' shows no difference. It is 4 commits ahead of the v3.12.0 tag, and SML.uplugin still reads 3.12.0. The SML runtime sources are unchanged from the release in those commits; the diff touched game headers and content.

Evidence:

- <https://github.com/satisfactorymodding/SatisfactoryModLoader/commit/d2162c999bbaa9650594ee9ad6e6e06190306009>

  ```text
  Update headers to CL502094
  ```
- <https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/d2162c999bbaa9650594ee9ad6e6e06190306009/Mods/SML/SML.uplugin>

  ```text
  "VersionName": "3.12.0",
  ```

### C25 (verified-primary)

Network compatibility: the .uplugin field RequiredOnRemote defaults to true, meaning the mod must be installed on both sides with a matching RemoteVersionRange. SML 3.9+ also makes clients refuse servers missing mods the client has. A mod that adds buildables needs client assets, so it will be required on both server and clients. A server-only logic add-on could set RequiredOnRemote false.

Evidence:

- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/ReleaseMod.html>

  ```text
  Controls if the mod is required to be on both sides in multiplayer.
  ```
- <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/ReleaseMod.html>

  ```text
  This field is optional, and defaults to `true`.
  ```

## Open questions

- Authority, threading and side effects of SetSwitchOn, SetIsProductionPaused, SetSortRules and SetLightEnabled when called from a mod subsystem or tick handler cannot be verified from the SDK (the .cpp files are stubs); needs a LinuxServer plus remote-client test.
- Whether a mod can link and register an IFGFactoryTickHandlerInterface implementation (UINTERFACE MinimalAPI), at what point in the frame the handlers run relative to the power-circuit tick, and whether they run once per frame or per factory sub-step.
- Exact in-game meaning of 'standby' versus IS_STANDBY versus mIsProductionPaused. Header comments imply standby == mIsProductionPaused, but the vanilla UI path is not visible.
- Whether per-building FTickFunction.TickInterval on mFactoryTickFunction is honoured (it targets the buildable subsystem). Likely not, so use own subsystem accumulators instead.
- Per-gate and per-frame cost: nothing official or measured was found. Needs a benchmark (UE-free engine with g++ here; in-engine measurement with Unreal Insights or stat commands on the owner's PC).
- What the owner's own server reports as averageTickRate, and whether Auto Pause is enabled on it. Read it from the existing dashboard poller; do not rely on the 29.93 value in the owner's notes, which belongs to another user's server.
- Behaviour of vanilla wires or poles attached to modded buildings after the mod is removed: the docs do not say.
- Docs/SDK discrepancies to resolve in a real build: GetRemoteCallObjectByClass versus GetRemoteCallObjectOfClass; the Savegame page's FGBuildable ShouldSave claim versus the SDK's override; ModModules lists Subsystems under the Game Instance module while the SDK puts ModSubsystems on UGameWorldModule.
- Counting items on a belt without a placed vanilla throughput monitor (custom inline buildable via factory connections versus a native hook) is untested.
- Doors: whether an AccessTransformer on mDoorConfiguration plus manual OnRep handling replicates correctly; not investigated beyond the header.

## Recommendations

- Split the code in two: a Unreal-free C++ gate/netlist engine (pure data, fixed-step evaluation, serialisation to POD arrays) that is unit-tested and benchmarked on this VPS with g++/make, and a thin Unreal adapter. Only the adapter needs UHT, generated headers and the SDK.
- Adapter design: one AModSubsystem + IFGSaveInterface (ShouldSave true, own SaveGame version int) on SpawnOnServer, or SpawnOnServer_Replicate if clients need live state, implementing IFGFactoryTickHandlerInterface. PreFactoryTick reads sensors, runs the engine, then writes actuators (or uses PostFactoryTick). Use a fixed logic step with an accumulator and a cap on steps per frame.
- MVP scope that needs no hooks: sensors for power circuit (HasPower, fuse, 1 Hz stats), machine state, storage item counts and fullness (server-side), and fluid levels; actuators for power switch, priority switch, standby, lights, smart/programmable splitter rules. Defer doors, belt-item counting and train control.
- Keep every actuator write on the server and behind a HasAuthority() check. Route player input through IFGUseableInterface::OnUse (guarded) or an RCO with a Server RPC, never Multicast.
- Persist nothing but the netlist and an explicit version int. Document in the mod description that vanilla state the mod changes (switches, standby, sort rules) stays after uninstall, and that saving without the mod loses modded content.
- Plan the verification matrix on the owner's PC: Alpakit Windows + LinuxServer builds, dedicated server plus a remote client, a save/reload cycle, a remove-mod-and-reload cycle, an Auto Pause cycle, and a measured tick-rate and step-time profile. Do not claim anything about setter authority until this is done.
- What can be authored and verified as plain text on the VPS: the gate-engine library and tests, .uplugin/Build.cs/AccessTransformers.ini, docs and the website. What needs the Unreal editor: any .uasset (buildable descriptors, meshes, materials, recipes, schematics, widgets, icons), UHT-generated code and packaging (inference from the docs; the SDK ships .generated.h only via UHT).
- Wrap any SUBSCRIBE_METHOD use in `if (!WITH_EDITOR)`; the docs warn that hooks at editor time are unpredictable.
- Consider FRM only as a debugging aid on the existing server; keep the mod self-contained rather than depending on FRM's HTTP API.

