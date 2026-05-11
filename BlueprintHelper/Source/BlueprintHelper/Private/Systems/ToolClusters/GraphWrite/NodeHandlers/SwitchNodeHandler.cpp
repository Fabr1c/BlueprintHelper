#include "Systems/ToolClusters/GraphWrite/NodeHandlers/SwitchNodeHandler.h"

#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchString.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchEnum.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

bool FSwitchNodeHandler::CanHandle(EParsedBlueprintNodeType NodeType) const
{
	return NodeType == EParsedBlueprintNodeType::SwitchInteger
		|| NodeType == EParsedBlueprintNodeType::SwitchString
		|| NodeType == EParsedBlueprintNodeType::SwitchName
		|| NodeType == EParsedBlueprintNodeType::SwitchEnum;
}

UK2Node* FSwitchNodeHandler::Spawn(UEdGraph* TargetGraph, const FParsedNode& NodeData, FString& OutError) const
{
	if (!TargetGraph)
	{
		OutError = TEXT("Switch 节点生成失败：目标图表无效。");
		return nullptr;
	}

	UK2Node* ResultNode = nullptr;

	switch (NodeData.NodeType)
	{
	case EParsedBlueprintNodeType::SwitchInteger:
		{
			UK2Node_SwitchInteger* SwitchNode = NewObject<UK2Node_SwitchInteger>(TargetGraph);
			TargetGraph->AddNode(SwitchNode, true, false);
			SwitchNode->CreateNewGuid();
			SwitchNode->PostPlacedNewNode();
			SwitchNode->bHasDefaultPin = NodeData.SwitchReference.bHasDefaultPin;
			SwitchNode->StartIndex = NodeData.SwitchReference.StartIndex;
			SwitchNode->NodePosX = static_cast<int32>(NodeData.X);
			SwitchNode->NodePosY = static_cast<int32>(NodeData.Y);
			SwitchNode->AllocateDefaultPins();

			// 通过添加引脚来创。case 分支
			for (int32 i = 0; i < NodeData.SwitchReference.CaseValues.Num(); ++i)
			{
				SwitchNode->AddPinToSwitchNode();
			}

			ResultNode = SwitchNode;
		}
		break;

	case EParsedBlueprintNodeType::SwitchString:
		{
			UK2Node_SwitchString* SwitchNode = NewObject<UK2Node_SwitchString>(TargetGraph);
			TargetGraph->AddNode(SwitchNode, true, false);
			SwitchNode->CreateNewGuid();
			SwitchNode->PostPlacedNewNode();
			SwitchNode->bHasDefaultPin = NodeData.SwitchReference.bHasDefaultPin;
			SwitchNode->NodePosX = static_cast<int32>(NodeData.X);
			SwitchNode->NodePosY = static_cast<int32>(NodeData.Y);

			// 设置 PinNames
			SwitchNode->PinNames.Empty();
			for (const FString& CaseValue : NodeData.SwitchReference.CaseValues)
			{
				SwitchNode->PinNames.Add(FName(*CaseValue));
			}

			SwitchNode->AllocateDefaultPins();
			ResultNode = SwitchNode;
		}
		break;

	case EParsedBlueprintNodeType::SwitchName:
		{
			UK2Node_SwitchName* SwitchNode = NewObject<UK2Node_SwitchName>(TargetGraph);
			TargetGraph->AddNode(SwitchNode, true, false);
			SwitchNode->CreateNewGuid();
			SwitchNode->PostPlacedNewNode();
			SwitchNode->bHasDefaultPin = NodeData.SwitchReference.bHasDefaultPin;
			SwitchNode->NodePosX = static_cast<int32>(NodeData.X);
			SwitchNode->NodePosY = static_cast<int32>(NodeData.Y);

			// 设置 PinNames
			SwitchNode->PinNames.Empty();
			for (const FString& CaseValue : NodeData.SwitchReference.CaseValues)
			{
				SwitchNode->PinNames.Add(FName(*CaseValue));
			}

			SwitchNode->AllocateDefaultPins();
			ResultNode = SwitchNode;
		}
		break;

	case EParsedBlueprintNodeType::SwitchEnum:
		{
			const FString& EnumPath = NodeData.SwitchReference.EnumPath;
			if (EnumPath.IsEmpty())
			{
				OutError = TEXT("SwitchEnum 节点生成失败：enum_path 为空。");
				return nullptr;
			}

			UEnum* Enum = FindObject<UEnum>(nullptr, *EnumPath);
			if (!Enum)
			{
				Enum = LoadObject<UEnum>(nullptr, *EnumPath);
			}
			if (!Enum)
			{
				OutError = FString::Printf(TEXT("SwitchEnum 节点生成失败：未找到枚举类型 '%s'。"), *EnumPath);
				return nullptr;
			}

			UK2Node_SwitchEnum* SwitchNode = NewObject<UK2Node_SwitchEnum>(TargetGraph);
			TargetGraph->AddNode(SwitchNode, true, false);
			SwitchNode->CreateNewGuid();
			SwitchNode->PostPlacedNewNode();
			SwitchNode->bHasDefaultPin = NodeData.SwitchReference.bHasDefaultPin;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
			SwitchNode->SetEnum(Enum);
#else
			SwitchNode->Enum = Enum;
			SwitchNode->EnumEntries.Empty();
			SwitchNode->EnumFriendlyNames.Empty();
			if (Enum)
			{
				if (IsInGameThread() || Enum->IsPostLoadThreadSafe())
				{
					Enum->ConditionalPostLoad();
				}
				for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1; ++EnumIndex)
				{
					const bool bShouldBeHidden = Enum->HasMetaData(TEXT("Hidden"), EnumIndex)
						|| Enum->HasMetaData(TEXT("Spacer"), EnumIndex);
					if (!bShouldBeHidden)
					{
						const FString EnumValueName = Enum->GetNameStringByIndex(EnumIndex);
						SwitchNode->EnumEntries.Add(FName(*EnumValueName));
						SwitchNode->EnumFriendlyNames.Add(Enum->GetDisplayNameTextByIndex(EnumIndex));
					}
				}
			}
#endif
			SwitchNode->NodePosX = static_cast<int32>(NodeData.X);
			SwitchNode->NodePosY = static_cast<int32>(NodeData.Y);
			SwitchNode->AllocateDefaultPins();
			ResultNode = SwitchNode;
		}
		break;

	default:
		OutError = TEXT("Switch 节点生成失败：未知的 Switch 子类型。");
		return nullptr;
	}

	if (ResultNode)
	{
		TextToBlueprintGenerator::ApplyDefaultValues(ResultNode, NodeData.DefaultValues);
	}

	return ResultNode;
}
