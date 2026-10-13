#pragma once

#include "CoreMinimal.h"

// Every line this plugin writes starts with "[SLGSPIKE]" followed by a check id (S1 to S7) and key=value pairs, so the
// logs the owner sends back can be filtered with one search and parsed mechanically. Lines use the Display verbosity
// so they also reach the dedicated server console.
DECLARE_LOG_CATEGORY_EXTERN(LogSlgSpike, Log, All);

class UWorld;

namespace SlgSpikeUtil
{
	/** Version of this spike plugin, printed in the S1 line so a log can be matched to a build. */
	inline const TCHAR* BuildTag() { return TEXT("0.1.0"); }

	/** Seconds since the first call, from FDateTime::UtcNow(). Not monotonic: only differences inside one process are meaningful. */
	double WallSeconds();

	/** "dedicated", "client" or "other(<n>)"; the two names come from the SML subsystem manager's own checks. */
	FString NetModeToString(const UWorld* World);

	/** Number of player controllers the server knows about, counted like SML's own /list command. */
	int32 CountPlayers(UWorld* World);
}
