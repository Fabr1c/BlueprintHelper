// BlueprintHelper Review target validity resolver implementation.

#include "Systems/Review/BlueprintHelperReviewTargetValidityResolver.h"

#include "Blueprint/WidgetTree.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Systems/Review/Utils/BlueprintHelperReviewStoreTargetUtils.h"
#include "Systems/Review/Utils/BlueprintHelperReviewUtils.h"
#include "Shared/Review/BlueprintHelperReviewTargetKindRegistry.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace BlueprintHelperReviewTargetValidity
{
	static FBlueprintHelperReviewValidityResult Invalid(
		const FBlueprintHelperReviewValidityCandidate& Candidate,
		EBlueprintHelperReviewInvalidReason Reason,
		const FString& Message)
	{
		FBlueprintHelperReviewValidityResult Result;
		Result.Candidate = Candidate;
		Result.bValid = false;
		Result.InvalidReason = Reason;
		Result.Message = Message;
		return Result;
	}

	static FBlueprintHelperReviewValidityResult Valid(
		const FBlueprintHelperReviewValidityCandidate& Candidate)
	{
		FBlueprintHelperReviewValidityResult Result;
		Result.Candidate = Candidate;
		Result.bValid = true;
		return Result;
	}

	static FString MakeObjectPath(const FString& AssetPath)
	{
		if (AssetPath.Contains(TEXT(".")))
		{
			return AssetPath;
		}
		const FString AssetName = FPackageName::GetLongPackageAssetName(AssetPath);
		return AssetName.IsEmpty() ? AssetPath : AssetPath + TEXT(".") + AssetName;
	}

	static UObject* LoadReviewAsset(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		return FSoftObjectPath(MakeObjectPath(AssetPath)).TryLoad();
	}

	static FString ExtractPrefixedName(const FString& Value, const FString& Prefix)
	{
		const FString PrefixWithColon = Prefix + TEXT(":");
		if (!Value.StartsWith(PrefixWithColon, ESearchCase::IgnoreCase))
		{
			return FString();
		}
		FString Remainder = Value.Mid(PrefixWithColon.Len());
		int32 SeparatorIndex = INDEX_NONE;
		if (Remainder.FindChar(TEXT(':'), SeparatorIndex))
		{
			Remainder = Remainder.Left(SeparatorIndex);
		}
		Remainder.TrimStartAndEndInline();
		return Remainder;
	}

	static FString FirstPathSegment(FString Value)
	{
		Value.TrimStartAndEndInline();
		int32 SeparatorIndex = INDEX_NONE;
		if (Value.FindChar(TEXT('.'), SeparatorIndex) || Value.FindChar(TEXT('/'), SeparatorIndex))
		{
			Value = Value.Left(SeparatorIndex);
		}
		return Value;
	}

	static FString ResolveTargetName(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const TArray<FString>& Prefixes)
	{
		for (const FString& Prefix : Prefixes)
		{
			FString Name = ExtractPrefixedName(Target.TargetKey, Prefix);
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
		if (!Target.PropertyPath.IsEmpty())
		{
			return FirstPathSegment(Target.PropertyPath);
		}
		if (!Target.ComponentPath.IsEmpty())
		{
			return FirstPathSegment(Target.ComponentPath);
		}
		return FString();
	}

	static FString ResolveGraphBlockId(const FBlueprintHelperReviewAtomicTarget& Target)
	{
		const auto ExtractBlockSegment = [](const FString& Value) -> FString
		{
			static const FString BlockToken = TEXT(":block:");
			int32 TokenIndex = INDEX_NONE;
			TokenIndex = Value.Find(BlockToken, ESearchCase::IgnoreCase);
			if (TokenIndex != INDEX_NONE)
			{
				FString BlockId = Value.Mid(TokenIndex + BlockToken.Len());
				int32 SeparatorIndex = INDEX_NONE;
				if (BlockId.FindChar(TEXT(':'), SeparatorIndex))
				{
					BlockId = BlockId.Left(SeparatorIndex);
				}
				BlockId.TrimStartAndEndInline();
				return BlockId;
			}

			static const FString GraphBlockPrefix = TEXT("graph_block:");
			if (Value.StartsWith(GraphBlockPrefix, ESearchCase::IgnoreCase))
			{
				FString BlockId = Value.Mid(GraphBlockPrefix.Len());
				int32 SeparatorIndex = INDEX_NONE;
				if (BlockId.FindChar(TEXT(':'), SeparatorIndex))
				{
					BlockId = BlockId.Left(SeparatorIndex);
				}
				BlockId.TrimStartAndEndInline();
				return BlockId;
			}
			return FString();
		};

		FString BlockId = ExtractBlockSegment(Target.TargetKey);
		if (!BlockId.IsEmpty())
		{
			return BlockId;
		}
		return ExtractBlockSegment(Target.VisualGroupKey);
	}

	static bool GraphHasBlueprintHelperBlock(const UEdGraph* Graph, const FString& GraphName, const FString& BlockId)
	{
		if (!Graph || BlockId.IsEmpty())
		{
			return false;
		}

		TArray<FString> CandidateBlockIds;
		CandidateBlockIds.Add(BlockId);
		if (!GraphName.IsEmpty() && !BlockId.StartsWith(GraphName + TEXT("_"), ESearchCase::IgnoreCase))
		{
			CandidateBlockIds.Add(GraphName + TEXT("_") + BlockId);
		}

		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			const FString NodeBlockId = UBlueprintHelperReviewUtils::GetReviewSnapshotNodeBlockId(Node);
			if (NodeBlockId.IsEmpty())
			{
				continue;
			}
			for (const FString& CandidateBlockId : CandidateBlockIds)
			{
				if (NodeBlockId.Equals(CandidateBlockId, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool TargetLooksLike(
		const FBlueprintHelperReviewAtomicTarget& Target,
		const FString& Token)
	{
		return Target.TargetKind.Contains(Token, ESearchCase::IgnoreCase)
			|| Target.TargetKey.Contains(Token + TEXT(":"), ESearchCase::IgnoreCase);
	}

	static bool BlueprintHasVariable(const UBlueprint* Blueprint, const FString& VariableName)
	{
		return Blueprint
			&& !VariableName.IsEmpty()
			&& FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, FName(*VariableName)) != INDEX_NONE;
	}

	static bool BlueprintHasFunctionLikeGraph(const UBlueprint* Blueprint, const FString& FunctionName)
	{
		return Blueprint
			&& !FunctionName.IsEmpty()
			&& UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, FunctionName) != nullptr;
	}

	static bool BlueprintHasComponent(const UBlueprint* Blueprint, const FString& ComponentName)
	{
		if (!Blueprint || !Blueprint->SimpleConstructionScript || ComponentName.IsEmpty())
		{
			return false;
		}
		return Blueprint->SimpleConstructionScript->FindSCSNode(FName(*ComponentName)) != nullptr;
	}

	static bool WidgetBlueprintHasWidget(const UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName)
	{
		return WidgetBlueprint
			&& WidgetBlueprint->WidgetTree
			&& !WidgetName.IsEmpty()
			&& WidgetBlueprint->WidgetTree->FindWidget(FName(*WidgetName)) != nullptr;
	}
}

FBlueprintHelperReviewValidityResult FBlueprintHelperReviewTargetValidityResolver::ValidateOnGameThread(
	const FBlueprintHelperReviewValidityCandidate& Candidate) const
{
	check(IsInGameThread());

	const FString AssetPath = Candidate.AssetPath.IsEmpty()
		? Candidate.Target.AssetPath
		: Candidate.AssetPath;
	if (!FBlueprintHelperReviewStoreTargetUtils::DoesReviewAssetPackageExist(AssetPath))
	{
		return BlueprintHelperReviewTargetValidity::Invalid(
			Candidate,
			EBlueprintHelperReviewInvalidReason::AssetMissing,
			TEXT("review_asset_package_missing"));
	}

	UObject* Asset = BlueprintHelperReviewTargetValidity::LoadReviewAsset(AssetPath);
	if (!Asset)
	{
		return BlueprintHelperReviewTargetValidity::Invalid(
			Candidate,
			EBlueprintHelperReviewInvalidReason::AssetMissing,
			TEXT("review_asset_load_failed"));
	}

	const FBlueprintHelperReviewAtomicTarget& Target = Candidate.Target;
	if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
	{
		if (!Target.GraphName.IsEmpty()
			&& !UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, Target.GraphName))
		{
			return BlueprintHelperReviewTargetValidity::Invalid(
				Candidate,
				EBlueprintHelperReviewInvalidReason::GraphMissing,
				TEXT("review_graph_missing"));
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphNodeTarget(Target.TargetKind, Target.TargetKey)
			&& !Target.NodeGuid.IsEmpty())
		{
			const UEdGraph* Graph = UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, Target.GraphName);
			if (!Graph || !UBlueprintHelperReviewUtils::FindReviewSnapshotNodeByGuid(Graph, Target.NodeGuid))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::GraphNodeMissing,
					TEXT("review_graph_node_missing"));
			}
		}

		if (FBlueprintHelperReviewTargetKindRegistry::IsGraphBlockTarget(Target.TargetKind, Target.TargetKey))
		{
			const FString BlockId = BlueprintHelperReviewTargetValidity::ResolveGraphBlockId(Target);
			if (!BlockId.IsEmpty())
			{
				const UEdGraph* Graph = UBlueprintHelperReviewUtils::FindReviewSnapshotGraph(Blueprint, Target.GraphName);
				if (!BlueprintHelperReviewTargetValidity::GraphHasBlueprintHelperBlock(Graph, Target.GraphName, BlockId))
				{
					return BlueprintHelperReviewTargetValidity::Invalid(
						Candidate,
						EBlueprintHelperReviewInvalidReason::GraphNodeMissing,
						TEXT("review_graph_block_missing"));
				}
			}
		}

		if (BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("variable")))
		{
			const FString VariableName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
				Target,
				{ TEXT("blueprint_variable"), TEXT("variable"), TEXT("member_variable"), TEXT("local_variable") });
			if (!VariableName.IsEmpty()
				&& !BlueprintHelperReviewTargetValidity::BlueprintHasVariable(Blueprint, VariableName))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::VariableMissingOrRenamed,
					TEXT("review_variable_missing_or_renamed"));
			}
		}

		if (BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("function"))
			|| BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("custom_event")))
		{
			const FString FunctionName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
				Target,
				{ TEXT("function"), TEXT("custom_event") });
			if (!FunctionName.IsEmpty()
				&& !BlueprintHelperReviewTargetValidity::BlueprintHasFunctionLikeGraph(Blueprint, FunctionName))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::FunctionMissingOrRenamed,
					TEXT("review_function_missing_or_renamed"));
			}
		}

		if (BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("component")))
		{
			const FString ComponentName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
				Target,
				{ TEXT("component") });
			if (!ComponentName.IsEmpty()
				&& !BlueprintHelperReviewTargetValidity::BlueprintHasComponent(Blueprint, ComponentName))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::ComponentMissingOrRenamed,
					TEXT("review_component_missing_or_renamed"));
			}
		}
	}

	if (UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Asset))
	{
		if (BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("widget")))
		{
			const FString WidgetName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
				Target,
				{ TEXT("widget") });
			if (!WidgetName.IsEmpty()
				&& !BlueprintHelperReviewTargetValidity::WidgetBlueprintHasWidget(WidgetBlueprint, WidgetName))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::WidgetMissingOrRenamed,
					TEXT("review_widget_missing_or_renamed"));
			}
		}
	}

	if (UDataTable* DataTable = Cast<UDataTable>(Asset))
	{
		if (BlueprintHelperReviewTargetValidity::TargetLooksLike(Target, TEXT("datatable_row")))
		{
			const FString RowName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
				Target,
				{ TEXT("datatable_row") });
			if (!RowName.IsEmpty() && !DataTable->GetRowMap().Contains(FName(*RowName)))
			{
				return BlueprintHelperReviewTargetValidity::Invalid(
					Candidate,
					EBlueprintHelperReviewInvalidReason::DataTableRowMissing,
					TEXT("review_datatable_row_missing"));
			}
		}
	}

	if (Target.TargetKind.Equals(TEXT("data_asset_property"), ESearchCase::IgnoreCase)
		|| Target.TargetKey.StartsWith(TEXT("data_asset_property:"), ESearchCase::IgnoreCase))
	{
		const FString PropertyName = BlueprintHelperReviewTargetValidity::ResolveTargetName(
			Target,
			{ TEXT("data_asset_property") });
		if (!PropertyName.IsEmpty() && !FindFProperty<FProperty>(Asset->GetClass(), FName(*PropertyName)))
		{
			return BlueprintHelperReviewTargetValidity::Invalid(
				Candidate,
				EBlueprintHelperReviewInvalidReason::DataAssetPropertyMissing,
				TEXT("review_data_asset_property_missing"));
		}
	}

	return BlueprintHelperReviewTargetValidity::Valid(Candidate);
}
