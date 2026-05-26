#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperArrayTypedPinEvidenceGuard.h"

#include "Systems/ToolClusters/GraphWrite/GraphStatement/Utils/BlueprintHelperGraphStatementPinTypeParser.h"

namespace
{
static FString EvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key)
{
	if (const FString* Value = Evidence.Find(Key))
	{
		return Value->TrimStartAndEnd();
	}
	return FString();
}

static FString NormalizeToken(const FString& Token)
{
	return Token.TrimStartAndEnd().ToLower();
}

static bool ElementCategoryRequiresObjectPath(const FString& Category)
{
	return Category == TEXT("struct")
		|| Category == TEXT("object")
		|| Category == TEXT("class")
		|| Category == TEXT("interface")
		|| Category == TEXT("enum")
		|| Category == TEXT("softobject")
		|| Category == TEXT("softclass");
}

static FString BuildArrayElementIdentity(FBlueprintHelperCallFunctionPinType& PinType)
{
	const FString Category = NormalizeToken(PinType.Category);
	const FString Container = NormalizeToken(PinType.ContainerType);
	if (Category == TEXT("array") || Category == TEXT("tarray"))
	{
		PinType.ContainerType = TEXT("array");
		PinType.Category = PinType.SubCategory;
		PinType.SubCategory.Reset();
	}
	else if (Container == TEXT("array") || Container == TEXT("tarray"))
	{
		PinType.ContainerType = TEXT("array");
	}
	else
	{
		return FString();
	}

	const FString ElementCategory = NormalizeToken(PinType.Category);
	const FString ElementSubCategory = NormalizeToken(PinType.SubCategory);
	const FString ElementObjectPath = NormalizeToken(PinType.ObjectPath);
	if (ElementCategory.IsEmpty()
		|| ElementCategory == TEXT("wildcard")
		|| ElementSubCategory == TEXT("wildcard")
		|| ElementObjectPath == TEXT("wildcard"))
	{
		return FString();
	}
	if (ElementCategoryRequiresObjectPath(ElementCategory) && ElementObjectPath.IsEmpty())
	{
		return FString();
	}

	FString Identity = FString::Printf(TEXT("category=%s"), *ElementCategory);
	if (!ElementSubCategory.IsEmpty())
	{
		Identity += FString::Printf(TEXT("|subcategory=%s"), *ElementSubCategory);
	}
	if (!ElementObjectPath.IsEmpty())
	{
		Identity += FString::Printf(TEXT("|object=%s"), *ElementObjectPath);
	}
	return Identity;
}

static FBlueprintHelperArrayTypedPinEvidenceGuardResult Fail(const FString& ErrorCode, const FString& Message)
{
	FBlueprintHelperArrayTypedPinEvidenceGuardResult Result;
	Result.ErrorCode = ErrorCode;
	Result.Message = Message;
	return Result;
}
}

FBlueprintHelperArrayTypedPinEvidenceGuardResult FBlueprintHelperArrayTypedPinEvidenceGuard::ValidateArrayIdenticalEvidence(
	const TMap<FString, FString>& Evidence)
{
	const FString LhsEvidence = EvidenceValue(Evidence, TEXT("op.array_lhs_pin_type"));
	const FString RhsEvidence = EvidenceValue(Evidence, TEXT("op.array_rhs_pin_type"));
	if (LhsEvidence.IsEmpty() || RhsEvidence.IsEmpty())
	{
		return Fail(TEXT("array_typed_pin_missing"), TEXT("array_identical requires op.array_lhs_pin_type and op.array_rhs_pin_type."));
	}

	FBlueprintHelperArrayTypedPinEvidenceGuardResult Result;
	Result.LhsPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(LhsEvidence);
	Result.RhsPinType = FBlueprintHelperGraphStatementPinTypeParser::ParsePinTypeToken(RhsEvidence);

	const FString LhsElement = BuildArrayElementIdentity(Result.LhsPinType);
	const FString RhsElement = BuildArrayElementIdentity(Result.RhsPinType);
	if (LhsElement.IsEmpty() || RhsElement.IsEmpty())
	{
		return Fail(TEXT("array_typed_pin_missing"), TEXT("array_identical requires non-wildcard array element pin evidence on both sides."));
	}
	if (!LhsElement.Equals(RhsElement, ESearchCase::IgnoreCase))
	{
		return Fail(TEXT("array_typed_pin_mismatch"), FString::Printf(
			TEXT("array_identical requires matching array element evidence, got lhs=%s rhs=%s."),
			*LhsElement,
			*RhsElement));
	}

	Result.bPassed = true;
	return Result;
}
