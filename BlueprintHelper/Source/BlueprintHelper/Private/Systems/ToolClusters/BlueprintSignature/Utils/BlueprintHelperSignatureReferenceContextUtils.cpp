// BlueprintHelper signature reference-context helpers.

#include "Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureReferenceContextUtils.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "Systems/ToolClusters/BlueprintHelperToolClusterConfigResolver.h"

FString FBlueprintHelperSignatureReferenceContextUtils::ReferenceContextTargetTypeForSignatureKind(
	const FString& SignatureKind)
{
	if (SignatureKind == TEXT("event_dispatcher"))
	{
		return TEXT("event_dispatcher");
	}
	if (SignatureKind == TEXT("custom_event") || SignatureKind == TEXT("interface_event"))
	{
		return TEXT("custom_event");
	}
	if (SignatureKind == TEXT("override_event") || SignatureKind == TEXT("native_event"))
	{
		return TEXT("event");
	}
	return TEXT("function");
}

TSharedRef<FJsonObject> FBlueprintHelperSignatureReferenceContextUtils::MakeReferenceContextRequestJson(
	const FString& AssetPath,
	const FString& TargetType,
	const FString& TargetName,
	const FString& GraphName)
{
	const FBlueprintHelperSignatureToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadSignaturePolicy();
	TSharedRef<FJsonObject> ReferenceContextRequest = MakeShared<FJsonObject>();
	ReferenceContextRequest->SetStringField(TEXT("asset_path"), AssetPath);
	ReferenceContextRequest->SetStringField(TEXT("target_type"), TargetType);
	ReferenceContextRequest->SetStringField(TEXT("target_name"), TargetName);
	if (!GraphName.IsEmpty())
	{
		ReferenceContextRequest->SetStringField(TEXT("graph_name"), GraphName);
	}
	ReferenceContextRequest->SetStringField(TEXT("search_scope"), Policy.ReferenceContextSearchScope);
	ReferenceContextRequest->SetStringField(TEXT("resolution_policy"), Policy.ReferenceContextResolutionPolicy);
	ReferenceContextRequest->SetStringField(TEXT("detail"), Policy.ReferenceContextDetail);
	ReferenceContextRequest->SetNumberField(TEXT("max_results"), Policy.ReferenceContextMaxResults);
	return ReferenceContextRequest;
}

void FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextHint(
	const TSharedPtr<FJsonObject>& Data,
	const FBlueprintHelperRemoveSignatureRequest& Request)
{
	const FBlueprintHelperSignatureToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadSignaturePolicy();
	const TSharedPtr<FJsonObject>* InnerResult = nullptr;
	if (!Data.IsValid() ||
		!Data->TryGetObjectField(TEXT("remove_signature_result"), InnerResult) ||
		!InnerResult ||
		!InnerResult->IsValid())
	{
		return;
	}

	TSharedRef<FJsonObject> ReferenceContextRequest = MakeShared<FJsonObject>();
	ReferenceContextRequest->SetStringField(TEXT("asset_path"), Request.AssetPath);
	ReferenceContextRequest->SetStringField(TEXT("target_type"), ReferenceContextTargetTypeForSignatureKind(Request.SignatureKind));
	ReferenceContextRequest->SetStringField(TEXT("target_name"), Request.SignatureName);
	if (!Request.GraphName.IsEmpty())
	{
		ReferenceContextRequest->SetStringField(TEXT("graph_name"), Request.GraphName);
	}
	ReferenceContextRequest->SetStringField(TEXT("search_scope"), Policy.ReferenceContextSearchScope);
	ReferenceContextRequest->SetStringField(TEXT("resolution_policy"), TEXT("ue_only"));
	ReferenceContextRequest->SetStringField(TEXT("detail"), TEXT("samples"));
	ReferenceContextRequest->SetNumberField(TEXT("max_results"), Policy.ReferenceContextMaxResults);
	(*InnerResult)->SetObjectField(TEXT("reference_context_request"), ReferenceContextRequest);
}

