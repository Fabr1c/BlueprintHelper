#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionResolver.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperContainerActionVocabulary.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphWriteConnectivityContext.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Testing/BlueprintHelperContainerActionReadbackVerifier.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

namespace
{
static const TMap<FString, TArray<FString>>& ExpectedOperationsByKind()
{
	static const TMap<FString, TArray<FString>> Operations = {
		{ TEXT("array"), {
			TEXT("get"),
			TEXT("set"),
			TEXT("add"),
			TEXT("add_unique"),
			TEXT("append"),
			TEXT("insert"),
			TEXT("remove_item"),
			TEXT("remove_index"),
			TEXT("clear"),
			TEXT("contains"),
			TEXT("find"),
			TEXT("length"),
			TEXT("shuffle"),
			TEXT("shuffle_from_stream"),
			TEXT("identical"),
			TEXT("resize"),
			TEXT("reverse"),
			TEXT("is_empty"),
			TEXT("is_not_empty"),
			TEXT("last_index"),
			TEXT("swap"),
			TEXT("filter_array"),
			TEXT("is_valid_index"),
			TEXT("random"),
			TEXT("random_from_stream"),
			TEXT("sort_string"),
			TEXT("sort_name"),
			TEXT("sort_byte"),
			TEXT("sort_int"),
			TEXT("sort_int64"),
			TEXT("sort_float"),
		} },
		{ TEXT("map"), {
			TEXT("add"),
			TEXT("remove"),
			TEXT("find"),
			TEXT("contains"),
			TEXT("keys"),
			TEXT("values"),
			TEXT("clear"),
			TEXT("length"),
			TEXT("is_empty"),
			TEXT("is_not_empty"),
			TEXT("get_key_value_by_index"),
			TEXT("get_last_index"),
		} },
		{ TEXT("set"), {
			TEXT("add"),
			TEXT("remove"),
			TEXT("contains"),
			TEXT("clear"),
			TEXT("length"),
			TEXT("to_array"),
			TEXT("add_items"),
			TEXT("remove_items"),
			TEXT("is_empty"),
			TEXT("is_not_empty"),
			TEXT("intersection"),
			TEXT("union"),
			TEXT("difference"),
			TEXT("get_item_by_index"),
			TEXT("get_last_index"),
		} },
	};
	return Operations;
}

static FString Normalize(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static UFunction* ResolveStableUFunctionPath(const FString& StablePath)
{
	int32 ColonIndex = INDEX_NONE;
	if (!StablePath.FindLastChar(TEXT(':'), ColonIndex) || ColonIndex <= 0 || ColonIndex >= StablePath.Len() - 1)
	{
		return nullptr;
	}

	const FString OwnerPath = StablePath.Left(ColonIndex).TrimStartAndEnd();
	const FString FunctionName = StablePath.Mid(ColonIndex + 1).TrimStartAndEnd();
	UClass* OwnerClass = FindObject<UClass>(nullptr, *OwnerPath);
	if (!OwnerClass)
	{
		OwnerClass = LoadObject<UClass>(nullptr, *OwnerPath);
	}
	return OwnerClass ? OwnerClass->FindFunctionByName(FName(*FunctionName)) : nullptr;
}

static bool HasParamNamed(const UFunction* Function, const FString& ParamName)
{
	if (!Function)
	{
		return false;
	}

	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & CPF_Parm); ++It)
	{
		const FProperty* Property = *It;
		if (Property && Property->GetName().Equals(ParamName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

static FBlueprintHelperCallFunctionPinType MakePinType(const FString& Category, const FString& ContainerType = FString())
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Category;
	PinType.ContainerType = ContainerType;
	return PinType;
}

static FEdGraphTerminalType MakeTerminalType(const FName Category)
{
	FEdGraphTerminalType TerminalType;
	TerminalType.TerminalCategory = Category;
	return TerminalType;
}

static FEdGraphPinType MakeBlueprintPinType(
	const FName Category,
	const EPinContainerType ContainerType,
	const FEdGraphTerminalType& ValueType = FEdGraphTerminalType())
{
	return FEdGraphPinType(Category, NAME_None, nullptr, ContainerType, false, ValueType);
}

static FString MakeObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UBlueprint* MakeBlueprint()
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperContainerActionCoverage/%s"),
		*MakeObjectName(TEXT("Pkg"))));
	Package->SetDirtyFlag(false);

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeObjectName(TEXT("BP_ContainerCoverage")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperContainerActionCoverageExtensionTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* FindEventGraph(UBlueprint* Blueprint)
{
	return Blueprint && Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
}

static bool AddVariable(UBlueprint* Blueprint, const FString& Name, const FEdGraphPinType& Type)
{
	if (!Blueprint || !FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), Type))
	{
		return false;
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

static UK2Node_CustomEvent* AddCustomEvent(UEdGraph* Graph, const FString& EventName)
{
	if (!Graph || EventName.IsEmpty())
	{
		return nullptr;
	}

	UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
	Graph->AddNode(EventNode, true, false);
	EventNode->CreateNewGuid();
	EventNode->PostPlacedNewNode();
	EventNode->CustomFunctionName = FName(*EventName);
	EventNode->AllocateDefaultPins();
	return EventNode;
}

static FString MakeLogicJson(const FString& StatementJson, const FString& EntryName)
{
	return FString::Printf(TEXT(R"JSON({
		"options": { "reconstruct_existing_nodes": true },
		"logic_spec": {
			"schema": "BlueprintLogicSpec.v2",
			"entry": {
				"kind": "custom_event",
				"name": "%s",
				"id": "%s_entry",
				"signature_evidence_id": "signature:custom_event:%s",
				"signature_dependency": true,
				"source": "signature_dependency",
				"source_cluster": "blueprint_signature"
			},
			"statements": [%s]
		}
	})JSON"), *EntryName, *EntryName, *EntryName, *StatementJson);
}

static bool RunFixture(
	FAutomationTestBase& Test,
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FString& TestName,
	const FString& StatementJson,
	const FBlueprintHelperContainerActionReadbackExpectation& Expectation)
{
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FString EntryName = MakeObjectName(TEXT("BH_ContainerCoverageEntry"));
	Test.TestNotNull(
		*FString::Printf(TEXT("%s entry event"), *TestName),
		AddCustomEvent(Graph, EntryName));
	const FString GraphWriteJson = MakeLogicJson(StatementJson, EntryName);
	const FBlueprintGraphWriteConnectivityValidationInput ConnectivityInput =
		FBlueprintHelperGraphWriteConnectivityContextBuilder::BuildSemanticGenerationContext(
			Graph,
			TEXT("k2.automation.container_action_coverage"),
			TEXT("automation_container_action_coverage"),
			GraphWriteJson);
	const FBlueprintGenerateResult Result =
		FBlueprintGraphGenerationPipeline::GenerateBlueprintFromJson(
			Graph,
			GraphWriteJson,
			Unresolved,
			ConnectivityInput);
	if (!Test.TestTrue(*FString::Printf(TEXT("%s generation succeeds"), *TestName), Result.bSucceed))
	{
		Test.AddError(FString::Printf(
			TEXT("%s result: %s generated=%d unresolved=%d connectivity=%d requested_links=%d created_links=%d"),
			*TestName,
			*Result.Message,
			Result.GeneratedNodeCount,
			Result.UnresolvedNodeCount,
			Result.ConnectivityViolationCount,
			Result.RequestedConnectionCount,
			Result.CreatedConnectionCount));
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : Result.ConnectivityDiagnostics)
		{
			Test.AddError(FString::Printf(
				TEXT("%s connectivity: %s %s"),
				*TestName,
				*Diagnostic.Code,
				*Diagnostic.Message));
		}
		for (const TSharedPtr<FUnresolvedNodeItem>& Item : Unresolved)
		{
			if (Item.IsValid())
			{
				Test.AddError(FString::Printf(TEXT("%s unresolved: %s - %s"), *TestName, *Item->DisplayText, *Item->Reason));
			}
		}
		return false;
	}

	FString ReadbackFailure;
	if (!Test.TestTrue(
		*FString::Printf(TEXT("%s readback passes"), *TestName),
		FBlueprintHelperContainerActionReadbackVerifier::Verify(Blueprint, Graph, Expectation, ReadbackFailure)))
	{
		Test.AddError(ReadbackFailure);
		return false;
	}
	return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionCoverageVocabularyExtensionTest,
	"BlueprintHelper.GraphWrite.ContainerAction.CoverageExtension.Vocabulary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionCoverageVocabularyExtensionTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperContainerActionSpec> Specs = FBlueprintHelperContainerActionVocabulary::All();
	TestEqual(TEXT("expanded operation count"), Specs.Num(), 58);

	for (const TPair<FString, TArray<FString>>& Pair : ExpectedOperationsByKind())
	{
		for (const FString& Operation : Pair.Value)
		{
			const FBlueprintHelperContainerActionSpec* Spec =
				FBlueprintHelperContainerActionVocabulary::Find(Pair.Key, Operation);
			TestNotNull(*FString::Printf(TEXT("spec exists for %s.%s"), *Pair.Key, *Operation), Spec);
			if (!Spec)
			{
				continue;
			}

			TestEqual(TEXT("spec container kind"), Normalize(Spec->ContainerKind), Normalize(Pair.Key));
			TestEqual(TEXT("spec operation"), Normalize(Spec->ContainerOperation), Normalize(Operation));
			TestFalse(TEXT("stable ufunction path present"), Spec->StableUFunctionPath.IsEmpty());
			TestTrue(TEXT("stable ufunction path resolves"), ResolveStableUFunctionPath(Spec->StableUFunctionPath) != nullptr);
			TestTrue(TEXT("readback roles present"), Spec->ReadbackPinRoles.Num() > 0);
			TestTrue(TEXT("wildcard policy declared"), Spec->WildcardPolicy.TypedRoles.Num() > 0);
			TestTrue(TEXT("role bindings present"), Spec->RoleBindings.Num() > 0);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionCoverageMissingEvidenceTest,
	"BlueprintHelper.GraphWrite.ContainerAction.CoverageExtension.MissingEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionCoverageMissingEvidenceTest::RunTest(const FString& Parameters)
{
	{
		FBlueprintHelperActionResolutionRequest Request;
		Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Request.StatementId = TEXT("array-add-missing-element");
		Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
		Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
		Request.Semantic.ContainerKind = TEXT("array");
		Request.Semantic.ContainerOperation = TEXT("add");
		Request.Semantic.ArgumentNames = { TEXT("target"), TEXT("item") };
		Request.Semantic.ArgumentPinTypes.Add(TEXT("target"), MakePinType(TEXT("int"), TEXT("array")));

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperContainerActionResolver::Resolve(Request);
		TestEqual(TEXT("array add missing element status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestEqual(TEXT("array add missing element code"), Result.ErrorCode, FString(TEXT("container_action_type_evidence_missing")));
		TestEqual(TEXT("array add owner stays function"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	}

	{
		FBlueprintHelperActionResolutionRequest Request;
		Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Request.StatementId = TEXT("map-add-missing-key-value");
		Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
		Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
		Request.Semantic.ContainerKind = TEXT("map");
		Request.Semantic.ContainerOperation = TEXT("add");
		Request.Semantic.ArgumentNames = { TEXT("target"), TEXT("key"), TEXT("value") };
		Request.Semantic.ArgumentPinTypes.Add(TEXT("target"), MakePinType(TEXT("string"), TEXT("map")));

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperContainerActionResolver::Resolve(Request);
		TestEqual(TEXT("map add missing key/value status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestEqual(TEXT("map add missing key/value code"), Result.ErrorCode, FString(TEXT("container_action_type_evidence_missing")));
	}

	{
		FBlueprintHelperActionResolutionRequest Request;
		Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Request.StatementId = TEXT("set-add-items-missing-element");
		Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
		Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
		Request.Semantic.ContainerKind = TEXT("set");
		Request.Semantic.ContainerOperation = TEXT("add_items");
		Request.Semantic.ArgumentNames = { TEXT("target"), TEXT("items") };
		Request.Semantic.ArgumentPinTypes.Add(TEXT("target"), MakePinType(TEXT("name"), TEXT("set")));

		const FBlueprintHelperActionResolutionResult Result =
			FBlueprintHelperContainerActionResolver::Resolve(Request);
		TestEqual(TEXT("set add_items missing element status"), Result.Status, EBlueprintHelperActionResolutionStatus::InvalidRequest);
		TestEqual(TEXT("set add_items missing element code"), Result.ErrorCode, FString(TEXT("container_action_type_evidence_missing")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionCoverageFunctionOwnerTest,
	"BlueprintHelper.GraphWrite.ContainerAction.CoverageExtension.FunctionOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionCoverageFunctionOwnerTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperContainerActionSpec* Spec =
		FBlueprintHelperContainerActionVocabulary::Find(TEXT("array"), TEXT("sort_int"));
	TestNotNull(TEXT("array.sort_int spec"), Spec);
	if (!Spec)
	{
		return false;
	}

	FBlueprintHelperActionResolutionRequest Request;
	Request.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
	Request.StatementId = TEXT("array-sort-int");
	Request.Semantic.Kind = EBlueprintHelperActionSemanticKind::ContainerAction;
	Request.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Callable;
	Request.Semantic.ContainerKind = TEXT("array");
	Request.Semantic.ContainerOperation = TEXT("sort_int");
	Request.Semantic.ArgumentNames = { TEXT("target") };
	Request.Semantic.ArgumentPinTypes.Add(TEXT("target"), MakePinType(TEXT("int"), TEXT("array")));
	Request.Semantic.ContainerElementPinType = MakePinType(TEXT("int"));

	const FBlueprintHelperActionResolutionResult Result =
		FBlueprintHelperContainerActionResolver::Resolve(Request);
	TestEqual(TEXT("resolver owner cluster"), Result.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestNotEqual(TEXT("resolver not wrong-owner unsupported"), Result.ErrorCode, FString(TEXT("unsupported_container_operation")));
	if (Result.IsResolved())
	{
		TestTrue(TEXT("resolved via selected function"), Result.SelectedFunction.IsValid());
		TestEqual(TEXT("resolved function matches stable path"), Result.SelectedFunction.Get(), ResolveStableUFunctionPath(Spec->StableUFunctionPath));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionCoverageReadbackMetadataTest,
	"BlueprintHelper.GraphWrite.ContainerAction.CoverageExtension.ReadbackMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionCoverageReadbackMetadataTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperContainerActionSpec> Specs = FBlueprintHelperContainerActionVocabulary::All();
	for (const FBlueprintHelperContainerActionSpec& Spec : Specs)
	{
		const UFunction* Function = ResolveStableUFunctionPath(Spec.StableUFunctionPath);
		TestNotNull(*FString::Printf(TEXT("function resolves for %s"), *Spec.OperationId), Function);
		if (!Function)
		{
			continue;
		}

		for (const FString& ReadbackRole : Spec.ReadbackPinRoles)
		{
			const FBlueprintHelperContainerActionRoleBinding* Binding = Spec.RoleBindings.FindByPredicate(
				[&ReadbackRole](const FBlueprintHelperContainerActionRoleBinding& Candidate)
				{
					return Candidate.RoleName.Equals(ReadbackRole, ESearchCase::IgnoreCase);
				});
			TestNotNull(*FString::Printf(TEXT("binding exists for %s role %s"), *Spec.OperationId, *ReadbackRole), Binding);
			if (!Binding)
			{
				continue;
			}
			TestTrue(
				*FString::Printf(TEXT("ufunction pin exists for %s role %s"), *Spec.OperationId, *ReadbackRole),
				HasParamNamed(Function, Binding->FunctionPinName));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphWriteContainerActionCoverageVerifierCollectionPinTest,
	"BlueprintHelper.GraphWrite.ContainerAction.CoverageExtension.VerifierCollectionPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphWriteContainerActionCoverageVerifierCollectionPinTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = MakeBlueprint();
	UEdGraph* Graph = FindEventGraph(Blueprint);
	TestNotNull(TEXT("coverage blueprint"), Blueprint);
	TestNotNull(TEXT("coverage graph"), Graph);
	if (!Blueprint || !Graph)
	{
		return false;
	}

	const FEdGraphPinType TagSetType =
		MakeBlueprintPinType(UEdGraphSchema_K2::PC_String, EPinContainerType::Set);
	TestTrue(TEXT("TagSet variable added"), AddVariable(Blueprint, TEXT("TagSet"), TagSetType));

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		AddError(TEXT("fixture Blueprint failed to compile before verifier coverage test."));
		return false;
	}

	const FBlueprintHelperContainerActionReadbackExpectation Expectation{
		TEXT("container.set.add"),
		TEXT("set"),
		TEXT("add"),
		TEXT("TagSet"),
		TEXT("string"),
		FString(),
		FString(),
		{ TEXT("target"), TEXT("item") },
		false,
		false
	};

	const bool bPassed = RunFixture(
		*this,
		Blueprint,
		Graph,
		TEXT("set add readback"),
		TEXT(R"JSON({
			"id": "stmt_set_add",
			"kind": "container_action",
			"container_kind": "set",
			"container_operation": "add",
			"target": { "kind": "get", "name": "TagSet" },
			"item": { "kind": "literal", "value": "Ready", "type": "string" },
			"element_type": "string"
		})JSON"),
		Expectation);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	TestFalse(TEXT("generated Blueprint compiles"), Blueprint->Status == BS_Error);
	return bPassed && Blueprint->Status != BS_Error;
}

#endif
