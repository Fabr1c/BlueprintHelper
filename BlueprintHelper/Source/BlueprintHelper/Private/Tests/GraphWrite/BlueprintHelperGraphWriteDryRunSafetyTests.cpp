#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils
{
public:
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWriteDryRun/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_DryRunSafety")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphWriteDryRunSafetyTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* FindUbergraphPageByName(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Page : Blueprint->UbergraphPages)
		{
			if (Page && Page->GetName() == GraphName)
			{
				return Page;
			}
		}
		return nullptr;
	}

	static TSet<FGuid> CaptureNodeGuids(UEdGraph* Graph)
	{
		TSet<FGuid> NodeGuids;
		if (!Graph)
		{
			return NodeGuids;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				NodeGuids.Add(Node->NodeGuid);
			}
		}
		return NodeGuids;
	}

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakePrintStringLogicSpec(const FString& EventName)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), EventName + TEXT("_entry"));
		Entry->SetStringField(TEXT("kind"), TEXT("custom_event"));
		Entry->SetStringField(TEXT("event_kind"), TEXT("custom_event"));
		Entry->SetStringField(TEXT("name"), EventName);
		Entry->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
		Entry->SetStringField(TEXT("signature_evidence_id"), EventName + TEXT("_signature_evidence"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(TEXT("dry-run safety")));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetObjectField(TEXT("entry"), Entry);
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeAppendDryRunPayload(const FString& AssetPath, const FString& GraphName)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetBoolField(TEXT("dry_run"), true);
		Payload->SetBoolField(TEXT("allow_existing_graph"), true);
		Payload->SetObjectField(TEXT("logic_spec"), MakePrintStringLogicSpec(GraphName + TEXT("_DryRunEvent")));
		return Payload;
	}

	static FBlueprintHelperToolResultBase ExecuteAppendDryRun(const FString& AssetPath, const FString& GraphName)
	{
		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		const FBlueprintHelperAppendBlueprintGraphService AppendService(
			Resolver,
			BlockIdService,
			OwnershipService);
		return AppendService.Execute(MakeAppendDryRunPayload(AssetPath, GraphName));
	}

	static void AssertSandboxProofFields(
		FAutomationTestBase& Test,
		const FBlueprintHelperToolResultBase& Result)
	{
		Test.TestTrue(TEXT("dry-run result has data"), Result.Data.IsValid());
		if (!Result.Data.IsValid())
		{
			return;
		}

		FString SideEffects;
		FString Sandbox;
		Test.TestTrue(TEXT("dry-run reports no side effects"),
			Result.Data->TryGetStringField(TEXT("dry_run_side_effects"), SideEffects) &&
			SideEffects == TEXT("none"));
		Test.TestTrue(TEXT("dry-run reports transient blueprint duplicate sandbox"),
			Result.Data->TryGetStringField(TEXT("sandbox"), Sandbox) &&
			Sandbox == TEXT("transient_blueprint_duplicate"));
		Test.TestTrue(TEXT("dry-run reports generated node count"),
			Result.Data->HasTypedField<EJson::Number>(TEXT("generated_node_count")));
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAppendDryRunDoesNotCreateMissingGraphTest,
	"BlueprintHelper.GraphWrite.DryRun.AppendDoesNotCreateMissingGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAppendDryRunDoesNotCreateMissingGraphTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::MakeBlueprint(TEXT("MissingGraph"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString MissingGraphName = TEXT("DryRunMissingGraph");
	const int32 GraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false;

	const FBlueprintHelperToolResultBase Result = FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::ExecuteAppendDryRun(
		Blueprint->GetPathName(),
		MissingGraphName);

	TestTrue(TEXT("append dry-run succeeds"), Result.bOk);
	FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::AssertSandboxProofFields(*this, Result);
	TestEqual(TEXT("dry-run does not add a live graph"), Blueprint->UbergraphPages.Num(), GraphCountBefore);
	TestNull(TEXT("missing graph remains absent after dry-run"),
		FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::FindUbergraphPageByName(Blueprint, MissingGraphName));
	TestEqual(TEXT("dry-run restores package dirty flag"),
		Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false,
		bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAppendDryRunDoesNotChangeExistingGraphNodesTest,
	"BlueprintHelper.GraphWrite.DryRun.AppendDoesNotChangeExistingGraphNodes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAppendDryRunDoesNotChangeExistingGraphNodesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::MakeBlueprint(TEXT("ExistingGraph"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString GraphName = Graph->GetName();
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const TSet<FGuid> NodeGuidsBefore =
		FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::CaptureNodeGuids(Graph);
	const bool bDirtyBefore = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false;

	const FBlueprintHelperToolResultBase Result = FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::ExecuteAppendDryRun(
		Blueprint->GetPathName(),
		GraphName);

	TestTrue(TEXT("append dry-run succeeds"), Result.bOk);
	FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::AssertSandboxProofFields(*this, Result);
	TestEqual(TEXT("dry-run preserves existing graph node count"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("dry-run preserves existing graph node GUID set"),
		FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::CaptureNodeGuids(Graph).Difference(NodeGuidsBefore).Num() == 0);
	TestTrue(TEXT("dry-run preserves existing graph node GUID set reverse"),
		NodeGuidsBefore.Difference(FBlueprintHelperGraphWriteDryRunSafetyTestLocalUtils::CaptureNodeGuids(Graph)).Num() == 0);
	TestEqual(TEXT("dry-run restores package dirty flag"),
		Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false,
		bDirtyBefore);
	return true;
}

#endif
