#include "SlgSpikeRemoteCall.h"

#include "SlgSpikeLog.h"
#include "FGPlayerController.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

USlgSpikeRemoteCallObject::USlgSpikeRemoteCallObject()
{
	this->mForceNetField_USlgSpikeRemoteCallObject = false;
}

void USlgSpikeRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USlgSpikeRemoteCallObject, mForceNetField_USlgSpikeRemoteCallObject);
}

bool USlgSpikeRemoteCallObject::Server_Ping_Validate(int32 Nonce, const FString& Text)
{
	return Text.Len() < 256;
}

void USlgSpikeRemoteCallObject::Server_Ping_Implementation(int32 Nonce, const FString& Text)
{
	++PingsReceived;

	UWorld* World = GetWorld();
	AFGPlayerController* OwnerController = GetOwnerPlayerController();

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S5 event=Server_Ping nonce=%d text=%s netmode=%s owner_controller=%s pings_received=%d"),
		Nonce, *Text, *SlgSpikeUtil::NetModeToString(World),
		OwnerController ? *OwnerController->GetName() : TEXT("none"), PingsReceived);

	Client_Pong(Nonce, PingsReceived);
}

void USlgSpikeRemoteCallObject::Client_Pong_Implementation(int32 Nonce, int32 ServerPingsReceived)
{
	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S5 event=Client_Pong nonce=%d server_pings_received=%d netmode=%s"),
		Nonce, ServerPingsReceived, *SlgSpikeUtil::NetModeToString(GetWorld()));
}

namespace
{
	int32 GClientPingNonce = 0;

	void HandleClientCommand(AFGPlayerController* Controller, const FString& ChatMessage)
	{
		TArray<FString> Parts;
		ChatMessage.TrimStartAndEnd().ParseIntoArray(Parts, TEXT(" "));

		// Parts[0] is "!slg". Only the first word after the verb is kept as the ping text.
		const FString Verb = Parts.Num() > 1 ? Parts[1].ToLower() : FString();
		const FString Text = Parts.Num() > 2 ? Parts[2] : FString();

		USlgSpikeRemoteCallObject* Rco = Controller->GetRemoteCallObjectOfClass<USlgSpikeRemoteCallObject>();

		if (Verb == TEXT("status"))
		{
			UE_LOG(LogSlgSpike, Display,
				TEXT("[SLGSPIKE] S5 event=ClientStatus rco_available=%d netmode=%s"),
				Rco != nullptr ? 1 : 0, *SlgSpikeUtil::NetModeToString(Controller->GetWorld()));
			return;
		}

		if (Verb == TEXT("ping"))
		{
			if (Rco == nullptr)
			{
				UE_LOG(LogSlgSpike, Warning,
					TEXT("[SLGSPIKE] S5 event=Client_SendPing result=FAIL reason=rco_not_available netmode=%s"),
					*SlgSpikeUtil::NetModeToString(Controller->GetWorld()));
				return;
			}
			++GClientPingNonce;
			UE_LOG(LogSlgSpike, Display,
				TEXT("[SLGSPIKE] S5 event=Client_SendPing nonce=%d netmode=%s"),
				GClientPingNonce, *SlgSpikeUtil::NetModeToString(Controller->GetWorld()));
			// A failed _Validate drops the connection, so the text is cut here to stay below its 256 character limit.
			Rco->Server_Ping(GClientPingNonce, Text.Left(200));
			return;
		}

		UE_LOG(LogSlgSpike, Display, TEXT("[SLGSPIKE] S5 event=ClientUsage usage=!slg ping [text] | !slg status"));
	}
}

void SlgSpikeUtil::AttachClientChatHook(AFGPlayerController* Controller)
{
	if (Controller == nullptr)
	{
		return;
	}

	// SML's own chat patch captures the raw controller pointer; a weak pointer costs nothing and survives a travel.
	const TWeakObjectPtr<AFGPlayerController> WeakController(Controller);

	Controller->ChatMessageEntered.AddLambda([WeakController](const FString& ChatMessage, bool& bShouldSendMessage)
	{
		if (!ChatMessage.StartsWith(TEXT("!slg")))
		{
			return;
		}

		// Never broadcast the test message to the other players.
		bShouldSendMessage = false;

		AFGPlayerController* PlayerController = WeakController.Get();
		if (PlayerController == nullptr)
		{
			return;
		}
		HandleClientCommand(PlayerController, ChatMessage);
	});

	UE_LOG(LogSlgSpike, Display,
		TEXT("[SLGSPIKE] S5 event=ChatHookAttached controller=%s netmode=%s"),
		*Controller->GetName(), *SlgSpikeUtil::NetModeToString(Controller->GetWorld()));
}
