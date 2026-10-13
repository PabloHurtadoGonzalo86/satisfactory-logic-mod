// S5: Remote Call Object round trip. A client sends a Server RPC, the server logs it, and a Client RPC comes back.
// No multicast anywhere: RCOs exist only on the connection that owns them (docs: Multiplayer, "Do Not Use Multicast in RCOs").

#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "SlgSpikeRemoteCall.generated.h"

class AFGPlayerController;

UCLASS(NotBlueprintable)
class SLGSPIKE_API USlgSpikeRemoteCallObject : public UFGRemoteCallObject
{
	GENERATED_BODY()
public:
	USlgSpikeRemoteCallObject();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Client to server. The server logs it and answers with Client_Pong on the same RCO. */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Ping(int32 Nonce, const FString& Text);

	/** Server to the one client that owns this RCO. Logs the nonce and the server's running count. */
	UFUNCTION(Client, Reliable)
	void Client_Pong(int32 Nonce, int32 ServerPingsReceived);

	/** Pings this RCO has received on the server since it was created (plain member, not replicated). */
	int32 GetPingsReceived() const { return PingsReceived; }

private:
	/** An RCO without at least one replicated property does not work; this one only has to exist. */
	UPROPERTY(Replicated)
	bool mForceNetField_USlgSpikeRemoteCallObject;

	int32 PingsReceived = 0;
};

namespace SlgSpikeUtil
{
	/**
	 * Client only. Adds a ChatMessageEntered listener to the controller that handles messages starting with "!slg".
	 * The "!" prefix keeps these messages away from SML, which consumes everything that starts with "/".
	 */
	void AttachClientChatHook(AFGPlayerController* Controller);
}
