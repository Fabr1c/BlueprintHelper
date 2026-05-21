#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteFacade.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphDefaultValueApplier.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphExistingNodeMapper.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphJsonParser.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLinker.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphLocalVariableService.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeSpawner.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintGraphNodeUtility.h"
#include "Systems/ToolClusters/GraphWrite/Pipeline/BlueprintMultiGraphGenerationPipeline.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphComposer.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphStatementBuilder.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectIterator.h"
#include "EdGraphNode_Comment.h"

bool FBlueprintGraphLocalVariableService::ConvertToEdGraphPinType(const FParsedPinType& InPinType, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	if (!InPinType.IsValid())
	{
		OutErrorMessage = TEXT("引脚类型无效：缺少 category。");
		return false;
	}

	OutPinType = FEdGraphPinType();
	const FString Category = InPinType.Category.ToLower();

	if (Category == TEXT("bool") || Category == TEXT("boolean"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	}
	else if (Category == TEXT("byte"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	}
	else if (Category == TEXT("int") || Category == TEXT("int32"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
	}
	else if (Category == TEXT("int64"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
	}
	else if (Category == TEXT("float") || Category == TEXT("double") || Category == TEXT("real"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		OutPinType.PinSubCategory = FBlueprintGraphNodeUtility::ResolveRealSubCategory(Category);
	}
	else if (Category == TEXT("name"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
	}
	else if (Category == TEXT("string"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
	}
	else if (Category == TEXT("text"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
	}
	else if (Category == TEXT("object"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	}
	else if (Category == TEXT("class"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
	}
	else if (Category == TEXT("struct"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	}
	else if (Category == TEXT("enum"))
	{
		OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
	}
	else
	{
		OutErrorMessage = FString::Printf(TEXT("暂不支持的引脚类型：%s"), *InPinType.Category);
		return false;
	}

	if (!InPinType.SubCategory.IsEmpty() && Category != TEXT("float") && Category != TEXT("double") && Category != TEXT("real"))
	{
		OutPinType.PinSubCategory = FName(*InPinType.SubCategory);
	}

	if (!InPinType.SubCategoryObjectPath.IsEmpty())
	{
		if (UObject* SubCategoryObject = LoadObject<UObject>(nullptr, *InPinType.SubCategoryObjectPath))
		{
			OutPinType.PinSubCategoryObject = SubCategoryObject;
		}
		else
		{
			OutErrorMessage = FString::Printf(TEXT("无法加载引脚子分类对象：%s"), *InPinType.SubCategoryObjectPath);
			return false;
		}
	}

	const FString ContainerType = InPinType.ContainerType.ToLower();
	if (ContainerType == TEXT("array"))
	{
		OutPinType.ContainerType = EPinContainerType::Array;
	}
	else if (ContainerType == TEXT("set"))
	{
		OutPinType.ContainerType = EPinContainerType::Set;
	}
	else if (ContainerType == TEXT("map"))
	{
		OutPinType.ContainerType = EPinContainerType::Map;
	}

	OutPinType.bIsReference = InPinType.bIsReference;
	OutPinType.bIsConst = InPinType.bIsConst;
	return true;
}

bool FBlueprintGraphLocalVariableService::EnsureLocalVariableExists(UEdGraph* TargetGraph, const FParsedLocalVariableDeclaration& Declaration, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("本地变量创建失败：目标图表无效。");
		return false;
	}

	if (Declaration.Name.IsEmpty())
	{
		OutErrorMessage = TEXT("本地变量创建失败：变量名为空。");
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	UEdGraph* ScopeGraph = FBlueprintEditorUtils::GetTopLevelGraph(TargetGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Blueprint || !ScopeGraph || !Schema || Schema->GetGraphType(ScopeGraph) != GT_Function)
	{
		OutErrorMessage = TEXT("本地变量仅支持在函数图中创建。请先聚焦一个函数图。");
		return false;
	}

	if (FBlueprintEditorUtils::FindLocalVariable(Blueprint, ScopeGraph, *Declaration.Name))
	{
		return true;
	}

	FEdGraphPinType VariablePinType;
	if (!FBlueprintGraphLocalVariableService::ConvertToEdGraphPinType(Declaration.PinType, VariablePinType, OutErrorMessage))
	{
		return false;
	}

	if (!FBlueprintEditorUtils::AddLocalVariable(Blueprint, ScopeGraph, *Declaration.Name, VariablePinType, Declaration.DefaultValue))
	{
		OutErrorMessage = FString::Printf(TEXT("无法创建本地变量：%s"), *Declaration.Name);
		return false;
	}

	return true;
}

UStruct* FBlueprintGraphLocalVariableService::ResolveLocalVariableScope(UEdGraph* TargetGraph, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (!TargetGraph)
	{
		OutErrorMessage = TEXT("无法解析本地变量作用域：目标图表无效。");
		return nullptr;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	UEdGraph* ScopeGraph = FBlueprintEditorUtils::GetTopLevelGraph(TargetGraph);
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (!Blueprint || !ScopeGraph || !Schema || Schema->GetGraphType(ScopeGraph) != GT_Function)
	{
		OutErrorMessage = TEXT("当前图表不是函数图，无法解析本地变量作用域。");
		return nullptr;
	}

	if (!Blueprint->SkeletonGeneratedClass)
	{
		OutErrorMessage = TEXT("蓝图骨架类无效，无法解析本地变量作用域。");
		return nullptr;
	}

	if (UFunction* ScopeFunction = Blueprint->SkeletonGeneratedClass->FindFunctionByName(ScopeGraph->GetFName()))
	{
		return ScopeFunction;
	}

	OutErrorMessage = FString::Printf(TEXT("未在蓝图骨架类中找到作用域函数：%s"), *ScopeGraph->GetName());
	return nullptr;
}

UStruct* FBlueprintGraphLocalVariableService::ResolveVariableSource(UEdGraph* TargetGraph, const FParsedVariableReference& VariableReference, FString& OutErrorMessage)
{
	OutErrorMessage.Reset();
	if (VariableReference.IsLocalVariable())
	{
		return FBlueprintGraphLocalVariableService::ResolveLocalVariableScope(TargetGraph, OutErrorMessage);
	}

	if (VariableReference.bSelfContext || VariableReference.OwnerClassPath.IsEmpty() || VariableReference.OwnerClassPath.Equals(TEXT("self"), ESearchCase::IgnoreCase))
	{
		return nullptr;
	}

	if (UClass* OwnerClass = LoadObject<UClass>(nullptr, *VariableReference.OwnerClassPath))
	{
		return OwnerClass;
	}

	OutErrorMessage = FString::Printf(TEXT("无法加载成员变量所属类：%s"), *VariableReference.OwnerClassPath);
	return nullptr;
}
