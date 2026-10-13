#include "SlgSpikeChatCommand.h"

#include "SlgSpikeLog.h"
#include "SlgSpikeRemoteCall.h"
#include "SlgSpikeSubsystem.h"
#include "SlgSpikeTickHandler.h"
#include "Command/CommandSender.h"
#include "FGPlayerController.h"
#include "Engine/World.h"

namespace
{
	/** Writes the line to the log and sends it to the player that typed the command. */
	void Reply(UCommandSender* Sender, const FString& Message)
	{
		UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] CHAT reply=%s"), *Message);
		if (Sender != nullptr)
		{
			Sender->SendChatMessage(Message);
		}
	}

	const TCHAR* UsageText()
	{
		return TEXT("/slgspike status | ticks [n] | burst [n] | reset | hb on|off | rco | switch list | switch <name or #idx> read|on|off|toggle");
	}
}

ASlgSpikeChatCommand::ASlgSpikeChatCommand()
{
	CommandName = TEXT("slgspike");
	Usage = NSLOCTEXT("SlgSpike", "ChatCommand.Usage", "/slgspike status | ticks [n] | burst [n] | reset | hb on|off | rco | switch list | switch <name or #idx> read|on|off|toggle");
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = false;
}

EExecutionStatus ASlgSpikeChatCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	// The raw arguments go to the log first: SML's own help command indexes Arguments[1] for the first argument,
	// while RunChatCommand removes the command name before the call, so the real layout is checked here.
	UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] CHAT event=Execute label=%s argc=%d args=%s sender=%s"),
		*Label, Arguments.Num(), *FString::Join(Arguments, TEXT("|")),
		Sender != nullptr ? *Sender->GetSenderName() : TEXT("none"));

	TArray<FString> Args = Arguments;
	if (Args.Num() > 0 && Args[0].ToLower() == Label.ToLower())
	{
		Args.RemoveAt(0);
	}

	UWorld* World = GetWorld();
	ASlgSpikeSubsystem* Spike = ASlgSpikeSubsystem::Get(World);
	if (Spike == nullptr)
	{
		Reply(Sender, TEXT("S2 FAIL: the SlgSpike subsystem does not exist in this world."));
		return EExecutionStatus::UNCOMPLETED;
	}
	USlgSpikeTickHandler* Handler = Spike->GetTickHandler();

	const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString();

	if (Verb == TEXT("status"))
	{
		Reply(Sender, FString::Printf(TEXT("S1 plugin_build=%s netmode=%s"), SlgSpikeUtil::BuildTag(),
			*SlgSpikeUtil::NetModeToString(World)));
		Reply(Sender, FString::Printf(TEXT("S2 %s"), *Spike->BuildStatusLine()));
		Reply(Sender, Handler != nullptr
			? FString::Printf(TEXT("S3 %s"), *Handler->BuildStatusLine())
			: FString(TEXT("S3 FAIL: tick handler not registered")));
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("hb"))
	{
		const bool bOn = Args.Num() > 1 && Args[1].ToLower() == TEXT("on");
		Spike->SetHeartbeatEnabled(bOn);
		Reply(Sender, FString::Printf(TEXT("S6 heartbeat=%d"), bOn ? 1 : 0));
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("rco"))
	{
		// GetPlayer() is a checkf(false) on every sender that is not a player (CommandSender.cpp), so ask first.
		if (Sender == nullptr || !Sender->IsPlayerSender())
		{
			Reply(Sender, TEXT("S5 server_side rco_available=0 reason=sender_is_not_a_player"));
			return EExecutionStatus::UNCOMPLETED;
		}
		AFGPlayerController* Player = Sender->GetPlayer();
		USlgSpikeRemoteCallObject* Rco = Player != nullptr ? Player->GetRemoteCallObjectOfClass<USlgSpikeRemoteCallObject>() : nullptr;
		Reply(Sender, FString::Printf(TEXT("S5 server_side rco_available=%d pings_received=%d"), Rco != nullptr ? 1 : 0,
			Rco != nullptr ? Rco->GetPingsReceived() : -1));
		return EExecutionStatus::COMPLETED;
	}

	// Everything below needs the handler.
	if (Handler == nullptr)
	{
		Reply(Sender, TEXT("S3 FAIL: tick handler not registered, cannot run this subcommand."));
		return EExecutionStatus::UNCOMPLETED;
	}

	if (Verb == TEXT("burst"))
	{
		const int32 Requested = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 60;
		const int32 Calls = FMath::Clamp(Requested > 0 ? Requested : 60, 1, 2000);
		Handler->StartBurst(Calls);
		Reply(Sender, FString::Printf(TEXT("S3 per-call logging for the next %d calls (pre and post)"), Calls));
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("ticks"))
	{
		const int32 Requested = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 40;
		const int32 Count = FMath::Clamp(Requested > 0 ? Requested : 40, 1, 512);
		Handler->DumpRing(Count);
		Reply(Sender, FString::Printf(TEXT("S3 dumped up to %d recent calls to the log"), Count));
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("reset"))
	{
		Handler->ResetStats();
		Reply(Sender, TEXT("S3 statistics reset"));
		return EExecutionStatus::COMPLETED;
	}

	if (Verb == TEXT("switch"))
	{
		if (Args.Num() < 2)
		{
			Reply(Sender, UsageText());
			return EExecutionStatus::BAD_ARGUMENTS;
		}

		const FString Second = Args[1].ToLower();
		if (Second == TEXT("list"))
		{
			Handler->EnqueueSwitchRequest(ESlgSwitchAction::List, FString());
			Reply(Sender, TEXT("S4 list queued; the result is in the log on the next factory tick"));
			return EExecutionStatus::COMPLETED;
		}

		if (Args.Num() < 3)
		{
			Reply(Sender, UsageText());
			return EExecutionStatus::BAD_ARGUMENTS;
		}

		const FString ActionText = Args[2].ToLower();
		ESlgSwitchAction Action = ESlgSwitchAction::Read;
		if (ActionText == TEXT("on"))
		{
			Action = ESlgSwitchAction::On;
		}
		else if (ActionText == TEXT("off"))
		{
			Action = ESlgSwitchAction::Off;
		}
		else if (ActionText == TEXT("toggle"))
		{
			Action = ESlgSwitchAction::Toggle;
		}
		else if (ActionText != TEXT("read"))
		{
			Reply(Sender, UsageText());
			return EExecutionStatus::BAD_ARGUMENTS;
		}

		Handler->EnqueueSwitchRequest(Action, Args[1]);
		Reply(Sender, FString::Printf(TEXT("S4 %s queued for '%s'; the result is in the log on the next factory tick"),
			*ActionText, *Args[1]));
		return EExecutionStatus::COMPLETED;
	}

	Reply(Sender, UsageText());
	return EExecutionStatus::BAD_ARGUMENTS;
}
