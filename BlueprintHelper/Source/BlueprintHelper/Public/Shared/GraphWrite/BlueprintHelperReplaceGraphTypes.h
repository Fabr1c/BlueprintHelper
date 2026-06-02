// BlueprintHelper Service Layer - ReplaceBlueprintGraph shared types

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Shared/GraphWrite/BlueprintHelperAppendGraphTypes.h"

using FBlueprintHelperGraphWriteIssue = FBlueprintHelperDryRunIssue;

enum class EBlueprintHelperReplaceScope : uint8
{
	Graph,
	BlockImplementation,
	FunctionBody,
	EventBody,
	CustomEventBody,
	FunctionDefinition,
	EventDefinition
};

inline const TCHAR* ReplaceScopeToString(EBlueprintHelperReplaceScope Scope)
{
	switch (Scope)
	{
	case EBlueprintHelperReplaceScope::Graph: return TEXT("graph");
	case EBlueprintHelperReplaceScope::BlockImplementation: return TEXT("block_implementation");
	case EBlueprintHelperReplaceScope::FunctionBody: return TEXT("function_body");
	case EBlueprintHelperReplaceScope::EventBody: return TEXT("event_body");
	case EBlueprintHelperReplaceScope::CustomEventBody: return TEXT("custom_event_body");
	case EBlueprintHelperReplaceScope::FunctionDefinition: return TEXT("function_definition");
	case EBlueprintHelperReplaceScope::EventDefinition: return TEXT("event_definition");
	default: return TEXT("unknown");
	}
}

inline bool ParseReplaceScope(const FString& Str, EBlueprintHelperReplaceScope& Out)
{
	if (Str.Equals(TEXT("graph"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::Graph; return true; }
	if (Str.Equals(TEXT("block_implementation"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::BlockImplementation; return true; }
	if (Str.Equals(TEXT("function_body"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::FunctionBody; return true; }
	if (Str.Equals(TEXT("event_body"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::EventBody; return true; }
	if (Str.Equals(TEXT("custom_event_body"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::CustomEventBody; return true; }
	if (Str.Equals(TEXT("function_definition"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::FunctionDefinition; return true; }
	if (Str.Equals(TEXT("event_definition"), ESearchCase::IgnoreCase)) { Out = EBlueprintHelperReplaceScope::EventDefinition; return true; }
	return false;
}

struct FBlueprintHelperReplacedRef
{
	FString GraphId;
	FString TargetRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!GraphId.IsEmpty())
		{
			Json->SetStringField(TEXT("graph_id"), GraphId);
		}
		if (!TargetRef.IsEmpty())
		{
			Json->SetStringField(TEXT("target_ref"), TargetRef);
		}
		return Json;
	}
};

struct FBlueprintHelperReplaceGraphResult
{
	FBlueprintHelperReplacedRef ReplacedRef;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetObjectField(TEXT("replaced_ref"), ReplacedRef.ToJson());
		return Json;
	}
};

struct FBlueprintHelperReplaceGraphResultData
{
	FString Schema = TEXT("ReplaceBlueprintGraph.v1");
	FBlueprintHelperReplaceGraphResult ReplaceResult;
	FBlueprintHelperWriteRef WriteRef;
	TArray<FString> BlockRefs;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("replace_result"), ReplaceResult.ToJson());
		Json->SetObjectField(TEXT("write_ref"), WriteRef.ToJson());
		if (BlockRefs.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Ref : BlockRefs) { Arr.Add(MakeShared<FJsonValueString>(Ref)); }
			Json->SetArrayField(TEXT("block_refs"), Arr);
		}
		return Json;
	}
};

struct FBlueprintHelperReplaceDryRunResult
{
	FString Result = TEXT("passed");
	bool bCanExecute = true;
	TArray<FString> BlockedBy;
	TArray<FBlueprintHelperGraphWriteIssue> Conflicts;
	TArray<FBlueprintHelperGraphWriteIssue> Errors;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("result"), Result);
		Json->SetBoolField(TEXT("can_execute"), bCanExecute);
		if (BlockedBy.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FString& Item : BlockedBy)
			{
				Arr.Add(MakeShared<FJsonValueString>(Item));
			}
			Json->SetArrayField(TEXT("blocked_by"), Arr);
		}
		if (Conflicts.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FBlueprintHelperGraphWriteIssue& Issue : Conflicts)
			{
				Arr.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
			}
			Json->SetArrayField(TEXT("conflicts"), Arr);
		}
		if (Errors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> Arr;
			for (const FBlueprintHelperGraphWriteIssue& Issue : Errors)
			{
				Arr.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
			}
			Json->SetArrayField(TEXT("errors"), Arr);
		}
		return Json;
	}
};

struct FBlueprintHelperReplaceDryRunData
{
	FString Schema = TEXT("ReplaceBlueprintGraphDryRun.v1");
	FBlueprintHelperReplaceDryRunResult DryRun;

	TSharedRef<FJsonObject> ToJson() const
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("schema"), Schema);
		Json->SetObjectField(TEXT("dry_run"), DryRun.ToJson());
		return Json;
	}
};
