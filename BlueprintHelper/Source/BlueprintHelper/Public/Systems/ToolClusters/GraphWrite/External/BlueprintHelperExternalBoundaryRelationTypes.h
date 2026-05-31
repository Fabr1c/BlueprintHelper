// External GraphWrite boundary relation data.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBoundaryEndpoint
{
	FString NodeGuid;
	FString PinName;
	FString PinDirection;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("node_guid"), NodeGuid);
		Json->SetStringField(TEXT("pin_name"), PinName);
		Json->SetStringField(TEXT("pin_direction"), PinDirection);
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBoundaryLink
{
	FBlueprintHelperExternalBoundaryEndpoint From;
	FBlueprintHelperExternalBoundaryEndpoint To;

	FString ToStableString() const
	{
		return FString::Printf(
			TEXT("%s.%s->%s.%s"),
			*From.NodeGuid,
			*From.PinName,
			*To.NodeGuid,
			*To.PinName);
	}

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("from"), From.ToJson());
		Json->SetObjectField(TEXT("to"), To.ToJson());
		Json->SetStringField(TEXT("stable_ref"), ToStableString());
		return Json;
	}
};

struct BLUEPRINTHELPER_API FBlueprintHelperExternalBoundaryRelation
{
	FString Schema = TEXT("BlueprintHelper.ExternalBoundaryRelation.v1");
	FBlueprintHelperExternalGraphAnchor Anchor;
	FString InsertedBlockId;
	TArray<FBlueprintHelperExternalBoundaryLink> BeforeLinks;
	TArray<FBlueprintHelperExternalBoundaryLink> AfterLinks;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("anchor"), Anchor.ToJson());
		if (!InsertedBlockId.IsEmpty())
		{
			Json->SetStringField(TEXT("inserted_block_id"), InsertedBlockId);
		}

		TArray<TSharedPtr<FJsonValue>> Before;
		for (const FBlueprintHelperExternalBoundaryLink& Link : BeforeLinks)
		{
			Before.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
		}
		Json->SetArrayField(TEXT("before_links"), Before);

		TArray<TSharedPtr<FJsonValue>> After;
		for (const FBlueprintHelperExternalBoundaryLink& Link : AfterLinks)
		{
			After.Add(MakeShared<FJsonValueObject>(Link.ToJson()));
		}
		Json->SetArrayField(TEXT("after_links"), After);
		return Json;
	}
};
