// BlueprintHelper TaskRuntime - ClassSettings static cluster.

#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"

#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Shared/Review/BlueprintHelperReviewTypes.h"
#include "Shared/BlueprintClassSettings/BlueprintHelperClassDefaultMutationTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

class FBlueprintHelperClassSettingsReviewEvidenceLocalUtils
{
public:
	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object)
	{
		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Object, Writer);
		return JsonText;
	}

	static FString MakeSnapshotHash(const FString& SnapshotJson)
	{
		return SnapshotJson.IsEmpty()
			? FString()
			: FString::Printf(TEXT("%08x"), GetTypeHash(SnapshotJson));
	}

	static TSharedRef<FJsonObject> MakeBeforeSnapshot(
		const FBlueprintHelperClassDefaultSetterMutationEvidence& Evidence)
	{
		FBlueprintHelperClassDefaultSetterMutationEvidence Before = Evidence;
		Before.InputValue = Evidence.BeforeValue;
		Before.AfterValue = Evidence.BeforeValue;
		return Before.ToJson();
	}

	static void EnrichSetterMutationEvidence(
		const FBlueprintHelperToolResultBase& StepResult,
		FBlueprintHelperWriteReviewEvidence& OutEvidence)
	{
		if (!StepResult.Data.IsValid() || OutEvidence.AtomicTargets.Num() == 0)
		{
			return;
		}

		const TSharedPtr<FJsonObject>* DefaultResult = nullptr;
		if (!StepResult.Data->TryGetObjectField(TEXT("default_property_result"), DefaultResult) ||
			!DefaultResult ||
			!DefaultResult->IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* EvidenceValues = nullptr;
		if (!(*DefaultResult)->TryGetArrayField(TEXT("setter_mutation_evidence"), EvidenceValues) ||
			!EvidenceValues)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& EvidenceValue : *EvidenceValues)
		{
			FBlueprintHelperClassDefaultSetterMutationEvidence SetterEvidence;
			if (!FBlueprintHelperClassDefaultSetterMutationEvidence::FromJson(
				EvidenceValue.IsValid() ? EvidenceValue->AsObject() : nullptr,
				SetterEvidence))
			{
				continue;
			}

			FBlueprintHelperReviewAtomicTarget* Target = OutEvidence.AtomicTargets.FindByPredicate(
				[&SetterEvidence](const FBlueprintHelperReviewAtomicTarget& Candidate)
				{
					return Candidate.TargetKind.Equals(TEXT("class_default_setter_property"), ESearchCase::IgnoreCase) &&
						Candidate.PropertyPath.Equals(SetterEvidence.PropertyPath, ESearchCase::IgnoreCase);
				});
			if (!Target)
			{
				continue;
			}

			const FString BeforeJson = SerializeJsonObject(MakeBeforeSnapshot(SetterEvidence));
			const FString AfterJson = SerializeJsonObject(SetterEvidence.ToJson());
			Target->TargetSubKind = TEXT("setter_aware_property");
			Target->ComponentPath = SetterEvidence.OwnerObjectPath;
			Target->BeforeSnapshotJson = BeforeJson;
			Target->AfterSnapshotJson = AfterJson;
			Target->AnchorJson = AfterJson;
			Target->ReadbackFingerprintBefore = MakeSnapshotHash(BeforeJson);
			Target->ReadbackFingerprintAfter = MakeSnapshotHash(AfterJson);
			Target->RecordedAfterHash = Target->ReadbackFingerprintAfter;
		}
	}
};

FBlueprintHelperClassSettingsTaskRuntimeCluster::FBlueprintHelperClassSettingsTaskRuntimeCluster(
	const FBlueprintHelperClassSettingsService& InClassSettingsService)
	: ClassSettingsService(InClassSettingsService)
{
}

bool FBlueprintHelperClassSettingsTaskRuntimeCluster::CanExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
{
	return LoweredStep.Capability == FBlueprintHelperClassSettingsTaskPlanAdapter::CapabilityName;
}

bool FBlueprintHelperClassSettingsTaskRuntimeCluster::BuildReviewEvidence(
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
	if (bBuilt)
	{
		FBlueprintHelperClassSettingsReviewEvidenceLocalUtils::EnrichSetterMutationEvidence(StepResult, OutEvidence);
	}
	return bBuilt;
}

FBlueprintHelperToolResultBase FBlueprintHelperClassSettingsTaskRuntimeCluster::ExecuteStep(
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) const
{
	return FBlueprintHelperTaskRuntimeClusterExecutionUtils::ExecuteClassSettingsTaskPlanStep(
		ClassSettingsService,
		LoweredStep.AdapterOperation,
		LoweredStep.Payload);
}