void FBlueprintHelperSignatureReferenceContextUtils::AttachReferenceContextSummary(
	const TSharedPtr<FJsonObject>& Data,
	const TCHAR* ResultField,
	const FBlueprintHelperReferenceContextPack& Context)
{
	const TSharedPtr<FJsonObject>* InnerResult = nullptr;
	if (!Data.IsValid() ||
		!Data->TryGetObjectField(ResultField, InnerResult) ||
		!InnerResult ||
		!InnerResult->IsValid())
	{
		return;
	}

	(*InnerResult)->SetObjectField(TEXT("reference_context_summary"), Context.Summary.ToJson());
	(*InnerResult)->SetObjectField(TEXT("reference_context_index_status"), Context.IndexStatus.ToJson());
	if (Context.UnsupportedChecks.Num() > 0)
	{
		(*InnerResult)->SetArrayField(
			TEXT("unsupported_checks"),
			FBlueprintHelperDependencyAnalysisJson::StringArray(Context.UnsupportedChecks));
	}
}

bool FBlueprintHelperSignatureReferenceContextUtils::TryBuildSignatureReferenceContext(
	const FString& AssetPath,
	const FString& TargetType,
	const FString& TargetName,
	const FString& GraphName,
	FBlueprintHelperReferenceContextPack& OutContext,
	FString& OutError)
{
	FBlueprintHelperDependencyAnalysisTarget Target;
	Target.AssetPath = AssetPath;
	Target.TargetType = TargetType;
	Target.TargetName = TargetName;
	Target.GraphName = GraphName;

	const FBlueprintHelperSignatureToolClusterPolicy Policy =
		FBlueprintHelperToolClusterConfigResolver::LoadSignaturePolicy();
	FBlueprintHelperDependencyAnalysisOptions Options;
	Options.SearchScope = IsRegisteredAssetPath(AssetPath) ? Policy.ReferenceContextSearchScope : TEXT("asset");
	Options.ResolutionPolicy = Policy.ReferenceContextResolutionPolicy;
	Options.Detail = Policy.ReferenceContextDetail;
	Options.MaxResultCount = Policy.ReferenceContextMaxResults;

	FString ErrorCode;
	FString ErrorMessage;
	FBlueprintHelperDependencyAnalysisService DependencyAnalysisService;
	if (!DependencyAnalysisService.TryBuildReferenceContext(Target, Options, OutContext, ErrorCode, ErrorMessage))
	{
		OutError = ErrorMessage.IsEmpty()
			? FString::Printf(TEXT("Unable to build reference context for %s %s."), *TargetType, *TargetName)
			: ErrorMessage;
		if (!ErrorCode.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s: %s"), *ErrorCode, *OutError);
		}
		return false;
	}
	return true;
}

bool FBlueprintHelperSignatureReferenceContextUtils::IsReferenceContextSafeForMutation(
	const FBlueprintHelperReferenceContextPack& Context)
{
	return Context.AgentHints.bCanEditSafely &&
		Context.Summary.BlockingCount == 0 &&
		!Context.Summary.bPartial &&
		!Context.Summary.bTruncated;
}

void FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextSummary(
	const TSharedPtr<FJsonObject>& Data,
	const FBlueprintHelperRemoveSignatureRequest& Request,
	const FBlueprintHelperReferenceContextPack& Context)
{
	AttachRemoveSignatureReferenceContextHint(Data, Request);
	AttachReferenceContextSummary(Data, TEXT("remove_signature_result"), Context);
}

bool FBlueprintHelperSignatureReferenceContextUtils::IsRegisteredAssetPath(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	FString PackageName = AssetPath;
	if (FPackageName::IsValidObjectPath(AssetPath))
	{
		PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	}
	else if (!FPackageName::IsValidLongPackageName(PackageName) && AssetPath.Contains(TEXT(".")))
	{
		PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	}

	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssetsByPackageName(FName(*PackageName), Assets, false);
	return Assets.Num() > 0;
}
