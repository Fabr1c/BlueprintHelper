#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperFieldCapabilityTypes.h"

namespace
{
static FString NormalizeFieldCapabilityToken(const FString& Value)
{
	return Value.TrimStartAndEnd().ToLower();
}

static FBlueprintHelperFieldCapabilitySpec MakeSpec(
	const TCHAR* Id,
	EBlueprintHelperFieldCapabilityPriority Priority,
	EBlueprintHelperFieldCapabilityRootKind RootKind,
	EBlueprintHelperFieldCapabilityAccessMode AccessMode,
	const TCHAR* FieldOperation,
	const TCHAR* FieldScope,
	const TCHAR* ExpectedNodeFamily,
	const TCHAR* ExpectedNodeClass,
	const bool bRequiresOwnerClass,
	const bool bRequiresFunctionScope,
	const bool bRequiresTargetPin,
	const bool bRequiresPropertyPath,
	const bool bProducesExecPins)
{
	FBlueprintHelperFieldCapabilitySpec Spec;
	Spec.Id = Id;
	Spec.Priority = Priority;
	Spec.RootKind = RootKind;
	Spec.AccessMode = AccessMode;
	Spec.FieldOperation = FieldOperation;
	Spec.FieldScope = FieldScope;
	Spec.ExpectedNodeFamily = ExpectedNodeFamily;
	Spec.ExpectedNodeClass = ExpectedNodeClass;
	Spec.bFirstClassStatement = true;
	Spec.bRequiresOwnerClass = bRequiresOwnerClass;
	Spec.bRequiresFunctionScope = bRequiresFunctionScope;
	Spec.bRequiresTargetPin = bRequiresTargetPin;
	Spec.bRequiresPropertyPath = bRequiresPropertyPath;
	Spec.bProducesExecPins = bProducesExecPins;
	return Spec;
}

static FBlueprintHelperFieldCapabilitySpec MakeRejectedSpec(
	const TCHAR* Id,
	EBlueprintHelperFieldCapabilityPriority Priority,
	EBlueprintHelperFieldCapabilityAccessMode AccessMode,
	const TCHAR* FieldOperation,
	const TCHAR* FieldScope,
	const TCHAR* RejectReason)
{
	FBlueprintHelperFieldCapabilitySpec Spec;
	Spec.Id = Id;
	Spec.Priority = Priority;
	Spec.RootKind = EBlueprintHelperFieldCapabilityRootKind::Unsupported;
	Spec.AccessMode = AccessMode;
	Spec.FieldOperation = FieldOperation;
	Spec.FieldScope = FieldScope;
	Spec.RejectReason = RejectReason;
	return Spec;
}

static const TArray<FBlueprintHelperFieldCapabilitySpec>& GetAllSpecs()
{
	static const TArray<FBlueprintHelperFieldCapabilitySpec> Specs = {
		MakeSpec(TEXT("field.member_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeSpec(TEXT("field.member_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Member, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeSpec(TEXT("field.inherited_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeSpec(TEXT("field.inherited_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::InheritedMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, false, false, true),
		MakeSpec(TEXT("field.sparse_data_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::SparseData, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, false, false, false),
		MakeSpec(TEXT("field.function_param_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::FunctionParam, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeSpec(TEXT("field.local_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("variable"), TEXT("variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, true, false, false, false),
		MakeSpec(TEXT("field.local_set"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::Local, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("variable"), TEXT("variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, true, false, false, true),
		MakeSpec(TEXT("field.object_pin_member_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("field_access"), TEXT("variable_get_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, false, false),
		MakeSpec(TEXT("field.object_pin_member_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ObjectPinMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("field_access"), TEXT("variable_set_target"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, false, true),
		MakeSpec(TEXT("field.component_ref_get"), EBlueprintHelperFieldCapabilityPriority::P0, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("component_ref"), TEXT("component_variable_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), false, false, false, false, false),
		MakeSpec(TEXT("field.component_ref_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentRef, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("component_ref"), TEXT("component_variable_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), false, false, false, false, true),
		MakeSpec(TEXT("field.component_property_get"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("field_access"), TEXT("component_property_get"), TEXT("/Script/BlueprintGraph.K2Node_VariableGet"), true, false, true, true, false),
		MakeSpec(TEXT("field.component_property_set"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::ComponentProperty, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("field_access"), TEXT("component_property_set"), TEXT("/Script/BlueprintGraph.K2Node_VariableSet"), true, false, true, true, true),
		MakeSpec(TEXT("field.struct_member_get"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get_property"), TEXT("property_path"), TEXT("break_struct"), TEXT("/Script/BlueprintGraph.K2Node_BreakStruct"), false, false, false, true, false),
		MakeSpec(TEXT("field.struct_member_set"), EBlueprintHelperFieldCapabilityPriority::P1, EBlueprintHelperFieldCapabilityRootKind::StructMember, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set_property"), TEXT("property_path"), TEXT("set_fields_in_struct"), TEXT("/Script/BlueprintGraph.K2Node_SetFieldsInStruct"), false, false, false, true, true),
		MakeSpec(TEXT("field.nested_property_path"), EBlueprintHelperFieldCapabilityPriority::P2, EBlueprintHelperFieldCapabilityRootKind::NestedPropertyPath, EBlueprintHelperFieldCapabilityAccessMode::ReadWritePath, TEXT("get_property"), TEXT("property_path"), TEXT("property_path_fragment"), TEXT("BlueprintHelper.Field.PropertyPathFragment"), false, false, false, true, false),
		MakeRejectedSpec(TEXT("field.drag_get"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("ui_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedSpec(TEXT("field.drag_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("ui_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedSpec(TEXT("field.pin_drag_get"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Get, TEXT("get"), TEXT("ui_pin_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedSpec(TEXT("field.pin_drag_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("ui_pin_drag"), TEXT("unsupported_ui_entry_not_statement")),
		MakeRejectedSpec(TEXT("field.split_struct_pin_support"), EBlueprintHelperFieldCapabilityPriority::SupportOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("support"), TEXT("property_path"), TEXT("support_only_not_user_statement")),
		MakeRejectedSpec(TEXT("field.recombine_struct_pin_support"), EBlueprintHelperFieldCapabilityPriority::SupportOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("support"), TEXT("property_path"), TEXT("support_only_not_user_statement")),
		MakeRejectedSpec(TEXT("control.function_return_write"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("function_return"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedSpec(TEXT("function.selected_component_call"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("call"), TEXT("selected_component"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedSpec(TEXT("component.add_component_node"), EBlueprintHelperFieldCapabilityPriority::OtherCluster, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("spawn"), TEXT("component"), TEXT("other_cluster_not_field_statement")),
		MakeRejectedSpec(TEXT("field.unsupported_path_diagnostic"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Diagnostic, TEXT("diagnostic"), TEXT("property_path"), TEXT("diagnostic_only_not_success_capability")),
		MakeRejectedSpec(TEXT("field.by_ref_set"), EBlueprintHelperFieldCapabilityPriority::DiagnosticOnly, EBlueprintHelperFieldCapabilityAccessMode::Set, TEXT("set"), TEXT("by_ref"), TEXT("unsupported_by_ref_set_deferred"))
	};

	return Specs;
}

static const FBlueprintHelperFieldCapabilitySpec* FindKnownSpec(
	const FString& CapabilityId,
	const bool bFirstClassOnly)
{
	const FString NormalizedCapabilityId = NormalizeFieldCapabilityToken(CapabilityId);
	if (NormalizedCapabilityId.IsEmpty())
	{
		return nullptr;
	}

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllSpecs())
	{
		if (bFirstClassOnly && !Spec.bFirstClassStatement)
		{
			continue;
		}

		if (NormalizeFieldCapabilityToken(Spec.Id) == NormalizedCapabilityId)
		{
			return &Spec;
		}
	}

	return nullptr;
}
}

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::FindById(const FString& CapabilityId)
{
	return FindKnownSpec(CapabilityId, false);
}

TArray<FBlueprintHelperFieldCapabilitySpec> FBlueprintHelperFieldCapabilityRegistry::GetFirstClassSpecs()
{
	TArray<FBlueprintHelperFieldCapabilitySpec> Result;
	Result.Reserve(17);

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllSpecs())
	{
		if (Spec.bFirstClassStatement)
		{
			Result.Add(Spec);
		}
	}

	return Result;
}

bool FBlueprintHelperFieldCapabilityRegistry::IsAllowedUserStatement(
	const FString& CapabilityId,
	FString& OutRejectReason)
{
	const FBlueprintHelperFieldCapabilitySpec* Spec = FindById(CapabilityId);
	if (!Spec)
	{
		OutRejectReason = TEXT("unknown_field_capability");
		return false;
	}

	if (!Spec->bFirstClassStatement)
	{
		OutRejectReason = Spec->RejectReason.IsEmpty()
			? FString(TEXT("unsupported_field_capability"))
			: Spec->RejectReason;
		return false;
	}

	OutRejectReason.Reset();
	return true;
}

TArray<FBlueprintHelperFieldCapabilitySpec> FBlueprintHelperFieldCapabilityRegistry::GetSpecsByOperationAndScope(
	const FString& FieldOperation,
	const FString& FieldScope)
{
	const FString NormalizedOperation = NormalizeFieldCapabilityToken(FieldOperation);
	const FString NormalizedScope = NormalizeFieldCapabilityToken(FieldScope);
	TArray<FBlueprintHelperFieldCapabilitySpec> Result;

	if (NormalizedOperation.IsEmpty() || NormalizedScope.IsEmpty())
	{
		return Result;
	}

	for (const FBlueprintHelperFieldCapabilitySpec& Spec : GetAllSpecs())
	{
		if (!Spec.bFirstClassStatement)
		{
			continue;
		}

		if (NormalizeFieldCapabilityToken(Spec.FieldOperation) == NormalizedOperation &&
			NormalizeFieldCapabilityToken(Spec.FieldScope) == NormalizedScope)
		{
			Result.Add(Spec);
		}
	}

	return Result;
}

const FBlueprintHelperFieldCapabilitySpec* FBlueprintHelperFieldCapabilityRegistry::InferFromOperationAndScope(
	const FString& FieldOperation,
	const FString& FieldScope)
{
	const TArray<FBlueprintHelperFieldCapabilitySpec> Candidates =
		GetSpecsByOperationAndScope(FieldOperation, FieldScope);
	if (Candidates.Num() == 1)
	{
		return FindKnownSpec(Candidates[0].Id, true);
	}

	return nullptr;
}

FString FBlueprintHelperFieldCapabilityRegistry::MakeStableCapabilityKey(
	const FBlueprintHelperFieldCapabilitySpec& Spec)
{
	return FString::Printf(
		TEXT("field-capability:%s:%s:%s"),
		*NormalizeFieldCapabilityToken(Spec.Id),
		*NormalizeFieldCapabilityToken(Spec.FieldOperation),
		*NormalizeFieldCapabilityToken(Spec.FieldScope));
}
