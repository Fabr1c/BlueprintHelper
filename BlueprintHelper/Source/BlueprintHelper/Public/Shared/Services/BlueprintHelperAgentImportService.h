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

struct FBlueprintHelperAgentImportVariableDeclaration
{
	FString Name;
	FString Type;
	FString DefaultValue;
	bool bEditable = false;
	FString Category;
};

struct FBlueprintHelperAgentImportNode
{
	FString Id;
	FString Kind;
	FString Label;
	FString Function;
	FString EventName;
	FString CustomEventName;
	FString VariableName;
	FString VariableType;
	FString Value;
	FString Condition;
	FString CommentText;
	TArray<FString> Contains;
	TMap<FString, FString> Inputs;
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size = FVector2D(400.0f, 100.0f);
};

struct FBlueprintHelperAgentImportLink
{
	FString Kind;
	FString FromNode;
	FString FromPin;
	FString ToNode;
	FString ToPin;
	FString Path;
};

struct FBlueprintHelperAgentImportParsedRequest
{
	FBlueprintHelperGraphTarget Target;
	EBlueprintHelperAgentImportMode Mode = EBlueprintHelperAgentImportMode::Append;
	EBlueprintHelperAgentLayoutStrategy Layout = EBlueprintHelperAgentLayoutStrategy::Auto;
	FBlueprintHelperAgentImportOptions Options;
	TArray<FBlueprintHelperAgentImportVariableDeclaration> Variables;
	TArray<FBlueprintHelperAgentImportNode> Nodes;
	TArray<FBlueprintHelperAgentImportLink> Links;
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
