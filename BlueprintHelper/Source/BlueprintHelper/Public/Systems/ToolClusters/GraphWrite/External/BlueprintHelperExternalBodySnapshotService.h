// BlueprintHelper Service Layer - external body snapshot service.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBodyEndpoint
{
	FString NodeGuid;
	FString PinName;
	FString PinDirection;

	FString ToStableString() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBodyLink
{
	FBlueprintHelperExternalBodyEndpoint From;
	FBlueprintHelperExternalBodyEndpoint To;

	FString ToStableString() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBodySnapshot
{
	FString EntryNodeGuid;
	FString EntryNodeClass;
	FString BodyFingerprint;
	TArray<FString> BodyNodeGuids;
	FString RestoreText;
	TArray<FBlueprintHelperExternalBodyLink> EntryToBodyLinks;
	TArray<FBlueprintHelperExternalBodyLink> BodyToExternalLinks;

	TSharedRef<FJsonObject> ToJson() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperExternalBodySnapshotService
{
public:
	bool CaptureBody(
		UEdGraph* Graph,
		UEdGraphNode* EntryNode,
		FBlueprintHelperExternalBodySnapshot& OutSnapshot,
		FString& OutError) const;

	TArray<UEdGraphNode*> CollectBodyNodes(
		UEdGraph* Graph,
		UEdGraphNode* EntryNode) const;

	static FString ComputeBodyFingerprint(
		UEdGraphNode* EntryNode,
		const TArray<UEdGraphNode*>& BodyNodes,
		const TArray<FBlueprintHelperExternalBodyLink>& EntryToBodyLinks,
		const TArray<FBlueprintHelperExternalBodyLink>& BodyToExternalLinks);
};
