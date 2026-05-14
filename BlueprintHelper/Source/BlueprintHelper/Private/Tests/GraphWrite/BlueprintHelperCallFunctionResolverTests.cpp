#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
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
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Game/BlueprintHelperCallFunctionResolver/%s"), *MakeObjectName(TEXT("Pkg"))));
		Package->SetDirtyFlag(false);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			*MakeObjectName(TEXT("BP_CallFunctionResolver")),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("BlueprintHelperCallFunctionResolverTests"));
		Package->SetDirtyFlag(false);
		return Blueprint;
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

	static FString MakeSingleCallJson(const FString& FunctionName)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Nodes;
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("id"), TEXT("print_1"));
		Node->SetStringField(TEXT("type"), TEXT("K2Node_CallFunction"));
		Node->SetStringField(TEXT("function_name"), FunctionName);
		Nodes.Add(MakeShared<FJsonValueObject>(Node));
		Root->SetArrayField(TEXT("nodes"), Nodes);
		Root->SetArrayField(TEXT("links"), {});

		FString JsonText;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
		FJsonSerializer::Serialize(Root, Writer);
		return JsonText;
	}

	static UK2Node_CallFunction* FindCallNode(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node))
			{
				if (CallNode->GetFunctionName() == FName(TEXT("PrintString")))
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

#endif
