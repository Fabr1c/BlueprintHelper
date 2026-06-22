#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyReadbackService.h"

#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapter.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyAdapterResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyRequest.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyTarget.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalBodySnapshotService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorService.h"
#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperExternalGraphAnchorTypes.h"

class FBlueprintHelperGraphBodyReadbackServiceLocalUtils
{
public:
	static TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			if (!Value.IsEmpty())
			{
				JsonValues.Add(MakeShared<FJsonValueString>(Value));
			}
		}
		return JsonValues;
	}

	static bool SupportsBodyEvidence(const FBlueprintHelperGraphBodyBoundaryModel& Boundary)
	{
		return Boundary.RuntimeAdapterId.Equals(TEXT("k2.external_graph.replace_body"), ESearchCase::IgnoreCase)
			|| Boundary.TaskSpecStrategy.Equals(TEXT("replace_external_body"), ESearchCase::IgnoreCase);
	}

	static void PopulateBodyEvidence(
		const FBlueprintHelperGraphBodyTarget& Target,
		const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
		FBlueprintHelperGraphBodyReadbackProjection& InOutProjection)
	{
		if (!SupportsBodyEvidence(Boundary))
		{
			return;
		}

		if (!Target.Graph || Target.EntryBoundaryNodes.Num() == 0 || !Target.EntryBoundaryNodes[0])
		{
			if (InOutProjection.BodyEvidenceStatus.IsEmpty())
			{
				InOutProjection.BodyEvidenceStatus = TEXT("missing_entry");
				InOutProjection.BodyEvidenceErrorCode = TEXT("k2_entry_identity_not_found");
			}
			return;
		}

		FBlueprintHelperExternalGraphAnchor Anchor;
		FString AnchorError;
		const FBlueprintHelperExternalGraphAnchorService AnchorService;
		if (!AnchorService.BuildBodyEntryAnchor(
			Boundary.TargetAssetPath.IsEmpty() ? Target.AssetPath : Boundary.TargetAssetPath,
			Boundary.GraphName.IsEmpty() ? Target.GraphName : Boundary.GraphName,
			Target.EntryBoundaryNodes[0],
			Anchor,
			AnchorError))
		{
			InOutProjection.BodyEvidenceStatus = TEXT("body_entry_anchor_failed");
			InOutProjection.BodyEvidenceErrorCode = AnchorError.IsEmpty()
				? TEXT("body_entry_anchor_failed")
				: AnchorError;
			return;
		}

		InOutProjection.BodyEntryNodeGuid = Anchor.NodeGuid;
		InOutProjection.BodyEntryNodeClass = Anchor.NodeClass;
		InOutProjection.BodyEntryStableName = Anchor.StableName;
		InOutProjection.BodyEntryKind = Anchor.EntryKind;
		InOutProjection.BodyEntryMemberName = Anchor.MemberName;
		InOutProjection.BodyEntryFunctionName = Anchor.FunctionName;
		InOutProjection.BodyEntryDisplayName = Anchor.DisplayName;
		InOutProjection.BodyEntryFingerprint = Anchor.Fingerprint;

		FBlueprintHelperExternalBodySnapshot Snapshot;
		FString SnapshotError;
		const FBlueprintHelperExternalBodySnapshotService SnapshotService;
		if (!SnapshotService.CaptureBody(
			Target.Graph,
			Target.EntryBoundaryNodes[0],
			Snapshot,
			SnapshotError))
		{
			InOutProjection.BodyEvidenceStatus = TEXT("body_fingerprint_failed");
			InOutProjection.BodyEvidenceErrorCode = SnapshotError.IsEmpty()
				? TEXT("body_fingerprint_failed")
				: SnapshotError;
			return;
		}

		InOutProjection.BodyFingerprint = Snapshot.BodyFingerprint;
	}

	static FString DisplayNameForBoundaryRef(
		const FString& Ref,
		const FBlueprintHelperGraphBodyReadbackProjection& Projection)
	{
		if (const FString* DisplayName = Projection.BoundaryDisplayNames.Find(Ref))
		{
			return *DisplayName;
		}
		if (Ref.StartsWith(TEXT("CustomEvent:")))
		{
			return Ref.RightChop(12);
		}
		if (Ref.StartsWith(TEXT("Event:")))
		{
			return Ref.RightChop(6);
		}
		return Ref;
	}

	static TArray<TSharedPtr<FJsonValue>> BoundaryRefsToJson(
		const TArray<FString>& Refs,
		const FBlueprintHelperGraphBodyReadbackProjection& Projection)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Ref : Refs)
		{
			if (Ref.IsEmpty())
			{
				continue;
			}

			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("node_ref"), Ref);
			Item->SetStringField(TEXT("display_name"), DisplayNameForBoundaryRef(Ref, Projection));
			Values.Add(MakeShared<FJsonValueObject>(Item));
		}
		return Values;
	}

};

