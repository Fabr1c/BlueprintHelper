// BlueprintHelper signature reference-context helpers.

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperDependencyAnalysisTypes.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Shared/BlueprintSignature/BlueprintHelperSignatureTypes.h"

class FBlueprintHelperSignatureReferenceContextUtils
{
public:
	static FString ReferenceContextTargetTypeForSignatureKind(const FString& SignatureKind);

	static TSharedRef<FJsonObject> MakeReferenceContextRequestJson(
		const FString& AssetPath,
		const FString& TargetType,
		const FString& TargetName,
		const FString& GraphName);

	static void AttachRemoveSignatureReferenceContextHint(
		const TSharedPtr<FJsonObject>& Data,
		const FBlueprintHelperRemoveSignatureRequest& Request);

	static void AttachReferenceContextSummary(
		const TSharedPtr<FJsonObject>& Data,
		const TCHAR* ResultField,
		const FBlueprintHelperReferenceContextPack& Context);

	static bool TryBuildSignatureReferenceContext(
		const FString& AssetPath,
		const FString& TargetType,
		const FString& TargetName,
		const FString& GraphName,
		FBlueprintHelperReferenceContextPack& OutContext,
		FString& OutError);

	static bool IsReferenceContextSafeForMutation(const FBlueprintHelperReferenceContextPack& Context);

	static void AttachRemoveSignatureReferenceContextSummary(
		const TSharedPtr<FJsonObject>& Data,
		const FBlueprintHelperRemoveSignatureRequest& Request,
		const FBlueprintHelperReferenceContextPack& Context);

private:
	static bool IsRegisteredAssetPath(const FString& AssetPath);
};
