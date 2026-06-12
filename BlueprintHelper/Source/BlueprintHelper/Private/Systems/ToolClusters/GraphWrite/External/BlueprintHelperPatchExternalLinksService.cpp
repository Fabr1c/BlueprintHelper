#include "Systems/ToolClusters/GraphWrite/External/BlueprintHelperPatchExternalLinksService.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationCoordinator.h"
#include "Systems/ToolClusters/GraphWrite/Mutation/BlueprintHelperGraphWriteMutationIntent.h"
#include "Systems/ToolClusters/GraphWrite/UnitOfWork/BlueprintHelperGraphWriteUnitOfWork.h"

namespace BlueprintHelperPatchExternalLinks
{
	static constexpr const TCHAR* OperationName = TEXT("patch_external_links");

	static FBlueprintHelperDryRunIssue MakeIssue(
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		FBlueprintHelperDryRunIssue Issue;
		Issue.Code = Code;
		Issue.Message = Message;
		Issue.Target = Target;
		Issue.Source = Source;
		return Issue;
	}

	static void AddError(
		FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Errors.Add(MakeIssue(Code, Message, Target, Source));
	}

	static void AddConflict(
		FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& Target,
		const FString& Source)
	{
		Result.bPassed = false;
		Result.BlockedBy.AddUnique(Code);
		Result.Conflicts.Add(MakeIssue(Code, Message, Target, Source));
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FString& Value : Values)
		{
			Array.Add(MakeShared<FJsonValueString>(Value));
		}
		return Array;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeIssueArray(const TArray<FBlueprintHelperDryRunIssue>& Issues)
	{
		TArray<TSharedPtr<FJsonValue>> Array;
		for (const FBlueprintHelperDryRunIssue& Issue : Issues)
		{
			Array.Add(MakeShared<FJsonValueObject>(Issue.ToJson()));
		}
		return Array;
	}

	static FBlueprintHelperToolError MakeToolError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field,
		EBlueprintHelperRollbackResult RollbackResult = EBlueprintHelperRollbackResult::NotNeeded)
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.Field = Field;
		Error.bRetryable = false;
		Error.RollbackResult = RollbackResult;
		return Error;
	}

	static FBlueprintHelperToolError MakeErrorFromPreflight(
		const FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksPreflightResult& Preflight)
	{
		const FBlueprintHelperDryRunIssue* FirstIssue = Preflight.Conflicts.Num() > 0
			? &Preflight.Conflicts[0]
			: (Preflight.Errors.Num() > 0 ? &Preflight.Errors[0] : nullptr);
		return MakeToolError(
			Preflight.BlockedBy.Num() > 0 ? Preflight.BlockedBy[0] : TEXT("preflight_failed"),
			EBlueprintHelperToolStage::Preflight,
			FirstIssue && !FirstIssue->Message.IsEmpty()
				? FirstIssue->Message
				: TEXT("patch_external_links preflight blocked execution."),
			FirstIssue && !FirstIssue->Source.IsEmpty()
				? FirstIssue->Source
				: TEXT("payload"));
	}

	static UBlueprint* FindBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Blueprint;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static void AddGraphIfValid(TArray<UEdGraph*>& Graphs, UEdGraph* Graph)
	{
		if (Graph)
		{
			Graphs.Add(Graph);
		}
	}

	static UEdGraph* FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint || GraphName.IsEmpty())
		{
			return nullptr;
		}

		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}
		for (UEdGraph* Graph : Blueprint->DelegateSignatureGraphs)
		{
			AddGraphIfValid(Graphs, Graph);
		}

		for (UEdGraph* Graph : Graphs)
		{
			if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static TSharedRef<FJsonObject> MakeAnchorSummary(const FBlueprintHelperExternalCompactAnchor& Anchor)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("anchor_type"), Anchor.AnchorType);
		Json->SetStringField(TEXT("anchor_ref"), Anchor.AnchorRef);
		return Json;
	}

	static TSharedRef<FJsonObject> MakeDryRunData(
		const FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksPreflightResult& Preflight,
		const FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksRequest& Request,
		const FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksContext& Context)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("PatchExternalLinksDryRun.v1"));

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), Preflight.bPassed ? TEXT("passed") : TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), Preflight.bPassed);
		DryRun->SetArrayField(TEXT("blocked_by"), MakeStringArray(Preflight.BlockedBy));
		DryRun->SetArrayField(TEXT("conflicts"), MakeIssueArray(Preflight.Conflicts));
		DryRun->SetArrayField(TEXT("errors"), MakeIssueArray(Preflight.Errors));
		Data->SetObjectField(TEXT("dry_run"), DryRun);

		TSharedRef<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("patch_type"), Request.PatchType);
		if (Request.SourceAnchor.Type == EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			Patch->SetObjectField(TEXT("source_anchor"), MakeAnchorSummary(Request.SourceAnchor));
		}
		if (Request.TargetAnchor.Type == EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			Patch->SetObjectField(TEXT("target_anchor"), MakeAnchorSummary(Request.TargetAnchor));
		}
		if (Request.LinkAnchor.Type == EBlueprintHelperExternalCompactAnchorType::Link)
		{
			Patch->SetObjectField(TEXT("link_anchor"), MakeAnchorSummary(Request.LinkAnchor));
		}
		if (Request.ReplacementAnchor.Type == EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			Patch->SetObjectField(TEXT("replacement_anchor"), MakeAnchorSummary(Request.ReplacementAnchor));
		}
		Patch->SetStringField(TEXT("link_kind"), Context.LinkKind);
		Data->SetObjectField(TEXT("external_link_patch"), Patch);
		return Data;
	}

	static bool IsOutputPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->Direction == EGPD_Output;
	}

	static bool IsInputPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->Direction == EGPD_Input;
	}

	static bool CanConnectPins(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, FString& OutError)
	{
		if (!SourcePin || !TargetPin)
		{
			OutError = TEXT("external_pin_not_found");
			return false;
		}
		if (!IsOutputPin(SourcePin) || !IsInputPin(TargetPin))
		{
			OutError = TEXT("external_pin_direction_invalid");
			return false;
		}
		if (SourcePin->LinkedTo.Contains(TargetPin))
		{
			return true;
		}

		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			OutError = FString::Printf(TEXT("external_pin_incompatible: %s"), *Response.Message.ToString());
			return false;
		}
		if (SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && SourcePin->LinkedTo.Num() > 0)
		{
			OutError = TEXT("external_exec_link_requires_insert_between");
			return false;
		}
		return true;
	}
}

