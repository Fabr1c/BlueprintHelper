#include "Entry/Bridge/BlueprintHelperRequestValidationRegistry.h"

static FBlueprintHelperFieldRule BlueprintHelperRequestValidationField(
	const TCHAR* FieldName,
	EBlueprintHelperRequestFieldType Type)
{
	FBlueprintHelperFieldRule Rule;
	Rule.FieldName = FieldName;
	Rule.Type = Type;
	return Rule;
}

static FBlueprintHelperRequestValidationDescriptor BlueprintHelperRequestValidationDescriptor(
	const TCHAR* Command,
	TArray<FBlueprintHelperFieldRule> RequiredFields,
	TArray<FBlueprintHelperFieldRule> OptionalFields)
{
	FBlueprintHelperRequestValidationDescriptor Descriptor;
	Descriptor.Command = Command;
	Descriptor.RequiredFields = MoveTemp(RequiredFields);
	Descriptor.OptionalFields = MoveTemp(OptionalFields);
	return Descriptor;
}

TArray<FBlueprintHelperRequestValidationDescriptor>
FBlueprintHelperRequestValidationRegistry::GetRepresentativeDescriptors()
{
	return {
		BlueprintHelperRequestValidationDescriptor(
			TEXT("preview_task_plan"),
			{BlueprintHelperRequestValidationField(TEXT("task_plan"), EBlueprintHelperRequestFieldType::Object)},
			{}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("execute_task_plan"),
			{},
			{
				BlueprintHelperRequestValidationField(TEXT("task_plan"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("preview_token"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("task_spec_hash"), EBlueprintHelperRequestFieldType::String),
			}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("append_blueprint_graph"),
			{
				BlueprintHelperRequestValidationField(TEXT("target"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("nodes"), EBlueprintHelperRequestFieldType::Array),
			},
			{
				BlueprintHelperRequestValidationField(TEXT("feature_name"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("links"), EBlueprintHelperRequestFieldType::Array),
				BlueprintHelperRequestValidationField(TEXT("dry_run"), EBlueprintHelperRequestFieldType::Bool),
			}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("merge_external_flow"),
			{
				BlueprintHelperRequestValidationField(TEXT("target"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("insert_strategy"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("anchor"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("inserted"), EBlueprintHelperRequestFieldType::Object),
			},
			{
				BlueprintHelperRequestValidationField(TEXT("sequence_order"), EBlueprintHelperRequestFieldType::Array),
				BlueprintHelperRequestValidationField(TEXT("dry_run"), EBlueprintHelperRequestFieldType::Bool),
			}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("patch_external_graph"),
			{
				BlueprintHelperRequestValidationField(TEXT("target"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("patch_type"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("anchor"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("expected_old_state"), EBlueprintHelperRequestFieldType::Object),
			},
			{BlueprintHelperRequestValidationField(TEXT("dry_run"), EBlueprintHelperRequestFieldType::Bool)}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("replace_external_body"),
			{
				BlueprintHelperRequestValidationField(TEXT("target"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("scope"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("anchor"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("body"), EBlueprintHelperRequestFieldType::Object),
				BlueprintHelperRequestValidationField(TEXT("expected_body_fingerprint"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("require_full_dry_run"), EBlueprintHelperRequestFieldType::Bool),
			},
			{BlueprintHelperRequestValidationField(TEXT("dry_run"), EBlueprintHelperRequestFieldType::Bool)}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("read_blueprint_logic_json"),
			{BlueprintHelperRequestValidationField(TEXT("asset_path"), EBlueprintHelperRequestFieldType::String)},
			{BlueprintHelperRequestValidationField(TEXT("graph"), EBlueprintHelperRequestFieldType::String)}),
		BlueprintHelperRequestValidationDescriptor(
			TEXT("apply_review_action"),
			{
				BlueprintHelperRequestValidationField(TEXT("review_record_id"), EBlueprintHelperRequestFieldType::String),
				BlueprintHelperRequestValidationField(TEXT("action"), EBlueprintHelperRequestFieldType::String),
			},
			{BlueprintHelperRequestValidationField(TEXT("target_keys"), EBlueprintHelperRequestFieldType::Array)}),
	};
}

bool FBlueprintHelperRequestValidationRegistry::TryFindDescriptor(
	const FString& Command,
	FBlueprintHelperRequestValidationDescriptor& OutDescriptor)
{
	for (FBlueprintHelperRequestValidationDescriptor Descriptor : GetRepresentativeDescriptors())
	{
		if (Descriptor.Command == Command)
		{
			OutDescriptor = MoveTemp(Descriptor);
			return true;
		}
	}

	OutDescriptor = FBlueprintHelperRequestValidationDescriptor();
	return false;
}
