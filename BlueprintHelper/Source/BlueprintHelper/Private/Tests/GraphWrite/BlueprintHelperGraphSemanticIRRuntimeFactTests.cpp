#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

class FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils
{
public:
	static TSharedRef<FJsonObject> MakeLogicSpecWithResolvedStableId(const FString& StableId)
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));
		Statement->SetStringField(TEXT("resolved_stable_id"), StableId);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeLogicSpecWithResolvedExpressionStableId(const FString& StableId)
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));

		TSharedRef<FJsonObject> Expression = MakeShared<FJsonObject>();
		Expression->SetStringField(TEXT("kind"), TEXT("call"));
		Expression->SetStringField(TEXT("target"), TEXT("GetDisplayName"));
		Expression->SetStringField(TEXT("resolved_stable_id"), StableId);

		TSharedRef<FJsonObject> Args = MakeShared<FJsonObject>();
		Args->SetObjectField(TEXT("object"), Expression);

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("kind"), TEXT("call"));
		Statement->SetStringField(TEXT("target"), TEXT("PrintString"));
		Statement->SetObjectField(TEXT("args"), Args);

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRRuntimeFact_ParsesResolvedStableId,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.ParsesResolvedStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphSemanticIRRuntimeFact_ParsesResolvedStableId::RunTest(const FString& Parameters)
{
	const FString StableId = TEXT("/Script/Engine.KismetSystemLibrary:PrintString");
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithResolvedStableId(StableId),
		IR);

	TestTrue(TEXT("logic spec builds"), bBuilt);
	TestEqual(TEXT("one statement parsed"), IR.Statements.Num(), 1);
	if (IR.Statements.Num() > 0 && IR.Statements[0].IsValid())
	{
		TestEqual(TEXT("resolved stable id is preserved"), IR.Statements[0]->ResolvedCallFunctionStableId, StableId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRRuntimeFact_ParsesExpressionResolvedStableId,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.RuntimeFact.ParsesExpressionResolvedStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBlueprintHelperGraphSemanticIRRuntimeFact_ParsesExpressionResolvedStableId::RunTest(const FString& Parameters)
{
	const FString StableId = TEXT("/Script/Engine.KismetSystemLibrary:GetDisplayName");
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithResolvedExpressionStableId(StableId),
		IR);

	TestTrue(TEXT("logic spec builds"), bBuilt);
	TestEqual(TEXT("one statement parsed"), IR.Statements.Num(), 1);
	if (IR.Statements.Num() > 0 && IR.Statements[0].IsValid())
	{
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>* Expression = IR.Statements[0]->Args.Find(TEXT("object"));
		TestTrue(TEXT("expression exists"), Expression && Expression->IsValid());
		if (Expression && Expression->IsValid())
		{
			TestEqual(TEXT("expression resolved stable id is preserved"), (*Expression)->ResolvedCallFunctionStableId, StableId);
		}
	}
	return true;
}

#endif
