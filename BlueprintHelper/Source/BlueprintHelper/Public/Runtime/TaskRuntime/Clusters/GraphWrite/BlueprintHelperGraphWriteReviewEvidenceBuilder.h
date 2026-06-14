#pragma once

#include "CoreMinimal.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphBody/BlueprintHelperGraphBodyBoundaryModel.h"

struct FBlueprintHelperDiagnosticItem;
struct FBlueprintHelperReviewAtomicTarget;
struct FBlueprintHelperWriteReviewEvidence;

struct BLUEPRINTHELPER_API FBlueprintHelperGraphWriteReviewEvidenceBuildInput
{
	FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
	FBlueprintHelperToolResultBase StepResult;
	FBlueprintHelperGraphBodyBoundaryModel BoundaryModel;
	FString ArchiveSessionId;
	FString TaskRunId;
	int32 StepIndex = INDEX_NONE;
};

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteReviewEvidenceBuilder
{
public:
	static bool Build(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static TSharedRef<FJsonObject> BuildGraphBodyBoundaryEvidence(
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel);

private:
	static FString TrimmedPayloadString(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName);
	static FString ReadTargetStringField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* PrimaryFieldName,
		const TCHAR* AlternateFieldName);
	static FString ReadAssetPath(const TSharedPtr<FJsonObject>& Payload);
	static FString ReadGraphName(const TSharedPtr<FJsonObject>& Payload);
	static FString SerializePayloadForAnchor(const TSharedPtr<FJsonObject>& Payload);
	static FString SerializeJsonObject(const TSharedRef<FJsonObject>& Object);
	static void AugmentBoundaryModelFromStepResult(
		const FBlueprintHelperToolResultBase& StepResult,
		FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel);
	static FString TrimmedObjectStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static FString ReadSignatureEvidenceId(const TSharedPtr<FJsonObject>& Payload);
	static void ApplySignatureDependencyMetadata(
		FBlueprintHelperReviewAtomicTarget& Target,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FString& SignatureEvidenceId);
	static void AppendStringArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		TArray<FString>& OutValues);
	static TArray<FString> ReadGraphBlockRefs(const FBlueprintHelperToolResultBase& StepResult);
	static FString MakeGraphBlockTargetKey(
		const FString& GraphName,
		const FString& BlockRef);
	static FString ReadStringField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static TSharedPtr<FJsonObject> ReadObjectField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static FString ReadAnchorRefField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName);
	static FString MakeReviewKeySegment(const FString& Value);
	static FString MakeExternalLinkPatchAnchorRef(const TSharedPtr<FJsonObject>& Payload);
	static FString MakeExternalMergeBlockId(
		const FString& GraphName,
		const FString& InsertedBlockId);
	static bool BuildExternalMergeFlowEvidence(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OperationKind,
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static bool BuildExternalLinkPatchEvidence(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OperationKind,
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static bool BuildExternalPropertyPatchEvidence(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OperationKind,
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static bool BuildExternalBodyReplaceEvidence(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OperationKind,
		const FBlueprintHelperGraphBodyBoundaryModel& BoundaryModel,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static bool BuildMaterialGraphEvidence(
		const FBlueprintHelperGraphWriteReviewEvidenceBuildInput& Input,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& OperationKind,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static void NormalizeGraphWriteDiagnostic(
		FBlueprintHelperDiagnosticItem& Item,
		const FString& DefaultGraphName);
	static void AppendDiagnosticsFromArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& DefaultGraphName,
		TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics);
	static void AppendReadbackCorrelationFromObject(
		const TSharedPtr<FJsonObject>& Object,
		const FString& DefaultGraphName,
		TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics);
	static void AppendReadbackCorrelationsFromArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FString& DefaultGraphName,
		TArray<FBlueprintHelperDiagnosticItem>& OutDiagnostics);
	static TArray<FBlueprintHelperDiagnosticItem> ReadReviewDiagnostics(
		const FBlueprintHelperToolResultBase& StepResult,
		const FString& DefaultGraphName);
	static FString TargetKeyForDiagnostic(
		const FBlueprintHelperDiagnosticItem& Diagnostic,
		const FString& DefaultGraphName);
	static bool DiagnosticMatchesTarget(
		const FBlueprintHelperDiagnosticItem& Diagnostic,
		const FBlueprintHelperReviewAtomicTarget& Target);
	static void AttachDiagnosticsToEvidence(
		const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics,
		FBlueprintHelperWriteReviewEvidence& OutEvidence);
	static FString BuildScopeIdentity(
		const FString& AssetPath,
		const FString& GraphName,
		const FString& TargetKey);
};
