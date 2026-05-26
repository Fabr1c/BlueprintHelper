#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperStructFieldFragmentBuilder.h"

#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperGenericOpsEvidence.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperFieldFragmentBuilder.h"

namespace
{
static FString NormalizeOperation(const FString& Operation)
{
	return Operation.TrimStartAndEnd().ToLower();
}
}

bool FBlueprintHelperStructFieldFragmentBuilder::SupportsOperation(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	return Normalized == TEXT("break_struct")
		|| Normalized == TEXT("set_fields_in_struct");
}

TArray<FString> FBlueprintHelperStructFieldFragmentBuilder::RequiredEvidenceKeys(const FString& Operation)
{
	const FString Normalized = NormalizeOperation(Operation);
	if (Normalized == TEXT("set_fields_in_struct"))
	{
		return {
			TEXT("generic.struct.struct_path"),
			TEXT("generic.struct.selected_field_paths")
		};
	}
	if (Normalized == TEXT("break_struct"))
	{
		return { TEXT("generic.struct.struct_path") };
	}
	return {};
}

bool FBlueprintHelperStructFieldFragmentBuilder::BuildSetFieldsInStructFragment(
	UEdGraph* TargetGraph,
	const FBlueprintHelperActionResolutionRequest& Request,
	FBlueprintHelperNodeFragment& OutFragment,
	FString& OutError)
{
	OutFragment = FBlueprintHelperNodeFragment();

	FBlueprintHelperGenericOpsStructFieldPolicyEvidence Evidence;
	FString ErrorCode;
	FString Message;
	if (!FBlueprintHelperStructFieldPolicyEvidenceReader::Read(Request, Evidence, ErrorCode, Message))
	{
		OutError = ErrorCode.IsEmpty() ? Message : ErrorCode;
		if (!Message.IsEmpty())
		{
			OutError += FString::Printf(TEXT(": %s"), *Message);
		}
		return false;
	}
	const FString RequestedOperation = NormalizeOperation(
		Request.ContextEvidence.FindRef(TEXT("generic.struct.operation")).IsEmpty()
			? Request.Semantic.Query
			: Request.ContextEvidence.FindRef(TEXT("generic.struct.operation")));
	if (!RequestedOperation.Equals(TEXT("set_fields_in_struct"), ESearchCase::IgnoreCase))
	{
		OutError = FString::Printf(TEXT("unsupported_struct_field_operation: %s"), *RequestedOperation);
		return false;
	}
	if (Evidence.SelectedFieldPaths.Num() == 0)
	{
		OutError = TEXT("missing_evidence.generic.struct.selected_field_paths");
		return false;
	}

	FBlueprintHelperFieldFragmentPlan FieldPlan;
	FieldPlan.CapabilityId = TEXT("field.struct_member_set");
	FieldPlan.FieldName = Evidence.SelectedFieldPaths[0];
	FieldPlan.CapabilityFacts.Add(TEXT("field.struct_type"), Evidence.StructPath);
	FieldPlan.CapabilityFacts.Add(TEXT("field.property_path"), Evidence.SelectedFieldPaths[0]);
	FieldPlan.CapabilityFacts.Add(TEXT("generic.struct.selected_field_paths"), FString::Join(Evidence.SelectedFieldPaths, TEXT(",")));

	const bool bBuilt = FBlueprintHelperFieldFragmentBuilder::BuildStructWriteFragment(
		TargetGraph,
		FieldPlan,
		OutFragment,
		OutError);
	if (bBuilt)
	{
		OutFragment.OwnershipTags.Add(TEXT("generic.struct.operation"), TEXT("set_fields_in_struct"));
		OutFragment.OwnershipTags.Add(TEXT("generic.struct.struct_path"), Evidence.StructPath);
		OutFragment.OwnershipTags.Add(TEXT("generic.struct.selected_field_paths"), FString::Join(Evidence.SelectedFieldPaths, TEXT(",")));
	}
	return bBuilt;
}
