#include "Runtime/TaskRuntime/Review/BlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry.h"

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Clusters/AssetFactory/BlueprintHelperAssetFactoryTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/BlueprintVariables/BlueprintHelperBlueprintVariablesTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ClassSettings/BlueprintHelperClassSettingsTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Component/BlueprintHelperComponentTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/DataTable/BlueprintHelperDataTableTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/GraphWrite/BlueprintHelperGraphWriteTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/ObjectProperty/BlueprintHelperObjectPropertyTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/Signature/BlueprintHelperSignatureTaskRuntimeCluster.h"
#include "Runtime/TaskRuntime/Clusters/UMGWidget/BlueprintHelperUMGWidgetTaskRuntimeCluster.h"

class FBlueprintHelperTaskRuntimeReviewEvidenceBuilderCatalog
{
public:
	using FBuilder = bool (*)(
		const FBlueprintHelperTaskRuntimeLoweredStep&,
		const FBlueprintHelperToolResultBase&,
		const FString&,
		const FString&,
		int32,
		FBlueprintHelperWriteReviewEvidence&);

	struct FEntry
	{
		EBlueprintHelperTaskRuntimeCluster Cluster;
		FBuilder Builder = nullptr;
	};

	static const TArray<FEntry>& Get()
	{
		static const TArray<FEntry> Entries = {
			{EBlueprintHelperTaskRuntimeCluster::GraphWrite, &FBlueprintHelperGraphWriteTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::BlueprintVariables, &FBlueprintHelperBlueprintVariablesTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::AssetFactory, &FBlueprintHelperAssetFactoryTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::Component, &FBlueprintHelperComponentTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::ClassSettings, &FBlueprintHelperClassSettingsTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::Signature, &FBlueprintHelperSignatureTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::UMGWidget, &FBlueprintHelperUMGWidgetTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::DataTable, &FBlueprintHelperDataTableTaskRuntimeCluster::BuildReviewEvidence},
			{EBlueprintHelperTaskRuntimeCluster::ObjectProperty, &FBlueprintHelperObjectPropertyTaskRuntimeCluster::BuildReviewEvidence}
		};
		return Entries;
	}
};

bool FBlueprintHelperTaskRuntimeReviewEvidenceBuilderRegistry::TryBuild(
	EBlueprintHelperTaskRuntimeCluster Cluster,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	const FString& ArchiveSessionId,
	const FString& TaskRunId,
	int32 StepIndex,
	FBlueprintHelperWriteReviewEvidence& OutEvidence)
{
	for (const FBlueprintHelperTaskRuntimeReviewEvidenceBuilderCatalog::FEntry& Entry :
		FBlueprintHelperTaskRuntimeReviewEvidenceBuilderCatalog::Get())
	{
		if (Entry.Cluster == Cluster && Entry.Builder)
		{
			return Entry.Builder(
				LoweredStep,
				StepResult,
				ArchiveSessionId,
				TaskRunId,
				StepIndex,
				OutEvidence);
		}
	}
	return false;
}
