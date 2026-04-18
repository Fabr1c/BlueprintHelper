// BlueprintHelper Service Layer — UMG Widget Tree 操作服务实现

#include "Services/BlueprintHelperWidgetService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetService, Log, All);

// ═══════════════════════════════════════════════════════════
// 内部工具
// ═══════════════════════════════════════════════════════════

UWidgetBlueprint* FBlueprintHelperWidgetService::ResolveWidgetBlueprint(
	const FString& AssetPath, FString& OutError) const
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path 不能为空。");
		return nullptr;
	}

	// 尝试从已加载对象中查找
	UObject* Obj = StaticFindObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath);
	if (!Obj)
	{
		// 通过 AssetRegistry 加载
		Obj = StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *AssetPath);
	}

	UWidgetBlueprint* WBP = Cast<UWidgetBlueprint>(Obj);
	if (!WBP)
	{
		OutError = FString::Printf(TEXT("未找到 WidgetBlueprint: %s"), *AssetPath);
		return nullptr;
	}

	return WBP;
}

UWidget* FBlueprintHelperWidgetService::FindWidgetByName(
	UWidgetBlueprint* WBP, const FString& Name, FString& OutError) const
{
	check(WBP);

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		OutError = TEXT("WidgetBlueprint 没有 WidgetTree。");
		return nullptr;
	}

	UWidget* Found = Tree->FindWidget(FName(*Name));
	if (!Found)
	{
		OutError = FString::Printf(TEXT("Widget '%s' 不存在于 WidgetTree 中。"), *Name);
	}
	return Found;
}

UClass* FBlueprintHelperWidgetService::FindWidgetClass(
	const FString& ClassName, FString& OutError) const
{
	// 尝试直接查找 UClassName
	FString FullName = ClassName;
	if (!FullName.StartsWith(TEXT("U")))
	{
		FullName = TEXT("U") + FullName;
	}

	// 遍历所有 UClass 查找匹配
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (Cls->IsChildOf(UWidget::StaticClass()) && !Cls->HasAnyClassFlags(CLASS_Abstract))
		{
			// 匹配完整类名或不带 U 前缀的短名
			if (Cls->GetName() == FullName || Cls->GetName() == ClassName)
			{
				return Cls;
			}
		}
	}

	OutError = FString::Printf(TEXT("未找到 Widget 类: %s"), *ClassName);
	return nullptr;
}

void FBlueprintHelperWidgetService::CollectWidgetInfo(
	UWidget* Widget, const FString& ParentName, int32 Depth,
	TArray<FBlueprintHelperWidgetInfo>& OutWidgets) const
{
	if (!Widget) return;

	FBlueprintHelperWidgetInfo Info;
	Info.Name = Widget->GetName();
	Info.WidgetClass = Widget->GetClass()->GetName();
	// 去掉 U 前缀以保持一致
	if (Info.WidgetClass.StartsWith(TEXT("U")))
	{
		Info.WidgetClass.RemoveFromStart(TEXT("U"));
	}
	Info.ParentName = ParentName;
	Info.Depth = Depth;

	if (Widget->Slot)
	{
		Info.SlotClass = Widget->Slot->GetClass()->GetName();
		if (Info.SlotClass.StartsWith(TEXT("U")))
		{
			Info.SlotClass.RemoveFromStart(TEXT("U"));
		}
	}

	// 检查是否为面板
	UPanelWidget* Panel = Cast<UPanelWidget>(Widget);
	if (Panel)
	{
		Info.ChildCount = Panel->GetChildrenCount();
	}

	OutWidgets.Add(Info);

	// 递归子节点
	if (Panel)
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			CollectWidgetInfo(Panel->GetChildAt(i), Info.Name, Depth + 1, OutWidgets);
		}
	}
}

// ═══════════════════════════════════════════════════════════
// 公开 API
// ═══════════════════════════════════════════════════════════

FBlueprintHelperWidgetTreeResult FBlueprintHelperWidgetService::GetWidgetTree(
	const FString& AssetPath) const
{
	FBlueprintHelperWidgetTreeResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		Result.ErrorMessage = TEXT("WidgetBlueprint 没有 WidgetTree。");
		return Result;
	}

	if (Tree->RootWidget)
	{
		Result.RootWidgetName = Tree->RootWidget->GetName();
		CollectWidgetInfo(Tree->RootWidget, TEXT(""), 0, Result.Widgets);
	}

	Result.bSuccess = true;
	return Result;
}

