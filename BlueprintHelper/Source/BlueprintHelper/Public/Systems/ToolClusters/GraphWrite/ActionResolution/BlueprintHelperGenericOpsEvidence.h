#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsControlOperationEvidence
{
	FString Operation;
	TArray<FString> CaseValues;
	FString DefaultPolicy;
	int32 DynamicOutputCount = INDEX_NONE;
	TMap<FString, FString> Facts;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsMacroInstanceEvidence
{
	FString Operation;
	FString MacroGraphPath;
	FString MacroPinShapeSnapshot;
	FString WorldContextPolicy;
	TMap<FString, FString> Facts;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsCreateEvidence
{
	FString Operation;
	FString ClassPath;
	FString AssetPath;
	FString ExposeOnSpawnEvidence;
	TMap<FString, FString> Facts;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsTransformEvidence
{
	FString Operation;
	FString SourcePinType;
	FString TargetPinType;
	FString CastPolicy;
	TMap<FString, FString> Facts;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsScheduleEvidence
{
	FString Operation;
	FString GraphLatentAllowed;
	FString HandlerEvidenceId;
	TMap<FString, FString> Facts;
};

struct BLUEPRINTHELPER_API FBlueprintHelperGenericOpsStructFieldPolicyEvidence
{
	FString StructPath;
	TArray<FString> SelectedFieldPaths;
	FString OptionalPinPolicy;
	FString ResultTypeProof;
	TMap<FString, FString> Facts;
};

class BLUEPRINTHELPER_API FBlueprintHelperControlOperationEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsControlOperationEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};

class BLUEPRINTHELPER_API FBlueprintHelperMacroInstanceEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsMacroInstanceEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};

class BLUEPRINTHELPER_API FBlueprintHelperGenericCreateEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsCreateEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};

class BLUEPRINTHELPER_API FBlueprintHelperGenericTransformEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsTransformEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};

class BLUEPRINTHELPER_API FBlueprintHelperGenericScheduleEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsScheduleEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};

class BLUEPRINTHELPER_API FBlueprintHelperStructFieldPolicyEvidenceReader
{
public:
	static bool Read(
		const FBlueprintHelperActionResolutionRequest& Request,
		FBlueprintHelperGenericOpsStructFieldPolicyEvidence& OutEvidence,
		FString& OutErrorCode,
		FString& OutMessage);
};
