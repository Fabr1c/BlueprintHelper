#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextBundleProjector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextDemandCollector.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextInferenceService.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
static FString BuildBlueprintHelperSourceRoot()
{
	return FPaths::Combine(
		FPaths::ProjectPluginsDir(),
		TEXT("BlueprintHelper"),
		TEXT("BlueprintHelper"),
		TEXT("Source"),
		TEXT("BlueprintHelper"));
}

static FString BuildActionContextPublicPath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Public"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("Context"),
		FileName);
}

static FString BuildActionResolutionPublicPath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Public"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		FileName);
}

static FString BuildActionContextPrivatePath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("Context"),
		FileName);
}

static FString BuildActionResolutionPrivateUtilsPath(const TCHAR* FileName)
{
	return FPaths::Combine(
		BuildBlueprintHelperSourceRoot(),
		TEXT("Private"),
		TEXT("Systems"),
		TEXT("ToolClusters"),
		TEXT("GraphWrite"),
		TEXT("ActionResolution"),
		TEXT("Utils"),
		FileName);
}

static bool LoadRequiredSourceFile(FAutomationTestBase& Test, const FString& FilePath, FString& OutText)
{
	if (!IFileManager::Get().FileExists(*FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required ActionContext source file is missing: %s"), *FilePath));
		return false;
	}

	if (!FFileHelper::LoadFileToString(OutText, *FilePath))
	{
		Test.AddError(FString::Printf(TEXT("Required ActionContext source file could not be read: %s"), *FilePath));
		return false;
	}

	return true;
}

static bool RequireTokens(
	FAutomationTestBase& Test,
	const FString& SourceText,
	const FString& FilePath,
	const TArray<FString>& RequiredTokens)
{
	bool bComplete = true;
	for (const FString& Token : RequiredTokens)
	{
		if (!SourceText.Contains(Token))
		{
			Test.AddError(FString::Printf(TEXT("ActionContext contract token '%s' missing from %s"), *Token, *FilePath));
			bComplete = false;
		}
	}
	return bComplete;
}

static FBlueprintHelperCallFunctionPinType MakeActionContextTestPinType(const FString& Category, const FString& ObjectPath = FString())
{
	FBlueprintHelperCallFunctionPinType PinType;
	PinType.Category = Category;
	PinType.ObjectPath = ObjectPath;
	return PinType;
}

static bool CollectActionContextSourceFiles(FAutomationTestBase& Test, TArray<FString>& OutFiles)
{
	const FString PublicContextRoot = FPaths::GetPath(BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextTypes.h")));
	const FString PrivateContextRoot = FPaths::GetPath(BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextInferenceService.cpp")));

	const bool bPublicContextExists = IFileManager::Get().DirectoryExists(*PublicContextRoot);
	const bool bPrivateContextExists = IFileManager::Get().DirectoryExists(*PrivateContextRoot);

	if (!bPublicContextExists)
	{
		Test.AddError(FString::Printf(TEXT("ActionContext public source directory is missing: %s"), *PublicContextRoot));
	}

	if (!bPrivateContextExists)
	{
		Test.AddError(FString::Printf(TEXT("ActionContext private source directory is missing: %s"), *PrivateContextRoot));
	}

	if (bPublicContextExists)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *PublicContextRoot, TEXT("*.h"), true, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *PublicContextRoot, TEXT("*.cpp"), true, false);
	}

	if (bPrivateContextExists)
	{
		IFileManager::Get().FindFilesRecursive(OutFiles, *PrivateContextRoot, TEXT("*.h"), true, false);
		IFileManager::Get().FindFilesRecursive(OutFiles, *PrivateContextRoot, TEXT("*.cpp"), true, false);
	}

	return bPublicContextExists && bPrivateContextExists;
}

static FString ExtractStructBody(const FString& SourceText, const FString& StructName)
{
	const FString Needle = FString::Printf(TEXT("struct %s"), *StructName);
	const int32 Start = SourceText.Find(Needle);
	if (Start == INDEX_NONE)
	{
		return FString();
	}

	const int32 End = SourceText.Find(TEXT("\n};"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
	if (End == INDEX_NONE)
	{
		return SourceText.RightChop(Start);
	}
	return SourceText.Mid(Start, End - Start);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDtoSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.DTO.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDtoSourceContractTest::RunTest(const FString& Parameters)
{
	const FString FilePath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextTypes.h"));

	FString SourceText;
	if (!LoadRequiredSourceFile(*this, FilePath, SourceText))
	{
		return false;
	}

	const TArray<FString> RequiredTokens = {
		TEXT("enum class EBlueprintHelperActionContextDemandKind"),
		TEXT("enum class EBlueprintHelperActionContextSourceThread"),
		TEXT("GameThreadSnapshot"),
		TEXT("WorkerInference"),
		TEXT("struct FBlueprintHelperActionContextRevisionToken"),
		TEXT("bool IsCompatibleWith"),
		TEXT("struct FBlueprintHelperActionContextDemand"),
		TEXT("TSet<EBlueprintHelperActionContextDemandKind> RequiredKinds"),
		TEXT("SemanticFamily"),
		TEXT("TypeOperation"),
		TEXT("FunctionOperation"),
		TEXT("TransformOperation"),
		TEXT("ScheduleOperation"),
		TEXT("GraphLatentAllowed"),
		TEXT("StructPath"),
		TEXT("TypeStructureId"),
		TEXT("TMap<FString, FString> DefaultValues"),
		TEXT("ArgumentPinTypes"),
		TEXT("ComponentPath"),
		TEXT("BindingObjectPath"),
		TEXT("DelegateName"),
		TEXT("DelegateSignature"),
		TEXT("TargetGraphName"),
		TEXT("struct FBlueprintHelperActionContextSnapshot"),
		TEXT("struct FBlueprintHelperResolvedActionContextBundle"),
		TEXT("FindByStatementId")
	};

	const bool bComplete = RequireTokens(*this, SourceText, FilePath, RequiredTokens);
	TestTrue(TEXT("ActionContext DTO source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextEventDelegateEvidenceSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.P5.EventDelegateEvidenceProjectionSourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextEventDelegateEvidenceSourceContractTest::RunTest(const FString& Parameters)
{
	const FString DemandCollectorPath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextDemandCollector.cpp"));
	const FString ActionContextUtilsPath = BuildActionResolutionPrivateUtilsPath(TEXT("GraphWriteActionContextUtils.cpp"));
	const FString InferenceServicePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextInferenceService.cpp"));

	FString DemandCollectorText;
	FString ActionContextUtilsText;
	FString InferenceServiceText;
	if (!LoadRequiredSourceFile(*this, DemandCollectorPath, DemandCollectorText)
		|| !LoadRequiredSourceFile(*this, ActionContextUtilsPath, ActionContextUtilsText)
		|| !LoadRequiredSourceFile(*this, InferenceServicePath, InferenceServiceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		DemandCollectorText,
		DemandCollectorPath,
		{
			TEXT("ComponentBoundEvent"),
			TEXT("Delegate"),
			TEXT("ApplyEventDelegateStatementEvidence"),
			TEXT("ApplyEventDelegateExpressionEvidence")
		});
	bComplete &= RequireTokens(
		*this,
		ActionContextUtilsText,
		ActionContextUtilsPath,
		{
			TEXT("ComponentPath"),
			TEXT("BindingObjectPath"),
			TEXT("DelegateName"),
			TEXT("DelegateOperation"),
			TEXT("DelegateSignature")
		});
	bComplete &= RequireTokens(
		*this,
		InferenceServiceText,
		InferenceServicePath,
		{
			TEXT("event_delegate.component_path"),
			TEXT("event_delegate.binding_object_path"),
			TEXT("event_delegate.delegate_name"),
			TEXT("event_delegate.operation"),
			TEXT("event_delegate.delegate_signature"),
			TEXT("target_graph")
		});

	TestTrue(TEXT("ActionContext projects event/delegate evidence without defaults"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDelegateProjectionTest,
	"BlueprintHelper.GraphWrite.ActionContext.EventDelegate.DelegateProjectsEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDelegateProjectionTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_delegate_bind");
	Demand.SourcePath = TEXT("$.statements[0]");
	Demand.ClusterKind = EBlueprintHelperSpawnerClusterKind::EventDelegateAction;
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Delegate;
	Demand.Query = TEXT("OnDoorStateChanged");
	Demand.TargetPath = TEXT("self");
	Demand.BindingObjectPath = TEXT("self");
	Demand.DelegateName = TEXT("OnDoorStateChanged");
	Demand.DelegateOperation = TEXT("bind");
	Demand.DelegateSignature = TEXT("FDoorStateChangedSignature");
	Demand.HandlerName = TEXT("HandleDoorStateChanged");
	Demand.HandlerFunctionPath = TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged");
	Demand.HandlerSourceCluster = TEXT("BlueprintSignature");
	Demand.SignatureEvidenceId = TEXT("signature:custom_event:HandleDoorStateChanged");
	Demand.ArgumentNames = { TEXT("NewState") };

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	Snapshot.Graph.BlueprintClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("cluster"), Context.ClusterKind, EBlueprintHelperSpawnerClusterKind::EventDelegateAction);
	TestEqual(TEXT("semantic"), Context.Semantic.Kind, EBlueprintHelperActionSemanticKind::Delegate);
	TestEqual(TEXT("delegate_name"), Context.Evidence.FindRef(TEXT("event_delegate.delegate_name")), FString(TEXT("OnDoorStateChanged")));
	TestEqual(TEXT("delegate_operation"), Context.Evidence.FindRef(TEXT("event_delegate.operation")), FString(TEXT("bind")));
	TestEqual(TEXT("binding object kind"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_kind")), FString(TEXT("self")));
	TestEqual(TEXT("binding object"), Context.Evidence.FindRef(TEXT("event_delegate.binding_object_path")), FString(TEXT("self")));
	TestEqual(TEXT("handler"), Context.Evidence.FindRef(TEXT("event_delegate.handler_name")), FString(TEXT("HandleDoorStateChanged")));
	TestEqual(TEXT("handler scope"), Context.Evidence.FindRef(TEXT("event_delegate.handler_scope_class_path")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C")));
	TestEqual(TEXT("handler function path"), Context.Evidence.FindRef(TEXT("event_delegate.handler_function_path")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C:HandleDoorStateChanged")));
	TestEqual(TEXT("handler source cluster"), Context.Evidence.FindRef(TEXT("event_delegate.handler_source_cluster")), FString(TEXT("BlueprintSignature")));
	TestEqual(TEXT("signature evidence id"), Context.Evidence.FindRef(TEXT("event_delegate.signature_evidence_id")), FString(TEXT("signature:custom_event:HandleDoorStateChanged")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextDelegateStatementDemandTest,
	"BlueprintHelper.GraphWrite.ActionContext.EventDelegate.StatementDemand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextDelegateStatementDemandTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_delegate_unbind");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Delegate;
	Statement->Target = TEXT("self");
	Statement->DelegateName = TEXT("OnDoorStateChanged");
	Statement->DelegateOperation = TEXT("unbind");
	Statement->HandlerName = TEXT("HandleDoorStateChanged");
	Statement->UnbindMode = TEXT("single");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("delegate statement demand count"), Demands.Num(), 1);
	if (Demands.Num() == 0)
	{
		return false;
	}

	TestEqual(TEXT("semantic"), Demands[0].SemanticKind, EBlueprintHelperActionSemanticKind::Delegate);
	TestEqual(TEXT("cluster"), Demands[0].ClusterKind, EBlueprintHelperSpawnerClusterKind::EventDelegateAction);
	TestEqual(TEXT("binding object"), Demands[0].BindingObjectPath, FString(TEXT("self")));
	TestEqual(TEXT("delegate name"), Demands[0].DelegateName, FString(TEXT("OnDoorStateChanged")));
	TestEqual(TEXT("delegate operation"), Demands[0].DelegateOperation, FString(TEXT("unbind")));
	TestEqual(TEXT("delegate operation default"), Demands[0].DefaultValues.FindRef(TEXT("event_delegate.operation")), FString(TEXT("unbind")));
	TestEqual(TEXT("handler"), Demands[0].HandlerName, FString(TEXT("HandleDoorStateChanged")));
	TestEqual(TEXT("unbind mode"), Demands[0].UnbindMode, FString(TEXT("single")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextCallDemandPrefersTargetTest,
	"BlueprintHelper.GraphWrite.ActionContext.CallDemandPrefersTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextCallDemandPrefersTargetTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_call");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Call;
	Statement->Name = TEXT("call");
	Statement->Target = TEXT("PrintString");
	Statement->PatternName = TEXT("call_function");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("call statement demand count"), Demands.Num(), 1);
	if (Demands.Num() == 0)
	{
		return false;
	}

	TestEqual(TEXT("call statement query prefers target over literal kind/name"), Demands[0].Query, FString(TEXT("PrintString")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextCallStatementIdIsNotCallableStableIdTest,
	"BlueprintHelper.GraphWrite.ActionContext.CallStatementIdIsNotCallableStableId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextCallStatementIdIsNotCallableStableIdTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextDemand Demand;
	Demand.StatementId = TEXT("stmt_call");
	Demand.SemanticKind = EBlueprintHelperActionSemanticKind::Call;
	Demand.Query = TEXT("PrintString");

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("statement id remains context identity"), Context.StatementId, FString(TEXT("stmt_call")));
	TestEqual(TEXT("call query is preserved"), Context.Semantic.Query, FString(TEXT("PrintString")));
	TestTrue(TEXT("statement id must not become callable stable id"), Context.Semantic.StableId.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextCallExpressionDemandPrefersTargetTest,
	"BlueprintHelper.GraphWrite.ActionContext.CallExpressionDemandPrefersTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextCallExpressionDemandPrefersTargetTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphExpressionIR> CallExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
	CallExpression->ExpressionId = TEXT("expr_call");
	CallExpression->Path = TEXT("$.statements[0].value");
	CallExpression->Kind = EBlueprintHelperGraphExpressionKind::Call;
	CallExpression->Name = TEXT("call");
	CallExpression->Target = TEXT("PrintString");
	CallExpression->PatternName = TEXT("call_function");

	TSharedPtr<FBlueprintHelperGraphExpressionIR> OpExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
	OpExpression->ExpressionId = TEXT("expr_op");
	OpExpression->Path = TEXT("$.statements[0].condition");
	OpExpression->Kind = EBlueprintHelperGraphExpressionKind::Op;
	OpExpression->Name = TEXT("call");
	OpExpression->Target = TEXT("Greater");
	OpExpression->PatternName = TEXT("operator");
	OpExpression->Operator = TEXT(">");

	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_set");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Field;
	Statement->FieldOperation = TEXT("set");
	Statement->FieldScope = TEXT("variable");
	Statement->Target = TEXT("Result");
	Statement->Value = CallExpression;
	Statement->Condition = OpExpression;

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	const FBlueprintHelperActionContextDemand* CallDemand = Demands.FindByPredicate([](const FBlueprintHelperActionContextDemand& Demand)
	{
		return Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Call;
	});
	const FBlueprintHelperActionContextDemand* OpDemand = Demands.FindByPredicate([](const FBlueprintHelperActionContextDemand& Demand)
	{
		return Demand.SemanticKind == EBlueprintHelperActionSemanticKind::Op;
	});

	TestNotNull(TEXT("call expression demand exists"), CallDemand);
	TestNotNull(TEXT("operator expression demand exists"), OpDemand);
	if (CallDemand)
	{
		TestEqual(TEXT("call expression query prefers target over literal kind/name"), CallDemand->Query, FString(TEXT("PrintString")));
	}
	if (OpDemand)
	{
		TestEqual(TEXT("operator expression keeps operator token query"), OpDemand->Query, FString(TEXT(">")));
	}
	return CallDemand != nullptr && OpDemand != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextOpExpressionProjectsCanonicalEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionContext.OpExpressionProjectsCanonicalEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextOpExpressionProjectsCanonicalEvidenceTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphExpressionIR> Left = MakeShared<FBlueprintHelperGraphExpressionIR>();
	Left->ExpressionId = TEXT("expr_left");
	Left->Kind = EBlueprintHelperGraphExpressionKind::Literal;
	Left->Type = TEXT("bool");
	Left->LiteralValue = TEXT("true");

	TSharedPtr<FBlueprintHelperGraphExpressionIR> Right = MakeShared<FBlueprintHelperGraphExpressionIR>();
	Right->ExpressionId = TEXT("expr_right");
	Right->Kind = EBlueprintHelperGraphExpressionKind::Literal;
	Right->Type = TEXT("bool");
	Right->LiteralValue = TEXT("false");

	TSharedPtr<FBlueprintHelperGraphExpressionIR> OpExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
	OpExpression->ExpressionId = TEXT("expr_boolean_and");
	OpExpression->Path = TEXT("$.statements[0].args.condition");
	OpExpression->Kind = EBlueprintHelperGraphExpressionKind::Op;
	OpExpression->Operator = TEXT("boolean_and");
	OpExpression->Left = Left;
	OpExpression->Right = Right;
	OpExpression->ContextEvidence.Add(TEXT("op.argument_pin_type.0"), TEXT("bool"));
	OpExpression->ContextEvidence.Add(TEXT("op.argument_pin_type.1"), TEXT("bool"));
	OpExpression->ContextEvidence.Add(TEXT("op.expected_return_pin_type"), TEXT("bool"));

	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_gate");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Call;
	Statement->Target = TEXT("PrintString");
	Statement->Args.Add(TEXT("condition"), OpExpression);

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	const FBlueprintHelperActionContextDemand* OpDemand = Demands.FindByPredicate([](const FBlueprintHelperActionContextDemand& Demand)
	{
		return Demand.StatementId == TEXT("expr_boolean_and");
	});

	TestNotNull(TEXT("op demand exists"), OpDemand);
	if (!OpDemand)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("op function operation"), OpDemand->FunctionOperation, FString(TEXT("op.boolean_and")));
	bPassed &= TestEqual(TEXT("op operation id default"), OpDemand->DefaultValues.FindRef(TEXT("op.operation_id")), FString(TEXT("boolean_and")));
	bPassed &= TestEqual(TEXT("op first arg evidence default"), OpDemand->DefaultValues.FindRef(TEXT("op.argument_pin_type.0")), FString(TEXT("bool")));
	bPassed &= TestEqual(TEXT("op second arg evidence default"), OpDemand->DefaultValues.FindRef(TEXT("op.argument_pin_type.1")), FString(TEXT("bool")));
	bPassed &= TestEqual(TEXT("op return evidence default"), OpDemand->DefaultValues.FindRef(TEXT("op.expected_return_pin_type")), FString(TEXT("bool")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, *OpDemand);
	bPassed &= TestEqual(TEXT("projected semantic op"), Context.Semantic.FunctionOperation, FString(TEXT("op.boolean_and")));
	bPassed &= TestEqual(TEXT("projected op operation evidence"), Context.Evidence.FindRef(TEXT("op.operation_id")), FString(TEXT("boolean_and")));
	bPassed &= TestEqual(TEXT("projected op first arg evidence"), Context.Evidence.FindRef(TEXT("op.argument_pin_type.0")), FString(TEXT("bool")));
	bPassed &= TestEqual(TEXT("projected op return evidence"), Context.Evidence.FindRef(TEXT("op.expected_return_pin_type")), FString(TEXT("bool")));
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextFieldExpressionProjectsOwnerEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldExpressionProjectsOwnerEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextFieldExpressionProjectsOwnerEvidenceTest::RunTest(const FString& Parameters)
{
	const FString OwnerClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");

	TSharedPtr<FBlueprintHelperGraphExpressionIR> FieldExpression = MakeShared<FBlueprintHelperGraphExpressionIR>();
	FieldExpression->ExpressionId = TEXT("expr_session_id");
	FieldExpression->Path = TEXT("$.statements[0].args.InString");
	FieldExpression->Kind = EBlueprintHelperGraphExpressionKind::Field;
	FieldExpression->Target = TEXT("SessionId");
	FieldExpression->FieldOperation = TEXT("get");
	FieldExpression->FieldScope = TEXT("variable");
	FieldExpression->ContextEvidence.Add(TEXT("field_owner_class"), OwnerClassPath);

	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_print");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Call;
	Statement->Target = TEXT("PrintString");
	Statement->Args.Add(TEXT("InString"), FieldExpression);

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	const FBlueprintHelperActionContextDemand* FieldDemand = Demands.FindByPredicate([](const FBlueprintHelperActionContextDemand& Demand)
	{
		return Demand.StatementId == TEXT("expr_session_id");
	});

	TestNotNull(TEXT("field demand exists"), FieldDemand);
	if (!FieldDemand)
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestEqual(TEXT("field member name fact"), FieldDemand->CapabilityFacts.FindRef(TEXT("field.member_name")), FString(TEXT("SessionId")));
	bPassed &= TestEqual(TEXT("field owner class fact"), FieldDemand->CapabilityFacts.FindRef(TEXT("field.owner_class")), OwnerClassPath);

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, *FieldDemand);
	bPassed &= TestEqual(TEXT("projected raw owner evidence"), Context.Evidence.FindRef(TEXT("field_owner_class")), OwnerClassPath);
	bPassed &= TestEqual(TEXT("projected canonical owner evidence"), Context.Evidence.FindRef(TEXT("field.owner_class")), OwnerClassPath);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandSetPropertyMapsToFieldVariableTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.SetPropertyMapsToFieldVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandSetPropertyMapsToFieldVariableTest::RunTest(const FString& Parameters)
{
	TArray<FString> CategoryPriority;
	CategoryPriority.Add(TEXT("field_variable"));

	TArray<FString> ArgumentNames;
	ArgumentNames.Add(TEXT("target"));
	ArgumentNames.Add(TEXT("value"));

	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("stmt_set_property"),
			TEXT("$.statements[0]"),
			EBlueprintHelperActionSemanticKind::Field,
			TEXT("field"),
			TEXT("DoorMesh"),
			TEXT("DoorMesh.RelativeRotation"),
			TEXT("Rotator"),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			CategoryPriority,
			ArgumentNames,
			TEXT("set"),
			TEXT("property_path"));

	TestEqual(TEXT("SetProperty cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("SetProperty semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Field);
	TestEqual(TEXT("SetProperty field operation"), Demand.FieldOperation, FString(TEXT("set")));
	TestEqual(TEXT("SetProperty field scope"), Demand.FieldScope, FString(TEXT("property_path")));
	TestEqual(TEXT("SetProperty query"), Demand.Query, FString(TEXT("DoorMesh.RelativeRotation")));
	TestTrue(TEXT("SetProperty requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("SetProperty requires target"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextPropertyPathStatementKeepsOwnerRootTargetTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldPropertyPath.KeepsOwnerRootTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextPropertyPathStatementKeepsOwnerRootTargetTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_set_roll");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Field;
	Statement->Target = TEXT("DoorMesh");
	Statement->Property = TEXT("RelativeRotation.Roll");
	Statement->FieldOperation = TEXT("set");
	Statement->FieldScope = TEXT("property_path");
	Statement->ResolvedTarget.Raw = TEXT("DoorMesh.RelativeRotation.Roll");
	Statement->ResolvedTarget.Owner = TEXT("DoorMesh");
	Statement->ResolvedTarget.PropertyPath = TEXT("RelativeRotation.Roll");
	Statement->ResolvedTarget.Type = TEXT("Rotator");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("one field demand"), Demands.Num(), 1);
	if (Demands.Num() == 0)
	{
		return false;
	}

	TestEqual(TEXT("semantic"), Demands[0].SemanticKind, EBlueprintHelperActionSemanticKind::Field);
	TestEqual(TEXT("cluster"), Demands[0].ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("field operation"), Demands[0].FieldOperation, FString(TEXT("set")));
	TestEqual(TEXT("field scope"), Demands[0].FieldScope, FString(TEXT("property_path")));
	TestEqual(TEXT("target keeps owner root"), Demands[0].TargetPath, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("property path keeps member path"), Demands[0].PropertyPath, FString(TEXT("RelativeRotation.Roll")));
	TestEqual(TEXT("query stays property path"), Demands[0].Query, FString(TEXT("RelativeRotation.Roll")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	FBlueprintHelperActionContextFieldSnapshot ComponentField;
	ComponentField.Name = TEXT("DoorMesh");
	ComponentField.OwnerClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");
	ComponentField.FieldPath = TEXT("/Game/Test/BP_Door.BP_Door_C.DoorMesh");
	ComponentField.PinCategory = TEXT("object");
	ComponentField.PinSubCategory = TEXT("StaticMeshComponent");
	ComponentField.bComponent = true;
	Snapshot.Fields.Add(ComponentField);

	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demands[0]);

	TestEqual(TEXT("field_name evidence resolves owner root"), Context.Evidence.FindRef(TEXT("field_name")), FString(TEXT("DoorMesh")));
	TestEqual(TEXT("field owner evidence projected"), Context.Evidence.FindRef(TEXT("field_owner_class")), FString(TEXT("/Game/Test/BP_Door.BP_Door_C")));
	TestEqual(TEXT("component property evidence projected"), Context.Evidence.FindRef(TEXT("event_delegate.component_property_name")), FString(TEXT("DoorMesh")));
	TestEqual(TEXT("semantic target root"), Context.Semantic.TargetPath, FString(TEXT("DoorMesh")));
	TestEqual(TEXT("semantic property path"), Context.Semantic.PropertyPath, FString(TEXT("RelativeRotation.Roll")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextFieldScopesAndTypedPinInferenceTest,
	"BlueprintHelper.GraphWrite.ActionContext.FieldScopesAndTypedPinInference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextFieldScopesAndTypedPinInferenceTest::RunTest(const FString& Parameters)
{
	const FBlueprintHelperActionContextDemand ComponentDemand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_component_ref"),
			TEXT("$.statements[0].target_object"),
			EBlueprintHelperActionSemanticKind::Field,
			TEXT("DoorMesh"),
			TEXT("DoorMesh"),
			FString(),
			FString(),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			TArray<FString>(),
			TArray<FString>(),
			TEXT("get"),
			TEXT("component_ref"));
	TestEqual(TEXT("component_ref cluster"), ComponentDemand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FieldVariableAction);
	TestEqual(TEXT("component_ref scope"), ComponentDemand.FieldScope, FString(TEXT("component_ref")));

	FBlueprintHelperActionContextDemand FieldAccessDemand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_field_access"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Field,
			TEXT("RelativeRotation"),
			TEXT("DoorMesh"),
			TEXT("RelativeRotation"),
			FString(),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			TArray<FString>(),
			TArray<FString>(),
			TEXT("get"),
			TEXT("field_access"));
	FieldAccessDemand.SourceSymbolIds.Add(TEXT("symbol_owner"));
	FieldAccessDemand.ConsumerSymbolIds.Add(TEXT("symbol_consumer"));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	FBlueprintHelperActionContextFieldSnapshot ComponentField;
	ComponentField.Name = TEXT("DoorMesh");
	ComponentField.OwnerClassPath = TEXT("/Game/Test/BP_Door.BP_Door_C");
	ComponentField.FieldPath = TEXT("/Game/Test/BP_Door.BP_Door_C.DoorMesh");
	ComponentField.bComponent = true;
	Snapshot.Fields.Add(ComponentField);
	Snapshot.SymbolPinTypes.Add(TEXT("symbol_owner"), MakeActionContextTestPinType(TEXT("object"), TEXT("/Script/Engine.SceneComponent")));
	Snapshot.SymbolPinTypes.Add(TEXT("symbol_consumer"), MakeActionContextTestPinType(TEXT("struct"), TEXT("/Script/CoreUObject.Rotator")));

	const FBlueprintHelperResolvedActionContext ComponentContext =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, ComponentDemand);
	TestEqual(TEXT("component property evidence"), ComponentContext.Evidence.FindRef(TEXT("event_delegate.component_property_name")), FString(TEXT("DoorMesh")));

	const FBlueprintHelperResolvedActionContext FieldAccessContext =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, FieldAccessDemand);
	TestEqual(TEXT("field_access scope"), FieldAccessContext.Semantic.FieldScope, FString(TEXT("field_access")));
	TestTrue(TEXT("target object pin inferred"), FieldAccessContext.Semantic.TargetObjectPinType.IsValid());
	TestTrue(TEXT("expected return pin inferred"), FieldAccessContext.Semantic.ExpectedReturnPinType.IsValid());
	TestEqual(TEXT("linked source evidence"), FieldAccessContext.Evidence.FindRef(TEXT("linked_source_pin_type")), FString(TEXT("object|/Script/Engine.SceneComponent")));
	TestEqual(TEXT("linked consumer evidence"), FieldAccessContext.Evidence.FindRef(TEXT("linked_consumer_pin_type")), FString(TEXT("struct|/Script/CoreUObject.Rotator")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextCapturesFunctionLocalAndParamFieldsTest,
	"BlueprintHelper.GraphWrite.ActionContext.CapabilityFacts.CapturesFunctionLocalAndParamFields",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextCapturesFunctionLocalAndParamFieldsTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperActionContextFieldSnapshot LocalField;
	LocalField.Name = TEXT("LocalSpeed");
	LocalField.Kind = TEXT("local");
	LocalField.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("LocalSpeed"));
	LocalField.CapabilityFacts.Add(TEXT("field.function_name"), TEXT("ComputeSpeed"));
	LocalField.CapabilityFacts.Add(TEXT("field.local_scope"), TEXT("ComputeSpeed"));
	LocalField.CapabilityFacts.Add(TEXT("field.member_guid"), FGuid(0x12345678, 0x22222222, 0x33333333, 0x44444444).ToString(EGuidFormats::Digits));

	FBlueprintHelperActionContextFieldSnapshot ParamField;
	ParamField.Name = TEXT("InputSpeed");
	ParamField.Kind = TEXT("function_param");
	ParamField.CapabilityFacts.Add(TEXT("field.member_name"), TEXT("InputSpeed"));
	ParamField.CapabilityFacts.Add(TEXT("field.function_name"), TEXT("ComputeSpeed"));
	ParamField.CapabilityFacts.Add(TEXT("field.param_flags"), TEXT("FUNC_Parm"));

	TestEqual(TEXT("local kind"), LocalField.Kind, FString(TEXT("local")));
	TestEqual(TEXT("local scope"), LocalField.CapabilityFacts.FindRef(TEXT("field.local_scope")), FString(TEXT("ComputeSpeed")));
	TestFalse(TEXT("local guid"), LocalField.CapabilityFacts.FindRef(TEXT("field.member_guid")).IsEmpty());
	TestEqual(TEXT("param kind"), ParamField.Kind, FString(TEXT("function_param")));
	TestEqual(TEXT("param flags"), ParamField.CapabilityFacts.FindRef(TEXT("field.param_flags")), FString(TEXT("FUNC_Parm")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandSelectMapsToGenericClusterTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.SelectMapsToGenericCluster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandSelectMapsToGenericClusterTest::RunTest(const FString& Parameters)
{
	TArray<FString> CategoryPriority;
	CategoryPriority.Add(TEXT("generic_asset_struct_control"));

	TArray<FString> ArgumentNames;
	ArgumentNames.Add(TEXT("condition"));
	ArgumentNames.Add(TEXT("a"));
	ArgumentNames.Add(TEXT("b"));

	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_select"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Select,
			TEXT("select"),
			TEXT(""),
			TEXT(""),
			TEXT("bool"),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			CategoryPriority,
			ArgumentNames);

	TestEqual(TEXT("Select cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("Select semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Select);
	TestEqual(TEXT("Select query"), Demand.Query, FString(TEXT("select")));
	TestTrue(TEXT("Select requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandConvertMapsToFunctionActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConvertMapsToFunctionAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandConvertMapsToFunctionActionTest::RunTest(const FString& Parameters)
{
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;
	ArgumentNames.Add(TEXT("value"));

	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_convert"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Convert,
			TEXT("Conv_StringToName"),
			TEXT(""),
			TEXT(""),
			TEXT("Name"),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			CategoryPriority,
			ArgumentNames);

	TestEqual(TEXT("Convert cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("Convert semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Convert);
	TestEqual(TEXT("Convert function operation"), Demand.FunctionOperation, FString(TEXT("convert_function")));
	TestEqual(TEXT("Convert transform operation"), Demand.TransformOperation, FString(TEXT("convert")));
	TestTrue(TEXT("Convert requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("Convert requires target"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("Projected convert function operation"), Context.Semantic.FunctionOperation, FString(TEXT("convert_function")));
	TestEqual(TEXT("Projected convert transform operation"), Context.Semantic.TransformOperation, FString(TEXT("convert")));
	TestEqual(TEXT("Projected convert function evidence"), Context.Evidence.FindRef(TEXT("function_operation")), FString(TEXT("convert_function")));
	TestEqual(TEXT("Projected convert transform evidence"), Context.Evidence.FindRef(TEXT("transform_operation")), FString(TEXT("convert")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandScheduleMapsToFunctionActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ScheduleMapsToFunctionAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandScheduleMapsToFunctionActionTest::RunTest(const FString& Parameters)
{
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;

	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_schedule"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Schedule,
			TEXT("Delay"),
			TEXT(""),
			TEXT(""),
			TEXT(""),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			CategoryPriority,
			ArgumentNames);

	TestEqual(TEXT("Schedule cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::FunctionAction);
	TestEqual(TEXT("Schedule semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Schedule);
	TestEqual(TEXT("Schedule function operation"), Demand.FunctionOperation, FString(TEXT("schedule_function")));
	TestEqual(TEXT("Schedule operation"), Demand.ScheduleOperation, FString(TEXT("latent_or_async")));
	TestTrue(TEXT("Schedule requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));
	TestTrue(TEXT("Schedule requires target"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::Target));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("Projected schedule function operation"), Context.Semantic.FunctionOperation, FString(TEXT("schedule_function")));
	TestEqual(TEXT("Projected schedule operation"), Context.Semantic.ScheduleOperation, FString(TEXT("latent_or_async")));
	TestEqual(TEXT("Projected schedule function evidence"), Context.Evidence.FindRef(TEXT("function_operation")), FString(TEXT("schedule_function")));
	TestEqual(TEXT("Projected schedule evidence"), Context.Evidence.FindRef(TEXT("schedule_operation")), FString(TEXT("latent_or_async")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextStatementConvertRoutesToGenericActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.Statement.ConvertRoutesToGenericAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextStatementConvertRoutesToGenericActionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_generic_convert");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Convert;
	Statement->TransformOperation = TEXT("dynamic_cast");
	Statement->ClassPath = TEXT("/Script/Engine.Actor");
	Statement->Target = TEXT("/Script/Engine.Actor");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("one convert demand"), Demands.Num(), 1);
	const FBlueprintHelperActionContextDemand& Demand = Demands[0];
	TestEqual(TEXT("generic convert cluster"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("generic convert semantic"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Convert);
	TestEqual(TEXT("generic convert transform operation"), Demand.TransformOperation, FString(TEXT("dynamic_cast")));
	TestEqual(TEXT("generic convert clears function operation"), Demand.FunctionOperation, FString());
	TestEqual(TEXT("generic convert class path"), Demand.ClassPath, FString(TEXT("/Script/Engine.Actor")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("projected generic convert cluster"), Context.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("projected generic convert transform"), Context.Semantic.TransformOperation, FString(TEXT("dynamic_cast")));
	TestEqual(TEXT("projected generic convert no function op"), Context.Semantic.FunctionOperation, FString());
	TestEqual(TEXT("projected generic convert class path evidence"), Context.Evidence.FindRef(TEXT("class_path")), FString(TEXT("/Script/Engine.Actor")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextStatementScheduleRoutesToGenericActionTest,
	"BlueprintHelper.GraphWrite.ActionContext.Statement.ScheduleRoutesToGenericAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextStatementScheduleRoutesToGenericActionTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FBlueprintHelperGraphStatementIR> Statement = MakeShared<FBlueprintHelperGraphStatementIR>();
	Statement->StatementId = TEXT("stmt_generic_schedule");
	Statement->Path = TEXT("$.statements[0]");
	Statement->Kind = EBlueprintHelperGraphStatementKind::Schedule;
	Statement->ScheduleOperation = TEXT("timer_delegate_node");
	Statement->GraphLatentAllowed = TEXT("true");

	TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>> Statements;
	Statements.Add(Statement);
	const TArray<FBlueprintHelperActionContextDemand> Demands =
		FBlueprintHelperActionContextDemandCollector::CollectFromStatements(Statements);

	TestEqual(TEXT("one schedule demand"), Demands.Num(), 1);
	const FBlueprintHelperActionContextDemand& Demand = Demands[0];
	TestEqual(TEXT("generic schedule cluster"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("generic schedule semantic"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Schedule);
	TestEqual(TEXT("generic schedule operation"), Demand.ScheduleOperation, FString(TEXT("timer_delegate_node")));
	TestEqual(TEXT("generic schedule clears function operation"), Demand.FunctionOperation, FString());
	TestEqual(TEXT("generic schedule latent evidence"), Demand.GraphLatentAllowed, FString(TEXT("true")));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	Snapshot.Graph.bLatentAllowed = false;
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("projected generic schedule cluster"), Context.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("projected generic schedule operation"), Context.Semantic.ScheduleOperation, FString(TEXT("timer_delegate_node")));
	TestEqual(TEXT("projected generic schedule no function op"), Context.Semantic.FunctionOperation, FString());
	TestEqual(TEXT("projected generic schedule explicit latent evidence wins"), Context.Evidence.FindRef(TEXT("graph_latent_allowed")), FString(TEXT("true")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSingleDemandConstructMapsToStructTypeOperationTest,
	"BlueprintHelper.GraphWrite.ActionContext.SingleDemand.ConstructMapsToStructTypeOperation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSingleDemandConstructMapsToStructTypeOperationTest::RunTest(const FString& Parameters)
{
	TArray<FString> CategoryPriority;
	TArray<FString> ArgumentNames;
	ArgumentNames.Add(TEXT("X"));
	ArgumentNames.Add(TEXT("Y"));
	ArgumentNames.Add(TEXT("Z"));

	const FBlueprintHelperActionContextDemand Demand =
		FBlueprintHelperActionContextDemandCollector::BuildSingleDemand(
			TEXT("expr_construct_vector"),
			TEXT("$.statements[0].value"),
			EBlueprintHelperActionSemanticKind::Construct,
			TEXT("Vector"),
			TEXT(""),
			TEXT(""),
			TEXT("Vector"),
			TEXT("contextual"),
			TEXT("fail_on_ambiguity"),
			CategoryPriority,
			ArgumentNames);

	TestEqual(TEXT("Construct cluster kind"), Demand.ClusterKind, EBlueprintHelperSpawnerClusterKind::GenericAssetStructControlAction);
	TestEqual(TEXT("Construct semantic kind"), Demand.SemanticKind, EBlueprintHelperActionSemanticKind::Construct);
	TestEqual(TEXT("Construct semantic family"), Demand.SemanticFamily, EBlueprintHelperActionSemanticFamily::Struct);
	TestEqual(TEXT("Construct type operation"), Demand.TypeOperation, EBlueprintHelperTypeOperation::Construct);
	TestEqual(TEXT("Construct struct path evidence"), Demand.StructPath, FString(TEXT("Vector")));
	TestTrue(TEXT("Construct requires typed pins"), Demand.RequiredKinds.Contains(EBlueprintHelperActionContextDemandKind::TypedPins));

	FBlueprintHelperActionContextSnapshot Snapshot;
	Snapshot.Graph.GraphName = TEXT("EventGraph");
	const FBlueprintHelperResolvedActionContext Context =
		FBlueprintHelperActionContextInferenceService::BuildContextForTest(Snapshot, Demand);

	TestEqual(TEXT("Projected semantic family"), Context.Semantic.SemanticFamily, EBlueprintHelperActionSemanticFamily::Struct);
	TestEqual(TEXT("Projected type operation"), Context.Semantic.TypeOperation, EBlueprintHelperTypeOperation::Construct);
	TestEqual(TEXT("Projected type operation evidence"), Context.Evidence.FindRef(TEXT("type_operation")), FString(TEXT("construct")));
	TestEqual(TEXT("Projected semantic family evidence"), Context.Evidence.FindRef(TEXT("semantic_family")), FString(TEXT("struct")));
	TestEqual(TEXT("Projected struct path evidence"), Context.Evidence.FindRef(TEXT("struct_path")), FString(TEXT("Vector")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextScopeSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.Scope.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextScopeSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextScope.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextScope.cpp"));
	const FString BuildServiceHeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextBuildService.h"));
	const FString BuildServiceSourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextBuildService.cpp"));

	FString HeaderText;
	FString SourceText;
	FString BuildServiceHeaderText;
	FString BuildServiceSourceText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText)
		|| !LoadRequiredSourceFile(*this, SourcePath, SourceText)
		|| !LoadRequiredSourceFile(*this, BuildServiceHeaderPath, BuildServiceHeaderText)
		|| !LoadRequiredSourceFile(*this, BuildServiceSourcePath, BuildServiceSourceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextScope"),
			TEXT("static bool Build"),
			TEXT("TryBuildRequest"),
			TEXT("GetBundle")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("FBlueprintHelperActionContextSnapshotBuilder::BuildSnapshot"),
			TEXT("FBlueprintHelperActionContextInferenceService::Infer"),
			TEXT("FBlueprintHelperActionContextBundleProjector::TryBuildRequest")
		});
	bComplete &= RequireTokens(
		*this,
		BuildServiceHeaderText,
		BuildServiceHeaderPath,
		{
			TEXT("BuildSync"),
			TEXT("BuildAsyncFromSnapshot"),
			TEXT("FBuildComplete")
		});
	bComplete &= RequireTokens(
		*this,
		BuildServiceSourceText,
		BuildServiceSourcePath,
		{
			TEXT("EAsyncExecution::ThreadPool"),
			TEXT("ENamedThreads::GameThread"),
			TEXT("FBlueprintHelperActionContextInferenceService::Infer")
		});

	TestTrue(TEXT("ActionContext scope/build service source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextRevisionGuardSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.RevisionGuard.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextRevisionGuardSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextRevisionGuard.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextRevisionGuard.cpp"));
	const FString ServiceHeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextRevisionService.h"));
	const FString ServiceSourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextRevisionService.cpp"));

	FString HeaderText;
	FString SourceText;
	FString ServiceHeaderText;
	FString ServiceSourceText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText)
		|| !LoadRequiredSourceFile(*this, SourcePath, SourceText)
		|| !LoadRequiredSourceFile(*this, ServiceHeaderPath, ServiceHeaderText)
		|| !LoadRequiredSourceFile(*this, ServiceSourcePath, ServiceSourceText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextRevisionGuard"),
			TEXT("static bool Validate"),
			TEXT("FBlueprintHelperActionContextRevisionToken")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("Expected.IsCompatibleWith(Current)"),
			TEXT("action_context_stale"),
			TEXT("OutError")
		});
	bComplete &= RequireTokens(
		*this,
		ServiceHeaderText,
		ServiceHeaderPath,
		{
			TEXT("BuildRevisionToken"),
			TEXT("BuildBlueprintRevision"),
			TEXT("BuildGraphRevision")
		});
	bComplete &= RequireTokens(
		*this,
		ServiceSourceText,
		ServiceSourcePath,
		{
			TEXT("BlueprintHelperActionContextRevisionService.h"),
			TEXT("FBlueprintHelperActionContextRevisionService::BuildRevisionToken"),
			TEXT("FBlueprintHelperActionContextRevisionService::BuildBlueprintRevision"),
			TEXT("FBlueprintHelperActionContextRevisionService::BuildGraphRevision")
		});

	TestTrue(TEXT("ActionContext revision guard source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextBundleProjectionSourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.Projection.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextBundleProjectionSourceContractTest::RunTest(const FString& Parameters)
{
	const FString HeaderPath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextBundleProjector.h"));
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextBundleProjector.cpp"));
	const FString ActionContextUtilsPath = BuildActionResolutionPrivateUtilsPath(TEXT("GraphWriteActionContextUtils.cpp"));

	FString HeaderText;
	FString SourceText;
	FString ActionContextUtilsText;
	if (!LoadRequiredSourceFile(*this, HeaderPath, HeaderText)
		|| !LoadRequiredSourceFile(*this, SourcePath, SourceText)
		|| !LoadRequiredSourceFile(*this, ActionContextUtilsPath, ActionContextUtilsText))
	{
		return false;
	}

	bool bComplete = true;
	bComplete &= RequireTokens(
		*this,
		HeaderText,
		HeaderPath,
		{
			TEXT("class BLUEPRINTHELPER_API FBlueprintHelperActionContextBundleProjector"),
			TEXT("static bool TryBuildRequest"),
			TEXT("FBlueprintHelperResolvedActionContextBundle"),
			TEXT("FBlueprintHelperActionResolutionRequest")
		});
	bComplete &= RequireTokens(
		*this,
		SourceText,
		SourcePath,
		{
			TEXT("Bundle.FindByStatementId"),
			TEXT("action_context_not_found"),
			TEXT("action_context_missing_blueprint_or_graph"),
			TEXT("OutRequest.ClusterKind"),
			TEXT("OutRequest.Blueprint"),
			TEXT("OutRequest.TargetGraph"),
			TEXT("OutRequest.StatementId"),
			TEXT("OutRequest.ProjectedContextHash"),
			TEXT("OutRequest.SemanticConstraintsHash"),
			TEXT("OutRequest.Semantic")
		});
	bComplete &= RequireTokens(
		*this,
		ActionContextUtilsText,
		ActionContextUtilsPath,
		{
			TEXT("Semantic.FunctionOperation"),
			TEXT("Semantic.TransformOperation"),
			TEXT("Semantic.ScheduleOperation")
		});

	TestTrue(TEXT("ActionContext bundle projection source contract is complete"), bComplete);
	return bComplete;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextProjectionHashIncludesEvidenceTest,
	"BlueprintHelper.GraphWrite.ActionContext.Projection.SemanticHashIncludesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextProjectionHashIncludesEvidenceTest::RunTest(const FString& Parameters)
{
	auto BuildBundle = [](const FString& PinType)
	{
		FBlueprintHelperResolvedActionContextBundle Bundle;
		Bundle.Revision.AssetPath = TEXT("/Game/Test/BP_Op");
		Bundle.Revision.GraphName = TEXT("EventGraph");
		Bundle.Revision.TaskRunId = TEXT("task-op");
		Bundle.Revision.PlanHash = TEXT("plan-op");

		FBlueprintHelperResolvedActionContext Context;
		Context.StatementId = TEXT("expr_boolean_and");
		Context.ClusterKind = EBlueprintHelperSpawnerClusterKind::FunctionAction;
		Context.GraphName = TEXT("EventGraph");
		Context.Semantic.Kind = EBlueprintHelperActionSemanticKind::Op;
		Context.Semantic.SemanticFamily = EBlueprintHelperActionSemanticFamily::Operator;
		Context.Semantic.FunctionOperation = TEXT("op.boolean_and");
		Context.Evidence.Add(TEXT("op.operation_id"), TEXT("boolean_and"));
		Context.Evidence.Add(TEXT("op.argument_pin_type.0"), PinType);
		Context.Evidence.Add(TEXT("op.argument_pin_type.1"), TEXT("bool"));
		Context.Evidence.Add(TEXT("op.expected_return_pin_type"), TEXT("bool"));
		Bundle.Contexts.Add(MoveTemp(Context));
		return Bundle;
	};

	UBlueprint* Blueprint = NewObject<UBlueprint>();
	UEdGraph* Graph = NewObject<UEdGraph>();

	FBlueprintHelperActionResolutionRequest BoolRequest;
	FBlueprintHelperActionResolutionRequest ByteRequest;
	FString Error;
	const bool bBoolProjected = FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
		BuildBundle(TEXT("bool")),
		TEXT("expr_boolean_and"),
		Blueprint,
		Graph,
		BoolRequest,
		Error);
	const bool bByteProjected = FBlueprintHelperActionContextBundleProjector::TryBuildRequest(
		BuildBundle(TEXT("byte")),
		TEXT("expr_boolean_and"),
		Blueprint,
		Graph,
		ByteRequest,
		Error);

	bool bPassed = true;
	bPassed &= TestTrue(TEXT("bool context projects"), bBoolProjected);
	bPassed &= TestTrue(TEXT("byte context projects"), bByteProjected);
	bPassed &= TestNotEqual(TEXT("semantic hash changes with op evidence"), BoolRequest.SemanticConstraintsHash, ByteRequest.SemanticConstraintsHash);
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextOpDtoBoundarySourceContractTest,
	"BlueprintHelper.GraphWrite.ActionContext.OpDtoBoundary.SourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextOpDtoBoundarySourceContractTest::RunTest(const FString& Parameters)
{
	const FString DemandFilePath = BuildActionContextPublicPath(TEXT("BlueprintHelperActionContextTypes.h"));
	const FString SemanticFilePath = BuildActionResolutionPublicPath(TEXT("BlueprintHelperActionResolutionCore.h"));

	FString DemandSourceText;
	FString SemanticSourceText;
	if (!LoadRequiredSourceFile(*this, DemandFilePath, DemandSourceText)
		|| !LoadRequiredSourceFile(*this, SemanticFilePath, SemanticSourceText))
	{
		return false;
	}

	const FString DemandBody = ExtractStructBody(DemandSourceText, TEXT("FBlueprintHelperActionContextDemand"));
	const FString SemanticBody = ExtractStructBody(SemanticSourceText, TEXT("FBlueprintHelperActionSemanticConstraints"));
	if (DemandBody.IsEmpty() || SemanticBody.IsEmpty())
	{
		AddError(TEXT("ActionContext op DTO boundary could not locate core structs."));
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("OpSpawnPath"),
		TEXT("FunctionStableId"),
		TEXT("OwnerClass"),
		TEXT("FunctionName"),
		TEXT("bRequiresArrayTypedPins")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (DemandBody.Contains(Token) || SemanticBody.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("Op capability evidence must not add core DTO field '%s'; use ContextEvidence and focused readers."),
				*Token));
			bClean = false;
		}
	}

	TestTrue(TEXT("op-specific details stay out of core ActionContext DTOs"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextWorkerInferenceSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionContext.SourceHygiene.WorkerInferencePureDto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextWorkerInferenceSourceHygieneTest::RunTest(const FString& Parameters)
{
	const FString SourcePath = BuildActionContextPrivatePath(TEXT("BlueprintHelperActionContextInferenceService.cpp"));

	FString SourceText;
	if (!LoadRequiredSourceFile(*this, SourcePath, SourceText))
	{
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("UObject*"),
		TEXT("UBlueprint*"),
		TEXT("UEdGraph*"),
		TEXT("UEdGraphPin*"),
		TEXT("FindObject"),
		TEXT("LoadObject"),
		TEXT("GetSchema")
	};

	bool bClean = true;
	for (const FString& Token : ForbiddenTokens)
	{
		if (SourceText.Contains(Token))
		{
			AddError(FString::Printf(
				TEXT("Worker inference must not access UObject or UE graph APIs; forbidden token '%s' found in %s"),
				*Token,
				*SourcePath));
			bClean = false;
		}
	}

	TestTrue(TEXT("Worker inference remains pure DTO source"), bClean);
	return bClean;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperActionContextSettingsHardcodedSourceHygieneTest,
	"BlueprintHelper.GraphWrite.ActionContext.SourceHygiene.SettingsDefaultsNotHardcoded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperActionContextSettingsHardcodedSourceHygieneTest::RunTest(const FString& Parameters)
{
	TArray<FString> Files;
	const bool bHasContextRoots = CollectActionContextSourceFiles(*this, Files);
	if (!bHasContextRoots)
	{
		return false;
	}

	const TArray<FString> ForbiddenTokens = {
		TEXT("settings_default"),
		TEXT("SearchMode = TEXT("),
		TEXT("AmbiguityPolicy = TEXT("),
		TEXT("MaxCandidates = "),
		TEXT("CandidateLimit = ")
	};

	bool bClean = true;
	for (const FString& File : Files)
	{
		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *File))
		{
			continue;
		}

		for (const FString& Token : ForbiddenTokens)
		{
			if (SourceText.Contains(Token))
			{
				AddError(FString::Printf(
					TEXT("ActionContext policy/default values must come from the unified settings runtime boundary; forbidden token '%s' found in %s"),
					*Token,
					*File));
				bClean = false;
			}
		}
	}

	TestTrue(TEXT("ActionContext source avoids hardcoded settings defaults"), bClean);
	return bClean;
}

#endif