bool FBlueprintHelperGraphBodyReadbackService::BuildAdapterBoundaryForTarget(
	const FBlueprintHelperTargetRef& Target,
	TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
	FString& OutError) const
{
	OutAdapterBoundaryJson.Reset();
	OutError.Reset();

	if (Target.AssetPath.IsEmpty())
	{
		OutError = TEXT("Readback adapter boundary requires an asset path.");
		return false;
	}

	UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *Target.AssetPath);
	if (!Blueprint)
	{
		Blueprint = LoadObject<UBlueprint>(nullptr, *Target.AssetPath);
	}
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Unable to load Blueprint for adapter boundary: %s."), *Target.AssetPath);
		return false;
	}

	FBlueprintHelperGraphBodyRequest Request =
		FBlueprintHelperGraphBodyAdapterResolver::MakeReadRequestForTarget(Target, Blueprint);

	TUniquePtr<IBlueprintHelperGraphBodyAdapter> Adapter;
	return FBlueprintHelperGraphBodyAdapterResolver::TryCreateForReadTarget(Target, Adapter, OutError) &&
		Adapter.IsValid() &&
		TryBuildAdapterBoundary(*Adapter, Request, OutAdapterBoundaryJson, OutError);
}

TSharedRef<FJsonObject> FBlueprintHelperGraphBodyReadbackService::BuildAdapterBoundaryJson(
	const FBlueprintHelperGraphBodyBoundaryModel& Boundary,
	const FBlueprintHelperGraphBodyReadbackProjection& Projection) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("runtime_adapter_id"), Boundary.RuntimeAdapterId);
	Json->SetStringField(TEXT("body_kind"), FBlueprintHelperGraphBodyBoundaryModelUtils::BodyKindToString(Boundary.BodyKind));
	Json->SetStringField(TEXT("graph_name"), Boundary.GraphName);
	if (!Projection.FunctionName.IsEmpty())
	{
		Json->SetStringField(TEXT("function_name"), Projection.FunctionName);
	}
	Json->SetArrayField(
		TEXT("entry_boundaries"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.EntryNodeRefs, Projection));
	Json->SetArrayField(
		TEXT("exit_boundaries"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::BoundaryRefsToJson(Boundary.ExitNodeRefs, Projection));
	Json->SetArrayField(
		TEXT("entry_boundary_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.EntryBoundaryRefs));
	Json->SetArrayField(
		TEXT("result_boundary_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.ResultBoundaryRefs));
	Json->SetArrayField(
		TEXT("function_input_pin_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.FunctionInputPinRefs));
	Json->SetArrayField(
		TEXT("function_output_pin_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.FunctionOutputPinRefs));
	Json->SetArrayField(
		TEXT("generated_node_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.GeneratedNodeRefs));
	Json->SetArrayField(
		TEXT("exec_link_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.ExecLinkRefs));
	Json->SetArrayField(
		TEXT("data_link_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.DataLinkRefs));
	Json->SetArrayField(
		TEXT("folded_boundary_node_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.FoldedBoundaryNodeRefs));
	Json->SetArrayField(
		TEXT("visible_boundary_node_refs"),
		FBlueprintHelperGraphBodyReadbackServiceLocalUtils::StringsToJson(Projection.VisibleBoundaryNodeRefs));
	if (!Projection.BodyEntryNodeGuid.IsEmpty())
	{
		TSharedRef<FJsonObject> BodyEntry = MakeShared<FJsonObject>();
		BodyEntry->SetStringField(TEXT("schema"), FBlueprintHelperExternalGraphAnchor::SchemaString);
		BodyEntry->SetStringField(TEXT("asset_path"), Boundary.TargetAssetPath);
		BodyEntry->SetStringField(TEXT("graph_name"), Boundary.GraphName);
		BodyEntry->SetStringField(TEXT("node_guid"), Projection.BodyEntryNodeGuid);
		BodyEntry->SetStringField(TEXT("node_class"), Projection.BodyEntryNodeClass);
		if (!Projection.BodyEntryStableName.IsEmpty())
		{
			BodyEntry->SetStringField(TEXT("stable_name"), Projection.BodyEntryStableName);
		}
		if (!Projection.BodyEntryKind.IsEmpty())
		{
			BodyEntry->SetStringField(TEXT("entry_kind"), Projection.BodyEntryKind);
		}
		if (!Projection.BodyEntryMemberName.IsEmpty())
		{
			BodyEntry->SetStringField(TEXT("member_name"), Projection.BodyEntryMemberName);
		}
		if (!Projection.BodyEntryFunctionName.IsEmpty())
		{
			BodyEntry->SetStringField(TEXT("function_name"), Projection.BodyEntryFunctionName);
		}
		if (!Projection.BodyEntryDisplayName.IsEmpty())
		{
			BodyEntry->SetStringField(TEXT("display_name"), Projection.BodyEntryDisplayName);
		}
		BodyEntry->SetStringField(
			TEXT("semantic_role"),
			FBlueprintHelperExternalGraphAnchor::RoleToString(EBlueprintHelperExternalGraphAnchorRole::BodyEntry));
		BodyEntry->SetStringField(TEXT("fingerprint"), Projection.BodyEntryFingerprint);
		Json->SetObjectField(TEXT("body_entry"), BodyEntry);
	}
	if (!Projection.BodyFingerprint.IsEmpty())
	{
		Json->SetStringField(TEXT("body_fingerprint"), Projection.BodyFingerprint);
	}
	if (!Projection.BodyEvidenceStatus.IsEmpty())
	{
		Json->SetStringField(TEXT("body_evidence_status"), Projection.BodyEvidenceStatus);
	}
	if (!Projection.BodyEvidenceErrorCode.IsEmpty())
	{
		Json->SetStringField(TEXT("body_evidence_error_code"), Projection.BodyEvidenceErrorCode);
	}
	if (!Projection.BodyEvidenceErrorMessage.IsEmpty())
	{
		Json->SetStringField(TEXT("body_evidence_error_message"), Projection.BodyEvidenceErrorMessage);
	}
	return Json;
}

bool FBlueprintHelperGraphBodyReadbackService::TryBuildAdapterBoundary(
	const IBlueprintHelperGraphBodyAdapter& Adapter,
	const FBlueprintHelperGraphBodyRequest& Request,
	TSharedPtr<FJsonObject>& OutAdapterBoundaryJson,
	FString& OutError) const
{
	FBlueprintHelperGraphBodyTarget Target;
	if (!Adapter.ResolveTarget(Request, Target, OutError))
	{
		return false;
	}

	const FBlueprintHelperGraphBodyBoundaryModel Boundary = Adapter.BuildBoundaryModel(Target, Request);
	FBlueprintHelperGraphBodyReadbackProjection Projection =
		Adapter.BuildReadbackProjection(Target, Boundary);
	FBlueprintHelperGraphBodyReadbackServiceLocalUtils::PopulateBodyEvidence(Target, Boundary, Projection);
	OutAdapterBoundaryJson = BuildAdapterBoundaryJson(Boundary, Projection);
	OutError.Reset();
	return true;
}
