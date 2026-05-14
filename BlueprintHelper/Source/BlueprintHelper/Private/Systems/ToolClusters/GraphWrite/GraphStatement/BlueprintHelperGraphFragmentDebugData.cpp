#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDebugData.h"

#include "Dom/JsonObject.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

TSharedPtr<FJsonObject> FBlueprintHelperGraphFragmentDebugData::BuildFromLogicSpec(
	const TSharedPtr<FJsonObject>& LogicSpec,
	UBlueprint* Blueprint)
{
	if (!LogicSpec.IsValid())
	{
		return nullptr;
	}

	FBlueprintHelperGraphSemanticIR SemanticIR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(LogicSpec, Blueprint, SemanticIR);

	FBlueprintHelperGraphFragmentDag Dag;
	FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(SemanticIR, Dag);

	FBlueprintHelperGraphFragmentEvidenceBundle Evidence =
		FBlueprintHelperGraphFragmentEvidenceBuilder::BuildFromDag(Dag);

	TSharedRef<FJsonObject> FragmentArtifacts = MakeShared<FJsonObject>();
	FragmentArtifacts->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.DebugFragmentArtifacts.v1"));
	FragmentArtifacts->SetNumberField(TEXT("fragment_count"), Dag.Fragments.Num());
	FragmentArtifacts->SetNumberField(TEXT("evidence_fragment_count"), Evidence.Fragments.Num());

	TSharedRef<FJsonObject> FragmentDebug = MakeShared<FJsonObject>();
	FragmentDebug->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.GraphFragmentDebugData.v1"));
	FragmentDebug->SetObjectField(TEXT("fragment_dag"), Dag.ToJson());
	FragmentDebug->SetObjectField(TEXT("fragment_evidence"), Evidence.ToJson());
	FragmentDebug->SetObjectField(TEXT("fragment_artifacts"), FragmentArtifacts);
	return FragmentDebug;
}

void FBlueprintHelperGraphFragmentDebugData::AttachToData(
	TSharedPtr<FJsonObject>& Data,
	const TSharedPtr<FJsonObject>& FragmentDebugData)
{
	if (!FragmentDebugData.IsValid())
	{
		return;
	}

	if (!Data.IsValid())
	{
		Data = MakeShared<FJsonObject>();
	}

	Data->SetObjectField(TEXT("fragment_debug"), FragmentDebugData);
}
