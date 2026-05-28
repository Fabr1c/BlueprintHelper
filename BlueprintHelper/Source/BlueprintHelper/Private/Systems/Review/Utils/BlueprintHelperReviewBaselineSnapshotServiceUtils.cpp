// BlueprintHelper Review baseline snapshot service utilities implementation.

#include "Systems/Review/Utils/BlueprintHelperReviewBaselineSnapshotServiceUtils.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectBaseUtility.h"
#include "BlueprintHelperReviewUtils.h"

TArray<TSharedPtr<FJsonValue>> FBlueprintHelperReviewBaselineSnapshotServiceUtils::MakeStringArray(
	const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	for (const FString& Value : Values)
	{
		Result.Add(MakeShared<FJsonValueString>(Value));
	}
	return Result;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectPathNameSafe(const UObject* Object)
{
	return Object ? Object->GetPathName() : FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::GetObjectClassPathNameSafe(
	const UObject* Object)
{
	return Object && Object->GetClass() ? Object->GetClass()->GetPathName() : FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObject(
	const TSharedRef<FJsonObject>& Json)
{
	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Json, Writer);
	return Serialized;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::SerializeJsonObjectCanonical(
	const TSharedRef<FJsonObject>& Json)
{
	FString Serialized;
	UBlueprintHelperReviewUtils::AppendCanonicalJsonObject(Json, Serialized);
	return Serialized;
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::ExtractTargetName(
	const FBlueprintHelperReviewAtomicTarget& Target)
{
	if (!Target.PropertyPath.IsEmpty())
	{
		return Target.PropertyPath;
	}
	if (!Target.ComponentPath.IsEmpty())
	{
		return Target.ComponentPath;
	}

	int32 LastColon = INDEX_NONE;
	if (Target.TargetKey.FindLastChar(TEXT(':'), LastColon))
	{
		return Target.TargetKey.Mid(LastColon + 1);
	}
	return Target.DisplayLabel;
}

UObject* FBlueprintHelperReviewBaselineSnapshotServiceUtils::ResolveClassDefaultSnapshotObject(
	UObject* Asset)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (!Blueprint)
	{
		return Asset;
	}

	UClass* DefaultClass = Blueprint->GeneratedClass
		? Blueprint->GeneratedClass
		: Blueprint->SkeletonGeneratedClass;
	return DefaultClass ? DefaultClass->GetDefaultObject() : nullptr;
}

void FBlueprintHelperReviewBaselineSnapshotServiceUtils::SplitWidgetPropertyTarget(
	const FString& TargetName,
	FString& OutWidgetName,
	FString& OutPropertyName)
{
	OutWidgetName = TargetName;
	OutPropertyName.Reset();

	FString Left;
	FString Right;
	if (TargetName.Split(TEXT("."), &Left, &Right) && !Left.IsEmpty())
	{
		OutWidgetName = Left;
		OutPropertyName = Right;
	}
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::FindScsParentComponentName(
	const UBlueprint* Blueprint,
	const USCS_Node* ChildNode)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript || !ChildNode)
	{
		return FString();
	}

	for (const USCS_Node* CandidateParent : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (!CandidateParent)
		{
			continue;
		}
		for (const USCS_Node* CandidateChild : CandidateParent->GetChildNodes())
		{
			if (CandidateChild == ChildNode)
			{
				return CandidateParent->GetVariableName().ToString();
			}
		}
	}
	return FString();
}

FString FBlueprintHelperReviewBaselineSnapshotServiceUtils::PinDirectionToString(
	EEdGraphPinDirection Direction)
{
	return Direction == EGPD_Output ? TEXT("output") : TEXT("input");
}

void FBlueprintHelperReviewBaselineSnapshotServiceUtils::AppendGraphs(
	TArray<UEdGraph*>& OutGraphs,
	const TArray<UEdGraph*>& InGraphs)
{
	for (UEdGraph* Graph : InGraphs)
	{
		if (Graph)
		{
			OutGraphs.Add(Graph);
		}
	}
}
