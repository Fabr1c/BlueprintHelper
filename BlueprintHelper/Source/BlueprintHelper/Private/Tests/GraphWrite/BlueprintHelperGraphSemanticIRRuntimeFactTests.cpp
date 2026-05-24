#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"

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

	static TSharedRef<FJsonObject> MakeLogicSpecWithRawStatementKind(
		const FString& Kind,
		const FString& DelegateOperation = FString())
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v1"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_delegate_boundary"));
		Statement->SetStringField(TEXT("kind"), Kind);
		Statement->SetStringField(TEXT("target"), TEXT("DoorSensor"));
		Statement->SetStringField(TEXT("delegate"), TEXT("OnDoorOpened"));
		Statement->SetStringField(TEXT("handler"), TEXT("HandleDoorOpened"));
		if (!DelegateOperation.IsEmpty())
		{
			Statement->SetStringField(TEXT("delegate_operation"), DelegateOperation);
		}

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(Statement));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static TSharedRef<FJsonObject> MakeLogicSpecWithEventDelegateStatements()
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));

		TSharedRef<FJsonObject> ComponentEvent = MakeShared<FJsonObject>();
		ComponentEvent->SetStringField(TEXT("id"), TEXT("stmt_component_bound_event"));
		ComponentEvent->SetStringField(TEXT("kind"), TEXT("component_bound_event"));
		ComponentEvent->SetStringField(TEXT("component"), TEXT("TriggerBox"));
		ComponentEvent->SetStringField(TEXT("delegate"), TEXT("OnComponentBeginOverlap"));
		ComponentEvent->SetStringField(TEXT("handler"), TEXT("HandleSmokeOverlap"));

		TSharedRef<FJsonObject> DelegateBind = MakeShared<FJsonObject>();
		DelegateBind->SetStringField(TEXT("id"), TEXT("stmt_delegate_bind"));
		DelegateBind->SetStringField(TEXT("kind"), TEXT("delegate"));
		DelegateBind->SetStringField(TEXT("target"), TEXT("TriggerBox"));
		DelegateBind->SetStringField(TEXT("delegate"), TEXT("OnComponentBeginOverlap"));
		DelegateBind->SetStringField(TEXT("delegate_operation"), TEXT("bind"));
		DelegateBind->SetStringField(TEXT("handler"), TEXT("HandleSmokeOverlap"));

		TArray<TSharedPtr<FJsonValue>> Statements;
		Statements.Add(MakeShared<FJsonValueObject>(ComponentEvent));
		Statements.Add(MakeShared<FJsonValueObject>(DelegateBind));
		LogicSpec->SetArrayField(TEXT("statements"), Statements);
		return LogicSpec;
	}

	static bool HasDiagnosticCode(const FBlueprintHelperGraphSemanticIR& IR, const FString& Code)
	{
		for (const FBlueprintHelperGraphSemanticDiagnostic& Diagnostic : IR.Diagnostics)
		{
			if (Diagnostic.Code == Code)
			{
				return true;
			}
		}
		return false;
	}

	static TSharedRef<FJsonObject> MakeLogicSpecWithFieldScope(const FString& FieldScope)
	{
		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));

		TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetStringField(TEXT("kind"), TEXT("literal"));
		Value->SetStringField(TEXT("type"), TEXT("bool"));
		Value->SetStringField(TEXT("value"), TEXT("true"));

		TSharedRef<FJsonObject> Statement = MakeShared<FJsonObject>();
		Statement->SetStringField(TEXT("id"), TEXT("stmt_field_scope"));
		Statement->SetStringField(TEXT("kind"), TEXT("field"));
		Statement->SetStringField(TEXT("target"), TEXT("DoorMesh"));
		Statement->SetStringField(TEXT("property_path"), TEXT("bVisible"));
		Statement->SetStringField(TEXT("field_operation"), TEXT("set"));
		Statement->SetStringField(TEXT("field_scope"), FieldScope);
		Statement->SetObjectField(TEXT("value"), Value);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRDelegateBoundary_RejectsDottedPublicKinds,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary.RejectsDottedPublicKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRDelegateBoundary_RejectsDottedPublicKinds::RunTest(const FString& Parameters)
{
	const TArray<FString> PublicOnlyKinds = {
		TEXT("delegate.bind"),
		TEXT("delegate.assign"),
		TEXT("delegate.unbind"),
		TEXT("delegate.unbind_all"),
		TEXT("delegate.call")
	};

	bool bPassed = true;
	for (const FString& Kind : PublicOnlyKinds)
	{
		FBlueprintHelperGraphSemanticIR IR;
		FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithRawStatementKind(Kind),
			IR);

		bPassed &= TestTrue(*FString::Printf(TEXT("%s produces unsupported kind diagnostic"), *Kind),
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("statement_kind_unsupported")));
		if (IR.Statements.Num() > 0 && IR.Statements[0].IsValid())
		{
			bPassed &= TestEqual(*FString::Printf(TEXT("%s remains unknown internally"), *Kind),
				IR.Statements[0]->Kind,
				EBlueprintHelperGraphStatementKind::Unknown);
		}
	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRDelegateBoundary_AcceptsCanonicalInternalDelegate,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary.AcceptsCanonicalInternalDelegate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRDelegateBoundary_AcceptsCanonicalInternalDelegate::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithRawStatementKind(TEXT("delegate"), TEXT("bind")),
		IR);

	TestTrue(TEXT("canonical delegate logic spec builds"), bBuilt);
	TestEqual(TEXT("statement count"), IR.Statements.Num(), 1);
	if (IR.Statements.Num() == 1 && IR.Statements[0].IsValid())
	{
		TestEqual(TEXT("canonical statement kind"), IR.Statements[0]->Kind, EBlueprintHelperGraphStatementKind::Delegate);
		TestEqual(TEXT("canonical delegate operation"), IR.Statements[0]->DelegateOperation, FString(TEXT("bind")));
	}
	TestFalse(TEXT("no unsupported kind diagnostic"),
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("statement_kind_unsupported")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRDelegateBoundary_FragmentDagAcceptsCanonicalEventDelegateKinds,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.DelegateBoundary.FragmentDagAcceptsCanonicalEventDelegateKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRDelegateBoundary_FragmentDagAcceptsCanonicalEventDelegateKinds::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithEventDelegateStatements(),
		IR);

	TestTrue(TEXT("event delegate logic spec builds"), bBuilt);
	TestEqual(TEXT("statement count"), IR.Statements.Num(), 2);

	FBlueprintHelperGraphFragmentDag Dag;
	const bool bDagBuilt = FBlueprintHelperGraphFragmentDagBuilder::BuildFromSemanticIR(IR, Dag);
	TestTrue(TEXT("event delegate fragment dag builds"), bDagBuilt);
	TestFalse(TEXT("no placeholder unknown diagnostic"), Dag.HasErrors());
	TestEqual(TEXT("fragment count"), Dag.Fragments.Num(), 2);
	if (Dag.Fragments.Num() == 2)
	{
		TestEqual(TEXT("component event fragment kind"), Dag.Fragments[0].Kind, FString(TEXT("statement_component_bound_event")));
		TestEqual(TEXT("delegate fragment kind"), Dag.Fragments[1].Kind, FString(TEXT("statement_delegate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRFieldScopes_AcceptsComponentRefAndFieldAccess,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.FieldScopes.AcceptsComponentRefAndFieldAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRFieldScopes_AcceptsComponentRefAndFieldAccess::RunTest(const FString& Parameters)
{
	bool bPassed = true;
	for (const FString& Scope : { FString(TEXT("component_ref")), FString(TEXT("field_access")) })
	{
		FBlueprintHelperGraphSemanticIR IR;
		const bool bBuilt = FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithFieldScope(Scope),
			IR);

		bPassed &= TestTrue(*FString::Printf(TEXT("%s builds"), *Scope), bBuilt);
		bPassed &= TestFalse(*FString::Printf(TEXT("%s has no unsupported scope diagnostic"), *Scope),
			FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("field_scope_unsupported")));
	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperGraphSemanticIRFieldScopes_RejectsUnsupportedScope,
	"BlueprintHelper.GraphWrite.GraphSemanticIR.FieldScopes.RejectsUnsupportedScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperGraphSemanticIRFieldScopes_RejectsUnsupportedScope::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphSemanticIR IR;
	FBlueprintHelperGraphSemanticIRBuilder::BuildFromLogicSpec(
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::MakeLogicSpecWithFieldScope(TEXT("legacy_field")),
		IR);

	TestTrue(TEXT("unsupported field scope diagnostic"),
		FBlueprintHelperGraphSemanticIRRuntimeFactTestsLocalUtils::HasDiagnosticCode(IR, TEXT("field_scope_unsupported")));
	return true;
}

#endif
