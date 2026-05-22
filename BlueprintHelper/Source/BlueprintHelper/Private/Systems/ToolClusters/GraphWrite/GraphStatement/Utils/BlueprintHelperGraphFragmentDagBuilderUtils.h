// BlueprintHelper GraphStatement FBlueprintHelperGraphFragmentDagBuilderUtils declarations.

#pragma once

#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDagBuilder.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphFragmentDag.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"

class FBlueprintHelperGraphFragmentDagBuilderUtils
{
public:
	struct FBlueprintHelperDagDataProducer
	{
		FBlueprintHelperGraphFragmentEndpointRef Endpoint;
		FString SymbolId;
		FString Type;
		FString Path;

		bool IsValid() const
		{
			return Endpoint.IsValid();
		}
	};

	struct FBlueprintHelperDagExecFlow
	{
		TArray<FBlueprintHelperGraphFragmentEndpointRef> Entries;
		TArray<FBlueprintHelperGraphFragmentEndpointRef> Exits;

		bool IsEmpty() const
		{
			return Entries.Num() == 0 && Exits.Num() == 0;
		}
	};

	struct FBlueprintHelperDagBuildState
	{
		FBlueprintHelperGraphFragmentDag* Dag = nullptr;
		TSet<FString> FragmentIds;
		int32 FragmentSerial = 0;
		int32 ExecEdgeSerial = 0;
		int32 DataEdgeSerial = 0;
	};

