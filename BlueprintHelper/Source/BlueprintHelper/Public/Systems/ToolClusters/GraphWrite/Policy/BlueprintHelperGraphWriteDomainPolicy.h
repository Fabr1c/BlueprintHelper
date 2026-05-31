// BlueprintHelper GraphWrite domain policy.

#pragma once

#include "CoreMinimal.h"

enum class EBlueprintHelperGraphWriteTargetDomain : uint8
{
	BlueprintHelperOwned,
	ExternalUserAuthored
};

struct FBlueprintHelperGraphWriteDomainPolicyRequest
{
	EBlueprintHelperGraphWriteTargetDomain Domain = EBlueprintHelperGraphWriteTargetDomain::BlueprintHelperOwned;
	FString Strategy;
	FString OwnershipScope;
	bool bAllowModifyUserNodes = false;
	TArray<FString> AllowedExternalMutations;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteDomainPolicy
{
public:
	static bool ValidateOwnedRequest(
		const FBlueprintHelperGraphWriteDomainPolicyRequest& Request,
		FString& OutError);

	static bool ValidateExternalRequest(
		const FBlueprintHelperGraphWriteDomainPolicyRequest& Request,
		FString& OutError);
};
