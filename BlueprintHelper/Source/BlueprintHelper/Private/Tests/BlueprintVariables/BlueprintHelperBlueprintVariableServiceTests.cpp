#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "UObject/Package.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"

class FBlueprintHelperBlueprintVariableServiceTestsLocalUtils
{
public:
static FString MakeVariableServiceTestObjectName(const FString& Prefix)
{
	return FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static UPackage* MakeVariableServiceTestPackage(const FString& Prefix)
{
	UPackage* Package = CreatePackage(*FString::Printf(
		TEXT("/Game/BlueprintHelperSafety/%s"),
		*MakeVariableServiceTestObjectName(Prefix)));
	Package->SetDirtyFlag(false);
	return Package;
}

static UBlueprint* MakeSafetyActorBlueprint(const FString& Prefix)
{
	UPackage* Package = MakeVariableServiceTestPackage(Prefix);
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		Package,
		*MakeVariableServiceTestObjectName(TEXT("BP_VariableService")),
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		TEXT("BlueprintHelperVariableServiceTests"));
	Package->SetDirtyFlag(false);
	return Blueprint;
}

static UEdGraph* AddSafetyFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
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

static UK2Node_FunctionEntry* FindSafetyFunctionEntry(UEdGraph* FunctionGraph)
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

static FBPVariableDescription* FindSafetyLocalVariable(UEdGraph* FunctionGraph, const FString& VariableName)
{
	UK2Node_FunctionEntry* Entry = FindSafetyFunctionEntry(FunctionGraph);
	if (!Entry)
	{
		return nullptr;
	}

	const FName VariableFName(*VariableName);
	for (FBPVariableDescription& Variable : Entry->LocalVariables)
	{
		if (Variable.VarName == VariableFName)
		{
			return &Variable;
		}
	}
	return nullptr;
}

static FBPVariableDescription* FindSafetyMemberVariable(UBlueprint* Blueprint, const FString& VariableName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	const FName VariableFName(*VariableName);
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarName == VariableFName)
		{
			return &Variable;
		}
	}
	return nullptr;
}

static TSharedRef<FJsonObject> MakePinType(const FString& Category)
{
	TSharedRef<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), Category);
	return PinType;
}

static TSharedRef<FJsonObject> MakeStringIntMapPinType()
{
	TSharedRef<FJsonObject> PinType = MakePinType(TEXT("string"));
	PinType->SetStringField(TEXT("container_type"), TEXT("map"));
	PinType->SetObjectField(TEXT("value_type"), MakePinType(TEXT("int")));
	return PinType;
}

static TSharedRef<FJsonObject> MakeServicePayload(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const FString& VariableName,
	const FString& Category)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : TEXT(""));
	Payload->SetStringField(TEXT("function_name"), FunctionName);
	Payload->SetStringField(TEXT("name"), VariableName);
	Payload->SetObjectField(TEXT("pin_type"), MakePinType(Category));
	return Payload;
}

static TSharedPtr<FJsonValue> MakeLocalVariableValue(const FString& VariableName, const FString& Category)
{
	TSharedRef<FJsonObject> Variable = MakeShared<FJsonObject>();
	Variable->SetStringField(TEXT("name"), VariableName);
	Variable->SetObjectField(TEXT("pin_type"), MakePinType(Category));
	return MakeShared<FJsonValueObject>(Variable);
}

static TSharedRef<FJsonObject> MakeLocalVariablesPayload(
	UBlueprint* Blueprint,
	const FString& FunctionName,
	const TArray<TSharedPtr<FJsonValue>>& Variables,
	bool bDryRun)
{
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : TEXT(""));
	Payload->SetStringField(TEXT("function_name"), FunctionName);
	Payload->SetArrayField(TEXT("variables"), Variables);
	Payload->SetBoolField(TEXT("dry_run"), bDryRun);
	return Payload;
}