	static FString BoolText(const bool bValue);
	static FString NormalizeSymbolKey(const FString& Name);
	static FString SanitizeIdPart(const FString& Value);
	static FString StatementKindName(const EBlueprintHelperGraphStatementKind Kind);
	static FString ExpressionKindName(const EBlueprintHelperGraphExpressionKind Kind);
	static FString TargetKindName(const EBlueprintHelperGraphTargetKind Kind);
	static EBlueprintHelperGraphFragmentDiagnosticSeverity ConvertSeverity(const FString& Severity);
	static void AddMetadata(FBlueprintHelperGraphFragmentRef& Fragment, const FString& Key, const FString& Value);
	static void AddResolvedTargetMetadata(
			FBlueprintHelperGraphFragmentRef& Fragment,
			const FBlueprintHelperGraphResolvedTarget& Target);
	static FString MakeUniqueFragmentId(FBlueprintHelperDagBuildState& State, const FString& PreferredId);
	static FBlueprintHelperGraphFragmentRef& AddFragment(
			FBlueprintHelperDagBuildState& State,
			const FString& PreferredId,
			const FString& SourceStatementId,
			const FString& Path,
			const FString& Kind);
	static FBlueprintHelperGraphFragmentEndpointRef MakeEndpoint(
			const FString& FragmentId,
			const FString& PortId,
			const FString& PinName,
			const FString& Type,
			const EBlueprintHelperGraphFragmentPortDirection Direction);
	static FBlueprintHelperGraphFragmentEndpointRef MakeExecEndpoint(
			const FString& FragmentId,
			const FString& PortId,
			const FString& PinName,
			const EBlueprintHelperGraphFragmentPortDirection Direction = EBlueprintHelperGraphFragmentPortDirection::Unknown);
	static FBlueprintHelperGraphFragmentEndpointRef MakeExecEntry(const FString& FragmentId);
	static FBlueprintHelperGraphFragmentEndpointRef MakeExecExit(const FString& FragmentId);
	static FBlueprintHelperGraphFragmentEndpointRef MakeDataInput(
			const FString& FragmentId,
			const FString& Name,
			const FString& Type);
	static FBlueprintHelperGraphFragmentEndpointRef MakeDataOutput(
			const FString& FragmentId,
			const FString& Name,
			const FString& Type);
	static void AddBuilderDiagnostic(
			FBlueprintHelperDagBuildState& State,
			const FString& Code,
			const FString& Path,
			const FString& Message,
			const EBlueprintHelperGraphFragmentDiagnosticSeverity Severity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Warning);
	static void AddExecEdge(
			FBlueprintHelperDagBuildState& State,
			const FBlueprintHelperGraphFragmentEndpointRef& From,
			const FBlueprintHelperGraphFragmentEndpointRef& To,
			const FString& Path);
	static void AddDataEdge(
			FBlueprintHelperDagBuildState& State,
			const FBlueprintHelperDagDataProducer& Producer,
			const FBlueprintHelperGraphFragmentEndpointRef& To,
			const FString& Path);
	static FString MakeStatementFragmentId(
			const FBlueprintHelperGraphStatementIR& Statement,
			const FString& Suffix);
	static FString MakeExpressionFragmentId(
			const FBlueprintHelperGraphExpressionIR& Expression,
			const FString& Suffix);
	static FBlueprintHelperDagDataProducer BuildExpression(
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
			const FString& FallbackPath,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static bool FindSymbolProducer(
			const FString& Name,
			const TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes,
			FBlueprintHelperDagDataProducer& OutProducer);
	static void RegisterSymbolProducer(
			const FString& Name,
			const FBlueprintHelperDagDataProducer& Producer,
			const FString& Path,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static void ConnectExpressionToInput(
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
			const FString& FallbackPath,
			const FString& InputName,
			const FString& InputType,
			const FString& ConsumerFragmentId,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static void ConnectExpressionMapToInputs(
			const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args,
			const FString& FallbackPath,
			const FString& ConsumerFragmentId,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperGraphFragmentRef& AddExpressionFragment(
			const FBlueprintHelperGraphExpressionIR& Expression,
			const FString& Kind,
			const FString& Suffix,
			FBlueprintHelperDagBuildState& State);
	static FBlueprintHelperDagDataProducer MakeExpressionProducerFromId(
			const FBlueprintHelperGraphExpressionIR& Expression,
			const FString& FragmentId,
			const FString& OutputName,
			const FString& FallbackType);
	static FBlueprintHelperDagDataProducer MakeExpressionProducer(
			const FBlueprintHelperGraphExpressionIR& Expression,
			const FBlueprintHelperGraphFragmentRef& Fragment,
			const FString& OutputName,
			const FString& FallbackType);
	static FBlueprintHelperDagDataProducer BuildResolvableExpressionFragment(
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
			const FString& Kind,
			const FString& Suffix,
			const FString& OutputName,
			const FString& OutputType,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperDagDataProducer BuildPlaceholderExpression(
			const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
			const FString& Kind,
			const FString& Suffix,
			const FString& OutputName,
			const FString& OutputType,
			const FString& DiagnosticCode,
			const FString& DiagnosticMessage,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes,
			const EBlueprintHelperGraphFragmentDiagnosticSeverity DiagnosticSeverity = EBlueprintHelperGraphFragmentDiagnosticSeverity::Info);
	static FBlueprintHelperGraphFragmentRef& AddStatementFragment(
			const FBlueprintHelperGraphStatementIR& Statement,
			const FString& Kind,
			const FString& Suffix,
			FBlueprintHelperDagBuildState& State);
	static FBlueprintHelperDagExecFlow BuildStatementArray(
			const TArray<TSharedPtr<FBlueprintHelperGraphStatementIR>>& Statements,
			const FString& Path,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperDagExecFlow BuildSimpleStatement(
			const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
			const FString& Kind,
			const FString& Suffix,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperDagExecFlow BuildLetStatement(
			const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperDagExecFlow BuildBranchStatement(
			const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static FBlueprintHelperDagExecFlow BuildStatement(
			const TSharedPtr<FBlueprintHelperGraphStatementIR>& Statement,
			const FString& FallbackPath,
			FBlueprintHelperDagBuildState& State,
			TArray<TMap<FString, FBlueprintHelperDagDataProducer>>& SymbolScopes);
	static void CopySemanticDiagnostics(
			const FBlueprintHelperGraphSemanticIR& SemanticIR,
			FBlueprintHelperGraphFragmentDag& OutDag);
};