FBlueprintHelperWidgetMutationResult FBlueprintHelperWidgetService::AddWidget(
	const FString& AssetPath,
	const FString& ParentName,
	const FString& WidgetClass,
	const FString& WidgetName) const
{
	FBlueprintHelperWidgetMutationResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		Result.ErrorMessage = TEXT("WidgetBlueprint 没有 WidgetTree。");
		return Result;
	}

	// 查找 Widget 类
	UClass* Cls = FindWidgetClass(WidgetClass, Result.ErrorMessage);
	if (!Cls) return Result;

	// 确定父面板
	UPanelWidget* ParentPanel = nullptr;
	if (!ParentName.IsEmpty())
	{
		UWidget* ParentWidget = Tree->FindWidget(FName(*ParentName));
		if (!ParentWidget)
		{
			Result.ErrorMessage = FString::Printf(TEXT("父 Widget '%s' 不存在。"), *ParentName);
			return Result;
		}
		ParentPanel = Cast<UPanelWidget>(ParentWidget);
		if (!ParentPanel)
		{
			Result.ErrorMessage = FString::Printf(TEXT("'%s' 不是面板 Widget，不能添加子节点。"), *ParentName);
			return Result;
		}
	}
	else
	{
		// 添加到根面板
		if (Tree->RootWidget)
		{
			ParentPanel = Cast<UPanelWidget>(Tree->RootWidget);
			if (!ParentPanel)
			{
				Result.ErrorMessage = TEXT("根 Widget 不是面板，无法添加子节点。需要先指定一个面板作为父节点。");
				return Result;
			}
		}
		else
		{
			// 没有根 Widget，新 Widget 将成为根
			// 只有面板类型可以作为根
			if (!Cls->IsChildOf(UPanelWidget::StaticClass()))
			{
				Result.ErrorMessage = TEXT("WidgetTree 没有根节点，只能设置面板类型为根。");
				return Result;
			}
		}
	}

	// 确定名称
	FName NewName = WidgetName.IsEmpty() ? NAME_None : FName(*WidgetName);

	// 使用事务操作
	WBP->Modify();

	// 构造新 Widget
	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(Cls, NewName);
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法创建 Widget 类 '%s' 的实例。"), *WidgetClass);
		return Result;
	}

	if (ParentPanel)
	{
		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			Result.ErrorMessage = TEXT("AddChild 返回 null，可能面板已满或类型不兼容。");
			return Result;
		}
	}
	else
	{
		// 设置为根
		Tree->RootWidget = NewWidget;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);

	Result.bSuccess = true;
	Result.AffectedWidget = NewWidget->GetName();
	UE_LOG(LogWidgetService, Log, TEXT("添加 Widget '%s' (类型 %s) 到 '%s'"),
		*Result.AffectedWidget, *WidgetClass, *ParentName);
	return Result;
}

FBlueprintHelperWidgetMutationResult FBlueprintHelperWidgetService::RemoveWidget(
	const FString& AssetPath,
	const FString& WidgetName) const
{
	FBlueprintHelperWidgetMutationResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName, Result.ErrorMessage);
	if (!Widget) return Result;

	UWidgetTree* Tree = WBP->WidgetTree;

	// 不允许删除根 Widget（除非显式指定）
	if (Widget == Tree->RootWidget)
	{
		WBP->Modify();
		Tree->RootWidget = nullptr;
	}

	WBP->Modify();
	const bool bRemoved = Tree->RemoveWidget(Widget);
	if (!bRemoved)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法从 WidgetTree 移除 '%s'。"), *WidgetName);
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);

	Result.bSuccess = true;
	Result.AffectedWidget = WidgetName;
	UE_LOG(LogWidgetService, Log, TEXT("移除 Widget '%s'"), *WidgetName);
	return Result;
}