FBlueprintHelperPatchExternalLinksService::FPatchExternalLinksRequest
FBlueprintHelperPatchExternalLinksService::ParseRequest(const TSharedRef<FJsonObject>& Payload) const
{
	FPatchExternalLinksRequest Request;

	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		(*TargetObject)->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		(*TargetObject)->TryGetStringField(TEXT("graph"), Request.GraphName);
		if (Request.GraphName.IsEmpty())
		{
			(*TargetObject)->TryGetStringField(TEXT("graph_name"), Request.GraphName);
		}
	}

	Payload->TryGetStringField(TEXT("patch_type"), Request.PatchType);
	Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

	const TSharedPtr<FJsonObject>* AnchorObject = nullptr;
	if (Payload->TryGetObjectField(TEXT("source_anchor"), AnchorObject) && AnchorObject && AnchorObject->IsValid())
	{
		FBlueprintHelperExternalCompactAnchor::FromJson(*AnchorObject, Request.SourceAnchor, Request.SourceAnchorParseError);
	}
	if (Payload->TryGetObjectField(TEXT("target_anchor"), AnchorObject) && AnchorObject && AnchorObject->IsValid())
	{
		FBlueprintHelperExternalCompactAnchor::FromJson(*AnchorObject, Request.TargetAnchor, Request.TargetAnchorParseError);
	}
	if (Payload->TryGetObjectField(TEXT("link_anchor"), AnchorObject) && AnchorObject && AnchorObject->IsValid())
	{
		FBlueprintHelperExternalCompactAnchor::FromJson(*AnchorObject, Request.LinkAnchor, Request.LinkAnchorParseError);
	}
	if (Payload->TryGetObjectField(TEXT("replacement_anchor"), AnchorObject) && AnchorObject && AnchorObject->IsValid())
	{
		FBlueprintHelperExternalCompactAnchor::FromJson(*AnchorObject, Request.ReplacementAnchor, Request.ReplacementAnchorParseError);
	}

	return Request;
}

