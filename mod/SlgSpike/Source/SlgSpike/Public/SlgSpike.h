// Spike 1 plugin module. See mod/SlgSpike/README.md and docs/spike/01-objetivos-y-criterios.md.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSlgSpikeModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Client only: lets the module attach its "!slg" chat hook to every player controller that begins play. */
	FDelegateHandle ControllerBegunPlayHandle;
};
