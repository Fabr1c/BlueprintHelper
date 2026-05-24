#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphParsedTypes.h"

class FBlueprintGraphNodeUtility
{
public:
	inline static const TCHAR* StandardMacroLibraryPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");

	static FString NormalizeNodeTypeName(const FString& InNodeType)
	{
		FString Result = InNodeType;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT("\""), TEXT(""));

		const int32 LastSlashIndex = Result.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastSlashIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastSlashIndex + 1);
		}

		const int32 LastDotIndex = Result.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (LastDotIndex != INDEX_NONE)
		{
			Result = Result.Mid(LastDotIndex + 1);
		}

		return Result.TrimStartAndEnd();
	}

	static bool IsPlaceholderMacroName(const FString& InMacroName)
	{
		return InMacroName.Equals(TEXT("BlueprintGraph.MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("MacroInstance"), ESearchCase::IgnoreCase);
	}

	static FString NormalizePinKey(const FString& InPinName)
	{
		FString Result = InPinName;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		return Result.ToLower();
	}

	static FName ResolveRealSubCategory(const FString& Category)
	{
		if (Category.Equals(TEXT("double"), ESearchCase::IgnoreCase))
		{
			return UEdGraphSchema_K2::PC_Double;
		}

		return UEdGraphSchema_K2::PC_Float;
	}

	static FBlueprintGeneratorDiagnostic MakeGeneratorDiagnostic(
		const FString& Code,
		const FString& NodeId,
		const FString& PinName,
		const FString& Message,
		const FString& Severity = TEXT("error"))
	{
		FBlueprintGeneratorDiagnostic Diagnostic;
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.NodeId = NodeId;
		Diagnostic.PinName = PinName;
		Diagnostic.Message = Message;
		return Diagnostic;
	}

	static bool IsInvalidPinTypeFailure(const FString& ErrorMessage)
	{
		return ErrorMessage.Contains(TEXT("pin type"))
			|| ErrorMessage.Contains(TEXT("PinType"))
			|| ErrorMessage.Contains(TEXT("pin_type"))
			|| ErrorMessage.Contains(TEXT("sub category"));
	}

	static UEdGraph* ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage);
	static UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);
	static FBlueprintHelperCallFunctionResolveResult ResolveFunctionForGraph(UEdGraph* TargetGraph, const FString& FunctionQuery, const TMap<FString, FString>& DefaultValues);
	static TArray<TSharedPtr<FEngineFunctionItem>> GetAllBlueprintFunctions();
};