bool FBlueprintHelperPatchExternalLinksService::Preflight(
	const FPatchExternalLinksRequest& Request,
	FPatchExternalLinksContext& Context,
	FPatchExternalLinksPreflightResult& OutResult) const
{
	if (Request.AssetPath.IsEmpty())
	{
		BlueprintHelperPatchExternalLinks::AddError(OutResult, TEXT("target_blueprint_not_found"), TEXT("patch_external_links requires target.asset_path."), TEXT("target.asset_path"), TEXT("payload.target.asset_path"));
		return false;
	}
	if (Request.GraphName.IsEmpty())
	{
		BlueprintHelperPatchExternalLinks::AddError(OutResult, TEXT("target_graph_not_found"), TEXT("patch_external_links requires target.graph."), TEXT("target.graph"), TEXT("payload.target.graph"));
		return false;
	}
	if (Request.PatchType != TEXT("connect_pins") &&
		Request.PatchType != TEXT("disconnect_link") &&
		Request.PatchType != TEXT("replace_link"))
	{
		BlueprintHelperPatchExternalLinks::AddError(OutResult, TEXT("unsupported_external_link_patch_type"), TEXT("patch_external_links supports connect_pins, disconnect_link, and replace_link."), TEXT("patch_type"), TEXT("payload.patch_type"));
		return false;
	}

	Context.Blueprint = BlueprintHelperPatchExternalLinks::FindBlueprint(Request.AssetPath);
	Context.Graph = BlueprintHelperPatchExternalLinks::FindGraphByName(Context.Blueprint, Request.GraphName);
	if (!Context.Blueprint || !Context.Graph)
	{
		BlueprintHelperPatchExternalLinks::AddError(OutResult, Context.Blueprint ? TEXT("target_graph_not_found") : TEXT("target_blueprint_not_found"), Context.Blueprint ? TEXT("Target graph was not found.") : TEXT("Target blueprint was not found."), Context.Blueprint ? TEXT("target.graph") : TEXT("target.asset_path"), Context.Blueprint ? TEXT("payload.target.graph") : TEXT("payload.target.asset_path"));
		return false;
	}

	const FBlueprintHelperExternalGraphAnchorResolver Resolver;
	FString ResolveError;
	if (Request.PatchType == TEXT("connect_pins"))
	{
		if (!Request.SourceAnchorParseError.IsEmpty() || Request.SourceAnchor.Type != EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			BlueprintHelperPatchExternalLinks::AddError(OutResult, Request.SourceAnchorParseError.IsEmpty() ? TEXT("external_anchor_ref_invalid") : Request.SourceAnchorParseError, TEXT("connect_pins requires source_anchor external_pin compact anchor."), TEXT("source_anchor"), TEXT("payload.source_anchor"));
			return false;
		}
		if (!Request.TargetAnchorParseError.IsEmpty() || Request.TargetAnchor.Type != EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			BlueprintHelperPatchExternalLinks::AddError(OutResult, Request.TargetAnchorParseError.IsEmpty() ? TEXT("external_anchor_ref_invalid") : Request.TargetAnchorParseError, TEXT("connect_pins requires target_anchor external_pin compact anchor."), TEXT("target_anchor"), TEXT("payload.target_anchor"));
			return false;
		}
		if (!Resolver.ResolveCompactPin(Request.AssetPath, Request.GraphName, Request.SourceAnchor, Context.SourcePin, ResolveError) ||
			!Resolver.ResolveCompactPin(Request.AssetPath, Request.GraphName, Request.TargetAnchor, Context.TargetPin, ResolveError))
		{
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, ResolveError.IsEmpty() ? TEXT("external_pin_not_found") : ResolveError, TEXT("External compact pin anchor cannot be resolved."), TEXT("anchor_ref"), TEXT("payload"));
			return false;
		}
		Context.LinkKind = Request.SourceAnchor.LinkKind == EBlueprintHelperExternalCompactLinkKind::Exec ? TEXT("exec") : TEXT("data");
		FString CompatibilityError;
		if (!BlueprintHelperPatchExternalLinks::CanConnectPins(Context.SourcePin, Context.TargetPin, CompatibilityError))
		{
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, CompatibilityError, TEXT("External pins cannot be connected."), TEXT("anchor_ref"), TEXT("payload"));
			return false;
		}
		return OutResult.bPassed;
	}

	if (!Request.LinkAnchorParseError.IsEmpty() || Request.LinkAnchor.Type != EBlueprintHelperExternalCompactAnchorType::Link)
	{
		BlueprintHelperPatchExternalLinks::AddError(OutResult, Request.LinkAnchorParseError.IsEmpty() ? TEXT("external_anchor_ref_invalid") : Request.LinkAnchorParseError, TEXT("disconnect_link and replace_link require link_anchor external_link compact anchor."), TEXT("link_anchor"), TEXT("payload.link_anchor"));
		return false;
	}

	FBlueprintHelperExternalGraphLinkResolution Link;
	if (!Resolver.ResolveCompactLink(Request.AssetPath, Request.GraphName, Request.LinkAnchor, Link, ResolveError))
	{
		BlueprintHelperPatchExternalLinks::AddConflict(OutResult, ResolveError.IsEmpty() ? TEXT("external_link_not_found") : ResolveError, TEXT("External compact link anchor is stale or cannot be resolved."), TEXT("link_anchor.anchor_ref"), TEXT("payload.link_anchor.anchor_ref"));
		return false;
	}
	Context.SourcePin = Link.SourcePin;
	Context.TargetPin = Link.TargetPin;
	Context.LinkKind = Link.LinkKind;

	if (Request.PatchType == TEXT("replace_link"))
	{
		if (!Request.ReplacementAnchorParseError.IsEmpty() || Request.ReplacementAnchor.Type != EBlueprintHelperExternalCompactAnchorType::Pin)
		{
			BlueprintHelperPatchExternalLinks::AddError(OutResult, Request.ReplacementAnchorParseError.IsEmpty() ? TEXT("external_anchor_ref_invalid") : Request.ReplacementAnchorParseError, TEXT("replace_link requires replacement_anchor external_pin compact anchor."), TEXT("replacement_anchor"), TEXT("payload.replacement_anchor"));
			return false;
		}
		if (!Resolver.ResolveCompactPin(Request.AssetPath, Request.GraphName, Request.ReplacementAnchor, Context.ReplacementPin, ResolveError))
		{
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, ResolveError.IsEmpty() ? TEXT("external_pin_not_found") : ResolveError, TEXT("External replacement pin anchor cannot be resolved."), TEXT("replacement_anchor.anchor_ref"), TEXT("payload.replacement_anchor.anchor_ref"));
			return false;
		}

		FString CompatibilityError;
		if (!Context.SourcePin || !Context.ReplacementPin)
		{
			CompatibilityError = TEXT("external_pin_not_found");
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, CompatibilityError, TEXT("External replacement pin cannot be connected."), TEXT("replacement_anchor.anchor_ref"), TEXT("payload.replacement_anchor.anchor_ref"));
			return false;
		}
		if (!BlueprintHelperPatchExternalLinks::IsInputPin(Context.TargetPin) ||
			!BlueprintHelperPatchExternalLinks::IsOutputPin(Context.ReplacementPin))
		{
			CompatibilityError = TEXT("external_pin_direction_invalid");
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, CompatibilityError, TEXT("External replacement pin cannot be connected."), TEXT("replacement_anchor.anchor_ref"), TEXT("payload.replacement_anchor.anchor_ref"));
			return false;
		}
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		const FPinConnectionResponse Response = Schema->CanCreateConnection(Context.ReplacementPin, Context.TargetPin);
		if (Response.Response == CONNECT_RESPONSE_DISALLOW)
		{
			CompatibilityError = FString::Printf(TEXT("external_pin_incompatible: %s"), *Response.Message.ToString());
			BlueprintHelperPatchExternalLinks::AddConflict(OutResult, CompatibilityError, TEXT("External replacement pin cannot be connected."), TEXT("replacement_anchor.anchor_ref"), TEXT("payload.replacement_anchor.anchor_ref"));
			return false;
		}
	}

	return OutResult.bPassed;
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalLinksService::Execute(const TSharedRef<FJsonObject>& Payload) const
{
	const FPatchExternalLinksRequest Request = ParseRequest(Payload);
	return FBlueprintHelperGraphWriteUnitOfWork::RunExistingOperation(
		Request.bDryRun ? EBlueprintHelperGraphWriteUnitOfWorkMode::Preview : EBlueprintHelperGraphWriteUnitOfWorkMode::Execute,
		TEXT("patch_external_links"),
		TEXT("patch_external_links"),
		EBlueprintHelperGraphBodyKind::K2ExternalBody,
		[this, &Request]()
		{
			return Request.bDryRun ? ExecuteDryRun(Request) : ExecuteWrite(Request);
		});
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalLinksService::ExecuteDryRun(
	const FPatchExternalLinksRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FPatchExternalLinksContext Context;
	FPatchExternalLinksPreflightResult PreflightResult;
	Preflight(Request, Context, PreflightResult);

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
		BlueprintHelperPatchExternalLinks::OperationName,
		TraceId);
	Result.Data = BlueprintHelperPatchExternalLinks::MakeDryRunData(PreflightResult, Request, Context);
	Result.bOk = PreflightResult.bPassed;
	if (!PreflightResult.bPassed)
	{
		Result.Status = EBlueprintHelperToolStatus::Failed;
		Result.Error = BlueprintHelperPatchExternalLinks::MakeErrorFromPreflight(PreflightResult);
	}
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperPatchExternalLinksService::ExecuteWrite(
	const FPatchExternalLinksRequest& Request) const
{
	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FPatchExternalLinksContext Context;
	FPatchExternalLinksPreflightResult PreflightResult;
	if (!Preflight(Request, Context, PreflightResult))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperPatchExternalLinks::OperationName,
			TraceId,
			BlueprintHelperPatchExternalLinks::MakeErrorFromPreflight(PreflightResult));
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Patch External Links")),
		Context.Blueprint);
	Mutation.Modify(Context.Graph);

	bool bChanged = false;
	FString ApplyError;
	if (!ApplyPatch(Request, Context, bChanged, ApplyError))
	{
		Mutation.Rollback();
		return FBlueprintHelperToolResultBuilder::Failure(
			BlueprintHelperPatchExternalLinks::OperationName,
			TraceId,
			BlueprintHelperPatchExternalLinks::MakeToolError(
				TEXT("external_link_patch_apply_failed"),
				EBlueprintHelperToolStage::Execute,
				ApplyError.IsEmpty() ? TEXT("patch_external_links apply failed.") : ApplyError,
				TEXT("payload"),
				EBlueprintHelperRollbackResult::RolledBack));
	}

	if (bChanged)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Context.Blueprint);
		if (Context.Blueprint && Context.Blueprint->GetOutermost())
		{
			Context.Blueprint->GetOutermost()->MarkPackageDirty();
		}
	}
	Mutation.Commit();

	FBlueprintHelperToolResultBase Result = bChanged
		? FBlueprintHelperToolResultBuilder::Applied(BlueprintHelperPatchExternalLinks::OperationName, TraceId)
		: FBlueprintHelperToolResultBuilder::NoOp(BlueprintHelperPatchExternalLinks::OperationName, TraceId);
	Result.Data = BlueprintHelperPatchExternalLinks::MakeDryRunData(PreflightResult, Request, Context);
	FBlueprintHelperValidationSummary Validation;
	Validation.bShouldCompile = bChanged;
	Validation.bShouldSave = bChanged;
	Result.Validation = Validation;
	return Result;
}

