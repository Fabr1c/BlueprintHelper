// BlueprintHelper MaterialInstance mutation adapter.

#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceMutationAdapter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialInstanceMutationLocalUtils
{
public:
	static FBlueprintHelperToolError MakeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.Field = Field;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		return Error;
	}

	static FBlueprintHelperToolResultBase MakeFailure(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			FBlueprintHelperMaterialInstanceMutationAdapter::OperationMaterialInstanceEdit,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeError(Code, Stage, Message, Field));
	}

	static FString NormalizeAssetObjectPath(const FString& AssetPath)
	{
		return FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(AssetPath);
	}

	static bool TrySplitAssetPath(
		const FString& AssetPath,
		FString& OutPackagePath,
		FString& OutAssetName)
	{
		FString Normalized = AssetPath;
		Normalized.TrimStartAndEndInline();
		if (Normalized.IsEmpty())
		{
			return false;
		}

		FString ObjectPath = Normalized;
		const int32 DotIndex = ObjectPath.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (DotIndex != INDEX_NONE)
		{
			ObjectPath = ObjectPath.Left(DotIndex);
		}

		int32 SlashIndex = INDEX_NONE;
		if (!ObjectPath.FindLastChar(TEXT('/'), SlashIndex) || SlashIndex <= 0 || SlashIndex >= ObjectPath.Len() - 1)
		{
			return false;
		}

		OutPackagePath = ObjectPath.Left(SlashIndex);
		OutAssetName = ObjectPath.Mid(SlashIndex + 1);
		return FPackageName::IsValidLongPackageName(OutPackagePath) && !OutAssetName.IsEmpty();
	}

	static UMaterialInterface* LoadParentMaterial(const FString& ParentPath)
	{
		if (ParentPath.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UMaterialInterface>(
			nullptr,
			*FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(ParentPath));
	}

	static UMaterialInstanceConstant* ResolveInstanceForOp(
		const FString& AssetPath,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		const FBlueprintHelperMaterialInstanceAssetResolveResult AssetResult =
			FBlueprintHelperMaterialInstanceResolver::ResolveAsset(AssetPath);
		if (!AssetResult.bSuccess)
		{
			OutErrorCode = AssetResult.ErrorCode;
			OutErrorMessage = AssetResult.ErrorMessage;
			return nullptr;
		}
		return AssetResult.Instance;
	}

	static TSharedRef<FJsonObject> MakeParameterValueJson(
		const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("parameter_name"), Parameter.ParameterInfo.Name.ToString());
		Json->SetStringField(TEXT("parameter_type"), BlueprintHelperMaterialInstanceParameterTypeToString(Parameter.Type));
		Json->SetBoolField(TEXT("has_override"), Parameter.bHasOverride);
		Json->SetStringField(TEXT("source"), BlueprintHelperMaterialInstanceParameterSourceToString(Parameter.Source));
		Json->SetStringField(TEXT("effective_value"), Parameter.EffectiveValue.ToDebugString(Parameter.Type));
		Json->SetStringField(TEXT("override_value"), Parameter.OverrideValue.ToDebugString(Parameter.Type));
		return Json;
	}

	static TSharedRef<FJsonObject> MakeOpResultJson(
		const FString& Op,
		const FString& Status)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("op"), Op);
		Json->SetStringField(TEXT("status"), Status);
		return Json;
	}

	static bool TryReadOperation(
		const TSharedPtr<FJsonObject>& OpObject,
		FString& OutOp,
		FString& OutError)
	{
		if (!OpObject.IsValid())
		{
			OutError = TEXT("MaterialInstance operation must be an object.");
			return false;
		}
		if (!OpObject->TryGetStringField(TEXT("op"), OutOp) || OutOp.IsEmpty())
		{
			OutError = TEXT("MaterialInstance operation requires op.");
			return false;
		}
		return true;
	}

	static bool TryRequireParameterName(
		const TSharedPtr<FJsonObject>& OpObject,
		FName& OutParameterName,
		FString& OutError)
	{
		FString ParameterName;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetStringField(TEXT("parameter_name"), ParameterName) ||
			ParameterName.IsEmpty())
		{
			OutError = TEXT("MaterialInstance parameter operation requires parameter_name.");
			return false;
		}
		OutParameterName = FName(*ParameterName);
		return true;
	}

	static EBlueprintHelperMaterialInstanceParameterType ReadRequestedType(
		const TSharedPtr<FJsonObject>& OpObject)
	{
		FString TypeString;
		EBlueprintHelperMaterialInstanceParameterType Type =
			EBlueprintHelperMaterialInstanceParameterType::Unknown;
		if (OpObject.IsValid() &&
			OpObject->TryGetStringField(TEXT("parameter_type"), TypeString))
		{
			FBlueprintHelperMaterialInstanceResolver::TryParseParameterType(TypeString, Type);
		}
		return Type;
	}

	static bool RemoveScalarOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo)
	{
		if (!Instance)
		{
			return false;
		}
		const int32 Removed = Instance->ScalarParameterValues.RemoveAll(
			[&ParameterInfo](const FScalarParameterValue& Value)
			{
				return Value.ParameterInfo == ParameterInfo;
			});
		return Removed > 0;
	}

	static bool RemoveVectorOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo)
	{
		if (!Instance)
		{
			return false;
		}
		const int32 Removed = Instance->VectorParameterValues.RemoveAll(
			[&ParameterInfo](const FVectorParameterValue& Value)
			{
				return Value.ParameterInfo == ParameterInfo;
			});
		return Removed > 0;
	}

	static bool RemoveTextureOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo)
	{
		if (!Instance)
		{
			return false;
		}
		const int32 Removed = Instance->TextureParameterValues.RemoveAll(
			[&ParameterInfo](const FTextureParameterValue& Value)
			{
				return Value.ParameterInfo == ParameterInfo;
			});
		return Removed > 0;
	}

	static bool RemoveStaticSwitchOverride(
		UMaterialInstanceConstant* Instance,
		const FMaterialParameterInfo& ParameterInfo)
	{
		if (!Instance)
		{
			return false;
		}
		FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
		const int32 Removed = StaticParameters.StaticSwitchParameters.RemoveAll(
			[&ParameterInfo](const FStaticSwitchParameter& Value)
			{
				return Value.ParameterInfo == ParameterInfo;
			});
		if (Removed > 0)
		{
			Instance->UpdateStaticPermutation(StaticParameters);
		}
		return Removed > 0;
	}

	static bool ClearSingleOverride(
		UMaterialInstanceConstant* Instance,
		const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter)
	{
		bool bRemoved = false;
		switch (Parameter.Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
			bRemoved = RemoveScalarOverride(Instance, Parameter.ParameterInfo);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
			bRemoved = RemoveVectorOverride(Instance, Parameter.ParameterInfo);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
			bRemoved = RemoveTextureOverride(Instance, Parameter.ParameterInfo);
			break;
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
			bRemoved = RemoveStaticSwitchOverride(Instance, Parameter.ParameterInfo);
			break;
		default:
			break;
		}
		if (bRemoved)
		{
			UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
			Instance->MarkPackageDirty();
		}
		return bRemoved;
	}

	static bool TryExecuteCreate(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedRef<FJsonObject>& OutOpJson,
		bool& bOutModified,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		FString PackagePath;
		FString AssetName;
		if (!TrySplitAssetPath(AssetPath, PackagePath, AssetName))
		{
			OutErrorCode = TEXT("material_instance_invalid_asset_path");
			OutErrorMessage = TEXT("MaterialInstance create requires a valid /Game asset path.");
			return false;
		}

		const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		if (FindObject<UObject>(nullptr, *ObjectPath) || LoadObject<UObject>(nullptr, *ObjectPath))
		{
			OutErrorCode = TEXT("material_instance_asset_already_exists");
			OutErrorMessage = FString::Printf(TEXT("MaterialInstance asset already exists: %s"), *AssetPath);
			return false;
		}

		FString ParentPath;
		OpObject->TryGetStringField(TEXT("parent_material"), ParentPath);
		UMaterialInterface* Parent = ParentPath.IsEmpty() ? nullptr : LoadParentMaterial(ParentPath);
		if (!ParentPath.IsEmpty() && !Parent)
		{
			OutErrorCode = TEXT("material_instance_parent_not_found");
			OutErrorMessage = FString::Printf(TEXT("Parent material was not found: %s"), *ParentPath);
			return false;
		}

		OutOpJson->SetStringField(TEXT("asset_path"), AssetPath);
		if (!ParentPath.IsEmpty())
		{
			OutOpJson->SetStringField(TEXT("parent_material"), ParentPath);
		}

		if (bDryRun)
		{
			OutOpJson->SetStringField(TEXT("status"), TEXT("preview"));
			return true;
		}

		UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName));
		if (!Package)
		{
			OutErrorCode = TEXT("material_instance_create_package_failed");
			OutErrorMessage = TEXT("Failed to create MaterialInstance package.");
			return false;
		}

		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Package,
			UMaterialInstanceConstant::StaticClass(),
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
		if (!Instance)
		{
			OutErrorCode = TEXT("material_instance_create_failed");
			OutErrorMessage = TEXT("Failed to create MaterialInstanceConstant object.");
			return false;
		}

		if (Parent)
		{
			Instance->SetParentEditorOnly(Parent);
			UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		}

		FAssetRegistryModule::AssetCreated(Instance);
		Package->MarkPackageDirty();
		bOutModified = true;
		OutOpJson->SetStringField(TEXT("status"), TEXT("applied"));
		OutOpJson->SetStringField(TEXT("object_path"), Instance->GetPathName());
		return true;
	}

	static bool TryResolveInstanceAndParameter(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		UMaterialInstanceConstant*& OutInstance,
		FBlueprintHelperMaterialInstanceParameterResolveResult& OutParameter,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		OutInstance = ResolveInstanceForOp(AssetPath, OutErrorCode, OutErrorMessage);
		if (!OutInstance)
		{
			return false;
		}

		FName ParameterName;
		if (!TryRequireParameterName(OpObject, ParameterName, OutErrorMessage))
		{
			OutErrorCode = TEXT("material_instance_parameter_name_required");
			return false;
		}

		OutParameter = FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
			OutInstance,
			ParameterName,
			ReadRequestedType(OpObject));
		if (!OutParameter.bSuccess)
		{
			OutErrorCode = OutParameter.ErrorCode;
			OutErrorMessage = OutParameter.ErrorMessage;
			return false;
		}
		return true;
	}

	static bool TryExecuteSetParent(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedRef<FJsonObject>& OutOpJson,
		bool& bOutModified,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		FString ParentPath;
		if (!OpObject->TryGetStringField(TEXT("parent_material"), ParentPath) || ParentPath.IsEmpty())
		{
			OutErrorCode = TEXT("material_instance_parent_required");
			OutErrorMessage = TEXT("set_parent requires parent_material.");
			return false;
		}

		UMaterialInterface* Parent = LoadParentMaterial(ParentPath);
		if (!Parent)
		{
			OutErrorCode = TEXT("material_instance_parent_not_found");
			OutErrorMessage = FString::Printf(TEXT("Parent material was not found: %s"), *ParentPath);
			return false;
		}

		FString ResolveErrorCode;
		FString ResolveErrorMessage;
		UMaterialInstanceConstant* Instance = ResolveInstanceForOp(AssetPath, ResolveErrorCode, ResolveErrorMessage);
		if (!Instance)
		{
			OutErrorCode = ResolveErrorCode;
			OutErrorMessage = ResolveErrorMessage;
			return false;
		}

		OutOpJson->SetStringField(TEXT("parent_material"), ParentPath);
		if (bDryRun)
		{
			OutOpJson->SetStringField(TEXT("status"), TEXT("preview"));
			return true;
		}

		Instance->Modify();
		Instance->SetParentEditorOnly(Parent);
		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		Instance->MarkPackageDirty();
		bOutModified = true;
		OutOpJson->SetStringField(TEXT("status"), TEXT("applied"));
		return true;
	}

	static bool TryExecuteSetOverride(
		const FString& AssetPath,
		const FString& Op,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedRef<FJsonObject>& OutOpJson,
		bool& bOutModified,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		UMaterialInstanceConstant* Instance = nullptr;
		FBlueprintHelperMaterialInstanceParameterResolveResult Parameter;
		if (!TryResolveInstanceAndParameter(AssetPath, OpObject, Instance, Parameter, OutErrorCode, OutErrorMessage))
		{
			return false;
		}

		OutOpJson->SetObjectField(TEXT("before"), MakeParameterValueJson(Parameter.Parameter));
		if (bDryRun)
		{
			OutOpJson->SetStringField(TEXT("status"), TEXT("preview"));
			return true;
		}

		Instance->Modify();
		if (Op == TEXT("set_scalar_override"))
		{
			double Value = 0.0;
			if (!OpObject->TryGetNumberField(TEXT("value"), Value))
			{
				OutErrorCode = TEXT("material_instance_scalar_value_required");
				OutErrorMessage = TEXT("set_scalar_override requires numeric value.");
				return false;
			}
			Instance->SetScalarParameterValueEditorOnly(Parameter.Parameter.ParameterInfo, static_cast<float>(Value));
		}
		else if (Op == TEXT("set_vector_override"))
		{
			const TSharedPtr<FJsonObject>* ValueObject = nullptr;
			if (!OpObject->TryGetObjectField(TEXT("value"), ValueObject) ||
				!ValueObject || !ValueObject->IsValid())
			{
				OutErrorCode = TEXT("material_instance_vector_value_required");
				OutErrorMessage = TEXT("set_vector_override requires value object.");
				return false;
			}
			double R = 0.0;
			double G = 0.0;
			double B = 0.0;
			double A = 1.0;
			(*ValueObject)->TryGetNumberField(TEXT("r"), R);
			(*ValueObject)->TryGetNumberField(TEXT("g"), G);
			(*ValueObject)->TryGetNumberField(TEXT("b"), B);
			(*ValueObject)->TryGetNumberField(TEXT("a"), A);
			Instance->SetVectorParameterValueEditorOnly(
				Parameter.Parameter.ParameterInfo,
				FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A)));
		}
		else if (Op == TEXT("set_texture_override"))
		{
			FString TexturePath;
			if (!OpObject->TryGetStringField(TEXT("texture_asset"), TexturePath) || TexturePath.IsEmpty())
			{
				OutErrorCode = TEXT("material_instance_texture_value_required");
				OutErrorMessage = TEXT("set_texture_override requires texture_asset.");
				return false;
			}
			UTexture* Texture = LoadObject<UTexture>(
				nullptr,
				*FBlueprintHelperMaterialInstanceResolver::NormalizeMaterialInstanceObjectPath(TexturePath));
			if (!Texture)
			{
				OutErrorCode = TEXT("material_instance_texture_not_found");
				OutErrorMessage = FString::Printf(TEXT("Texture asset was not found: %s"), *TexturePath);
				return false;
			}
			Instance->SetTextureParameterValueEditorOnly(Parameter.Parameter.ParameterInfo, Texture);
		}
		else if (Op == TEXT("set_static_switch_override"))
		{
			bool bValue = false;
			if (!OpObject->TryGetBoolField(TEXT("value"), bValue))
			{
				OutErrorCode = TEXT("material_instance_static_switch_value_required");
				OutErrorMessage = TEXT("set_static_switch_override requires boolean value.");
				return false;
			}
			Instance->SetStaticSwitchParameterValueEditorOnly(Parameter.Parameter.ParameterInfo, bValue);
			Instance->UpdateStaticPermutation();
		}
		else
		{
			OutErrorCode = TEXT("material_instance_unsupported_override_op");
			OutErrorMessage = TEXT("Unsupported MaterialInstance override op.");
			return false;
		}

		UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
		Instance->MarkPackageDirty();
		bOutModified = true;
		const FBlueprintHelperMaterialInstanceParameterResolveResult After =
			FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
				Instance,
				Parameter.Parameter.ParameterInfo.Name,
				Parameter.Parameter.Type);
		if (After.bSuccess)
		{
			OutOpJson->SetObjectField(TEXT("after"), MakeParameterValueJson(After.Parameter));
		}
		OutOpJson->SetStringField(TEXT("status"), TEXT("applied"));
		return true;
	}

	static bool TryExecuteClearOverride(
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedRef<FJsonObject>& OutOpJson,
		bool& bOutModified,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		UMaterialInstanceConstant* Instance = nullptr;
		FBlueprintHelperMaterialInstanceParameterResolveResult Parameter;
		if (!TryResolveInstanceAndParameter(AssetPath, OpObject, Instance, Parameter, OutErrorCode, OutErrorMessage))
		{
			return false;
		}

		OutOpJson->SetObjectField(TEXT("before"), MakeParameterValueJson(Parameter.Parameter));
		if (bDryRun)
		{
			OutOpJson->SetStringField(TEXT("status"), TEXT("preview"));
			return true;
		}

		Instance->Modify();
		const bool bRemoved = ClearSingleOverride(Instance, Parameter.Parameter);
		const FBlueprintHelperMaterialInstanceParameterResolveResult After =
			FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
				Instance,
				Parameter.Parameter.ParameterInfo.Name,
				Parameter.Parameter.Type);
		if (After.bSuccess)
		{
			OutOpJson->SetObjectField(TEXT("after"), MakeParameterValueJson(After.Parameter));
		}
		OutOpJson->SetBoolField(TEXT("override_removed"), bRemoved);
		OutOpJson->SetStringField(TEXT("status"), TEXT("applied"));
		bOutModified = bOutModified || bRemoved;
		return true;
	}

	static bool TryExecuteRead(
		const FString& AssetPath,
		const FString& Op,
		const TSharedPtr<FJsonObject>& OpObject,
		TSharedRef<FJsonObject>& OutOpJson,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		FString ResolveErrorCode;
		FString ResolveErrorMessage;
		UMaterialInstanceConstant* Instance = ResolveInstanceForOp(AssetPath, ResolveErrorCode, ResolveErrorMessage);
		if (!Instance)
		{
			OutErrorCode = ResolveErrorCode;
			OutErrorMessage = ResolveErrorMessage;
			return false;
		}

		if (Op == TEXT("read_parameter_schema"))
		{
			FString ParameterName;
			OpObject->TryGetStringField(TEXT("parameter_name"), ParameterName);
			if (!ParameterName.IsEmpty())
			{
				const FBlueprintHelperMaterialInstanceParameterResolveResult Parameter =
					FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
						Instance,
						FName(*ParameterName),
						ReadRequestedType(OpObject));
				if (!Parameter.bSuccess)
				{
					OutErrorCode = Parameter.ErrorCode;
					OutErrorMessage = Parameter.ErrorMessage;
					return false;
				}
				OutOpJson->SetObjectField(TEXT("parameter"), MakeParameterValueJson(Parameter.Parameter));
				OutOpJson->SetStringField(TEXT("status"), TEXT("read"));
				return true;
			}

			TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
			if (!FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
				Instance,
				Schema,
				OutErrorCode,
				OutErrorMessage))
			{
				return false;
			}
			TArray<TSharedPtr<FJsonValue>> Values;
			for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Entry : Schema)
			{
				Values.Add(MakeShared<FJsonValueObject>(MakeParameterValueJson(Entry)));
			}
			OutOpJson->SetArrayField(TEXT("parameters"), MoveTemp(Values));
			OutOpJson->SetStringField(TEXT("status"), TEXT("read"));
			return true;
		}

		if (Op == TEXT("read_effective_value"))
		{
			FName ParameterName;
			if (!TryRequireParameterName(OpObject, ParameterName, OutErrorMessage))
			{
				OutErrorCode = TEXT("material_instance_parameter_name_required");
				return false;
			}
			const FBlueprintHelperMaterialInstanceParameterResolveResult Parameter =
				FBlueprintHelperMaterialInstanceResolver::ResolveParameter(
					Instance,
					ParameterName,
					ReadRequestedType(OpObject));
			if (!Parameter.bSuccess)
			{
				OutErrorCode = Parameter.ErrorCode;
				OutErrorMessage = Parameter.ErrorMessage;
				return false;
			}
			OutOpJson->SetObjectField(TEXT("parameter"), MakeParameterValueJson(Parameter.Parameter));
			OutOpJson->SetStringField(TEXT("status"), TEXT("read"));
			return true;
		}

		OutErrorCode = TEXT("material_instance_unsupported_read_op");
		OutErrorMessage = TEXT("Unsupported MaterialInstance read op.");
		return false;
	}
};

