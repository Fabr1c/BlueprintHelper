// BlueprintHelper GraphWrite classification utilities.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperLogicReadTypes.h"

class FJsonObject;

class FBlueprintHelperGraphWriteClassificationUtils
{
public:
	static FString NormalizeToken(const FString& InValue);
	static FString NormalizeNodeTypeName(const FString& InValue);
	static FString ClassifyLogicNode(const TSharedPtr<FJsonObject>& NodeObject, const FString& RawType);
	static FString NormalizeExplicitLinkKind(const FString& RawKind);
	static bool IsBlueprintHelperOwnedNode(const TSharedPtr<FJsonObject>& NodeObject);
	static FString ClassifyLinkOwnership(bool bSourceOwned, bool bTargetOwned);
	static bool IsExecPinName(const FString& PinName);
	static EBlueprintHelperLogicLinkType IdentifyGraphLinkType(
		const FString& ExplicitKind,
		const FString& PinType,
		const FString& FromPin,
		const FString& ToPin);
	static EBlueprintHelperLogicNodeKind IdentifyNodeKind(
		const FString& ClassName,
		const FString& MemberName);
};