bool FBlueprintHelperPatchExternalLinksService::ApplyPatch(
	const FPatchExternalLinksRequest& Request,
	const FPatchExternalLinksContext& Context,
	bool& bOutChanged,
	FString& OutError) const
{
	bOutChanged = false;
	FBlueprintHelperGraphWriteMutationIntent Intent;
	if (Request.PatchType == TEXT("connect_pins"))
	{
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::ConnectPins;
		Intent.IntentId = TEXT("patch_external_connect_pins");
		Intent.Source.Pin = Context.SourcePin;
		Intent.Target.Pin = Context.TargetPin;
	}
	else if (Request.PatchType == TEXT("disconnect_link"))
	{
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::DisconnectPins;
		Intent.IntentId = TEXT("patch_external_disconnect_link");
		Intent.Source.Pin = Context.SourcePin;
		Intent.Target.Pin = Context.TargetPin;
	}
	else if (Request.PatchType == TEXT("replace_link"))
	{
		Intent.Kind = EBlueprintHelperGraphWriteMutationIntentKind::ReplaceSourcePinConnection;
		Intent.IntentId = TEXT("patch_external_replace_link");
		Intent.Source.Pin = Context.SourcePin;
		Intent.Target.Pin = Context.TargetPin;
		Intent.ReplacementSource.Pin = Context.ReplacementPin;
	}
	else
	{
		OutError = TEXT("unsupported_external_link_patch_type");
		return false;
	}

	TArray<FString> Unresolved;
	const FBlueprintGenerateResult Result =
		FBlueprintHelperGraphWriteMutationCoordinator::ExecuteIntents(Context.Graph, {Intent}, Unresolved);
	bOutChanged = Result.CreatedConnectionCount > 0;
	if (!Result.bSucceed)
	{
		OutError = Unresolved.Num() > 0 ? Unresolved[0] : Result.Message;
		return false;
	}
	return true;
}
