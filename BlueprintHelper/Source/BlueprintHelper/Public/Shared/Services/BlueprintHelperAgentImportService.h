// BlueprintHelper Service Layer - Agent semantic import

#pragma once

#include "CoreMinimal.h"
#include "Shared/BlueprintHelperServiceTypes.h"

class FBlueprintHelperAssetBrowseService;
class FBlueprintHelperCompileService;
class FBlueprintHelperGraphResolver;

enum class EBlueprintHelperAgentImportDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error
};

enum class EBlueprintHelperAgentImportMode : uint8
{
	Append
};

enum class EBlueprintHelperAgentLayoutStrategy : uint8
{
	Auto,
	AppendRight
};

struct FBlueprintHelperAgentImportDiagnostic
{
	EBlueprintHelperAgentImportDiagnosticSeverity Severity = EBlueprintHelperAgentImportDiagnosticSeverity::Info;
	FString Code;
	FString Path;
	FString Message;
	FString Suggestion;
};

struct FBlueprintHelperAgentImportOptions
{
	bool bCompile = true;
	bool bSave = false;
	bool bStrict = true;
	bool bDryRun = false;
	bool bCreateMissingVariables = true;
	bool bReconstructExistingNodes = false;
};


struct FBlueprintHelperAgentImportParsedRequest
{
	FBlueprintHelperGraphTarget Target;
	EBlueprintHelperAgentImportMode Mode = EBlueprintHelperAgentImportMode::Append;
	EBlueprintHelperAgentLayoutStrategy Layout = EBlueprintHelperAgentLayoutStrategy::Auto;
	FBlueprintHelperAgentImportOptions Options;
};

struct FBlueprintHelperAgentImportRequest
{
	FString JsonText;
};

struct FBlueprintHelperAgentImportResult
{
	bool bSuccess = false;
	FString Status = TEXT("failed");
	FString ErrorCode;
	FString Message;
	TArray<FString> Warnings;
	TArray<FBlueprintHelperAgentImportDiagnostic> Diagnostics;
	int32 CreatedNodeCount = 0;
	int32 CreatedLinkCount = 0;
	int32 CreatedVariableCount = 0;
	int32 WarningCount = 0;
	int32 ErrorCount = 0;
	int32 RollbackCount = 0;
	bool bRolledBack = false;
	bool bCompiled = false;
	bool bSaved = false;
	bool bDryRun = false;

	bool HasErrors() const;
	FString GetSummaryText() const;
};

class BLUEPRINTHELPER_API FBlueprintHelperAgentImportService
{
public:
	FBlueprintHelperAgentImportService(
		const FBlueprintHelperGraphResolver& InResolver,
		const FBlueprintHelperCompileService& InCompileService,
		const FBlueprintHelperAssetBrowseService& InAssetBrowseService);

	FBlueprintHelperAgentImportResult Import(const FBlueprintHelperAgentImportRequest& Request) const;

private:
	const FBlueprintHelperGraphResolver& Resolver;
	const FBlueprintHelperCompileService& CompileService;
	const FBlueprintHelperAssetBrowseService& AssetBrowseService;
};
