// BlueprintHelper Service Layer 。UMG Widget Tree 操作服务实现

#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Shared/BlueprintHelperServiceTypes.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogWidgetService, Log, All);

class FBlueprintHelperWidgetServiceLocalUtils
{
public:
struct FBlueprintHelperSlotSnapshot
{
	UPanelWidget* Parent = nullptr;
	int32 ChildIndex = INDEX_NONE;
	UClass* SlotClass = nullptr;
	TMap<FName, FString> SlotPropertyValues;
	bool bHasSlotZOrder = false;
	int32 SlotZOrder = 0;
	bool bHasCanvasSlotValues = false;
	FAnchorData CanvasLayout;
	bool bCanvasAutoSize = false;
	int32 CanvasZOrder = 0;
};

static bool CaptureSlotSnapshot(UWidget* Widget, FBlueprintHelperSlotSnapshot& OutSnapshot, FString& OutError)
{
	if (!Widget || !Widget->Slot)
	{
		OutError = TEXT("Widget 没有关联的 Slot，无法快照。");
		return false;
	}

	OutSnapshot.Parent = UWidgetTree::FindWidgetParent(Widget, OutSnapshot.ChildIndex);
	if (!OutSnapshot.Parent)
	{
		OutError = TEXT("不允许移。RootWidget 或无父节点的 Widget。");
		return false;
	}

	OutSnapshot.SlotClass = Widget->Slot->GetClass();
	if (const FIntProperty* ZOrderProperty = FindFProperty<FIntProperty>(OutSnapshot.SlotClass, TEXT("ZOrder")))
	{
		OutSnapshot.bHasSlotZOrder = true;
		OutSnapshot.SlotZOrder = ZOrderProperty->GetPropertyValue_InContainer(Widget->Slot);
	}

	if (const UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		OutSnapshot.bHasCanvasSlotValues = true;
		OutSnapshot.CanvasLayout = CanvasSlot->GetLayout();
		OutSnapshot.bCanvasAutoSize = CanvasSlot->GetAutoSize();
		if (const FIntProperty* ZOrderProperty = FindFProperty<FIntProperty>(CanvasSlot->GetClass(), TEXT("ZOrder")))
		{
			OutSnapshot.CanvasZOrder = ZOrderProperty->GetPropertyValue_InContainer(CanvasSlot);
		}
		else
		{
			OutSnapshot.CanvasZOrder = CanvasSlot->GetZOrder();
		}
	}

	for (TFieldIterator<FProperty> It(OutSnapshot.SlotClass); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Edit) || Prop->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget->Slot);
		FString Value;
		Prop->ExportText_Direct(Value, ValuePtr, nullptr, Widget->Slot, PPF_None);
		OutSnapshot.SlotPropertyValues.Add(Prop->GetFName(), MoveTemp(Value));
	}

	return true;
}

