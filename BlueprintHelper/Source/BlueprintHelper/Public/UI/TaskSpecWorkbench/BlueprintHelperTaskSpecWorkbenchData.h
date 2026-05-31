// BlueprintHelper TaskSpec / ReadContext workbench data models.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

enum class EBlueprintHelperWorkbenchInputType : uint8
{
	Unknown,
	TaskSpec,
	T3D
};

enum class EBlueprintHelperReadContextExportFormat : uint8
{
	LogicFlow,
	LogicMd,
	LogicJson
};

enum class EBlueprintHelperTaskSpecPreviewBlockKind : uint8
{
	GraphLogic,
	NonGraphLogic,
	Diagnostic
};

struct BLUEPRINTHELPER_API FBlueprintHelperInputDocument
{
	FString RawText;
	EBlueprintHelperWorkbenchInputType InputType = EBlueprintHelperWorkbenchInputType::Unknown;
	bool bRecognized = false;
	bool bParseSucceeded = false;
	FString StatusText;
};

struct BLUEPRINTHELPER_API FBlueprintHelperCallFunctionCandidateRowModel
{
	FString CandidateId;
	FString StableId;
	FString DisplayName;
	FString NativeFunctionName;
	FString OwnerClassPath;
	FString Category;
	FString MatchReason;
	FString MismatchReason;
	int32 Score = 0;
	bool bSelected = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperCallFunctionCardModel
{
	FString CardId;
	FString SourcePath;
	FString Query;
	FString StatusText;
	TArray<FBlueprintHelperCallFunctionCandidateRowModel> Candidates;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecPreviewBlock
{
	FString BlockId;
	FString SourcePath;
	FString Title;
	FString Detail;
	EBlueprintHelperTaskSpecPreviewBlockKind Kind =
		EBlueprintHelperTaskSpecPreviewBlockKind::Diagnostic;
	int32 Row = 0;
	int32 Column = 0;
	bool bSelected = false;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecPreviewConnection
{
	FString FromBlockId;
	FString ToBlockId;
	FString Label;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecPreviewModel
{
	int32 Revision = 0;
	TArray<FBlueprintHelperTaskSpecPreviewBlock> Blocks;
	TArray<FBlueprintHelperTaskSpecPreviewConnection> Connections;
	FString StatusText;
};

struct BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchSnapshot
{
	FBlueprintHelperInputDocument Input;
	TArray<FBlueprintHelperCallFunctionCardModel> CandidateCards;
	FBlueprintHelperTaskSpecPreviewModel Preview;
	FString SelectedCardId;
	FString SelectedCandidateId;
	FString StatusText;
};

DECLARE_MULTICAST_DELEGATE(FBlueprintHelperTaskSpecWorkbenchDataChanged);

class BLUEPRINTHELPER_API FBlueprintHelperTaskSpecWorkbenchStore
{
public:
	const FBlueprintHelperTaskSpecWorkbenchSnapshot& GetSnapshot() const;
	void ReplaceSnapshot(const FBlueprintHelperTaskSpecWorkbenchSnapshot& InSnapshot);
	void SelectCandidate(const FString& CardId, const FString& CandidateId);
	FBlueprintHelperTaskSpecWorkbenchDataChanged& OnDataChanged();

private:
	FBlueprintHelperTaskSpecWorkbenchSnapshot Snapshot;
	FBlueprintHelperTaskSpecWorkbenchDataChanged DataChanged;
};
