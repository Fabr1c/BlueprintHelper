#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "UObject/Package.h"

class FBlueprintHelperCallFunctionResolverTestsLocalUtils
{
public:
	static FString MakeObjectName(const FString& Prefix)
	{
		return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	static UBlueprint* MakeBlueprint()
	{
		return MakeBlueprint(AActor::StaticClass());
	}

	static UBlueprint* MakeBlueprint(UClass* ParentClass)
	{
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperCallFunctionResolver/%s"), *MakeObjectName(TEXT("Pkg"))));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass ? ParentClass : AActor::StaticClass(),
			Package,
			*MakeObjectName(TEXT("BP_CallFunctionResolver")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperCallFunctionResolverTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
	}

	static UEdGraph* AddResolverFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			FName(*FunctionName),
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!FunctionGraph)
		{
			return nullptr;
		}

		FBlueprintEditorUtils::AddFunctionGraph<UFunction>(
			Blueprint,
			FunctionGraph,
			/*bIsUserCreated=*/ true,
			nullptr);
		return FunctionGraph;
	}

	static UK2Node_FunctionEntry* FindResolverFunctionEntry(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindResolverFunctionResult(UEdGraph* FunctionGraph)
	{
		if (!FunctionGraph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : FunctionGraph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

	static bool AddFunctionInputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FName PinCategory)
	{
		UK2Node_FunctionEntry* Entry = FindResolverFunctionEntry(FunctionGraph);
		if (!Blueprint || !Entry || PinName.IsEmpty())
		{
			return false;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategory;
		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Output;
		Entry->UserDefinedPins.Add(NewPin);
		Entry->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return true;
	}

	static bool AddFunctionOutputPin(
		UBlueprint* Blueprint,
		UEdGraph* FunctionGraph,
		const FString& PinName,
		const FName PinCategory)
	{
		if (!Blueprint || !FunctionGraph || PinName.IsEmpty())
		{
			return false;
		}

		UK2Node_FunctionResult* Result = FindResolverFunctionResult(FunctionGraph);
		if (!Result)
		{
			FGraphNodeCreator<UK2Node_FunctionResult> NodeCreator(*FunctionGraph);
			Result = NodeCreator.CreateNode(true);
			Result->NodePosX = 600;
			Result->NodePosY = 0;
			NodeCreator.Finalize();
		}
		if (!Result)
		{
			return false;
		}

		FEdGraphPinType PinType;
		PinType.PinCategory = PinCategory;
		TSharedPtr<FUserPinInfo> NewPin = MakeShared<FUserPinInfo>();
		NewPin->PinName = FName(*PinName);
		NewPin->PinType = PinType;
		NewPin->DesiredPinDirection = EGPD_Input;
		Result->UserDefinedPins.Add(NewPin);
		Result->ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return true;
	}

	static UEdGraph* FindEventGraph(UBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static FBlueprintHelperCallFunctionResolveResult Resolve(UEdGraph* Graph, const FString& Query)
	{
		FBlueprintHelperCallFunctionResolveRequest Request;
		Request.Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
		Request.Graph = Graph;
		Request.Query = Query;
		return FBlueprintHelperCallFunctionResolver::Resolve(Request);
	}

	static FBlueprintHelperCallFunctionResolveRequest MakeRequest(UEdGraph* Graph, const FString& Query)
	{
		FBlueprintHelperCallFunctionResolveRequest Request;
		Request.Blueprint = Graph ? FBlueprintEditorUtils::FindBlueprintForGraph(Graph) : nullptr;
		Request.Graph = Graph;
		Request.Query = Query;
		return Request;
	}

	static bool HasCandidateStableId(
		const FBlueprintHelperCallFunctionResolveResult& Result,
		const FString& StableId)
	{
		return FindCandidateStableId(Result, StableId) != nullptr;
	}

	static const FBlueprintHelperCallFunctionCandidate* FindCandidateStableId(
		const FBlueprintHelperCallFunctionResolveResult& Result,
		const FString& StableId)
	{
		for (const FBlueprintHelperCallFunctionCandidate& Candidate : Result.Candidates)
		{
			if (Candidate.StableId == StableId)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	static bool StableIdEndsWithFunctionName(const FString& StableId, const FString& FunctionName)
	{
		return StableId.EndsWith(FString::Printf(TEXT(":%s"), *FunctionName));
	}

	static bool HasCandidateFunctionName(
		const FBlueprintHelperCallFunctionResolveResult& Result,
		const FString& FunctionName)
	{
		for (const FBlueprintHelperCallFunctionCandidate& Candidate : Result.Candidates)
		{
			if (StableIdEndsWithFunctionName(Candidate.StableId, FunctionName))
			{
				return true;
			}
		}
		return false;
	}

	static int32 CountCandidatesByFunctionName(
		const FBlueprintHelperCallFunctionResolveResult& Result,
		const FString& FunctionName)
	{
		int32 Count = 0;
		for (const FBlueprintHelperCallFunctionCandidate& Candidate : Result.Candidates)
		{
			if (StableIdEndsWithFunctionName(Candidate.StableId, FunctionName))
			{
				++Count;
			}
		}
		return Count;
	}

	struct FBlueprintAuthoredOverloadLikeFixture
	{
		UBlueprint* ParentBlueprint = nullptr;
		UBlueprint* ChildBlueprint = nullptr;
		UEdGraph* ChildGraph = nullptr;
	};

	static bool BuildBlueprintAuthoredOverloadLikeFixture(FBlueprintAuthoredOverloadLikeFixture& OutFixture)
	{
		OutFixture = FBlueprintAuthoredOverloadLikeFixture();

		UBlueprint* ParentBlueprint = MakeBlueprint();
		UEdGraph* ParentFunctionGraph = AddResolverFunctionGraph(ParentBlueprint, TEXT("ResolvePayload_Int"));
		if (!ParentFunctionGraph ||
			!AddFunctionInputPin(ParentBlueprint, ParentFunctionGraph, TEXT("Value"), UEdGraphSchema_K2::PC_Int) ||
			!AddFunctionOutputPin(ParentBlueprint, ParentFunctionGraph, TEXT("ReturnValue"), UEdGraphSchema_K2::PC_Boolean))
		{
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(ParentBlueprint);
		UClass* ParentGeneratedClass = ParentBlueprint ? ParentBlueprint->GeneratedClass.Get() : nullptr;
		if (!ParentGeneratedClass)
		{
			return false;
		}

		UBlueprint* ChildBlueprint = MakeBlueprint(ParentGeneratedClass);
		UEdGraph* ChildFunctionGraph = AddResolverFunctionGraph(ChildBlueprint, TEXT("ResolvePayload_String"));
		if (!ChildFunctionGraph ||
			!AddFunctionInputPin(ChildBlueprint, ChildFunctionGraph, TEXT("Value"), UEdGraphSchema_K2::PC_String) ||
			!AddFunctionOutputPin(ChildBlueprint, ChildFunctionGraph, TEXT("ReturnValue"), UEdGraphSchema_K2::PC_Boolean))
		{
			return false;
		}

		FKismetEditorUtilities::CompileBlueprint(ChildBlueprint);
		if (!ChildBlueprint || !ChildBlueprint->GeneratedClass.Get())
		{
			return false;
		}

		OutFixture.ParentBlueprint = ParentBlueprint;
		OutFixture.ChildBlueprint = ChildBlueprint;
		OutFixture.ChildGraph = FindEventGraph(ChildBlueprint);
		return OutFixture.ChildGraph != nullptr;
	}

	static FString MakeSingleCallJson(const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), FunctionName);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		Root->SetObjectField(TEXT("logic_spec"), LogicSpec);

		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Root, Writer);
		return JsonText;
	}

	static UK2Node_CallFunction* FindCallNode(UEdGraph* Graph, const FName FunctionName = FName(TEXT("PrintString")))
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (CallNode->GetFunctionName() == FunctionName)
				{
					return CallNode;
				}
			}
		}
		return nullptr;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverPrintStringNativeTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PrintStringNativeNameResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverPrintStringNativeTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::Resolve(Graph, TEXT("PrintString"));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
	TestTrue(TEXT("function valid"), Result.Selected.Function.IsValid());
	TestTrue(TEXT("graph compatible"), Result.Selected.bGraphCompatible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverPrintStringDisplayTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.PrintStringDisplayNameResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverPrintStringDisplayTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::Resolve(Graph, TEXT("Print String"));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
	TestTrue(TEXT("function valid"), Result.Selected.Function.IsValid());
	TestTrue(TEXT("graph compatible"), Result.Selected.bGraphCompatible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverQualifiedTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.QualifiedNameResolvesStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverQualifiedTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::Resolve(Graph, TEXT("/Script/Engine.KismetSystemLibrary:PrintString"));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")));
	TestTrue(TEXT("function valid"), Result.Selected.Function.IsValid());
	TestTrue(TEXT("graph compatible"), Result.Selected.bGraphCompatible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverAmbiguousTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.AmbiguousShortNameBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverAmbiguousTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::Resolve(Graph, TEXT("Set"));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Ambiguous);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("ambiguous_function_call")));
	TestTrue(TEXT("has candidates"), Result.Candidates.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverMemberPrefixTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.MemberPrefixBlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverMemberPrefixTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::Resolve(Graph, TEXT("DoorMesh.AddAngularImpulseInDegrees"));

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Blocked);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("explicit_member_call_not_supported")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverGeneratorDisplayNameTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorDisplayNameSpawnsPrintString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverGeneratorDisplayNameTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result = FBlueprintGraphWriteFacade::GenerateBlueprintFromJson(
		Graph,
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeSingleCallJson(TEXT("Print String")),
		Unresolved);

	TestTrue(TEXT("generation succeeds"), Result.bSucceed);
	UK2Node_CallFunction* CallNode = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCallNode(Graph);
	TestNotNull(TEXT("call node"), CallNode);
	if (CallNode)
	{
		TestEqual(TEXT("target function"), CallNode->GetFunctionName(), FName(TEXT("PrintString")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverGeneratorQualifiedNameTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorQualifiedNameSpawnsPrintString",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverGeneratorQualifiedNameTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result = FBlueprintGraphWriteFacade::GenerateBlueprintFromJson(
		Graph,
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeSingleCallJson(TEXT("/Script/Engine.KismetSystemLibrary:PrintString")),
		Unresolved);

	TestTrue(TEXT("generation succeeds"), Result.bSucceed);
	UK2Node_CallFunction* CallNode = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCallNode(Graph);
	TestNotNull(TEXT("call node"), CallNode);
	if (CallNode)
	{
		TestEqual(TEXT("target function"), CallNode->GetFunctionName(), FName(TEXT("PrintString")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverGeneratorAmbiguousNameTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.GeneratorAmbiguousNameDoesNotSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverGeneratorAmbiguousNameTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult Result = FBlueprintGraphWriteFacade::GenerateBlueprintFromJson(
		Graph,
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeSingleCallJson(TEXT("Set")),
		Unresolved);

	TestFalse(TEXT("generation does not succeed"), Result.bSucceed);
	TestNull(TEXT("no print string call node"), FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCallNode(Graph));
	TestTrue(TEXT("unresolved recorded"), Unresolved.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressMultiParameterTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.MultiParameterInRangeResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressMultiParameterTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Graph, TEXT("InRange_IntInt"));
	Request.ArgumentTypes.Add(TEXT("Value"), TEXT("int"));
	Request.ArgumentTypes.Add(TEXT("Min"), TEXT("int"));
	Request.ArgumentTypes.Add(TEXT("Max"), TEXT("int"));
	Request.ArgumentTypes.Add(TEXT("InclusiveMin"), TEXT("bool"));
	Request.ArgumentTypes.Add(TEXT("InclusiveMax"), TEXT("bool"));
	Request.ExpectedReturnType = TEXT("bool");

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetMathLibrary:InRange_IntInt")));
	TestTrue(TEXT("has Value input"), Result.Selected.InputPins.Contains(TEXT("Value")));
	TestTrue(TEXT("has Min input"), Result.Selected.InputPins.Contains(TEXT("Min")));
	TestTrue(TEXT("has Max input"), Result.Selected.InputPins.Contains(TEXT("Max")));
	TestTrue(TEXT("has InclusiveMin input"), Result.Selected.InputPins.Contains(TEXT("InclusiveMin")));
	TestTrue(TEXT("has InclusiveMax input"), Result.Selected.InputPins.Contains(TEXT("InclusiveMax")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressInheritedTargetTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.InheritedTargetResolvesParentFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressInheritedTargetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Graph, TEXT("K2_DestroyActor"));
	Request.TargetObjectType = ACharacter::StaticClass()->GetPathName();

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.Actor:K2_DestroyActor")));
	TestEqual(TEXT("owner class"), Result.Selected.OwnerClassPath, FString(TEXT("/Script/Engine.Actor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressRejectsUnrelatedTargetTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.UnrelatedTargetRejectsParentFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressRejectsUnrelatedTargetTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Graph, TEXT("K2_DestroyActor"));
	Request.TargetObjectType = UStaticMeshComponent::StaticClass()->GetPathName();

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestFalse(TEXT("unrelated target does not resolve"), Result.IsResolved());
	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::NotFound);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("function_call_not_found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressOverloadBlocksAmbiguousTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.OverloadShortQueryBlocksAmbiguous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressOverloadBlocksAmbiguousTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Graph, TEXT("EqualEqual"));
	Request.ArgumentTypes.Add(TEXT("A"), TEXT("int"));
	Request.ArgumentTypes.Add(TEXT("B"), TEXT("int"));
	Request.ExpectedReturnType = TEXT("bool");

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Ambiguous);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("ambiguous_function_call")));
	TestTrue(TEXT("integer overload is a candidate"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::HasCandidateStableId(
			Result,
			TEXT("/Script/Engine.KismetMathLibrary:EqualEqual_IntInt")));
	TestTrue(TEXT("float overload is a candidate"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::HasCandidateStableId(
			Result,
			TEXT("/Script/Engine.KismetMathLibrary:EqualEqual_DoubleDouble")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressOverloadPriorityTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.OverloadPrioritySelectsInteger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressOverloadPriorityTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* Graph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(Blueprint);
	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Graph, TEXT("EqualEqual"));
	Request.ArgumentTypes.Add(TEXT("A"), TEXT("int"));
	Request.ArgumentTypes.Add(TEXT("B"), TEXT("int"));
	Request.ExpectedReturnType = TEXT("bool");
	Request.CategoryPriority.Add(TEXT("EqualEqual_IntInt"));

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestEqual(TEXT("stable id"), Result.Selected.StableId, FString(TEXT("/Script/Engine.KismetMathLibrary:EqualEqual_IntInt")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredInheritedTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.BlueprintAuthoredInheritedFunctionResolves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredInheritedTest::RunTest(const FString& Parameters)
{
	UBlueprint* ParentBlueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* ParentFunctionGraph =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::AddResolverFunctionGraph(ParentBlueprint, TEXT("ParentBlueprintUtility"));
	TestNotNull(TEXT("parent function graph"), ParentFunctionGraph);
	TestTrue(TEXT("parent function input pin"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::AddFunctionInputPin(
			ParentBlueprint,
			ParentFunctionGraph,
			TEXT("Amount"),
			UEdGraphSchema_K2::PC_Int));
	FKismetEditorUtilities::CompileBlueprint(ParentBlueprint);
	UClass* ParentGeneratedClass = ParentBlueprint ? ParentBlueprint->GeneratedClass.Get() : nullptr;
	TestNotNull(TEXT("parent generated class"), ParentGeneratedClass);
	if (!ParentGeneratedClass)
	{
		return true;
	}

	UBlueprint* ChildBlueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint(ParentGeneratedClass);
	FKismetEditorUtilities::CompileBlueprint(ChildBlueprint);
	UClass* ChildGeneratedClass = ChildBlueprint ? ChildBlueprint->GeneratedClass.Get() : nullptr;
	UEdGraph* ChildGraph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(ChildBlueprint);
	TestNotNull(TEXT("child generated class"), ChildGeneratedClass);
	TestNotNull(TEXT("child graph"), ChildGraph);
	if (!ChildGeneratedClass || !ChildGraph)
	{
		return true;
	}

	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(ChildGraph, TEXT("ParentBlueprintUtility"));
	Request.ArgumentTypes.Add(TEXT("Amount"), TEXT("int"));

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestTrue(TEXT("stable id uses parent function name"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::StableIdEndsWithFunctionName(
			Result.Selected.StableId,
			TEXT("ParentBlueprintUtility")));
	TestEqual(TEXT("owner class"), Result.Selected.OwnerClassPath, ParentGeneratedClass->GetPathName());
	TestTrue(TEXT("has Amount input"), Result.Selected.InputPins.Contains(TEXT("Amount")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredInheritedGenerationTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.BlueprintAuthoredInheritedFunctionGraphGenerationSpawns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredInheritedGenerationTest::RunTest(const FString& Parameters)
{
	UBlueprint* ParentBlueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint();
	UEdGraph* ParentFunctionGraph =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::AddResolverFunctionGraph(ParentBlueprint, TEXT("ParentBlueprintNoArgUtility"));
	TestNotNull(TEXT("parent function graph"), ParentFunctionGraph);
	FKismetEditorUtilities::CompileBlueprint(ParentBlueprint);
	UClass* ParentGeneratedClass = ParentBlueprint ? ParentBlueprint->GeneratedClass.Get() : nullptr;
	TestNotNull(TEXT("parent generated class"), ParentGeneratedClass);
	if (!ParentGeneratedClass)
	{
		return true;
	}

	UBlueprint* ChildBlueprint = FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeBlueprint(ParentGeneratedClass);
	FKismetEditorUtilities::CompileBlueprint(ChildBlueprint);
	UEdGraph* ChildGraph = FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindEventGraph(ChildBlueprint);
	TestNotNull(TEXT("child graph"), ChildGraph);
	if (!ChildGraph)
	{
		return true;
	}

	FBlueprintHelperCallFunctionResolveRequest ResolveRequest =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(ChildGraph, TEXT("ParentBlueprintNoArgUtility"));
	const FBlueprintHelperCallFunctionResolveResult ResolveResult =
		FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);
	TestEqual(TEXT("resolver status"), ResolveResult.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestTrue(TEXT("resolver stable id uses parent function name"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::StableIdEndsWithFunctionName(
			ResolveResult.Selected.StableId,
			TEXT("ParentBlueprintNoArgUtility")));

	TArray<TSharedPtr<FUnresolvedNodeItem>> Unresolved;
	const FBlueprintGenerateResult GenerateResult = FBlueprintGraphWriteFacade::GenerateBlueprintFromJson(
		ChildGraph,
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeSingleCallJson(TEXT("ParentBlueprintNoArgUtility")),
		Unresolved);
	if (!GenerateResult.bSucceed || Unresolved.Num() > 0)
	{
		const FString FirstUnresolved = Unresolved.Num() > 0 && Unresolved[0].IsValid()
			? FString::Printf(TEXT("%s - %s"), *Unresolved[0]->DisplayText, *Unresolved[0]->Reason)
			: TEXT("<none>");
		AddError(FString::Printf(
			TEXT("Graph generation failed. message='%s' first_unresolved='%s'"),
			*GenerateResult.Message,
			*FirstUnresolved));
	}

	TestTrue(TEXT("generation succeeds"), GenerateResult.bSucceed);
	TestEqual(TEXT("unresolved count"), Unresolved.Num(), 0);
	TestNotNull(TEXT("custom parent call node"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCallNode(ChildGraph, FName(TEXT("ParentBlueprintNoArgUtility"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredOverloadLikeAmbiguousTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.BlueprintAuthoredOverloadLikeNameBlocksAmbiguous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredOverloadLikeAmbiguousTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperCallFunctionResolverTestsLocalUtils::FBlueprintAuthoredOverloadLikeFixture Fixture;
	TestTrue(TEXT("fixture built"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::BuildBlueprintAuthoredOverloadLikeFixture(Fixture));
	UClass* ChildGeneratedClass = Fixture.ChildBlueprint ? Fixture.ChildBlueprint->GeneratedClass.Get() : nullptr;
	if (!Fixture.ChildGraph || !ChildGeneratedClass)
	{
		return true;
	}

	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Fixture.ChildGraph, TEXT("ResolvePayload"));

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	const FString ParentStableId = FString::Printf(
		TEXT("%s:%s"),
		*Fixture.ParentBlueprint->GeneratedClass->GetPathName(),
		TEXT("ResolvePayload_Int"));
	const FString ChildStableId = FString::Printf(
		TEXT("%s:%s"),
		*ChildGeneratedClass->GetPathName(),
		TEXT("ResolvePayload_String"));
	const FBlueprintHelperCallFunctionCandidate* ParentCandidate =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCandidateStableId(Result, ParentStableId);
	const FBlueprintHelperCallFunctionCandidate* ChildCandidate =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::FindCandidateStableId(Result, ChildStableId);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Ambiguous);
	TestEqual(TEXT("error code"), Result.ErrorCode, FString(TEXT("ambiguous_function_call")));
	TestEqual(TEXT("overload-like candidate count"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::CountCandidatesByFunctionName(Result, TEXT("ResolvePayload_Int")) +
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::CountCandidatesByFunctionName(Result, TEXT("ResolvePayload_String")),
		2);
	TestNotNull(TEXT("parent overload-like function is a candidate"), ParentCandidate);
	TestNotNull(TEXT("child overload-like function is a candidate"), ChildCandidate);
	if (ParentCandidate)
	{
		TestEqual(TEXT("parent owner class"), ParentCandidate->OwnerClassPath, Fixture.ParentBlueprint->GeneratedClass->GetPathName());
	}
	if (ChildCandidate)
	{
		TestEqual(TEXT("child owner class"), ChildCandidate->OwnerClassPath, ChildGeneratedClass->GetPathName());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredOverloadLikePriorityTest,
	"BlueprintHelper.GraphWrite.CallFunctionResolver.Stress.BlueprintAuthoredOverloadLikePrioritySelectsChild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperCallFunctionResolverStressBlueprintAuthoredOverloadLikePriorityTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperCallFunctionResolverTestsLocalUtils::FBlueprintAuthoredOverloadLikeFixture Fixture;
	TestTrue(TEXT("fixture built"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::BuildBlueprintAuthoredOverloadLikeFixture(Fixture));
	UClass* ChildGeneratedClass = Fixture.ChildBlueprint ? Fixture.ChildBlueprint->GeneratedClass.Get() : nullptr;
	if (!Fixture.ChildGraph || !ChildGeneratedClass)
	{
		return true;
	}

	FBlueprintHelperCallFunctionResolveRequest Request =
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::MakeRequest(Fixture.ChildGraph, TEXT("ResolvePayload"));
	Request.ExpectedReturnType = TEXT("bool");
	Request.CategoryPriority.Add(TEXT("ResolvePayload_String"));

	const FBlueprintHelperCallFunctionResolveResult Result =
		FBlueprintHelperCallFunctionResolver::Resolve(Request);

	TestEqual(TEXT("status"), Result.Status, EBlueprintHelperCallFunctionResolveStatus::Resolved);
	TestTrue(TEXT("stable id uses child function name"),
		FBlueprintHelperCallFunctionResolverTestsLocalUtils::StableIdEndsWithFunctionName(
			Result.Selected.StableId,
			TEXT("ResolvePayload_String")));
	TestEqual(TEXT("owner class"), Result.Selected.OwnerClassPath, ChildGeneratedClass->GetPathName());
	TestEqual(TEXT("Value pin type"), Result.Selected.InputPinTypes.FindRef(TEXT("Value")), FString(TEXT("string")));
	return true;
}

#endif