static bool ApplySlotSnapshotToSlot(UPanelSlot* Slot, const FBlueprintHelperSlotSnapshot& Snapshot, bool bModifySlot, FString& OutError)
{
	if (!Slot)
	{
		OutError = TEXT("恢复 Widget 旧 Slot 失败：Slot 为空。");
		return false;
	}

	if (Snapshot.SlotClass && Slot->GetClass() != Snapshot.SlotClass)
	{
		OutError = FString::Printf(
			TEXT("恢复 Widget 旧 Slot 失败：期望 %s，实际 %s。"),
			*Snapshot.SlotClass->GetName(),
			*Slot->GetClass()->GetName());
		return false;
	}

	if (bModifySlot)
	{
		Slot->Modify();
	}

	for (const TPair<FName, FString>& Pair : Snapshot.SlotPropertyValues)
	{
		if (Snapshot.bHasCanvasSlotValues
			&& (Pair.Key == TEXT("LayoutData") || Pair.Key == TEXT("bAutoSize") || Pair.Key == TEXT("ZOrder")))
		{
			continue;
		}

		FProperty* Prop = Slot->GetClass()->FindPropertyByName(Pair.Key);
		if (!Prop)
		{
			OutError = FString::Printf(TEXT("恢复 Slot 属性失败：未找到属性 %s。"), *Pair.Key.ToString());
			return false;
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Slot);
		const TCHAR* ImportResult = Prop->ImportText_Direct(*Pair.Value, ValuePtr, Slot, PPF_None);
		if (!ImportResult)
		{
			OutError = FString::Printf(TEXT("恢复 Slot 属性失败：无法导入属性 %s。"), *Pair.Key.ToString());
			return false;
		}
	}

	if (Snapshot.bHasCanvasSlotValues)
	{
		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
		if (!CanvasSlot)
		{
			OutError = TEXT("恢复 Canvas Slot 属性失败：恢复后的 Slot 不是 CanvasPanelSlot。");
			return false;
		}

		CanvasSlot->SetLayout(Snapshot.CanvasLayout);
		CanvasSlot->SetAutoSize(Snapshot.bCanvasAutoSize);
		CanvasSlot->SetZOrder(Snapshot.CanvasZOrder);
	}

	if (Snapshot.bHasSlotZOrder)
	{
		if (FIntProperty* ZOrderProperty = FindFProperty<FIntProperty>(Slot->GetClass(), TEXT("ZOrder")))
		{
			ZOrderProperty->SetPropertyValue_InContainer(Slot, Snapshot.SlotZOrder);
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
		{
			CanvasSlot->SetZOrder(Snapshot.SlotZOrder);
		}
	}

	Slot->SynchronizeProperties();
	return true;
}

static bool RestoreSlotSnapshot(UWidget* Widget, const FBlueprintHelperSlotSnapshot& Snapshot, FString& OutError)
{
	if (!Widget || !Snapshot.Parent)
	{
		OutError = TEXT("恢复 Widget 旧父节点失败：快照无效。");
		return false;
	}

	int32 CurrentChildIndex = INDEX_NONE;
	if (UPanelWidget* CurrentParent = UWidgetTree::FindWidgetParent(Widget, CurrentChildIndex))
	{
		CurrentParent->RemoveChild(Widget);
	}

	UPanelSlot* RestoredSlot = nullptr;
	if (Snapshot.ChildIndex >= 0 && Snapshot.ChildIndex <= Snapshot.Parent->GetChildrenCount())
	{
		RestoredSlot = Snapshot.Parent->InsertChildAt(Snapshot.ChildIndex, Widget);
	}
	else
	{
		RestoredSlot = Snapshot.Parent->AddChild(Widget);
	}

	if (!RestoredSlot)
	{
		OutError = TEXT("恢复 Widget 旧父节点失败：无法重新添加到旧父节点。");
		return false;
	}

	return ApplySlotSnapshotToSlot(RestoredSlot, Snapshot, true, OutError);
}

};

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

	// 遍历所。UClass 查找匹配
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Cls = *It;
		if (Cls->IsChildOf(UWidget::StaticClass()) && !Cls->HasAnyClassFlags(CLASS_Abstract))
		{
			// 匹配完整类名或不带 U 前缀的短名。
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
	const FString& WidgetName,
	bool bDryRun) const
{
	FBlueprintHelperWidgetMutationResult Result;
	Result.bDryRun = bDryRun;

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

	if (bDryRun)
	{
		Result.bSuccess = true;
		Result.AffectedWidget = WidgetName;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Add Widget")), WBP);
	Mutation.Modify(Tree);

	// 构造新 Widget
	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(Cls, NewName);
	if (!NewWidget)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法创建 Widget 类 '%s' 的实例。"), *WidgetClass);
		Mutation.Rollback();
		return Result;
	}

	NewWidget->SetFlags(RF_Transactional);

	if (ParentPanel)
	{
		UPanelSlot* Slot = ParentPanel->AddChild(NewWidget);
		if (!Slot)
		{
			Result.ErrorMessage = TEXT("AddChild 返回 null，可能面板已满或类型不兼容。");
			Mutation.Rollback();
			return Result;
		}
	}
	else
	{
		// 设置为根
		Tree->RootWidget = NewWidget;
	}

#if WITH_EDITORONLY_DATA
	if (!WBP->WidgetVariableNameToGuidMap.Contains(NewWidget->GetFName()))
	{
		WBP->OnVariableAdded(NewWidget->GetFName());
	}
#endif

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	Mutation.Commit();

	Result.bSuccess = true;
	Result.AffectedWidget = NewWidget->GetName();
	UE_LOG(LogWidgetService, Log, TEXT("设置 Widget '%s' 属性 '%s' = '%s'"),
		*Result.AffectedWidget, *WidgetClass, *ParentName);
	return Result;
}