FBlueprintHelperWidgetMutationResult FBlueprintHelperWidgetService::MoveWidget(
	const FString& AssetPath,
	const FString& WidgetName,
	const FString& NewParentName,
	int32 InsertIndex) const
{
	FBlueprintHelperWidgetMutationResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName, Result.ErrorMessage);
	if (!Widget) return Result;

	UWidget* NewParentWidget = FindWidgetByName(WBP, NewParentName, Result.ErrorMessage);
	if (!NewParentWidget) return Result;

	UPanelWidget* NewParent = Cast<UPanelWidget>(NewParentWidget);
	if (!NewParent)
	{
		Result.ErrorMessage = FString::Printf(TEXT("'%s' 不是面板 Widget。"), *NewParentName);
		return Result;
	}

	// 不允许移动到自己的子树下
	TArray<UWidget*> Descendants;
	UWidgetTree::GetChildWidgets(Widget, Descendants);
	if (Descendants.Contains(NewParentWidget))
	{
		Result.ErrorMessage = TEXT("不能将 Widget 移动到自己的子树下。");
		return Result;
	}

	WBP->Modify();

	// 先从旧父节点移除
	int32 OldChildIndex = -1;
	UPanelWidget* OldParent = UWidgetTree::FindWidgetParent(Widget, OldChildIndex);
	if (OldParent)
	{
		OldParent->RemoveChild(Widget);
	}

	// 添加到新父节点
	if (InsertIndex >= 0 && InsertIndex <= NewParent->GetChildrenCount())
	{
		NewParent->InsertChildAt(InsertIndex, Widget);
	}
	else
	{
		NewParent->AddChild(Widget);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);

	Result.bSuccess = true;
	Result.AffectedWidget = WidgetName;
	UE_LOG(LogWidgetService, Log, TEXT("移动 Widget '%s' 到 '%s'"), *WidgetName, *NewParentName);
	return Result;
}

FBlueprintHelperWidgetPropertyResult FBlueprintHelperWidgetService::GetWidgetProperties(
	const FString& AssetPath,
	const FString& WidgetName) const
{
	FBlueprintHelperWidgetPropertyResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName, Result.ErrorMessage);
	if (!Widget) return Result;

	UClass* WidgetCls = Widget->GetClass();

	for (TFieldIterator<FProperty> PropIt(WidgetCls); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;

		// 只返回可编辑且非废弃属性
		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}
		if (Prop->HasAnyPropertyFlags(CPF_Deprecated))
		{
			continue;
		}

		FBlueprintHelperWidgetPropertyInfo Info;
		Info.Name = Prop->GetName();
		Info.TypeName = Prop->GetCPPType();

		// 导出当前值
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
		Prop->ExportText_Direct(Info.Value, ValuePtr, nullptr, Widget, PPF_None);

		// 标志
		if (Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
		{
			Info.Flags = TEXT("ReadOnly");
		}
		else
		{
			Info.Flags = TEXT("ReadWrite");
		}

		Result.Properties.Add(MoveTemp(Info));
	}

	Result.bSuccess = true;
	return Result;
}

FBlueprintHelperWidgetMutationResult FBlueprintHelperWidgetService::SetWidgetProperty(
	const FString& AssetPath,
	const FString& WidgetName,
	const FString& PropertyName,
	const FString& Value) const
{
	FBlueprintHelperWidgetMutationResult Result;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName, Result.ErrorMessage);
	if (!Widget) return Result;

	FProperty* Prop = Widget->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Prop)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Widget '%s' 没有属性 '%s'。"), *WidgetName, *PropertyName);
		return Result;
	}

	// 安全检查：不修改只读属性
	if (Prop->HasAnyPropertyFlags(CPF_BlueprintReadOnly) && !Prop->HasAnyPropertyFlags(CPF_Edit))
	{
		Result.ErrorMessage = FString::Printf(TEXT("属性 '%s' 是只读的。"), *PropertyName);
		return Result;
	}

	WBP->Modify();
	Widget->Modify();

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
	const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None);
	if (!ImportResult)
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("无法将 '%s' 设置为 '%s'。值格式可能不正确。"), *PropertyName, *Value);
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);

	Result.bSuccess = true;
	Result.AffectedWidget = WidgetName;
	UE_LOG(LogWidgetService, Log, TEXT("设置 Widget '%s' 属性 '%s' = '%s'"),
		*WidgetName, *PropertyName, *Value);
	return Result;
}