FBlueprintHelperToolResultBase FBlueprintHelperMaterialInstanceMutationAdapter::ExecutePayload(
	const TSharedPtr<FJsonObject>& Payload)
{
	if (!Payload.IsValid())
	{
		return FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeFailure(
			TEXT("material_instance_payload_invalid"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialInstance runtime payload is required."),
			TEXT("payload"));
	}

	FString AssetPath;
	if (!Payload->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeFailure(
			TEXT("material_instance_asset_path_required"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialInstance runtime payload requires asset_path."),
			TEXT("payload.asset_path"));
	}

	bool bDryRun = false;
	Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);

	const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
	if (!Payload->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray || OpsArray->Num() == 0)
	{
		return FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeFailure(
			TEXT("material_instance_ops_required"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("MaterialInstance runtime payload requires non-empty ops array."),
			TEXT("payload.ops"));
	}

	bool bModified = false;
	TArray<TSharedPtr<FJsonValue>> OperationResults;
	for (int32 Index = 0; Index < OpsArray->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> OpObject =
			(*OpsArray)[Index].IsValid() ? (*OpsArray)[Index]->AsObject() : nullptr;

		FString Op;
		FString ErrorMessage;
		if (!FBlueprintHelperMaterialInstanceMutationLocalUtils::TryReadOperation(OpObject, Op, ErrorMessage))
		{
			return FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeFailure(
				TEXT("material_instance_op_invalid"),
				EBlueprintHelperToolStage::ParseInput,
				ErrorMessage,
				FString::Printf(TEXT("payload.ops[%d]"), Index));
		}

		TSharedRef<FJsonObject> OpJson =
			FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeOpResultJson(Op, TEXT("pending"));
		FString ErrorCode;
		bool bSuccess = false;
		if (Op == TEXT("create_material_instance"))
		{
			bSuccess = FBlueprintHelperMaterialInstanceMutationLocalUtils::TryExecuteCreate(
				AssetPath,
				OpObject,
				bDryRun,
				OpJson,
				bModified,
				ErrorCode,
				ErrorMessage);
		}
		else if (Op == TEXT("set_parent"))
		{
			bSuccess = FBlueprintHelperMaterialInstanceMutationLocalUtils::TryExecuteSetParent(
				AssetPath,
				OpObject,
				bDryRun,
				OpJson,
				bModified,
				ErrorCode,
				ErrorMessage);
		}
		else if (Op == TEXT("set_scalar_override") ||
			Op == TEXT("set_vector_override") ||
			Op == TEXT("set_texture_override") ||
			Op == TEXT("set_static_switch_override"))
		{
			bSuccess = FBlueprintHelperMaterialInstanceMutationLocalUtils::TryExecuteSetOverride(
				AssetPath,
				Op,
				OpObject,
				bDryRun,
				OpJson,
				bModified,
				ErrorCode,
				ErrorMessage);
		}
		else if (Op == TEXT("clear_override"))
		{
			bSuccess = FBlueprintHelperMaterialInstanceMutationLocalUtils::TryExecuteClearOverride(
				AssetPath,
				OpObject,
				bDryRun,
				OpJson,
				bModified,
				ErrorCode,
				ErrorMessage);
		}
		else if (Op == TEXT("read_parameter_schema") || Op == TEXT("read_effective_value"))
		{
			bSuccess = FBlueprintHelperMaterialInstanceMutationLocalUtils::TryExecuteRead(
				AssetPath,
				Op,
				OpObject,
				OpJson,
				ErrorCode,
				ErrorMessage);
		}
		else
		{
			ErrorCode = TEXT("material_instance_unsupported_op");
			ErrorMessage = FString::Printf(TEXT("Unsupported MaterialInstance op: %s"), *Op);
		}

		if (!bSuccess)
		{
			return FBlueprintHelperMaterialInstanceMutationLocalUtils::MakeFailure(
				ErrorCode.IsEmpty() ? TEXT("material_instance_op_failed") : ErrorCode,
				EBlueprintHelperToolStage::Execute,
				ErrorMessage,
				FString::Printf(TEXT("payload.ops[%d]"), Index));
		}

		OperationResults.Add(MakeShared<FJsonValueObject>(OpJson));
	}

	const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
	FBlueprintHelperToolResultBase Result = bDryRun
		? FBlueprintHelperToolResultBuilder::DryRun(OperationMaterialInstanceEdit, TraceId)
		: (bModified
			? FBlueprintHelperToolResultBuilder::Applied(OperationMaterialInstanceEdit, TraceId)
			: FBlueprintHelperToolResultBuilder::Completed(OperationMaterialInstanceEdit, TraceId));
	Result.bModified = !bDryRun && bModified;
	Result.Data = MakeShared<FJsonObject>();
	Result.Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialInstanceMutationResult.v1"));
	Result.Data->SetStringField(TEXT("asset_path"), AssetPath);
	Result.Data->SetBoolField(TEXT("dry_run"), bDryRun);
	Result.Data->SetBoolField(TEXT("modified"), Result.bModified);
	Result.Data->SetArrayField(TEXT("operations"), MoveTemp(OperationResults));
	return Result;
}
