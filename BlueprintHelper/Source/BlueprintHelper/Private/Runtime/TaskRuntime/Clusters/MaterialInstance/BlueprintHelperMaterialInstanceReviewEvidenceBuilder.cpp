// BlueprintHelper TaskRuntime - MaterialInstance review evidence builder.

#include "Runtime/TaskRuntime/Clusters/MaterialInstance/BlueprintHelperMaterialInstanceReviewEvidenceBuilder.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"

class FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils
{
public:
	static FString ReadStringField(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		const FString& Fallback = FString())
	{
		FString Value;
		if (Json.IsValid() && Json->TryGetStringField(FieldName, Value))
		{
			return Value;
		}
		return Fallback;
	}

	static bool ReadBoolField(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName,
		bool bFallback = false)
	{
		bool bValue = bFallback;
		if (Json.IsValid())
		{
			Json->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	static TSharedPtr<FJsonObject> ReadObjectField(
		const TSharedPtr<FJsonObject>& Json,
		const TCHAR* FieldName)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Json.IsValid() &&
			Json->TryGetObjectField(FieldName, Object) &&
			Object &&
			Object->IsValid())
		{
			return *Object;
		}
		return nullptr;
	}

	static FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Json)
	{
		FString JsonText;
		FBlueprintHelperMaterialInstanceParameterJsonUtils::SerializeJsonObject(Json, JsonText);
		return JsonText;
	}

	static FString MakeReviewKeySegment(FString Value)
	{
		Value.TrimStartAndEndInline();
		Value.ReplaceInline(TEXT("\\"), TEXT("_"));
		Value.ReplaceInline(TEXT("/"), TEXT("_"));
		Value.ReplaceInline(TEXT("."), TEXT("_"));
		Value.ReplaceInline(TEXT("|"), TEXT("_"));
		Value.ReplaceInline(TEXT(":"), TEXT("_"));
		Value.ReplaceInline(TEXT(" "), TEXT("_"));
		return Value;
	}

	static FString MakeTargetHash(const FString& SnapshotJson)
	{
		if (SnapshotJson.IsEmpty())
		{
			return FString();
		}
		return FString::Printf(TEXT("%08x"), GetTypeHash(SnapshotJson));
	}

	static void AddCommonTargetFields(
		FBlueprintHelperReviewAtomicTarget& Target,
		const FString& AssetPath,
		int32 StepIndex,
		int32 AtomicIndex)
	{
		Target.AssetPath = AssetPath;
		Target.Surface = EBlueprintHelperReviewSurface::Material;
		Target.GraphName = TEXT("MaterialGraph");
		Target.Status = EBlueprintHelperReviewChangeStatus::Pending;
		Target.TaskStepIndex = StepIndex;
		Target.AtomicIndex = AtomicIndex;
		Target.ExecutionOrder = AtomicIndex;
		Target.Ownership = TEXT("agent_authored");
	}

	static TSharedRef<FJsonObject> MakeAssetSnapshot(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Op,
		bool bExists)
	{
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("target_kind"), TEXT("asset_factory"));
		Snapshot->SetStringField(TEXT("target_subkind"), TEXT("material_instance"));
		Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
		Snapshot->SetBoolField(TEXT("exists"), bExists);
		Snapshot->SetStringField(TEXT("parent_material"), ReadStringField(Op, TEXT("parent_material")));
		Snapshot->SetStringField(TEXT("object_path"), ReadStringField(Op, TEXT("object_path")));
		return Snapshot;
	}

	static void AppendCreateTarget(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Op,
		int32 StepIndex,
		int32 AtomicIndex,
		TArray<FBlueprintHelperReviewAtomicTarget>& OutTargets)
	{
		FBlueprintHelperReviewAtomicTarget Target;
		AddCommonTargetFields(Target, AssetPath, StepIndex, AtomicIndex);
		Target.TargetKind = TEXT("asset_factory");
		Target.TargetSubKind = TEXT("material_instance");
		Target.TargetKey = FString::Printf(
			TEXT("asset_factory:material_instance:%s"),
			*MakeReviewKeySegment(AssetPath));
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = FString::Printf(TEXT("Create MaterialInstance %s"), *AssetPath);
		Target.LifecycleObjectKey = FString::Printf(TEXT("asset:%s"), *AssetPath.ToLower());
		Target.ScopeIdentity = Target.LifecycleObjectKey;

		const TSharedRef<FJsonObject> Before = MakeAssetSnapshot(AssetPath, Op, false);
		const TSharedRef<FJsonObject> After = MakeAssetSnapshot(AssetPath, Op, true);
		Target.BeforeSnapshotJson = SerializeJsonObject(Before);
		Target.AfterSnapshotJson = SerializeJsonObject(After);
		Target.AnchorJson = Target.AfterSnapshotJson;
		Target.RecordedAfterHash = MakeTargetHash(Target.AfterSnapshotJson);
		OutTargets.Add(Target);
	}

	static TSharedRef<FJsonObject> MakeParentSnapshot(
		const FString& AssetPath,
		const FString& ParentPath)
	{
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("target_kind"), TEXT("material_instance"));
		Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
		Snapshot->SetBoolField(TEXT("exists"), true);
		Snapshot->SetStringField(TEXT("property"), TEXT("parent_material"));
		Snapshot->SetStringField(TEXT("parent_material"), ParentPath);
		Snapshot->SetStringField(TEXT("before_parent_material"), ParentPath);
		return Snapshot;
	}

	static void AppendParentTarget(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Op,
		int32 StepIndex,
		int32 AtomicIndex,
		TArray<FBlueprintHelperReviewAtomicTarget>& OutTargets)
	{
		const FString BeforeParent = ReadStringField(Op, TEXT("before_parent_material"));
		const FString AfterParent = ReadStringField(
			Op,
			TEXT("after_parent_material"),
			ReadStringField(Op, TEXT("parent_material")));

		FBlueprintHelperReviewAtomicTarget Target;
		AddCommonTargetFields(Target, AssetPath, StepIndex, AtomicIndex);
		Target.TargetKind = TEXT("material_instance");
		Target.TargetSubKind = TEXT("parent");
		Target.TargetKey = FString::Printf(
			TEXT("material_instance:parent:%s"),
			*MakeReviewKeySegment(AssetPath));
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = TEXT("MaterialInstance parent");
		Target.PropertyPath = TEXT("parent_material");
		Target.BeforeParent = BeforeParent;
		Target.AfterParent = AfterParent;

		const TSharedRef<FJsonObject> Before = MakeParentSnapshot(AssetPath, BeforeParent);
		const TSharedRef<FJsonObject> After = MakeParentSnapshot(AssetPath, AfterParent);
		After->SetStringField(TEXT("after_parent_material"), AfterParent);

		TSharedRef<FJsonObject> Changed = MakeShared<FJsonObject>();
		Changed->SetStringField(TEXT("property"), TEXT("parent_material"));
		Changed->SetStringField(TEXT("before"), BeforeParent);
		Changed->SetStringField(TEXT("after"), AfterParent);

		Target.BeforeSnapshotJson = SerializeJsonObject(Before);
		Target.AfterSnapshotJson = SerializeJsonObject(After);
		Target.ChangedPropertiesJson = SerializeJsonObject(Changed);
		Target.AnchorJson = Target.AfterSnapshotJson;
		Target.RecordedAfterHash = MakeTargetHash(Target.AfterSnapshotJson);
		OutTargets.Add(Target);
	}

	static FString ReadParameterName(
		const TSharedPtr<FJsonObject>& Before,
		const TSharedPtr<FJsonObject>& After)
	{
		return ReadStringField(Before, TEXT("parameter_name"), ReadStringField(After, TEXT("parameter_name")));
	}

	static FString ReadParameterType(
		const TSharedPtr<FJsonObject>& Before,
		const TSharedPtr<FJsonObject>& After)
	{
		return ReadStringField(Before, TEXT("parameter_type"), ReadStringField(After, TEXT("parameter_type")));
	}

	static void EnsureParameterSnapshotFields(
		const TSharedPtr<FJsonObject>& Snapshot,
		const FString& AssetPath)
	{
		if (!Snapshot.IsValid())
		{
			return;
		}
		if (!Snapshot->HasField(TEXT("target_kind")))
		{
			Snapshot->SetStringField(TEXT("target_kind"), TEXT("material_instance_parameter"));
		}
		if (!Snapshot->HasField(TEXT("asset_path")))
		{
			Snapshot->SetStringField(TEXT("asset_path"), AssetPath);
		}
		if (!Snapshot->HasField(TEXT("override_state")))
		{
			const bool bHasOverride = ReadBoolField(Snapshot, TEXT("has_override"));
			Snapshot->SetStringField(
				TEXT("override_state"),
				FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeOverrideState(
					bHasOverride,
					ReadStringField(Snapshot, TEXT("source"))));
		}
	}

	static void AppendParameterTarget(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Op,
		int32 StepIndex,
		int32 AtomicIndex,
		TArray<FBlueprintHelperReviewAtomicTarget>& OutTargets)
	{
		TSharedPtr<FJsonObject> Before = ReadObjectField(Op, TEXT("before"));
		TSharedPtr<FJsonObject> After = ReadObjectField(Op, TEXT("after"));
		if (!Before.IsValid())
		{
			Before = MakeShared<FJsonObject>();
		}
		if (!After.IsValid())
		{
			After = MakeShared<FJsonObject>();
		}
		EnsureParameterSnapshotFields(Before, AssetPath);
		EnsureParameterSnapshotFields(After, AssetPath);

		const FString ParameterName = ReadParameterName(Before, After);
		const FString ParameterType = ReadParameterType(Before, After);
		if (ParameterName.IsEmpty())
		{
			return;
		}

		TSharedRef<FJsonObject> Changed = MakeShared<FJsonObject>();
		Changed->SetStringField(TEXT("op"), ReadStringField(Op, TEXT("op")));
		Changed->SetStringField(TEXT("parameter_name"), ParameterName);
		Changed->SetStringField(TEXT("parameter_type"), ParameterType);
		Changed->SetObjectField(TEXT("before"), Before.ToSharedRef());
		Changed->SetObjectField(TEXT("after"), After.ToSharedRef());
		Changed->SetStringField(
			TEXT("before_value"),
			FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(Before, TEXT("effective_value")));
		Changed->SetStringField(
			TEXT("after_value"),
			FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(After, TEXT("effective_value")));
		Changed->SetStringField(TEXT("effective_value"), ReadStringField(After, TEXT("effective_value")));
		Changed->SetStringField(TEXT("source"), ReadStringField(After, TEXT("source")));
		Changed->SetStringField(TEXT("override_state"), ReadStringField(After, TEXT("override_state")));

		FBlueprintHelperReviewAtomicTarget Target;
		AddCommonTargetFields(Target, AssetPath, StepIndex, AtomicIndex);
		Target.TargetKind = TEXT("material_instance_parameter");
		Target.TargetSubKind = ParameterType;
		Target.TargetKey = FString::Printf(
			TEXT("material_instance_parameter:%s:%s"),
			*MakeReviewKeySegment(ParameterType),
			*MakeReviewKeySegment(ParameterName));
		Target.VisualGroupKey = Target.TargetKey;
		Target.DisplayLabel = FString::Printf(TEXT("%s (%s)"), *ParameterName, *ParameterType);
		Target.PropertyPath = ParameterName;
		Target.ScopeIdentity = Target.TargetKey;
		Target.BeforeSnapshotJson = SerializeJsonObject(Before);
		Target.AfterSnapshotJson = SerializeJsonObject(After);
		Target.ChangedPropertiesJson = SerializeJsonObject(Changed);
		Target.AnchorJson = Target.AfterSnapshotJson;
		Target.ReadbackFingerprintBefore = FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(
			Before,
			TEXT("effective_value"));
		Target.ReadbackFingerprintAfter = FBlueprintHelperMaterialInstanceParameterJsonUtils::ReadParameterValueString(
			After,
			TEXT("effective_value"));
		Target.RecordedAfterHash = MakeTargetHash(Target.AfterSnapshotJson);
		OutTargets.Add(Target);
	}

	static bool IsParameterMutationOp(const FString& Op)
	{
		return Op == TEXT("set_scalar_override") ||
			Op == TEXT("set_vector_override") ||
			Op == TEXT("set_texture_override") ||
			Op == TEXT("set_static_switch_override") ||
			Op == TEXT("clear_override");
	}
};

