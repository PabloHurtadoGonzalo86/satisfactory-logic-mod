// The two native root modules. SML discovers a native UGameWorldModule or UGameInstanceModule subclass whose CDO has
// bRootModule set (FPluginModuleLoader::FindRootModulesOfType); each mod may have exactly one root module per type,
// a second one is a Fatal log. They carry the registrations, so spike 1 needs no Blueprint asset and no
// GameFeatureData asset.

#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "Module/GameWorldModule.h"
#include "SlgSpikeModules.generated.h"

/** World module: registers the mod subsystem (S2, S3, S6) and the /slgspike chat command (S4). */
UCLASS()
class SLGSPIKE_API USlgSpikeWorldModule : public UGameWorldModule
{
	GENERATED_BODY()
public:
	USlgSpikeWorldModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};

/** Game instance module: registers the remote call object (S5). */
UCLASS()
class SLGSPIKE_API USlgSpikeInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()
public:
	USlgSpikeInstanceModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
