// The control surface of the spike: one SML chat command, "/slgspike". SML runs chat commands on the server (the
// client sends the line through SML's own remote call object), so every subcommand here executes server side.
// Subcommands that touch vanilla buildables only queue a request; the tick handler runs it in PreFactoryTick.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "SlgSpikeChatCommand.generated.h"

UCLASS()
class SLGSPIKE_API ASlgSpikeChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	ASlgSpikeChatCommand();

	virtual EExecutionStatus ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};