bool FBlueprintHelperMaterialInstanceReviewEvidenceBuilder::BuildEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	OutEvidence = FBlueprintHelperWriteReviewEvidence();
	if (!StepResult.bOk || !StepResult.Data.IsValid())
	{
		return false;
	}

	const FString AssetPath =
		FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::ReadStringField(
			StepResult.Data,
			TEXT("asset_path"),
			FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::ReadStringField(
				LoweredStep.Payload,
				TEXT("asset_path")));
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
	if (!StepResult.Data->TryGetArrayField(TEXT("operations"), Operations) ||
		!Operations ||
		Operations->Num() == 0)
	{
		return false;
	}

	TArray<FBlueprintHelperReviewAtomicTarget> Targets;
	bool bHasCreate = false;
	bool bHasMutation = false;
	for (int32 Index = 0; Index < Operations->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Op =
			(*Operations)[Index].IsValid() ? (*Operations)[Index]->AsObject() : nullptr;
		const FString OpName =
			FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::ReadStringField(Op, TEXT("op"));
		if (OpName.IsEmpty())
		{
			continue;
		}

		if (OpName == TEXT("create_material_instance"))
		{
			bHasCreate = true;
			FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::AppendCreateTarget(
				AssetPath,
				Op,
				StepIndex,
				Targets.Num(),
				Targets);
			continue;
		}

		if (OpName == TEXT("set_parent"))
		{
			bHasMutation = true;
			FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::AppendParentTarget(
				AssetPath,
				Op,
				StepIndex,
				Targets.Num(),
				Targets);
			continue;
		}

		if (FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::IsParameterMutationOp(OpName))
		{
			bHasMutation = true;
			FBlueprintHelperMaterialInstanceReviewEvidenceBuilderLocalUtils::AppendParameterTarget(
				AssetPath,
				Op,
				StepIndex,
				Targets.Num(),
				Targets);
		}
	}

	if (Targets.IsEmpty())
	{
		return false;
	}

	OutEvidence.ArchiveSessionId = ArchiveSessionId;
	OutEvidence.TaskRunId = TaskRunId;
	OutEvidence.EvidenceId = FString::Printf(
		TEXT("%s:%s:%d:material_instance"),
		*ArchiveSessionId,
		*TaskRunId,
		StepIndex);
	OutEvidence.AssetPath = AssetPath;
	OutEvidence.OperationKind = TEXT("material_instance_edit");
	OutEvidence.ChangeKind = bHasCreate && !bHasMutation
		? EBlueprintHelperReviewChangeKind::Added
		: EBlueprintHelperReviewChangeKind::Modified;
	OutEvidence.DisplayLabel = TEXT("MaterialInstance edit");
	OutEvidence.BeforeSummary = bHasCreate ? TEXT("before MaterialInstance edit") : TEXT("before parameter values");
	OutEvidence.AfterSummary = TEXT("after MaterialInstance edit");
	OutEvidence.TaskStepIndex = StepIndex;
	OutEvidence.AtomicTargets = MoveTemp(Targets);
	return true;
}
