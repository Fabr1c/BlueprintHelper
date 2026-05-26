#if WITH_DEV_AUTOMATION_TESTS

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

#include "Misc/AutomationTest.h"

namespace
{
struct FExpectedFieldCapabilityMatrixRow
{
	const TCHAR* Id;
	const TCHAR* NodeFamily;
	const TCHAR* NodeClass;
	EBlueprintHelperFieldCapabilityPriority Priority;
	EBlueprintHelperFieldCapabilityRootKind RootKind;
	EBlueprintHelperFieldCapabilityAccessMode AccessMode;
	bool bRequiresOwnerClass;
	bool bRequiresFunctionScope;
	bool bRequiresTargetPin;
	bool bRequiresPropertyPath;
	bool bExec;
};

struct FRejectedFieldCapabilityRow
{
	const TCHAR* Id;
	const TCHAR* Reason;
};

static const TArray<FExpectedFieldCapabilityMatrixRow>& ExpectedFieldCapabilityMatrix()
{
	static const TArray<FExpectedFieldCapabilityMatrixRow> Rows = {
		{TEXT("field.member_get"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Get, false, false, false, false, false},
		{TEXT("field.member_set"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Set, false, false, false, false, true},
		{TEXT("field.inherited_member_get"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Get, true, false, false, false, false},
		{TEXT("field.inherited_member_set"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Set, true, false, false, false, true},
		{TEXT("field.sparse_data_get"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::SparseData, EBlueprintHelperFieldCapabilityAccessMode::Get, true, false, false, false, false},
		{TEXT("field.function_param_get"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::FunctionParam, EBlueprintHelperFieldCapabilityAccessMode::Get, false, true, false, false, false},
		{TEXT("field.local_get"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Get, false, true, false, false, false},
		{TEXT("field.local_set"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Set, false, true, false, false, true},
		{TEXT("field.object_pin_member_get"), TEXT("variable_get_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Get, true, false, true, false, false},
		{TEXT("field.object_pin_member_set"), TEXT("variable_set_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Set, true, false, true, false, true},
		{TEXT("field.component_ref_get"), TEXT("component_variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Get, false, false, false, false, false},
		{TEXT("field.component_ref_set"), TEXT("component_variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Set, false, false, false, false, true},
		{TEXT("field.component_property_get"), TEXT("component_property_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Get, true, false, true, true, false},
		{TEXT("field.component_property_set"), TEXT("component_property_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Set, true, false, true, true, true},
		{TEXT("field.struct_member_get"), TEXT("break_struct"), TEXT("/Script/BlueprintGraph.K2Node_BreakStruct"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Get, false, false, false, true, false},
		{TEXT("field.struct_member_set"), TEXT("set_fields_in_struct"), TEXT("/Script/BlueprintGraph.K2Node_SetFieldsInStruct"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Set, false, false, false, true, true},
		{TEXT("field.nested_property_path"), TEXT("property_path_fragment"), TEXT("BlueprintHelper.Field.PropertyPathFragment"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath, EBlueprintHelperFieldCapabilityAccessMode::ReadWritePath, false, false, false, true, false}
	};

	return Rows;
}

static const TArray<FRejectedFieldCapabilityRow>& ExpectedRejectedFieldCapabilities()
{
	static const TArray<FRejectedFieldCapabilityRow> Rows = {
		{TEXT("field.drag_get"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.drag_set"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.pin_drag_get"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.pin_drag_set"), TEXT("unsupported_ui_entry_not_statement")},
		{TEXT("field.split_struct_pin_support"), TEXT("support_only_not_user_statement")},
		{TEXT("field.recombine_struct_pin_support"), TEXT("support_only_not_user_statement")},
		{TEXT("control.function_return_write"), TEXT("other_cluster_not_field_statement")},
		{TEXT("function.selected_component_call"), TEXT("other_cluster_not_field_statement")},
		{TEXT("component.add_component_node"), TEXT("other_cluster_not_field_statement")},
		{TEXT("field.unsupported_path_diagnostic"), TEXT("diagnostic_only_not_success_capability")},
		{TEXT("field.by_ref_set"), TEXT("unsupported_by_ref_set_deferred")}
	};

	return Rows;
}

static FString NormalizeTestToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static bool ContainsCapabilityId(
	const TArray<FBlueprintHelperFieldCapabilitySpec>& Specs,
	const TCHAR* CapabilityId)
{
	const FString ExpectedId = NormalizeTestToken(CapabilityId);
	for (const FBlueprintHelperFieldCapabilitySpec& Spec : Specs)
	{
		if (NormalizeTestToken(Spec.Id) == ExpectedId)
		{
			return true;
		}
	}

	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyHasSeventeenFirstClassIdsTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.HasSeventeenFirstClassIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyHasSeventeenFirstClassIdsTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperFieldCapabilitySpec> Specs = FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs();
	TestEqual(TEXT("first-class capability count"), Specs.Num(), 17);
	TestEqual(TEXT("expected matrix count"), ExpectedFieldCapabilityMatrix().Num(), 17);

	for (const FExpectedFieldCapabilityMatrixRow& Row : ExpectedFieldCapabilityMatrix())
	{
		const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(Row.Id);
		TestNotNull(FString::Printf(TEXT("capability exists: %s"), Row.Id), Spec);
		if (Spec)
		{
			TestTrue(FString::Printf(TEXT("capability is first-class: %s"), Row.Id), Spec->bFirstClassStatement);
			TestFalse(FString::Printf(TEXT("node family set: %s"), Row.Id), Spec->ExpectedNodeFamily.IsEmpty());
			TestFalse(FString::Printf(TEXT("stable key set: %s"), Row.Id), FBlueprintHelperFieldCapabilityRegistry::MakeStableCapabilityKey(*Spec).IsEmpty());
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyMatrixMatchesEvidenceTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.MatrixMatchesEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyMatrixMatchesEvidenceTest::RunTest(const FString& Parameters)
{
	for (const FExpectedFieldCapabilityMatrixRow& Row : ExpectedFieldCapabilityMatrix())
	{
		const FBlueprintHelperFieldCapabilitySpec* Spec = FBlueprintHelperFieldCapabilityRegistry::FindById(Row.Id);
		TestNotNull(FString::Printf(TEXT("spec exists: %s"), Row.Id), Spec);
		if (Spec)
		{
			TestEqual(FString::Printf(TEXT("node family: %s"), Row.Id), Spec->ExpectedNodeFamily, FString(Row.NodeFamily));
			TestEqual(FString::Printf(TEXT("node class: %s"), Row.Id), Spec->ExpectedNodeClass, FString(Row.NodeClass));
			TestEqual(FString::Printf(TEXT("priority: %s"), Row.Id), static_cast<uint8>(Spec->Priority), static_cast<uint8>(Row.Priority));
			TestEqual(FString::Printf(TEXT("root kind: %s"), Row.Id), static_cast<uint8>(Spec->RootKind), static_cast<uint8>(Row.RootKind));
			TestEqual(FString::Printf(TEXT("access mode: %s"), Row.Id), static_cast<uint8>(Spec->AccessMode), static_cast<uint8>(Row.AccessMode));
			TestEqual(FString::Printf(TEXT("owner class requirement: %s"), Row.Id), Spec->bRequiresOwnerClass, Row.bRequiresOwnerClass);
			TestEqual(FString::Printf(TEXT("function scope requirement: %s"), Row.Id), Spec->bRequiresFunctionScope, Row.bRequiresFunctionScope);
			TestEqual(FString::Printf(TEXT("target pin requirement: %s"), Row.Id), Spec->bRequiresTargetPin, Row.bRequiresTargetPin);
			TestEqual(FString::Printf(TEXT("property path requirement: %s"), Row.Id), Spec->bRequiresPropertyPath, Row.bRequiresPropertyPath);
			TestEqual(FString::Printf(TEXT("exec pins: %s"), Row.Id), Spec->bProducesExecPins, Row.bExec);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyRejectsExcludedUserStatementsTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.RejectsExcludedUserStatements",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyRejectsExcludedUserStatementsTest::RunTest(const FString& Parameters)
{
	for (const FRejectedFieldCapabilityRow& Row : ExpectedRejectedFieldCapabilities())
	{
		FString RejectReason;
		const bool bAllowed = FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(Row.Id, RejectReason);
		TestFalse(FString::Printf(TEXT("excluded id rejected: %s"), Row.Id), bAllowed);
		TestEqual(FString::Printf(TEXT("excluded reason: %s"), Row.Id), RejectReason, FString(Row.Reason));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyKeysAreUniqueTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.KeysAreUnique",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyKeysAreUniqueTest::RunTest(const FString& Parameters)
{
	TSet<FString> SeenIds;
	TSet<FString> SeenStableKeys;

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs())
	{
		const FString NormalizedId = NormalizeTestToken(Spec.Id);
		const FString StableKey = FBlueprintHelperFieldCapabilityRegistry::MakeStableCapabilityKey(Spec);

		TestFalse(FString::Printf(TEXT("duplicate capability id: %s"), *Spec.Id), SeenIds.Contains(NormalizedId));
		TestFalse(FString::Printf(TEXT("duplicate stable key: %s"), *StableKey), SeenStableKeys.Contains(StableKey));

		SeenIds.Add(NormalizedId);
		SeenStableKeys.Add(StableKey);
	}

	TestEqual(TEXT("unique first-class ids"), SeenIds.Num(), 17);
	TestEqual(TEXT("unique stable keys"), SeenStableKeys.Num(), 17);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityTaxonomyInfersLegacyOperationAndScopeTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Taxonomy.InfersLegacyOperationAndScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityTaxonomyInfersLegacyOperationAndScopeTest::RunTest(const FString& Parameters)
{
	const TArray<FBlueprintHelperFieldCapabilitySpec> VariableGetCandidates =
		FBlueprintHelperFieldCapabilityRegistry::GetSpecsByOperationAndScope(TEXT("get"), TEXT("variable"));
	TestEqual(TEXT("variable get candidate count"), VariableGetCandidates.Num(), 5);
	TestTrue(TEXT("variable get includes member get"), ContainsCapabilityId(VariableGetCandidates, TEXT("field.member_get")));
	TestTrue(TEXT("variable get includes inherited member get"), ContainsCapabilityId(VariableGetCandidates, TEXT("field.inherited_member_get")));
	TestTrue(TEXT("variable get includes sparse data get"), ContainsCapabilityId(VariableGetCandidates, TEXT("field.sparse_data_get")));
	TestTrue(TEXT("variable get includes function param get"), ContainsCapabilityId(VariableGetCandidates, TEXT("field.function_param_get")));
	TestTrue(TEXT("variable get includes local get"), ContainsCapabilityId(VariableGetCandidates, TEXT("field.local_get")));
	TestNull(
		TEXT("ambiguous variable get is not inferred"),
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("get"), TEXT("variable")));

	const TArray<FBlueprintHelperFieldCapabilitySpec> VariableSetCandidates =
		FBlueprintHelperFieldCapabilityRegistry::GetSpecsByOperationAndScope(TEXT("set"), TEXT("variable"));
	TestEqual(TEXT("variable set candidate count"), VariableSetCandidates.Num(), 3);
	TestTrue(TEXT("variable set includes member set"), ContainsCapabilityId(VariableSetCandidates, TEXT("field.member_set")));
	TestTrue(TEXT("variable set includes inherited member set"), ContainsCapabilityId(VariableSetCandidates, TEXT("field.inherited_member_set")));
	TestTrue(TEXT("variable set includes local set"), ContainsCapabilityId(VariableSetCandidates, TEXT("field.local_set")));
	TestNull(
		TEXT("ambiguous variable set is not inferred"),
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("set"), TEXT("variable")));

	const TArray<FBlueprintHelperFieldCapabilitySpec> PropertyPathGetCandidates =
		FBlueprintHelperFieldCapabilityRegistry::GetSpecsByOperationAndScope(TEXT("get_property"), TEXT("property_path"));
	TestEqual(TEXT("property path get candidate count"), PropertyPathGetCandidates.Num(), 2);
	TestTrue(TEXT("property path get includes struct member get"), ContainsCapabilityId(PropertyPathGetCandidates, TEXT("field.struct_member_get")));
	TestTrue(TEXT("property path get includes nested property path"), ContainsCapabilityId(PropertyPathGetCandidates, TEXT("field.nested_property_path")));
	TestNull(
		TEXT("ambiguous property path get is not inferred"),
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("get_property"), TEXT("property_path")));

	const FBlueprintHelperFieldCapabilitySpec* ComponentGet =
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("get"), TEXT("component_ref"));
	TestNotNull(TEXT("component ref get inferred"), ComponentGet);
	if (ComponentGet)
	{
		TestEqual(TEXT("component ref get id"), ComponentGet->Id, FString(TEXT("field.component_ref_get")));
	}

	const FBlueprintHelperFieldCapabilitySpec* ComponentSet =
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("set"), TEXT("component_ref"));
	TestNotNull(TEXT("component ref set inferred"), ComponentSet);
	if (ComponentSet)
	{
		TestEqual(TEXT("component ref set id"), ComponentSet->Id, FString(TEXT("field.component_ref_set")));
	}

	const FBlueprintHelperFieldCapabilitySpec* ObjectPinGet =
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("get"), TEXT("field_access"));
	TestNotNull(TEXT("object pin member get inferred"), ObjectPinGet);
	if (ObjectPinGet)
	{
		TestEqual(TEXT("object pin member get id"), ObjectPinGet->Id, FString(TEXT("field.object_pin_member_get")));
	}

	const FBlueprintHelperFieldCapabilitySpec* StructSet =
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("set_property"), TEXT("property_path"));
	TestNotNull(TEXT("struct member set inferred"), StructSet);
	if (StructSet)
	{
		TestEqual(TEXT("struct member set id"), StructSet->Id, FString(TEXT("field.struct_member_set")));
	}

	TestNull(
		TEXT("unknown legacy operation/scope remains explicit"),
		FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(TEXT("unknown"), TEXT("field_access")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilitySemanticIrParsesCapabilityIdTest,
	"BlueprintHelper.GraphWrite.FieldCapability.SemanticIR.ParsesCapabilityId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilitySemanticIrParsesCapabilityIdTest::RunTest(const FString& Parameters)
{
	FBlueprintHelperGraphStatementIR Statement;
	Statement.Kind = EBlueprintHelperGraphStatementKind::Field;
	Statement.FieldOperation = TEXT("get");
	Statement.FieldScope = TEXT("component_ref");
	Statement.CapabilityId = TEXT("field.component_ref_get");
	Statement.Name = TEXT("StaticMeshComponent");
	Statement.Target = TEXT("StaticMeshComponent");
	Statement.ComponentName = TEXT("StaticMeshComponent");
	Statement.CapabilityFacts.Add(TEXT("field.component_owner_class"), TEXT("/Script/Engine.Actor"));
	Statement.CapabilityFacts.Add(TEXT("field.component_kind"), TEXT("scs_or_native_property"));

	FString Error;
	const bool bValid = FBlueprintHelperGraphSemanticIR::ValidateStatement(Statement, Error);
	TestTrue(TEXT("component ref get statement is valid"), bValid);
	TestTrue(TEXT("error is empty"), Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBlueprintHelperFieldCapabilityRegistryRejectsUiOnlyIdTest,
	"BlueprintHelper.GraphWrite.FieldCapability.Registry.RejectsUiOnlyId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintHelperFieldCapabilityRegistryRejectsUiOnlyIdTest::RunTest(const FString& Parameters)
{
	FString RejectReason;
	const bool bAllowed = FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(TEXT("field.drag_get"), RejectReason);
	TestFalse(TEXT("UI-only capability is invalid"), bAllowed);
	TestEqual(TEXT("reject reason"), RejectReason, FString(TEXT("unsupported_ui_entry_not_statement")));
	return true;
}

#endif
