// BlueprintHelper MaterialInstance read-context projection.

#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceReadContextProjection.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceParameterJsonUtils.h"
#include "Systems/ToolClusters/MaterialInstance/BlueprintHelperMaterialInstanceResolver.h"

class FBlueprintHelperMaterialInstanceReadContextProjectionLocalUtils
{
public:
	static const TCHAR* TypedArrayField(EBlueprintHelperMaterialInstanceParameterType Type)
	{
		switch (Type)
		{
		case EBlueprintHelperMaterialInstanceParameterType::Scalar:
			return TEXT("scalar_parameters");
		case EBlueprintHelperMaterialInstanceParameterType::Vector:
			return TEXT("vector_parameters");
		case EBlueprintHelperMaterialInstanceParameterType::Texture:
			return TEXT("texture_parameters");
		case EBlueprintHelperMaterialInstanceParameterType::StaticSwitch:
			return TEXT("static_switch_parameters");
		case EBlueprintHelperMaterialInstanceParameterType::Unknown:
		default:
			return TEXT("unknown_parameters");
		}
	}

	static const TCHAR* ParameterAssociationToString(EMaterialParameterAssociation Association)
	{
		switch (Association)
		{
		case EMaterialParameterAssociation::GlobalParameter:
			return TEXT("global_parameter");
		case EMaterialParameterAssociation::LayerParameter:
			return TEXT("layer_parameter");
		case EMaterialParameterAssociation::BlendParameter:
			return TEXT("blend_parameter");
		default:
			return TEXT("unknown");
		}
	}

	static void AddParameterToArray(
		TMap<FString, TArray<TSharedPtr<FJsonValue>>>& ArraysByField,
		const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter,
		const FString& AssetPath,
		TArray<TSharedPtr<FJsonValue>>& AllParameters)
	{
		const TSharedRef<FJsonObject> ParameterJson =
			FBlueprintHelperMaterialInstanceParameterJsonUtils::MakeParameterValueJson(Parameter, AssetPath);
		ParameterJson->SetStringField(TEXT("parameter_group"), Parameter.ParameterInfo.Association == EMaterialParameterAssociation::GlobalParameter
			? TEXT("global")
			: TEXT("layered"));
		ParameterJson->SetStringField(TEXT("parameter_association"), ParameterAssociationToString(Parameter.ParameterInfo.Association));
		ParameterJson->SetNumberField(TEXT("parameter_index"), Parameter.ParameterInfo.Index);
		ParameterJson->SetStringField(TEXT("parameter_id"), Parameter.ParameterId.ToString(EGuidFormats::DigitsWithHyphens));

		AllParameters.Add(MakeShared<FJsonValueObject>(ParameterJson));
		ArraysByField.FindOrAdd(TypedArrayField(Parameter.Type)).Add(MakeShared<FJsonValueObject>(ParameterJson));
	}
};

bool FBlueprintHelperMaterialInstanceReadContextProjection::BuildReadContextJson(
	const FString& AssetPath,
	TSharedPtr<FJsonObject>& OutJson,
	FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();

	const FBlueprintHelperMaterialInstanceAssetResolveResult AssetResult =
		FBlueprintHelperMaterialInstanceResolver::ResolveAsset(AssetPath);
	if (!AssetResult.bSuccess || !AssetResult.Instance)
	{
		OutError = AssetResult.ErrorMessage.IsEmpty() ? AssetResult.ErrorCode : AssetResult.ErrorMessage;
		return false;
	}

	TArray<FBlueprintHelperMaterialInstanceParameterSchemaEntry> Schema;
	FString ErrorCode;
	FString ErrorMessage;
	if (!FBlueprintHelperMaterialInstanceResolver::CollectParameterSchema(
		AssetResult.Instance,
		Schema,
		ErrorCode,
		ErrorMessage))
	{
		OutError = ErrorMessage.IsEmpty() ? ErrorCode : ErrorMessage;
		return false;
	}

	TMap<FString, TArray<TSharedPtr<FJsonValue>>> ArraysByField;
	ArraysByField.Add(TEXT("scalar_parameters"), {});
	ArraysByField.Add(TEXT("vector_parameters"), {});
	ArraysByField.Add(TEXT("texture_parameters"), {});
	ArraysByField.Add(TEXT("static_switch_parameters"), {});
	ArraysByField.Add(TEXT("unknown_parameters"), {});

	TArray<TSharedPtr<FJsonValue>> AllParameters;
	for (const FBlueprintHelperMaterialInstanceParameterSchemaEntry& Parameter : Schema)
	{
		FBlueprintHelperMaterialInstanceReadContextProjectionLocalUtils::AddParameterToArray(
			ArraysByField,
			Parameter,
			AssetResult.InputAssetPath.IsEmpty() ? AssetPath : AssetResult.InputAssetPath,
			AllParameters);
	}

	OutJson = MakeShared<FJsonObject>();
	OutJson->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialInstanceReadContext.v1"));
	OutJson->SetStringField(TEXT("read_type"), TEXT("material_instance_context"));
	OutJson->SetStringField(TEXT("target_type"), TEXT("material_instance"));
	OutJson->SetStringField(TEXT("format"), TEXT("schema_json"));
	OutJson->SetStringField(TEXT("asset_path"), AssetResult.InputAssetPath.IsEmpty() ? AssetPath : AssetResult.InputAssetPath);
	OutJson->SetStringField(TEXT("object_path"), AssetResult.ObjectPath);
	OutJson->SetStringField(TEXT("parent_material"), AssetResult.Parent ? AssetResult.Parent->GetPathName() : FString());
	OutJson->SetBoolField(TEXT("has_parent"), AssetResult.bHasParent);
	OutJson->SetArrayField(TEXT("parameters"), MoveTemp(AllParameters));
	for (const TPair<FString, TArray<TSharedPtr<FJsonValue>>>& Entry : ArraysByField)
	{
		OutJson->SetArrayField(Entry.Key, Entry.Value);
	}
	return true;
}
