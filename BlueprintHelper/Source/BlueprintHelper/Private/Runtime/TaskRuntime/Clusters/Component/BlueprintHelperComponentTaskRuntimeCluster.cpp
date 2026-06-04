// BlueprintHelper TaskRuntime - Component static cluster.

#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"

class FBlueprintHelperComponentReviewEvidenceLocalUtils
{
public:
	static FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	static bool ReadBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		bool bValue = false;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Object, Writer);
		return JsonText;
	}

	static FString SerializeJsonArray(const TArray<TSharedPtr<FJsonValue>>& Values)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Values, Writer);
		return JsonText;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeComponentPropertySnapshotEntries(
		const TArray<TSharedPtr<FJsonValue>>& ChangedProperties,
		const TCHAR* ValueFieldName)
	{
		TArray<TSharedPtr<FJsonValue>> Entries;
		for (const TSharedPtr<FJsonValue>& ChangedValue : ChangedProperties)
		{
			const TSharedPtr<FJsonObject> ChangedObject = ChangedValue.IsValid()
				? ChangedValue->AsObject()
				: nullptr;
			if (!ChangedObject.IsValid())
			{
				continue;
			}

			FString PropertyPath;
			FString ValueText;
			if (!ChangedObject->TryGetStringField(TEXT("property_path"), PropertyPath) ||
				!ChangedObject->TryGetStringField(ValueFieldName, ValueText) ||
				PropertyPath.IsEmpty())
			{
				continue;
			}

			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("property_path"), PropertyPath);
			Entry->SetStringField(TEXT("name"), PropertyPath);
			Entry->SetStringField(TEXT("value"), ValueText);
			Entries.Add(MakeShared<FJsonValueObject>(Entry));
		}
		return Entries;
	}

	static TSharedRef<FJsonObject> MakeComponentSnapshot(
		const TSharedPtr<FJsonObject>& ComponentObject,
		const TArray<TSharedPtr<FJsonValue>>& ChangedProperties,
		bool bBefore)
	{
		TSharedRef<FJsonObject> Snapshot = MakeShared<FJsonObject>();
		Snapshot->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.ComponentSnapshot.v1"));
		Snapshot->SetBoolField(TEXT("exists"), true);

		const FString ComponentName = ReadStringField(ComponentObject, TEXT("component_name"));
		Snapshot->SetStringField(TEXT("component_name"), ComponentName);
		Snapshot->SetStringField(TEXT("target_name"), ComponentName);

		const FString ClassPath = ReadStringField(ComponentObject, TEXT("class_path"));
		Snapshot->SetStringField(
			TEXT("component_class"),
			ClassPath.IsEmpty() ? ReadStringField(ComponentObject, TEXT("component_class")) : ClassPath);
		Snapshot->SetStringField(
			TEXT("component_template_path"),
			ReadStringField(ComponentObject, TEXT("component_template_path")));
		Snapshot->SetStringField(
			TEXT("component_id"),
			ReadStringField(ComponentObject, TEXT("component_id")));

		const FString ParentComponent = ReadStringField(ComponentObject, TEXT("parent"));
		if (!ParentComponent.IsEmpty())
		{
			Snapshot->SetStringField(TEXT("parent_component"), ParentComponent);
		}
		const FString SocketName = ReadStringField(ComponentObject, TEXT("socket"));
		if (!SocketName.IsEmpty())
		{
			Snapshot->SetStringField(TEXT("socket_name"), SocketName);
		}

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetArrayField(
			TEXT("properties"),
			MakeComponentPropertySnapshotEntries(
				ChangedProperties,
				bBefore ? TEXT("before_value") : TEXT("after_value")));
		Snapshot->SetObjectField(TEXT("properties"), Properties);
		return Snapshot;
	}

	static TSharedRef<FJsonObject> MakeDeletedComponentSnapshot(const TSharedPtr<FJsonObject>& ComponentObject)
	{
		TArray<TSharedPtr<FJsonValue>> EmptyProperties;
		TSharedRef<FJsonObject> Snapshot = MakeComponentSnapshot(ComponentObject, EmptyProperties, false);
		Snapshot->SetBoolField(TEXT("exists"), false);
		return Snapshot;
	}

	static FString ResolveComponentOrigin(const TSharedPtr<FJsonObject>& ComponentObject)
	{
		if (ReadBoolField(ComponentObject, TEXT("is_owned_scs")))
		{
			return TEXT("owned_scs");
		}
		if (ReadBoolField(ComponentObject, TEXT("is_inherited")))
		{
			return TEXT("inherited");
		}
		if (ReadBoolField(ComponentObject, TEXT("is_native")))
		{
			return TEXT("native");
		}
		return TEXT("unknown");
	}

	static void EnrichSetComponentPropertiesEvidence(
		const FBlueprintHelperToolResultBase& StepResult,
		FBlueprintHelperWriteReviewEvidence& OutEvidence)
	{
		if (!StepResult.Data.IsValid() || OutEvidence.AtomicTargets.Num() == 0)
		{
			return;
		}

		const TSharedPtr<FJsonObject>* ComponentObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* PropertyResultPtr = nullptr;
		if (!StepResult.Data->TryGetObjectField(TEXT("component"), ComponentObjectPtr) ||
			!ComponentObjectPtr ||
			!ComponentObjectPtr->IsValid() ||
			!StepResult.Data->TryGetObjectField(TEXT("property_result"), PropertyResultPtr) ||
			!PropertyResultPtr ||
			!PropertyResultPtr->IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* ChangedPropertiesPtr = nullptr;
		if (!(*PropertyResultPtr)->TryGetArrayField(TEXT("changed_properties"), ChangedPropertiesPtr) ||
			!ChangedPropertiesPtr)
		{
			return;
		}

		const TSharedPtr<FJsonObject> ComponentObject = *ComponentObjectPtr;
		FBlueprintHelperReviewAtomicTarget& Target = OutEvidence.AtomicTargets[0];
		Target.ComponentId = ReadStringField(ComponentObject, TEXT("component_id"));
		Target.ComponentTemplatePath = ReadStringField(ComponentObject, TEXT("component_template_path"));
		Target.ComponentOrigin = ResolveComponentOrigin(ComponentObject);
		Target.ReadbackFingerprintBefore = ReadStringField(ComponentObject, TEXT("readback_fingerprint"));
		Target.ReadbackFingerprintAfter = ReadStringField(ComponentObject, TEXT("readback_fingerprint"));
		if (!Target.ComponentPath.IsEmpty() && Target.DisplayLabel.IsEmpty())
		{
			Target.DisplayLabel = Target.ComponentPath;
		}

		if (ChangedPropertiesPtr->Num() == 0)
		{
			return;
		}

		Target.ChangedPropertiesJson = SerializeJsonArray(*ChangedPropertiesPtr);
		Target.BeforeSnapshotJson = SerializeJsonObject(
			MakeComponentSnapshot(ComponentObject, *ChangedPropertiesPtr, true));
		Target.AfterSnapshotJson = SerializeJsonObject(
			MakeComponentSnapshot(ComponentObject, *ChangedPropertiesPtr, false));
	}

	static void EnrichComponentMutationEvidence(
		const FBlueprintHelperToolResultBase& StepResult,
		FBlueprintHelperWriteReviewEvidence& OutEvidence)
	{
		if (!StepResult.Data.IsValid() || OutEvidence.AtomicTargets.Num() == 0)
		{
			return;
		}

		const TSharedPtr<FJsonObject>* ComponentObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* BeforeComponentObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* AfterComponentObjectPtr = nullptr;
		StepResult.Data->TryGetObjectField(TEXT("component"), ComponentObjectPtr);
		StepResult.Data->TryGetObjectField(TEXT("before_component"), BeforeComponentObjectPtr);
		StepResult.Data->TryGetObjectField(TEXT("after_component"), AfterComponentObjectPtr);

		const TSharedPtr<FJsonObject> ComponentObject =
			ComponentObjectPtr && ComponentObjectPtr->IsValid() ? *ComponentObjectPtr : nullptr;
		const TSharedPtr<FJsonObject> BeforeComponentObject =
			BeforeComponentObjectPtr && BeforeComponentObjectPtr->IsValid() ? *BeforeComponentObjectPtr : nullptr;
		const TSharedPtr<FJsonObject> AfterComponentObject =
			AfterComponentObjectPtr && AfterComponentObjectPtr->IsValid() ? *AfterComponentObjectPtr : nullptr;
		const TSharedPtr<FJsonObject> TargetComponentObject = AfterComponentObject.IsValid()
			? AfterComponentObject
			: (BeforeComponentObject.IsValid() ? BeforeComponentObject : ComponentObject);
		if (!TargetComponentObject.IsValid())
		{
			return;
		}

		FBlueprintHelperReviewAtomicTarget& Target = OutEvidence.AtomicTargets[0];
		Target.ComponentId = ReadStringField(TargetComponentObject, TEXT("component_id"));
		Target.ComponentTemplatePath = ReadStringField(TargetComponentObject, TEXT("component_template_path"));
		Target.ComponentOrigin = ResolveComponentOrigin(TargetComponentObject);
		Target.ReadbackFingerprintBefore = ReadStringField(BeforeComponentObject, TEXT("readback_fingerprint"));
		Target.ReadbackFingerprintAfter = ReadStringField(AfterComponentObject, TEXT("readback_fingerprint"));
		if (Target.ReadbackFingerprintBefore.IsEmpty())
		{
			Target.ReadbackFingerprintBefore = ReadStringField(TargetComponentObject, TEXT("readback_fingerprint"));
		}
		if (Target.ReadbackFingerprintAfter.IsEmpty())
		{
			Target.ReadbackFingerprintAfter = ReadStringField(TargetComponentObject, TEXT("readback_fingerprint"));
		}

		const FString ComponentName = ReadStringField(TargetComponentObject, TEXT("component_name"));
		if (!ComponentName.IsEmpty())
		{
			Target.ComponentPath = ComponentName;
			if (Target.DisplayLabel.IsEmpty())
			{
				Target.DisplayLabel = ComponentName;
			}
		}

		const TSharedPtr<FJsonObject>* HierarchyChangePtr = nullptr;
		if (StepResult.Data->TryGetObjectField(TEXT("hierarchy_change"), HierarchyChangePtr) &&
			HierarchyChangePtr &&
			HierarchyChangePtr->IsValid())
		{
			(*HierarchyChangePtr)->TryGetStringField(TEXT("before_parent"), Target.BeforeParent);
			(*HierarchyChangePtr)->TryGetStringField(TEXT("after_parent"), Target.AfterParent);
		}
		if (Target.BeforeParent.IsEmpty())
		{
			Target.BeforeParent = ReadStringField(BeforeComponentObject, TEXT("parent"));
		}
		if (Target.AfterParent.IsEmpty())
		{
			Target.AfterParent = ReadStringField(AfterComponentObject, TEXT("parent"));
		}

		const TSharedPtr<FJsonObject>* RootChangePtr = nullptr;
		if (StepResult.Data->TryGetObjectField(TEXT("root_change"), RootChangePtr) &&
			RootChangePtr &&
			RootChangePtr->IsValid())
		{
			(*RootChangePtr)->TryGetStringField(TEXT("before_root"), Target.BeforeRoot);
			(*RootChangePtr)->TryGetStringField(TEXT("after_root"), Target.AfterRoot);
		}
		StepResult.Data->TryGetStringField(TEXT("delete_policy"), Target.DeletePolicy);

		const TArray<TSharedPtr<FJsonValue>>* DeletedComponentIdsPtr = nullptr;
		if (StepResult.Data->TryGetArrayField(TEXT("deleted_component_ids"), DeletedComponentIdsPtr) && DeletedComponentIdsPtr)
		{
			Target.DeletedComponentIdsJson = SerializeJsonArray(*DeletedComponentIdsPtr);
		}
		const TArray<TSharedPtr<FJsonValue>>* MovedComponentIdsPtr = nullptr;
		if (StepResult.Data->TryGetArrayField(TEXT("moved_component_ids"), MovedComponentIdsPtr) && MovedComponentIdsPtr)
		{
			Target.MovedComponentIdsJson = SerializeJsonArray(*MovedComponentIdsPtr);
		}

		TArray<TSharedPtr<FJsonValue>> EmptyProperties;
		if (BeforeComponentObject.IsValid())
		{
			Target.BeforeSnapshotJson = SerializeJsonObject(
				MakeComponentSnapshot(BeforeComponentObject, EmptyProperties, true));
		}
		if (AfterComponentObject.IsValid())
		{
			Target.AfterSnapshotJson = SerializeJsonObject(
				MakeComponentSnapshot(AfterComponentObject, EmptyProperties, false));
		}
		else if (StepResult.Operation == TEXT("remove_component") && BeforeComponentObject.IsValid())
		{
			Target.AfterSnapshotJson = SerializeJsonObject(
				MakeDeletedComponentSnapshot(BeforeComponentObject));
		}
	}
};

FBlueprintHelperComponentTaskRuntimeCluster::FBlueprintHelperComponentTaskRuntimeCluster(
	const FBlueprintHelperComponentService& InComponentService)
	: ComponentService(InComponentService)
{
}

bool FBlueprintHelperComponentTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent;
}

bool FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	const bool bBuilt = StepResult.bOk && FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
		LoweredStep,
		ArchiveSessionId,
		TaskRunId,
		StepIndex,
		OutEvidence);
	if (!bBuilt)
	{
		return false;
	}

	if (LoweredStep.AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties)
	{
		FBlueprintHelperComponentReviewEvidenceLocalUtils::EnrichSetComponentPropertiesEvidence(
			StepResult,
			OutEvidence);
	}
	else
	{
		FBlueprintHelperComponentReviewEvidenceLocalUtils::EnrichComponentMutationEvidence(
			StepResult,
			OutEvidence);
	}
	return true;
}

FBlueprintHelperToolResultBase FBlueprintHelperComponentTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteComponentTaskPlanStep(
		ComponentService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
