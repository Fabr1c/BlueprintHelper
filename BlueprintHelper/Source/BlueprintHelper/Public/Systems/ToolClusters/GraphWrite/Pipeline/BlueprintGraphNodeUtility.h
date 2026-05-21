#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"

class BLUEPRINTHELPER_API FBlueprintGraphNodeUtility
{
public:
	/** 标准宏库资产路径。 */
	inline static const TCHAR* StandardMacroLibraryPath = TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros");

	/**
	 * 归一化节点类型名称，统一转成 K2Node_xxx 形式。
	 */
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

	/**
	 * 判断宏名称是否仍是占位符。
	 */
	static bool IsPlaceholderMacroName(const FString& InMacroName)
	{
		return InMacroName.Equals(TEXT("BlueprintGraph.MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase)
			|| InMacroName.Equals(TEXT("MacroInstance"), ESearchCase::IgnoreCase);
	}

	/**
	 * 归一化引脚名称，便于做别名比较。
	 */
	static FString NormalizePinKey(const FString& InPinName)
	{
		FString Result = InPinName;
		Result.TrimStartAndEndInline();
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-"), TEXT(""));
		return Result.ToLower();
	}

	/**
	 * 返回标准 K2 实数子分类。
	 */
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
		return ErrorMessage.Contains(TEXT("类型转换失败"))
			|| ErrorMessage.Contains(TEXT("引脚类型无效"))
			|| ErrorMessage.Contains(TEXT("暂不支持的引脚类型"))
			|| ErrorMessage.Contains(TEXT("无法加载引脚子分类对象"));
	}

	static FString FindDiagnosticPinName(const FParsedNode& NodeData, const FString& ErrorMessage)
	{
		for (const FParsedEventParam& Param : NodeData.EventReference.Params)
		{
			if (!Param.Name.IsEmpty() && ErrorMessage.Contains(Param.Name))
			{
				return Param.Name;
			}
		}

		return TEXT("");
	}

	static int32 CountRequestedPinTypes(const FParsedNode& NodeData)
	{
		int32 Count = 0;
		if (NodeData.VariableReference.PinType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.ElementType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.KeyType.IsValid())
		{
			++Count;
		}
		if (NodeData.ContainerReference.ValueType.IsValid())
		{
			++Count;
		}
		for (const FParsedEventParam& Param : NodeData.EventReference.Params)
		{
			if (Param.PinType.IsValid())
			{
				++Count;
			}
		}
		return Count;
	}



	static UEdGraph* ResolveMacroGraph(const FParsedMacroReference& MacroReference, FString& OutErrorMessage);
	static UEdGraphPin* FindPinByAlias(UK2Node* TargetNode, const FString& RequestedPinName);
	static FBlueprintHelperCallFunctionResolveResult ResolveFunctionForGraph( UEdGraph* TargetGraph, const FString& FunctionQuery, const TMap<FString, FString>& DefaultValues);
	static TArray<TSharedPtr<FEngineFunctionItem>> GetAllBlueprintFunctions();
};
