// BlueprintHelper Service Layer 。通用 UObject 属性反射服务实。

#include "Services/DataAssetObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Structure/BlueprintHelperServiceTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "Engine/AssetManager.h"
#include "FileHelpers.h"

// ═══════════════════════════════════════════════════════════
// 内部工具
// ═══════════════════════════════════════════════════════════

UObject* FBlueprintHelperPropertyReflectionService::ResolveAsset(
	const FString& AssetPath, FString& OutError) const
{
	if (AssetPath.IsEmpty())
	{
		OutError = TEXT("asset_path 不能为空。");
		return nullptr;
	}

	UObject* Obj = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Obj)
	{
		OutError = FString::Printf(TEXT("无法加载资产: %s"), *AssetPath);
		return nullptr;
	}
	return Obj;
}

FString FBlueprintHelperPropertyReflectionService::BuildFlagsSummary(uint64 PropertyFlags)
{
	return FBlueprintHelperEditablePropertyPolicy::BuildFlagsSummary(PropertyFlags);
}

// ═══════════════════════════════════════════════════════════
// GetObjectProperties
// ═══════════════════════════════════════════════════════════

FBlueprintHelperObjectPropertiesResult FBlueprintHelperPropertyReflectionService::GetObjectProperties(
	const FString& AssetPath) const
{
	FBlueprintHelperObjectPropertiesResult Result;

	UObject* Obj = ResolveAsset(AssetPath, Result.ErrorMessage);
	if (!Obj)
	{
		return Result;
	}

	Result.ClassName = Obj->GetClass()->GetName();
	Result.AssetPath = AssetPath;

	for (TFieldIterator<FProperty> It(Obj->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop) continue;

		// 跳过没有 Edit / BlueprintVisible 标志的属性
		if (!(Prop->PropertyFlags & (CPF_Edit | CPF_BlueprintVisible)))
		{
			continue;
		}

		FBlueprintHelperObjectPropertyInfo Info;
		Info.Name = Prop->GetName();
		Info.TypeName = Prop->GetCPPType();

		// 导出当前值
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);
		Prop->ExportTextItem_Direct(Info.Value, ValuePtr, nullptr, Obj, PPF_None);

		// 元数据：Category
		if (Prop->HasMetaData(TEXT("Category")))
		{
			Info.Category = Prop->GetMetaData(TEXT("Category"));
		}

		Info.Flags = BuildFlagsSummary(Prop->PropertyFlags);
		Result.Properties.Add(MoveTemp(Info));
	}

	Result.bSuccess = true;
	return Result;
}

// ═══════════════════════════════════════════════════════════
// SetObjectProperty
// ═══════════════════════════════════════════════════════════

FBlueprintHelperSetPropertyResult FBlueprintHelperPropertyReflectionService::SetObjectProperty(
	const FString& AssetPath,
	const FString& PropertyName,
	const FString& Value) const
{
	FBlueprintHelperSetPropertyResult Result;
	Result.PropertyName = PropertyName;

	FString LoadError;
	UObject* Obj = ResolveAsset(AssetPath, LoadError);
	if (!Obj)
	{
		Result.ErrorMessage = LoadError;
		return Result;
	}

	FProperty* Prop = Obj->GetClass()->FindPropertyByName(*PropertyName);
	if (!Prop)
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("在 %s 上未找到属性: %s"), *Obj->GetClass()->GetName(), *PropertyName);
		return Result;
	}

	if (!FBlueprintHelperEditablePropertyPolicy::AllowsWrite(Prop))
	{
		Result.ErrorMessage = FString::Printf(
			TEXT("属性 %s 不是编辑器中可安全写入的属性。Flags: %s"),
			*PropertyName,
			*BuildFlagsSummary(Prop->PropertyFlags));
		return Result;
	}

	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Obj);

	// 导出旧值
	Prop->ExportTextItem_Direct(Result.OldValue, ValuePtr, nullptr, Obj, PPF_None);

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Set Object Property")), Obj);

	// 导入新值
	const TCHAR* ImportResult = Prop->ImportText_Direct(*Value, ValuePtr, Obj, PPF_None);
	if (!ImportResult)
	{
		Prop->ImportText_Direct(*Result.OldValue, ValuePtr, Obj, PPF_None);
		Mutation.Rollback();
		Result.ErrorMessage = FString::Printf(
			TEXT("属性 %s 值导入失败，输入: \"%s\""), *PropertyName, *Value);
		return Result;
	}

	Obj->PostEditChange();
	Mutation.Commit();

	// 导出新值确认
	Prop->ExportTextItem_Direct(Result.NewValue, ValuePtr, nullptr, Obj, PPF_None);

	Result.bSuccess = true;
	return Result;
}