static TSharedPtr<FJsonValue> MakeSetting(const FString& PropertyPath, const TSharedPtr<FJsonValue>& Value)
{
	TSharedRef<FJsonObject> Setting = MakeShared<FJsonObject>();
	Setting->SetStringField(TEXT("property_path"), PropertyPath);
	Setting->SetField(TEXT("value"), Value);
	return MakeShared<FJsonValueObject>(Setting);
}

static bool JsonArrayContainsVariableName(
	const TArray<TSharedPtr<FJsonValue>>& Values,
	const FString& VariableName)
{
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		FString FoundName;
		if (Object.IsValid() &&
			Object->TryGetStringField(TEXT("variable_name"), FoundName) &&
			FoundName == VariableName)
		{
			return true;
		}
	}
	return false;
}

static bool IsRemoveDryRunCompatibleStatus(const EBlueprintHelperToolStatus Status)
{
	return Status == EBlueprintHelperToolStatus::DryRun ||
		Status == EBlueprintHelperToolStatus::Completed ||
		Status == EBlueprintHelperToolStatus::NoOp;
}

static TSharedPtr<FJsonObject> MakeLocalVariableTaskPlanStep(bool bUseRemoveOp)
{
	TSharedPtr<FJsonObject> Step = MakeShared<FJsonObject>();
	Step->SetStringField(TEXT("step_id"), TEXT("step_local_variables"));
	Step->SetStringField(TEXT("capability"), TEXT("blueprint_variable"));

	TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
	Target->SetStringField(TEXT("asset_path"), TEXT("/Game/Blueprints/BP_Test"));
	Target->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	Step->SetObjectField(TEXT("target"), Target);

	TSharedPtr<FJsonObject> PinType = MakeShared<FJsonObject>();
	PinType->SetStringField(TEXT("category"), TEXT("int"));

	TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
	Op->SetStringField(TEXT("op"), bUseRemoveOp ? TEXT("remove_local_variable") : TEXT("ensure_local_variable"));
	Op->SetStringField(TEXT("name"), bUseRemoveOp ? TEXT("OldScratchValue") : TEXT("ScratchValue"));
	if (!bUseRemoveOp)
	{
		Op->SetObjectField(TEXT("pin_type"), PinType);
	}

	TArray<TSharedPtr<FJsonValue>> Ops;
	Ops.Add(MakeShared<FJsonValueObject>(Op.ToSharedRef()));

	TSharedPtr<FJsonObject> Write = MakeShared<FJsonObject>();
	Write->SetStringField(TEXT("strategy"), TEXT("local_variables"));
	Write->SetArrayField(TEXT("ops"), Ops);
	Step->SetObjectField(TEXT("write"), Write);
	return Step;
}

};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableAddLocalVariableTest,
	"BlueprintHelper.Safety.BlueprintVariable.AddLocalVariableWritesFunctionLocalVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableAddLocalVariableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("AddLocalVariable"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestNotNull(TEXT("function entry is created"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyFunctionEntry(FunctionGraph));

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariable(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeServicePayload(Blueprint, TEXT("CalculateDamage"), TEXT("DamageScale"), TEXT("int")));

	TestTrue(TEXT("add local variable succeeds"), AddResult.bOk);
	TestEqual(TEXT("add local variable applies change"), AddResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("add local variable marks modified"), AddResult.bModified);
	TestTrue(TEXT("add local variable requests compile"), AddResult.Validation.IsSet() && AddResult.Validation->bShouldCompile);
	TestTrue(TEXT("add local variable requests save"), AddResult.Validation.IsSet() && AddResult.Validation->bShouldSave);

	const FBPVariableDescription* DamageScale = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("DamageScale"));
	TestNotNull(TEXT("DamageScale local variable exists"), DamageScale);
	if (DamageScale)
	{
		TestEqual(TEXT("DamageScale has int type"), DamageScale->VarType.PinCategory, UEdGraphSchema_K2::PC_Int);
	}

	FString DataSchema;
	TestTrue(TEXT("add local variable returns data schema"),
		AddResult.Data.IsValid() && AddResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("add local variable data schema"), DataSchema, FString(TEXT("AddLocalVariable.v1")));

	const TSharedPtr<FJsonObject>* AddResultObject = nullptr;
	TestTrue(TEXT("add local variable returns add_result"),
		AddResult.Data.IsValid() && AddResult.Data->TryGetObjectField(TEXT("add_result"), AddResultObject));
	bool bSuccess = false;
	TestTrue(TEXT("add local variable result success is present"),
		AddResultObject && AddResultObject->IsValid() && (*AddResultObject)->TryGetBoolField(TEXT("success"), bSuccess));
	TestTrue(TEXT("add local variable result success is true"), bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableAddLocalVariablesBatchTest,
	"BlueprintHelper.Safety.BlueprintVariable.AddLocalVariablesBatchWritesFunctionLocals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableAddLocalVariablesBatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("AddLocalVariablesBatch"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);
	TestNotNull(TEXT("function entry is created"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyFunctionEntry(FunctionGraph));

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	TArray<TSharedPtr<FJsonValue>> Variables;
	Variables.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariableValue(TEXT("DamageScale"), TEXT("int")));
	Variables.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariableValue(TEXT("DamageLabel"), TEXT("string")));

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariables(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariablesPayload(Blueprint, TEXT("CalculateDamage"), Variables, false));

	TestTrue(TEXT("add local variables succeeds"), AddResult.bOk);
	TestEqual(TEXT("add local variables applies change"), AddResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("add local variables marks modified"), AddResult.bModified);
	TestTrue(TEXT("add local variables requests compile"), AddResult.Validation.IsSet() && AddResult.Validation->bShouldCompile);
	TestTrue(TEXT("add local variables requests save"), AddResult.Validation.IsSet() && AddResult.Validation->bShouldSave);

	const FBPVariableDescription* DamageScale = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("DamageScale"));
	const FBPVariableDescription* DamageLabel = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("DamageLabel"));
	TestNotNull(TEXT("DamageScale local variable exists"), DamageScale);
	TestNotNull(TEXT("DamageLabel local variable exists"), DamageLabel);
	if (DamageScale)
	{
		TestEqual(TEXT("DamageScale has int type"), DamageScale->VarType.PinCategory, UEdGraphSchema_K2::PC_Int);
	}
	if (DamageLabel)
	{
		TestEqual(TEXT("DamageLabel has string type"), DamageLabel->VarType.PinCategory, UEdGraphSchema_K2::PC_String);
	}

	FString DataSchema;
	TestTrue(TEXT("add local variables returns data schema"),
		AddResult.Data.IsValid() && AddResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("add local variables data schema"), DataSchema, FString(TEXT("AddLocalVariables.v1")));

	const TSharedPtr<FJsonObject>* AddResultObject = nullptr;
	TestTrue(TEXT("add local variables returns add_result"),
		AddResult.Data.IsValid() && AddResult.Data->TryGetObjectField(TEXT("add_result"), AddResultObject));
	if (AddResultObject && AddResultObject->IsValid())
	{
		TestEqual(TEXT("add local variables requested count"),
			static_cast<int32>((*AddResultObject)->GetIntegerField(TEXT("requested_count"))), 2);
		TestEqual(TEXT("add local variables added count"),
			static_cast<int32>((*AddResultObject)->GetIntegerField(TEXT("added_count"))), 2);
		TestEqual(TEXT("add local variables no-op count"),
			static_cast<int32>((*AddResultObject)->GetIntegerField(TEXT("no_op_count"))), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableReadLocalVariablesTest,
	"BlueprintHelper.Safety.BlueprintVariable.ReadLocalVariablesReturnsFunctionLocals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableReadLocalVariablesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("ReadLocalVariables"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariable(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeServicePayload(Blueprint, TEXT("CalculateDamage"), TEXT("LocalDamage"), TEXT("int")));
	TestTrue(TEXT("local variable is added before read"), AddResult.bOk);
	TestNotNull(TEXT("LocalDamage local variable exists before read"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("LocalDamage")));

	TSharedRef<FJsonObject> ReadPayload = MakeShared<FJsonObject>();
	ReadPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	ReadPayload->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));

	const FBlueprintHelperToolResultBase ReadResult = VariableService.ReadLocalVariables(ReadPayload);
	TestTrue(TEXT("read local variables succeeds"), ReadResult.bOk);
	TestEqual(TEXT("read local variables completes"), ReadResult.Status, EBlueprintHelperToolStatus::Completed);

	FString DataSchema;
	TestTrue(TEXT("read local variables returns data schema"),
		ReadResult.Data.IsValid() && ReadResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("read local variables data schema"), DataSchema, FString(TEXT("ReadLocalVariables.v1")));

	FString FunctionName;
	TestTrue(TEXT("read local variables returns function name"),
		ReadResult.Data.IsValid() && ReadResult.Data->TryGetStringField(TEXT("function_name"), FunctionName));
	TestEqual(TEXT("read local variables function name"), FunctionName, FString(TEXT("CalculateDamage")));

	const TArray<TSharedPtr<FJsonValue>>* LocalVariables = nullptr;
	TestTrue(TEXT("read local variables returns local_variables array"),
		ReadResult.Data.IsValid() && ReadResult.Data->TryGetArrayField(TEXT("local_variables"), LocalVariables));
	TestTrue(TEXT("read local variables includes LocalDamage"),
		LocalVariables && FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::JsonArrayContainsVariableName(*LocalVariables, TEXT("LocalDamage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableSetLocalVariablePropertiesTest,
	"BlueprintHelper.Safety.BlueprintVariable.SetLocalVariablePropertiesWritesMetadataAndDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableSetLocalVariablePropertiesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("SetLocalVariableProperties"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariable(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeServicePayload(Blueprint, TEXT("CalculateDamage"), TEXT("DamageScale"), TEXT("int")));
	TestTrue(TEXT("local variable is added before property write"), AddResult.bOk);

	TArray<TSharedPtr<FJsonValue>> Settings;
	Settings.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSetting(TEXT("category"), MakeShared<FJsonValueString>(TEXT("Combat"))));
	Settings.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSetting(TEXT("tooltip"), MakeShared<FJsonValueString>(TEXT("Damage multiplier used by the function."))));
	Settings.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSetting(TEXT("default_value"), MakeShared<FJsonValueString>(TEXT("7"))));

	TSharedRef<FJsonObject> SetPayload = MakeShared<FJsonObject>();
	SetPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	SetPayload->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	SetPayload->SetStringField(TEXT("name"), TEXT("DamageScale"));
	SetPayload->SetArrayField(TEXT("settings"), Settings);

	const FBlueprintHelperToolResultBase SetResult = VariableService.SetLocalVariableProperties(SetPayload);
	TestTrue(TEXT("set local variable properties succeeds"), SetResult.bOk);
	TestEqual(TEXT("set local variable properties applies change"), SetResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("set local variable properties requests compile"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldCompile);
	TestTrue(TEXT("set local variable properties requests save"), SetResult.Validation.IsSet() && SetResult.Validation->bShouldSave);

	const FBPVariableDescription* DamageScale = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("DamageScale"));
	TestNotNull(TEXT("DamageScale local variable still exists"), DamageScale);
	if (DamageScale)
	{
		TestEqual(TEXT("DamageScale category is written"), DamageScale->Category.ToString(), FString(TEXT("Combat")));
		TestEqual(TEXT("DamageScale default value is written"), DamageScale->DefaultValue, FString(TEXT("7")));
		TestTrue(TEXT("DamageScale tooltip metadata exists"), DamageScale->HasMetaData(FBlueprintMetadata::MD_Tooltip));
		if (DamageScale->HasMetaData(FBlueprintMetadata::MD_Tooltip))
		{
			TestEqual(
				TEXT("DamageScale tooltip metadata is written"),
				DamageScale->GetMetaData(FBlueprintMetadata::MD_Tooltip),
				FString(TEXT("Damage multiplier used by the function.")));
		}
	}

	FString DataSchema;
	TestTrue(TEXT("set local variable properties returns data schema"),
		SetResult.Data.IsValid() && SetResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("set local variable properties data schema"), DataSchema, FString(TEXT("SetLocalVariableProperties.v1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableRemoveLocalVariableDryRunTest,
	"BlueprintHelper.Safety.BlueprintVariable.RemoveLocalVariableDryRunDoesNotModify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableRemoveLocalVariableDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("RemoveLocalVariableDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariable(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeServicePayload(Blueprint, TEXT("CalculateDamage"), TEXT("ScratchValue"), TEXT("int")));
	TestTrue(TEXT("local variable is added before dry-run remove"), AddResult.bOk);
	TestNotNull(TEXT("ScratchValue local variable exists before dry-run remove"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("ScratchValue")));

	TSharedRef<FJsonObject> RemovePayload = MakeShared<FJsonObject>();
	RemovePayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	RemovePayload->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	RemovePayload->SetStringField(TEXT("name"), TEXT("ScratchValue"));
	RemovePayload->SetBoolField(TEXT("dry_run"), true);

	const FBlueprintHelperToolResultBase RemoveResult = VariableService.RemoveLocalVariable(RemovePayload);
	TestTrue(TEXT("remove local variable dry run succeeds"), RemoveResult.bOk);
	TestTrue(TEXT("remove local variable dry-run status is non-mutating compatible"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::IsRemoveDryRunCompatibleStatus(RemoveResult.Status));
	TestFalse(TEXT("remove local variable dry run does not mark modified"), RemoveResult.bModified);
	TestNotNull(TEXT("ScratchValue local variable remains after dry-run remove"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("ScratchValue")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableRemoveLocalVariablesBatchDryRunTest,
	"BlueprintHelper.Safety.BlueprintVariable.RemoveLocalVariablesBatchDryRunDoesNotModify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableRemoveLocalVariablesBatchDryRunTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("RemoveLocalVariablesBatchDryRun"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	TArray<TSharedPtr<FJsonValue>> VariablesToAdd;
	VariablesToAdd.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariableValue(TEXT("FirstScratchValue"), TEXT("int")));
	VariablesToAdd.Add(FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariableValue(TEXT("SecondScratchValue"), TEXT("float")));
	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariables(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariablesPayload(Blueprint, TEXT("CalculateDamage"), VariablesToAdd, false));
	TestTrue(TEXT("local variables are added before dry-run remove"), AddResult.bOk);
	TestNotNull(TEXT("FirstScratchValue exists before dry-run remove"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("FirstScratchValue")));
	TestNotNull(TEXT("SecondScratchValue exists before dry-run remove"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("SecondScratchValue")));

	TArray<TSharedPtr<FJsonValue>> VariablesToRemove;
	VariablesToRemove.Add(MakeShared<FJsonValueString>(TEXT("FirstScratchValue")));
	VariablesToRemove.Add(MakeShared<FJsonValueString>(TEXT("SecondScratchValue")));
	const FBlueprintHelperToolResultBase RemoveResult = VariableService.RemoveLocalVariables(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariablesPayload(Blueprint, TEXT("CalculateDamage"), VariablesToRemove, true));

	TestTrue(TEXT("remove local variables dry run succeeds"), RemoveResult.bOk);
	TestEqual(TEXT("remove local variables dry run status"), RemoveResult.Status, EBlueprintHelperToolStatus::DryRun);
	TestFalse(TEXT("remove local variables dry run does not mark modified"), RemoveResult.bModified);
	TestFalse(TEXT("remove local variables dry run does not request validation"), RemoveResult.Validation.IsSet());
	TestNotNull(TEXT("FirstScratchValue remains after dry-run remove"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("FirstScratchValue")));
	TestNotNull(TEXT("SecondScratchValue remains after dry-run remove"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("SecondScratchValue")));

	FString DataSchema;
	TestTrue(TEXT("remove local variables returns data schema"),
		RemoveResult.Data.IsValid() && RemoveResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("remove local variables data schema"), DataSchema, FString(TEXT("RemoveLocalVariables.v1")));

	const TSharedPtr<FJsonObject>* RemoveResultObject = nullptr;
	TestTrue(TEXT("remove local variables returns remove_result"),
		RemoveResult.Data.IsValid() && RemoveResult.Data->TryGetObjectField(TEXT("remove_result"), RemoveResultObject));
	if (RemoveResultObject && RemoveResultObject->IsValid())
	{
		TestEqual(TEXT("remove local variables requested count"),
			static_cast<int32>((*RemoveResultObject)->GetIntegerField(TEXT("requested_count"))), 2);
		TestEqual(TEXT("remove local variables removed count"),
			static_cast<int32>((*RemoveResultObject)->GetIntegerField(TEXT("removed_count"))), 0);
		TestEqual(TEXT("remove local variables no-op count"),
			static_cast<int32>((*RemoveResultObject)->GetIntegerField(TEXT("no_op_count"))), 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableRemoveLocalVariableDeletesTest,
	"BlueprintHelper.Safety.BlueprintVariable.RemoveLocalVariableDeletesWhenNoReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableRemoveLocalVariableDeletesTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("RemoveLocalVariableDeletes"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);
	UEdGraph* FunctionGraph = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::AddSafetyFunctionGraph(Blueprint, TEXT("CalculateDamage"));
	TestNotNull(TEXT("function graph is created"), FunctionGraph);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddLocalVariable(
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeServicePayload(Blueprint, TEXT("CalculateDamage"), TEXT("ScratchValue"), TEXT("int")));
	TestTrue(TEXT("local variable is added before remove"), AddResult.bOk);
	TestNotNull(TEXT("ScratchValue local variable exists before remove"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("ScratchValue")));

	TSharedRef<FJsonObject> RemovePayload = MakeShared<FJsonObject>();
	RemovePayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	RemovePayload->SetStringField(TEXT("function_name"), TEXT("CalculateDamage"));
	RemovePayload->SetStringField(TEXT("name"), TEXT("ScratchValue"));
	RemovePayload->SetBoolField(TEXT("dry_run"), false);

	const FBlueprintHelperToolResultBase RemoveResult = VariableService.RemoveLocalVariable(RemovePayload);
	TestTrue(TEXT("remove local variable succeeds"), RemoveResult.bOk);
	TestEqual(TEXT("remove local variable applies change"), RemoveResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("remove local variable marks modified"), RemoveResult.bModified);
	TestTrue(TEXT("remove local variable requests compile"), RemoveResult.Validation.IsSet() && RemoveResult.Validation->bShouldCompile);
	TestTrue(TEXT("remove local variable requests save"), RemoveResult.Validation.IsSet() && RemoveResult.Validation->bShouldSave);
	TestTrue(TEXT("ScratchValue local variable is removed"),
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyLocalVariable(FunctionGraph, TEXT("ScratchValue")) == nullptr);

	FString DataSchema;
	TestTrue(TEXT("remove local variable returns data schema"),
		RemoveResult.Data.IsValid() && RemoveResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("remove local variable data schema"), DataSchema, FString(TEXT("RemoveLocalVariable.v1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableLocalVariablesTaskPlanDryRunAdapterTest,
	"BlueprintHelper.Safety.BlueprintVariable.LocalVariablesTaskPlanAdapterSupportsDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableLocalVariablesTaskPlanDryRunAdapterTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Step = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeLocalVariableTaskPlanStep(false);

	FBlueprintHelperBlueprintVariableTaskPlanPayload BuiltPayload;
	FBlueprintHelperToolError Error;
	const bool bBuilt = FBlueprintHelperBlueprintVariableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
		Step,
		true,
		BuiltPayload,
		Error);

	TestTrue(TEXT("local variable TaskPlan step builds"), bBuilt);
	TestEqual(TEXT("local variable step lowers to batch adapter"),
		BuiltPayload.AdapterOperation,
		FString(FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch));
	TestTrue(TEXT("local variable adapter supports true dry-run"), BuiltPayload.bAdapterDryRunSupported);
	TestNotNull(TEXT("local variable payload exists"), BuiltPayload.Payload.Get());

	bool bDryRun = false;
	FString Strategy;
	FString FunctionName;
	TestTrue(TEXT("local variable payload carries dry_run"),
		BuiltPayload.Payload.IsValid() && BuiltPayload.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun));
	TestTrue(TEXT("local variable dry_run is true"), bDryRun);
	TestTrue(TEXT("local variable payload carries strategy"),
		BuiltPayload.Payload.IsValid() && BuiltPayload.Payload->TryGetStringField(TEXT("strategy"), Strategy));
	TestEqual(TEXT("local variable strategy preserved"), Strategy, FString(TEXT("local_variables")));
	TestTrue(TEXT("local variable payload carries function_name"),
		BuiltPayload.Payload.IsValid() && BuiltPayload.Payload->TryGetStringField(TEXT("function_name"), FunctionName));
	TestEqual(TEXT("local variable function_name preserved"), FunctionName, FString(TEXT("CalculateDamage")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableAddMemberStringIntMapVariableTest,
	"BlueprintHelper.Safety.BlueprintVariable.AddMemberStringIntMapVariable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableAddMemberStringIntMapVariableTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("AddMemberStringIntMapVariable"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	TSharedRef<FJsonObject> AddPayload = MakeShared<FJsonObject>();
	AddPayload->SetStringField(TEXT("asset_path"), Blueprint ? Blueprint->GetPathName() : TEXT(""));
	AddPayload->SetStringField(TEXT("name"), TEXT("ScoresByPlayer"));
	AddPayload->SetObjectField(TEXT("pin_type"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeStringIntMapPinType());

	const FBlueprintHelperToolResultBase AddResult = VariableService.AddMemberVariable(AddPayload);
	TestTrue(TEXT("string-int map member variable is added"), AddResult.bOk);

	const FBPVariableDescription* Variable =
		FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("ScoresByPlayer"));
	TestNotNull(TEXT("ScoresByPlayer exists"), Variable);
	if (!Variable)
	{
		return false;
	}

	TestEqual(TEXT("map key pin category is string"), Variable->VarType.PinCategory, UEdGraphSchema_K2::PC_String);
	TestEqual(TEXT("variable container is map"), Variable->VarType.ContainerType, EPinContainerType::Map);
	TestEqual(TEXT("map value terminal category is int"), Variable->VarType.PinValueType.TerminalCategory, UEdGraphSchema_K2::PC_Int);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperBlueprintVariableRemoveMemberVariablesBatchTest,
	"BlueprintHelper.Safety.BlueprintVariable.RemoveMemberVariablesBatchDeletesMembers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperBlueprintVariableRemoveMemberVariablesBatchTest::RunTest(const FString& Parameters)
{
	UBlueprint* Blueprint = FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakeSafetyActorBlueprint(TEXT("RemoveMemberVariablesBatch"));
	TestNotNull(TEXT("test Blueprint is created"), Blueprint);

	FBlueprintHelperGraphResolver GraphResolver;
	FBlueprintHelperBlueprintStructureService StructureService(GraphResolver);
	FBlueprintHelperBlueprintVariableService VariableService(GraphResolver, StructureService);

	auto AddMember = [&VariableService, Blueprint](const FString& Name)
	{
		TSharedRef<FJsonObject> AddPayload = MakeShared<FJsonObject>();
		AddPayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
		AddPayload->SetStringField(TEXT("name"), Name);
		AddPayload->SetObjectField(TEXT("pin_type"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::MakePinType(TEXT("int")));
		return VariableService.AddMemberVariable(AddPayload);
	};

	TestTrue(TEXT("First member variable is added"), AddMember(TEXT("FirstValue")).bOk);
	TestTrue(TEXT("Second member variable is added"), AddMember(TEXT("SecondValue")).bOk);
	TestNotNull(TEXT("FirstValue exists before batch remove"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("FirstValue")));
	TestNotNull(TEXT("SecondValue exists before batch remove"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("SecondValue")));

	TSharedRef<FJsonObject> FirstVariable = MakeShared<FJsonObject>();
	FirstVariable->SetStringField(TEXT("name"), TEXT("FirstValue"));
	TSharedRef<FJsonObject> SecondVariable = MakeShared<FJsonObject>();
	SecondVariable->SetStringField(TEXT("variable_name"), TEXT("SecondValue"));

	TArray<TSharedPtr<FJsonValue>> Variables;
	Variables.Add(MakeShared<FJsonValueObject>(FirstVariable));
	Variables.Add(MakeShared<FJsonValueObject>(SecondVariable));

	TSharedRef<FJsonObject> RemovePayload = MakeShared<FJsonObject>();
	RemovePayload->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
	RemovePayload->SetArrayField(TEXT("variables"), Variables);
	RemovePayload->SetBoolField(TEXT("dry_run"), false);

	const FBlueprintHelperToolResultBase RemoveResult = VariableService.RemoveMemberVariables(RemovePayload);
	TestTrue(TEXT("remove member variables succeeds"), RemoveResult.bOk);
	TestEqual(TEXT("remove member variables applies change"), RemoveResult.Status, EBlueprintHelperToolStatus::Applied);
	TestTrue(TEXT("remove member variables requests compile"), RemoveResult.Validation.IsSet() && RemoveResult.Validation->bShouldCompile);
	TestTrue(TEXT("remove member variables requests save"), RemoveResult.Validation.IsSet() && RemoveResult.Validation->bShouldSave);
	TestTrue(TEXT("FirstValue is removed"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("FirstValue")) == nullptr);
	TestTrue(TEXT("SecondValue is removed"), FBlueprintHelperBlueprintVariableServiceTestsLocalUtils::FindSafetyMemberVariable(Blueprint, TEXT("SecondValue")) == nullptr);

	FString DataSchema;
	TestTrue(TEXT("remove member variables returns data schema"),
		RemoveResult.Data.IsValid() && RemoveResult.Data->TryGetStringField(TEXT("schema"), DataSchema));
	TestEqual(TEXT("remove member variables data schema"), DataSchema, FString(TEXT("RemoveMemberVariables.v1")));

	const TSharedPtr<FJsonObject>* RemoveResultObject = nullptr;
	TestTrue(TEXT("remove member variables returns remove_result"),
		RemoveResult.Data.IsValid() && RemoveResult.Data->TryGetObjectField(TEXT("remove_result"), RemoveResultObject));
	if (RemoveResultObject && RemoveResultObject->IsValid())
	{
		TestEqual(TEXT("remove member variables requested count"),
			static_cast<int32>((*RemoveResultObject)->GetIntegerField(TEXT("requested_count"))), 2);
		TestEqual(TEXT("remove member variables removed count"),
			static_cast<int32>((*RemoveResultObject)->GetIntegerField(TEXT("removed_count"))), 2);
	}
	return true;
}

#endif
