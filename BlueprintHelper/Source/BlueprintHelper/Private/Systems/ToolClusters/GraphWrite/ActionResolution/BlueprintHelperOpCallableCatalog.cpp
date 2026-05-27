#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperOpCallableCatalog.h"

namespace
{
static FString StableCallableId(const TCHAR* OwnerClassPath, const TCHAR* FunctionName)
{
	return FString::Printf(TEXT("%s:%s"), OwnerClassPath, FunctionName);
}

static FBlueprintHelperOpCallableSpec MakeSpec(
	const TCHAR* OperationId,
	const TCHAR* SpawnFamily,
	const TCHAR* OwnerClassPath,
	const TCHAR* FunctionName,
	const TCHAR* RequiredNodeClassPath = TEXT(""))
{
	FBlueprintHelperOpCallableSpec Spec;
	Spec.OperationId = OperationId;
	Spec.SpawnFamily = SpawnFamily;
	Spec.StableCallableId = StableCallableId(OwnerClassPath, FunctionName);
	Spec.RequiredNodeClassPath = RequiredNodeClassPath;
	return Spec;
}

static const TCHAR* CommutativeOperatorNodeClassPath()
{
	return TEXT("/Script/BlueprintGraph.K2Node_CommutativeAssociativeBinaryOperator");
}

static FBlueprintHelperOpCallableSpec MakeArrayIdenticalSpec()
{
	FBlueprintHelperOpCallableSpec Spec = MakeSpec(
		TEXT("array_identical"),
		TEXT("special_node"),
		TEXT("/Script/Engine.KismetArrayLibrary"),
		TEXT("Array_Identical"),
		TEXT("/Script/BlueprintGraph.K2Node_CallArrayFunction"));
	Spec.RequiredEvidenceKeys = { TEXT("op.array_lhs_pin_type"), TEXT("op.array_rhs_pin_type") };
	return Spec;
}

static FBlueprintHelperOpCallableSpec MakeRejectedSpec(const TCHAR* OperationId)
{
	FBlueprintHelperOpCallableSpec Spec;
	Spec.OperationId = OperationId;
	Spec.RejectionCode = TEXT("excluded_op_operation");
	return Spec;
}

static const TArray<FBlueprintHelperOpCallableSpec>& SupportedSpecs()
{
	static const TArray<FBlueprintHelperOpCallableSpec> Specs = {
		MakeSpec(TEXT("bitwise_and"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("And_IntInt"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("bitwise_or"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Or_IntInt"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("boolean_and"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanAND"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("boolean_or"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanOR"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("boolean_nand"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanNAND"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("max"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("FMax"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("min"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("FMin"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("string_append"), TEXT("commutative_function"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("Concat_StrStr"), CommutativeOperatorNodeClassPath()),
		MakeSpec(TEXT("boolean_not"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Not_PreBool")),
		MakeSpec(TEXT("boolean_xor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanXOR")),
		MakeSpec(TEXT("boolean_nor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("BooleanNOR")),
		MakeSpec(TEXT("bitwise_not"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Not_Int")),
		MakeSpec(TEXT("bitwise_xor"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Xor_IntInt")),
		MakeSpec(TEXT("abs"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Abs")),
		MakeSpec(TEXT("modulo"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Percent_FloatFloat")),
		MakeSpec(TEXT("negate"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NegateVector")),
		MakeSpec(TEXT("dot"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("DotProduct2D")),
		MakeSpec(TEXT("dot3"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Dot_VectorVector")),
		MakeSpec(TEXT("cross"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("CrossProduct2D")),
		MakeSpec(TEXT("cross3"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Cross_VectorVector")),
		MakeSpec(TEXT("near_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NearlyEqual_FloatFloat")),
		MakeSpec(TEXT("intpoint_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Equal_IntPointIntPoint")),
		MakeSpec(TEXT("transform_compose"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("ComposeTransforms")),
		MakeSpec(TEXT("equal_exact"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("EqualExactly_VectorVector")),
		MakeSpec(TEXT("not_equal_exact"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NotEqualExactly_VectorVector")),
		MakeSpec(TEXT("equal_ignore_case"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("EqualEqual_StriStri")),
		MakeSpec(TEXT("not_equal_ignore_case"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetStringLibrary"), TEXT("NotEqual_StriStri")),
		MakeSpec(TEXT("datetime_add_datetime"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Add_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_add_timespan"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Add_DateTimeTimespan")),
		MakeSpec(TEXT("datetime_subtract_datetime"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Subtract_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_subtract_timespan"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Subtract_DateTimeTimespan")),
		MakeSpec(TEXT("datetime_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("EqualEqual_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_not_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("NotEqual_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_greater"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Greater_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_greater_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("GreaterEqual_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_less"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("Less_DateTimeDateTime")),
		MakeSpec(TEXT("datetime_less_equal"), TEXT("call_function_compact"), TEXT("/Script/Engine.KismetMathLibrary"), TEXT("LessEqual_DateTimeDateTime")),
		MakeArrayIdenticalSpec()
	};
	return Specs;
}

static const TArray<FBlueprintHelperOpCallableSpec>& ExcludedSpecs()
{
	static const TArray<FBlueprintHelperOpCallableSpec> Specs = {
		MakeRejectedSpec(TEXT("enum_equal")),
		MakeRejectedSpec(TEXT("enum_not_equal")),
		MakeRejectedSpec(TEXT("slate_brush_equal")),
		MakeRejectedSpec(TEXT("slate_brush_not_equal")),
		MakeRejectedSpec(TEXT("convert_numeric")),
		MakeRejectedSpec(TEXT("convert_string_text_name")),
		MakeRejectedSpec(TEXT("array_map_set_mutation")),
		MakeRejectedSpec(TEXT("validity_predicate"))
	};
	return Specs;
}
}

const TArray<FBlueprintHelperOpCallableSpec>& FBlueprintHelperOpCallableCatalog::GetSupportedCallableSpecs()
{
	return SupportedSpecs();
}

const TArray<FBlueprintHelperOpCallableSpec>& FBlueprintHelperOpCallableCatalog::GetExcludedSpecs()
{
	return ExcludedSpecs();
}

const TArray<FString>& FBlueprintHelperOpCallableCatalog::GetTypePromotionOperationIds()
{
	static const TArray<FString> OperationIds = {
		TEXT("add"),
		TEXT("subtract"),
		TEXT("multiply"),
		TEXT("divide"),
		TEXT("greater"),
		TEXT("greater_equal"),
		TEXT("less"),
		TEXT("less_equal"),
		TEXT("equal"),
		TEXT("not_equal")
	};
	return OperationIds;
}

const FBlueprintHelperOpCallableSpec* FBlueprintHelperOpCallableCatalog::FindSupportedSpec(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return SupportedSpecs().FindByPredicate(
		[&Normalized](const FBlueprintHelperOpCallableSpec& Spec)
		{
			return Spec.OperationId.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

const FBlueprintHelperOpCallableSpec* FBlueprintHelperOpCallableCatalog::FindExcludedSpec(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return ExcludedSpecs().FindByPredicate(
		[&Normalized](const FBlueprintHelperOpCallableSpec& Spec)
		{
			return Spec.OperationId.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

bool FBlueprintHelperOpCallableCatalog::IsTypePromotionOperation(const FString& OperationId)
{
	const FString Normalized = NormalizeOperationId(OperationId);
	return GetTypePromotionOperationIds().ContainsByPredicate(
		[&Normalized](const FString& Candidate)
		{
			return Candidate.Equals(Normalized, ESearchCase::IgnoreCase);
		});
}

FString FBlueprintHelperOpCallableCatalog::NormalizeOperationId(const FString& OperationId)
{
	FString Normalized = OperationId.TrimStartAndEnd().ToLower();
	if (Normalized.StartsWith(TEXT("op.")))
	{
		Normalized.RightChopInline(3);
	}
	return Normalized;
}
