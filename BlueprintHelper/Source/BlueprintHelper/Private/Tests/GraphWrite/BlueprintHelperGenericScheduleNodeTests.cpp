#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
static FString MakeGenericScheduleNodeName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeGenericScheduleNodeBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperGenericScheduleNode/%s"),
		*MakeGenericScheduleNodeName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeGenericScheduleNodeName(TEXT("BP_GenericScheduleNode")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperGenericScheduleNodeTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGenericScheduleNodeRequiresProjectedSpawnerEvidenceTest,
	"BlueprintHelper.GraphWrite.GenericOps.Schedule.GenericNodeRequiresProjectedEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGenericScheduleNodeRequiresProjectedSpawnerEvidenceTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeGenericScheduleNodeBlueprint();
	UEdGraph* Graph = Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	TestNotNull(TEXT("blueprint"), Blueprint);
	TestNotNull(TEXT("graph"), Graph);

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction;
	Request.Blueprint = Blueprint;
	Request.TargetGraph = Graph;
	Request.StatementId = MakeGenericScheduleNodeName(TEXT("Stmt"));
	Request.ProjectedContextHash = TEXT("generic_schedule_node_projected_context");
	Request.SemanticConstraintsHash = TEXT("generic_schedule_node_semantic_constraints");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::Schedule;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Schedule;
	Request.Semantic.ScheduleOperation = TEXT("timer_delegate_node");
	Request.MaxCandidates = 4;

	const FBlueprintHelperActionResolutionResult Result = FBlueprintHelperActionResolutionCore::Resolve(Request);
	TestEqual(TEXT("timer_delegate_node without projection is invalid"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
	TestEqual(TEXT("projected schedule evidence required"), Result.ErrorCode, FString(TEXT("schedule_spawner_evidence_missing")));
	TestFalse(TEXT("missing evidence has no fake spawner"), Result.SelectedSpawner.IsValid());
	return true;
}

#endif
