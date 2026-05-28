#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Systems/Authorization/BlueprintHelperWriteAuthorizationService.h"
#include "BlueprintHelperAuthorizationUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UBlueprintHelperAuthorizationUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static int32 ClampTtlSeconds(int32 TtlSeconds);

	static FString NormalizeScope(const FString& Scope);

	static void SetAuthError(
		FBlueprintHelperBridgeValidationError& OutError,
		const FString& Code,
		const FString& Message);

	static void AddAssetPathIfPresent(TArray<FString>& OutAssetPaths, const FString& AssetPath);

	static void ReadAssetPathFields(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths);

	static void ReadTargetAssetsArray(const TSharedPtr<FJsonObject>& Json, TArray<FString>& OutAssetPaths);

	static bool TryReadPayloadAssets(const TSharedPtr<FJsonObject>& Payload, TArray<FString>& OutAssetPaths);

	static FString BuildWriteApprovalSummary(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope);

	static void ShowWriteApprovalEditorNotification(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope);

#if PLATFORM_WINDOWS
	static void FlashEditorWindowForWriteApproval();
#endif

	static void NotifyUserAboutPendingWriteApproval(const FBlueprintHelperWriteSessionRequest& Request, const FString& Scope);
};
