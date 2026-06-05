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
#include "Shared/BlueprintHelperVersionCompat.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils
{
public:
	static FString MakeTestObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint(const FString& Prefix)
	{
		UPackage* Package = CreatePackage(*FString::Printf(
			TEXT("/Game/BlueprintHelperGraphWriteRollback/%s"),
			*MakeTestObjectName(Prefix)));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeTestObjectName(TEXT("BP_RollbackFinalizer")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperGraphWriteRollbackFinalizerTests"));
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

	static UEdGraphNode* FirstValidNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static TSharedRef<FJsonObject> MakeStringLiteralExpression(const FString& Value)
	{
		TSharedRef<FJsonObject> Literal = MakeShared<FJsonObject>();
		Literal->SetStringField(TEXT("kind"), TEXT("literal"));
		Literal->SetStringField(TEXT("value_type"), TEXT("string"));
		Literal->SetStringField(TEXT("value"), Value);
		return Literal;
	}

	static TSharedRef<FJsonObject> MakeAppendLogicSpec(const FString& EventName)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("id"), EventName + TEXT("_entry"));
		Entry->SetStringField(TEXT("kind"), TEXT("custom_event"));
		Entry->SetStringField(TEXT("name"), EventName);
		Entry->SetStringField(TEXT("source_cluster"), TEXT("blueprint_signature"));
		Entry->SetStringField(TEXT("signature_evidence_id"), EventName + TEXT("_signature_evidence"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("InString"), MakeStringLiteralExpression(TEXT("rollback finalizer")));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		LogicSpec->SetObjectField(TEXT("entry"), Entry);
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeAppendExecutePayload(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph"), GraphName);

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetObjectField(TEXT("target"), Target);
		Payload->SetBoolField(TEXT("dry_run"), false);
		Payload->SetBoolField(TEXT("allow_existing_graph"), true);
		Payload->SetStringField(TEXT("feature_name"), TEXT("RollbackFinalizer"));
		Payload->SetObjectField(TEXT("logic_spec"), MakeAppendLogicSpec(EventName));
		return Payload;
	}

	static FBlueprintHelperToolResultBase ExecuteAppendWithForcedOwnershipFailure(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		FBlueprintHelperAppendBlueprintGraphService::SetAutomationOwnershipWriteFailure(
			true,
			TEXT("forced automation ownership failure"));

		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperBlockIdService BlockIdService;
		FBlueprintHelperOwnershipService OwnershipService;
		const FBlueprintHelperAppendBlueprintGraphService AppendService(
			Resolver,
			BlockIdService,
			OwnershipService);
		const FBlueprintHelperToolResultBase Result = AppendService.Execute(
			MakeAppendExecutePayload(AssetPath, GraphName, EventName));

		FBlueprintHelperAppendBlueprintGraphService::SetAutomationOwnershipWriteFailure(false, FString());
		return Result;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAppendRollbackRemovesNewGraphOnOwnershipFailureTest,
	"BlueprintHelper.GraphWrite.Rollback.AppendRemovesNewGraphOnOwnershipFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAppendRollbackRemovesNewGraphOnOwnershipFailureTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::MakeBlueprint(TEXT("NewGraphOwnershipFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint)
	{
		return false;
	}

	const FString GraphName = TEXT("RollbackNewGraph");
	const int32 GraphCountBefore = Blueprint->UbergraphPages.Num();
	const bool bDirtyBefore = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false;

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::ExecuteAppendWithForcedOwnershipFailure(
		Blueprint->GetPathName(),
		GraphName,
		TEXT("RollbackNewGraphEvent"));

	TestFalse(TEXT("forced ownership failure makes append fail"), Result.bOk);
	TestEqual(TEXT("rollback removes newly-created graph"), Blueprint->UbergraphPages.Num(), GraphCountBefore);
	TestNull(TEXT("new graph is absent after rollback"),
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::FindUbergraphPageByName(Blueprint, GraphName));
	TestEqual(TEXT("rollback restores package dirty flag"),
		Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false,
		bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperAppendRollbackRestoresExistingGraphNodesOnOwnershipFailureTest,
	"BlueprintHelper.GraphWrite.Rollback.AppendRestoresExistingGraphNodesOnOwnershipFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperAppendRollbackRestoresExistingGraphNodesOnOwnershipFailureTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::MakeBlueprint(TEXT("ExistingGraphOwnershipFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraph* Graph = Blueprint->UbergraphPages[0];
	const FString GraphName = Graph->GetName();
	const int32 NodeCountBefore = Graph->Nodes.Num();
	const TSet<FGuid> NodeGuidsBefore =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::CaptureNodeGuids(Graph);
	const bool bDirtyBefore = Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false;

	const FBlueprintHelperToolResultBase Result =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::ExecuteAppendWithForcedOwnershipFailure(
		Blueprint->GetPathName(),
		GraphName,
		TEXT("RollbackExistingGraphEvent"));

	TestFalse(TEXT("forced ownership failure makes append fail"), Result.bOk);
	TestEqual(TEXT("rollback preserves existing graph node count"), Graph->Nodes.Num(), NodeCountBefore);
	TestTrue(TEXT("rollback preserves existing graph node GUID set"),
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::CaptureNodeGuids(Graph).Difference(NodeGuidsBefore).Num() == 0);
	TestTrue(TEXT("rollback preserves existing graph node GUID set reverse"),
		NodeGuidsBefore.Difference(FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::CaptureNodeGuids(Graph)).Num() == 0);
	TestEqual(TEXT("rollback restores package dirty flag"),
		Blueprint->GetOutermost() ? Blueprint->GetOutermost()->IsDirty() : false,
		bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperOwnershipWriteRestoresMetadataOnPartialFailureTest,
	"BlueprintHelper.GraphWrite.Rollback.OwnershipWriteRestoresMetadataOnPartialFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperOwnershipWriteRestoresMetadataOnPartialFailureTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::MakeBlueprint(TEXT("PartialOwnershipFailure"));
	TestNotNull(TEXT("test blueprint is created"), Blueprint);
	if (!Blueprint || Blueprint->UbergraphPages.Num() == 0 || !Blueprint->UbergraphPages[0])
	{
		return false;
	}

	UEdGraphNode* ExistingNode =
		FBlueprintHelperGraphWriteRollbackFinalizerTestLocalUtils::FirstValidNode(Blueprint->UbergraphPages[0]);
	TestNotNull(TEXT("existing graph node is available"), ExistingNode);
	if (!ExistingNode)
	{
		return false;
	}

	UPackage* Package = ExistingNode->GetOutermost();
	TestNotNull(TEXT("node package is available"), Package);
	if (!Package)
	{
		return false;
	}

	FBlueprintHelperPackageMetaData& MetaData = FBlueprintHelperVersionCompat::GetPackageMetaData(Package);
	MetaData.SetValue(ExistingNode, TEXT("BlueprintHelperOwned"), TEXT("true"));
	MetaData.SetValue(ExistingNode, TEXT("BlueprintHelperBlockId"), TEXT("OriginalBlock"));
	MetaData.SetValue(ExistingNode, TEXT("BlueprintHelperFeatureName"), TEXT("OriginalFeature"));
	MetaData.SetValue(ExistingNode, TEXT("BlueprintHelperTool"), TEXT("OriginalTool"));

	TArray<UEdGraphNode*> Nodes;
	Nodes.Add(ExistingNode);
	Nodes.Add(nullptr);

	FString OwnershipError;
	const FBlueprintHelperOwnershipService OwnershipService;
	const bool bWriteSucceeded = OwnershipService.WriteBlockOwnership(
		Blueprint,
		Nodes,
		TEXT("NewBlock"),
		TEXT("NewFeature"),
		OwnershipError);

	TestFalse(TEXT("null node makes ownership write fail"), bWriteSucceeded);
	TestEqual(TEXT("owned metadata is restored after partial failure"),
		MetaData.GetValue(ExistingNode, TEXT("BlueprintHelperOwned")),
		FString(TEXT("true")));
	TestEqual(TEXT("block id metadata is restored after partial failure"),
		MetaData.GetValue(ExistingNode, TEXT("BlueprintHelperBlockId")),
		FString(TEXT("OriginalBlock")));
	TestEqual(TEXT("feature metadata is restored after partial failure"),
		MetaData.GetValue(ExistingNode, TEXT("BlueprintHelperFeatureName")),
		FString(TEXT("OriginalFeature")));
	TestEqual(TEXT("tool metadata is restored after partial failure"),
		MetaData.GetValue(ExistingNode, TEXT("BlueprintHelperTool")),
		FString(TEXT("OriginalTool")));
	return true;
}

#endif