FBlueprintHelperWidgetMutationResult FBlueprintHelperWidgetService::RemoveWidget(
	const FString& AssetPath,
	const FString& WidgetName,
	bool bDryRun) const
{
	FBlueprintHelperWidgetMutationResult Result;
	Result.bDryRun = bDryRun;

	UWidgetBlueprint* WBP = ResolveWidgetBlueprint(AssetPath, Result.ErrorMessage);
	if (!WBP) return Result;

	UWidget* Widget = FindWidgetByName(WBP, WidgetName, Result.ErrorMessage);
	if (!Widget) return Result;

	UWidgetTree* Tree = WBP->WidgetTree;

	// 不允许删除根 Widget（除非显式指定）
	if (Widget == Tree->RootWidget)
	{
		Result.ErrorMessage = TEXT("不允许通过 remove_widget 删除 RootWidget。需要显。root 删除策略。");
		return Result;
	}

	if (bDryRun)
	{
		Result.bSuccess = true;
		Result.AffectedWidget = WidgetName;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Remove Widget")), WBP);
	Mutation.Modify(Tree);
	const bool bRemoved = Tree->RemoveWidget(Widget);
	if (!bRemoved)
	{
		Result.ErrorMessage = FString::Printf(TEXT("无法从 WidgetTree 移除 '%s'。"), *WidgetName);
		Mutation.Rollback();
		return Result;
	}

#if WITH_EDITORONLY_DATA
	if (WBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
	{
		WBP->OnVariableRemoved(Widget->GetFName());
	}
#endif

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	Mutation.Commit();

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

	FBlueprintHelperWidgetServiceLocalUtils::FBlueprintHelperSlotSnapshot OldSlotSnapshot;
	if (!FBlueprintHelperWidgetServiceLocalUtils::CaptureSlotSnapshot(Widget, OldSlotSnapshot, Result.ErrorMessage))
	{
		return Result;
	}

	UWidgetTree* Tree = WBP->WidgetTree;
	if (!Tree)
	{
		Result.ErrorMessage = TEXT("WidgetBlueprint 没有 WidgetTree。");
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Move Widget")), WBP);
	Mutation.Modify(Tree);
	Mutation.Modify(OldSlotSnapshot.Parent);
	Mutation.Modify(NewParent);

	if (OldSlotSnapshot.Parent)
	{
		OldSlotSnapshot.Parent->RemoveChild(Widget);
	}

	// 添加到新父节点
	UPanelSlot* NewSlot = nullptr;
	if (InsertIndex >= 0 && InsertIndex <= NewParent->GetChildrenCount())
	{
		NewSlot = NewParent->InsertChildAt(InsertIndex, Widget);
	}
	else
	{
		NewSlot = NewParent->AddChild(Widget);
	}

	if (!NewSlot)
	{
		FString RestoreError;
		if (!FBlueprintHelperWidgetServiceLocalUtils::RestoreSlotSnapshot(Widget, OldSlotSnapshot, RestoreError))
		{
			Result.ErrorMessage = FString::Printf(TEXT("移动 Widget 失败，且恢复旧 Slot 失败: %s"), *RestoreError);
			Mutation.Rollback();
			return Result;
		}
		Mutation.Rollback();
		RestoreError.Reset();
		if (!FBlueprintHelperWidgetServiceLocalUtils::RestoreSlotSnapshot(Widget, OldSlotSnapshot, RestoreError))
		{
			Result.ErrorMessage = FString::Printf(TEXT("移动 Widget 失败，事务取消后恢复旧 Slot 失败: %s"), *RestoreError);
			Mutation.RestorePrimaryPackageDirtyState();
			return Result;
		}
		Mutation.RestorePrimaryPackageDirtyState();
		Result.ErrorMessage = TEXT("移动 Widget 失败，已恢复旧父节点、索引和 Slot 属性。");
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	Mutation.Commit();

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
	const FString& Value,
	bool bDryRun) const
{
	FBlueprintHelperWidgetMutationResult Result;
	Result.bDryRun = bDryRun;

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

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Prop))
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("属性 '%s' 不是编辑器中可安全写入的属性。Flags: %s"),
			*PropertyName,
			*FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(Prop->PropertyFlags));
		return Result;
	}

	if (bDryRun)
	{
		void* TempValue = FMemory_Alloca(Prop->GetSize());
		Prop->InitializeValue(TempValue);
		const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, TempValue, Widget, PPF_None);
		Prop->DestroyValue(TempValue);
		if (!ImportResult)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Cannot import widget property '%s' value '%s'."),
				*PropertyName,
				*Value);
			return Result;
		}

		Result.bSuccess = true;
		Result.AffectedWidget = WidgetName;
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Widget Property")), WBP);
	Mutation.Modify(Widget);

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Widget);
	FString OldValue;
	Prop->ExportText_Direct(OldValue, ValuePtr, nullptr, Widget, PPF_None);
	const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Widget, PPF_None);
	if (!ImportResult)
	{
		Prop->ImportText_Direct(*OldValue, ValuePtr, Widget, PPF_None);
		Mutation.Rollback();
		Result.ErrorMessage = FString::Printf(
			TEXT("无法。'%s' 设置。'%s'。值格式可能不正确。"), *PropertyName, *Value);
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WBP);
	Mutation.Commit();

	Result.bSuccess = true;
	Result.AffectedWidget = WidgetName;
	UE_LOG(LogWidgetService, Log, TEXT("设置 Widget '%s' 属性 '%s' = '%s'"),
		*WidgetName, *PropertyName, *Value);
	return Result;
}
