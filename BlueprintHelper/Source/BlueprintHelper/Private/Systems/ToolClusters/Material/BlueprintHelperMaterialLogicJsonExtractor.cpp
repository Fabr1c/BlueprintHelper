// BlueprintHelper Service Layer - Material LogicJson read extraction.

#include "Systems/ToolClusters/Material/BlueprintHelperMaterialLogicJsonExtractor.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "SceneTypes.h"
#include "Shared/BlueprintHelperVersionCompat.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

bool FBlueprintHelperMaterialLogicJsonExtractor::BuildLogicJson(
	const FString& AssetPath,
	TSharedPtr<FJsonObject>& OutLogicJson,
	FString& OutError) const
{
	OutLogicJson.Reset();
	OutError.Reset();

	UMaterial* Material = LoadMaterial(AssetPath, OutError);
	if (!Material)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> Nodes;
	TArray<TSharedPtr<FJsonValue>> Links;
	TArray<TSharedPtr<FJsonValue>> Parameters;
	TArray<TSharedPtr<FJsonValue>> Outputs;
	TArray<TSharedPtr<FJsonValue>> Diagnostics;
	TMap<const UMaterialExpression*, FString> Refs;

	AddExpressionNodes(Material, Nodes, Parameters, Refs, Diagnostics);
	AddExpressionLinks(Material, Refs, Links);
	AddMaterialOutputs(Material, Refs, Outputs, Links);

	TSharedPtr<FJsonObject> Asset = MakeShared<FJsonObject>();
	Asset->SetStringField(TEXT("asset_path"), AssetPath);
	Asset->SetStringField(TEXT("object_path"), Material->GetPathName());
	Asset->SetStringField(TEXT("asset_class"), Material->GetClass()->GetName());

	TSharedPtr<FJsonObject> Logic = MakeShared<FJsonObject>();
	Logic->SetStringField(TEXT("graph"), TEXT("MaterialGraph"));
	Logic->SetStringField(TEXT("graph_kind"), TEXT("material_graph"));
	Logic->SetArrayField(TEXT("nodes"), Nodes);
	Logic->SetArrayField(TEXT("links"), Links);
	Logic->SetArrayField(TEXT("material_outputs"), Outputs);

	TSharedPtr<FJsonObject> MaterialJson = MakeShared<FJsonObject>();
	MaterialJson->SetStringField(TEXT("domain"), TEXT("material"));
	MaterialJson->SetArrayField(TEXT("parameters"), Parameters);
	MaterialJson->SetArrayField(TEXT("outputs"), Outputs);

	TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("nodes"), Nodes.Num());
	Stats->SetNumberField(TEXT("links"), Links.Num());
	Stats->SetNumberField(TEXT("parameters"), Parameters.Num());
	Stats->SetNumberField(TEXT("outputs"), Outputs.Num());

	OutLogicJson = MakeShared<FJsonObject>();
	OutLogicJson->SetStringField(TEXT("schema"), TEXT("LogicJson.v1"));
	OutLogicJson->SetStringField(TEXT("scope"), TEXT("material_graph"));
	OutLogicJson->SetObjectField(TEXT("asset"), Asset);
	OutLogicJson->SetObjectField(TEXT("logic"), Logic);
	OutLogicJson->SetObjectField(TEXT("material"), MaterialJson);
	OutLogicJson->SetObjectField(TEXT("stats"), Stats);
	OutLogicJson->SetArrayField(TEXT("diagnostics"), Diagnostics);
	return true;
}

UMaterial* FBlueprintHelperMaterialLogicJsonExtractor::LoadMaterial(
	const FString& AssetPath,
	FString& OutError) const
{
	const FString ObjectPath = BuildObjectPath(AssetPath);
	if (ObjectPath.IsEmpty())
	{
		OutError = TEXT("material_asset_path_empty");
		return nullptr;
	}

	UObject* LoadedObject = LoadObject<UObject>(nullptr, *ObjectPath);
	if (!LoadedObject)
	{
		OutError = FString::Printf(TEXT("material_asset_not_found:%s"), *ObjectPath);
		return nullptr;
	}

	UMaterial* Material = Cast<UMaterial>(LoadedObject);
	if (!Material)
	{
		OutError = FString::Printf(
			TEXT("material_asset_type_mismatch:%s:%s"),
			*ObjectPath,
			*LoadedObject->GetClass()->GetName());
	}
	return Material;
}

FString FBlueprintHelperMaterialLogicJsonExtractor::BuildObjectPath(const FString& AssetPath) const
{
	const FString Trimmed = AssetPath.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}
	if (Trimmed.Contains(TEXT(".")))
	{
		return Trimmed;
	}

	const FString ShortName = FPackageName::GetShortName(Trimmed);
	return FString::Printf(TEXT("%s.%s"), *Trimmed, *ShortName);
}

FString FBlueprintHelperMaterialLogicJsonExtractor::BuildExpressionRef(
	const UMaterialExpression* Expression,
	bool bForcePathRef) const
{
	if (!Expression)
	{
		return FString();
	}
	if (!bForcePathRef && Expression->MaterialExpressionGuid.IsValid())
	{
		return FString::Printf(
			TEXT("mexpr:v1:%s"),
			*Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
	}
	return FString::Printf(TEXT("mexpr:v1:path:%s"), *Expression->GetPathName());
}

FString FBlueprintHelperMaterialLogicJsonExtractor::BuildExpressionNodeKey(
	const UMaterialExpression* Expression,
	const FString& ExpressionRef) const
{
	const FString NodeKey = ReadOwnershipMetadataValue(Expression, TEXT("BlueprintHelper.NodeKey"));
	return NodeKey.IsEmpty() ? ExpressionRef : NodeKey;
}

FString FBlueprintHelperMaterialLogicJsonExtractor::BuildOutputPinName(
	const UMaterialExpression* Expression,
	int32 OutputIndex) const
{
	if (!Expression)
	{
		return FString();
	}

	UMaterialExpression* MutableExpression = const_cast<UMaterialExpression*>(Expression);
	TArray<FExpressionOutput>& Outputs = MutableExpression->GetOutputs();
	if (Outputs.IsValidIndex(OutputIndex) && !Outputs[OutputIndex].OutputName.IsNone())
	{
		return Outputs[OutputIndex].OutputName.ToString();
	}
	return FString::Printf(TEXT("Output%d"), OutputIndex);
}

void FBlueprintHelperMaterialLogicJsonExtractor::AddExpressionNodes(
	UMaterial* Material,
	TArray<TSharedPtr<FJsonValue>>& OutNodes,
	TArray<TSharedPtr<FJsonValue>>& OutParameters,
	TMap<const UMaterialExpression*, FString>& OutRefs,
	TArray<TSharedPtr<FJsonValue>>& OutDiagnostics) const
{
	if (!Material)
	{
		return;
	}

	TMap<FGuid, int32> GuidCounts;
	for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
	{
		const UMaterialExpression* Expression = ExpressionPtr.Get();
		if (Expression && Expression->MaterialExpressionGuid.IsValid())
		{
			++GuidCounts.FindOrAdd(Expression->MaterialExpressionGuid);
		}
	}

	for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
	{
		UMaterialExpression* Expression = ExpressionPtr.Get();
		if (!Expression)
		{
			continue;
		}

		const int32* GuidCount = Expression->MaterialExpressionGuid.IsValid()
			? GuidCounts.Find(Expression->MaterialExpressionGuid)
			: nullptr;
		const bool bDuplicateGuid = GuidCount && *GuidCount > 1;
		const FString ExpressionRef = BuildExpressionRef(Expression, bDuplicateGuid);
		OutRefs.Add(Expression, ExpressionRef);
		if (!Expression->MaterialExpressionGuid.IsValid())
		{
			AddDiagnostic(
				OutDiagnostics,
				TEXT("material_expression_anchor_missing"),
				TEXT("warning"),
				TEXT("Material expression does not have a valid MaterialExpressionGuid; path fallback anchor was used."),
				Expression,
				ExpressionRef);
		}
		else if (bDuplicateGuid)
		{
			AddDiagnostic(
				OutDiagnostics,
				TEXT("material_expression_anchor_duplicate"),
				TEXT("warning"),
				TEXT("Material expression has a duplicate MaterialExpressionGuid in this material; path node_ref was used while preserving expression_guid."),
				Expression,
				ExpressionRef);
		}
		OutNodes.Add(MakeShared<FJsonValueObject>(BuildExpressionNodeJson(Expression, ExpressionRef).ToSharedRef()));

		TSharedPtr<FJsonObject> Parameter = BuildParameterJson(Expression, ExpressionRef, OutDiagnostics);
		if (Parameter.IsValid())
		{
			OutParameters.Add(MakeShared<FJsonValueObject>(Parameter.ToSharedRef()));
		}
	}
}

void FBlueprintHelperMaterialLogicJsonExtractor::AddExpressionLinks(
	UMaterial* Material,
	const TMap<const UMaterialExpression*, FString>& Refs,
	TArray<TSharedPtr<FJsonValue>>& OutLinks) const
{
	if (!Material)
	{
		return;
	}

	for (const TObjectPtr<UMaterialExpression>& ExpressionPtr : Material->GetExpressions())
	{
		UMaterialExpression* Expression = ExpressionPtr.Get();
		if (!Expression)
		{
			continue;
		}

		const FString* ToRef = Refs.Find(Expression);
		if (!ToRef)
		{
			continue;
		}

		const int32 InputCount = FBlueprintHelperVersionCompat::CountMaterialExpressionInputs(Expression);
		for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
		{
			const FExpressionInput* Input = Expression->GetInput(InputIndex);
			if (!Input || !Input->Expression)
			{
				continue;
			}

			const FString* FromRef = Refs.Find(Input->Expression);
			if (!FromRef)
			{
				continue;
			}

			const FString FromNodeKey = BuildExpressionNodeKey(Input->Expression, *FromRef);
			const FString ToNodeKey = BuildExpressionNodeKey(Expression, *ToRef);
			TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
			Link->SetStringField(TEXT("kind"), TEXT("material_expression_link"));
			Link->SetStringField(TEXT("from_node_ref"), *FromRef);
			Link->SetStringField(TEXT("from_node_key"), FromNodeKey);
			Link->SetStringField(TEXT("from_pin"), BuildOutputPinName(Input->Expression, Input->OutputIndex));
			Link->SetStringField(TEXT("to_node_ref"), *ToRef);
			Link->SetStringField(TEXT("to_node_key"), ToNodeKey);
			Link->SetStringField(TEXT("to_pin"), Expression->GetInputName(InputIndex).ToString());
			Link->SetStringField(TEXT("link_ref"), FString::Printf(
				TEXT("mlink:v1:%s:%s:%s"),
				**FromRef,
				*BuildOutputPinName(Input->Expression, Input->OutputIndex),
				**ToRef));
			OutLinks.Add(MakeShared<FJsonValueObject>(Link.ToSharedRef()));
		}
	}
}

void FBlueprintHelperMaterialLogicJsonExtractor::AddMaterialOutputs(
	UMaterial* Material,
	const TMap<const UMaterialExpression*, FString>& Refs,
	TArray<TSharedPtr<FJsonValue>>& OutOutputs,
	TArray<TSharedPtr<FJsonValue>>& OutLinks) const
{
	if (!Material)
	{
		return;
	}

	struct FBlueprintHelperMaterialOutputDescriptor
	{
		EMaterialProperty Property;
		const TCHAR* Name;
	};

	const FBlueprintHelperMaterialOutputDescriptor Descriptors[] = {
		{MP_BaseColor, TEXT("BaseColor")},
		{MP_Metallic, TEXT("Metallic")},
		{MP_Specular, TEXT("Specular")},
		{MP_Roughness, TEXT("Roughness")},
		{MP_EmissiveColor, TEXT("EmissiveColor")},
		{MP_Opacity, TEXT("Opacity")},
		{MP_OpacityMask, TEXT("OpacityMask")},
		{MP_Normal, TEXT("Normal")},
		{MP_WorldPositionOffset, TEXT("WorldPositionOffset")},
		{MP_AmbientOcclusion, TEXT("AmbientOcclusion")},
		{MP_Refraction, TEXT("Refraction")},
		{MP_MaterialAttributes, TEXT("MaterialAttributes")},
	};

	for (const FBlueprintHelperMaterialOutputDescriptor& Descriptor : Descriptors)
	{
		UMaterialExpression* Source = UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Descriptor.Property);
		if (!Source)
		{
			continue;
		}

		const FString* SourceRef = Refs.Find(Source);
		if (!SourceRef)
		{
			continue;
		}

		const FString OutputPin = UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, Descriptor.Property);
		const FString SourceNodeKey = BuildExpressionNodeKey(Source, *SourceRef);
		TSharedPtr<FJsonObject> Output = MakeShared<FJsonObject>();
		Output->SetStringField(TEXT("property"), Descriptor.Name);
		Output->SetStringField(TEXT("source_node_ref"), *SourceRef);
		Output->SetStringField(TEXT("source_node_key"), SourceNodeKey);
		Output->SetStringField(TEXT("source_pin"), OutputPin);
		Output->SetStringField(TEXT("output_ref"), FString::Printf(TEXT("mout:v1:%s"), Descriptor.Name));
		OutOutputs.Add(MakeShared<FJsonValueObject>(Output.ToSharedRef()));

		TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetStringField(TEXT("kind"), TEXT("material_output_link"));
		Link->SetStringField(TEXT("from_node_ref"), *SourceRef);
		Link->SetStringField(TEXT("from_node_key"), SourceNodeKey);
		Link->SetStringField(TEXT("from_pin"), OutputPin);
		Link->SetStringField(TEXT("to_node_ref"), TEXT("material_output"));
		Link->SetStringField(TEXT("to_node_key"), TEXT("$material_output"));
		Link->SetStringField(TEXT("to_pin"), Descriptor.Name);
		Link->SetStringField(TEXT("link_ref"), FString::Printf(TEXT("mlink:v1:%s:output:%s"), **SourceRef, Descriptor.Name));
		OutLinks.Add(MakeShared<FJsonValueObject>(Link.ToSharedRef()));
	}
}

TSharedPtr<FJsonObject> FBlueprintHelperMaterialLogicJsonExtractor::BuildExpressionNodeJson(
	const UMaterialExpression* Expression,
	const FString& ExpressionRef) const
{
	TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
	Node->SetStringField(TEXT("kind"), TEXT("material_expression"));
	Node->SetStringField(TEXT("node_ref"), ExpressionRef);
	Node->SetStringField(TEXT("node_key"), ExpressionRef);
	Node->SetStringField(TEXT("class"), Expression ? Expression->GetClass()->GetName() : FString());
	Node->SetStringField(TEXT("class_name"), Expression ? Expression->GetClass()->GetName() : FString());
	Node->SetStringField(TEXT("name"), Expression ? Expression->GetName() : FString());
	Node->SetStringField(TEXT("description"), Expression ? Expression->Desc : FString());
	if (Expression && Expression->MaterialExpressionGuid.IsValid())
	{
		Node->SetStringField(
			TEXT("expression_guid"),
			Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
	}
	AppendOwnershipMetadata(Expression, Node);

	TArray<TSharedPtr<FJsonValue>> OutputPins;
	if (Expression)
	{
		UMaterialExpression* MutableExpression = const_cast<UMaterialExpression*>(Expression);
		TArray<FExpressionOutput>& Outputs = MutableExpression->GetOutputs();
		for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
		{
			TSharedPtr<FJsonObject> OutputPin = MakeShared<FJsonObject>();
			OutputPin->SetStringField(TEXT("name"), BuildOutputPinName(Expression, OutputIndex));
			OutputPin->SetNumberField(TEXT("index"), OutputIndex);
			OutputPins.Add(MakeShared<FJsonValueObject>(OutputPin.ToSharedRef()));
		}
	}
	Node->SetArrayField(TEXT("outputs"), OutputPins);
	return Node;
}

TSharedPtr<FJsonObject> FBlueprintHelperMaterialLogicJsonExtractor::BuildParameterJson(
	const UMaterialExpression* Expression,
	const FString& ExpressionRef,
	TArray<TSharedPtr<FJsonValue>>& OutDiagnostics) const
{
	TSharedPtr<FJsonObject> Parameter = MakeShared<FJsonObject>();
	Parameter->SetStringField(TEXT("node_ref"), ExpressionRef);
	Parameter->SetStringField(TEXT("node_key"), ExpressionRef);

	if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		Parameter->SetStringField(TEXT("kind"), TEXT("scalar"));
		Parameter->SetStringField(TEXT("name"), Scalar->ParameterName.ToString());
		Parameter->SetStringField(TEXT("group"), Scalar->Group.ToString());
		Parameter->SetNumberField(TEXT("default_value"), Scalar->DefaultValue);
	}
	else if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		Parameter->SetStringField(TEXT("kind"), TEXT("vector"));
		Parameter->SetStringField(TEXT("name"), Vector->ParameterName.ToString());
		Parameter->SetStringField(TEXT("group"), Vector->Group.ToString());
		TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetNumberField(TEXT("r"), Vector->DefaultValue.R);
		Value->SetNumberField(TEXT("g"), Vector->DefaultValue.G);
		Value->SetNumberField(TEXT("b"), Vector->DefaultValue.B);
		Value->SetNumberField(TEXT("a"), Vector->DefaultValue.A);
		Parameter->SetObjectField(TEXT("default_value"), Value);
	}
	else if (const UMaterialExpressionStaticBoolParameter* StaticBool = Cast<UMaterialExpressionStaticBoolParameter>(Expression))
	{
		Parameter->SetStringField(TEXT("kind"), Cast<UMaterialExpressionStaticSwitchParameter>(Expression) ? TEXT("static_switch") : TEXT("static_bool"));
		Parameter->SetStringField(TEXT("name"), StaticBool->ParameterName.ToString());
		Parameter->SetStringField(TEXT("group"), StaticBool->Group.ToString());
		Parameter->SetBoolField(TEXT("default_value"), StaticBool->DefaultValue != 0);
	}
	else if (const UMaterialExpressionTextureSampleParameter* Texture = Cast<UMaterialExpressionTextureSampleParameter>(Expression))
	{
		Parameter->SetStringField(TEXT("kind"), TEXT("texture"));
		Parameter->SetStringField(TEXT("name"), Texture->ParameterName.ToString());
		Parameter->SetStringField(TEXT("group"), Texture->Group.ToString());
		Parameter->SetStringField(TEXT("default_value"), Texture->Texture ? Texture->Texture->GetPathName() : FString());
	}
	else
	{
		return nullptr;
	}

	if (const UMaterialExpressionParameter* GenericParameter = Cast<UMaterialExpressionParameter>(Expression))
	{
		if (GenericParameter->ParameterName.IsNone())
		{
			AddDiagnostic(
				OutDiagnostics,
				TEXT("material_parameter_read_partial"),
				TEXT("warning"),
				TEXT("Material parameter expression has no parameter name."),
				Expression,
				ExpressionRef);
		}
		if (GenericParameter->ExpressionGUID.IsValid())
		{
			Parameter->SetStringField(
				TEXT("parameter_guid"),
			GenericParameter->ExpressionGUID.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
		else
		{
			AddDiagnostic(
				OutDiagnostics,
				TEXT("material_parameter_read_partial"),
				TEXT("warning"),
				TEXT("Material parameter expression does not have a valid parameter guid."),
				Expression,
				ExpressionRef);
		}
	}
	AppendOwnershipMetadata(Expression, Parameter);
	return Parameter;
}

void FBlueprintHelperMaterialLogicJsonExtractor::AddDiagnostic(
	TArray<TSharedPtr<FJsonValue>>& OutDiagnostics,
	const FString& Code,
	const FString& Severity,
	const FString& Message,
	const UMaterialExpression* Expression,
	const FString& ExpressionRef) const
{
	TSharedPtr<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
	Diagnostic->SetStringField(TEXT("code"), Code);
	Diagnostic->SetStringField(TEXT("severity"), Severity);
	Diagnostic->SetStringField(TEXT("message"), Message);
	if (!ExpressionRef.IsEmpty())
	{
		Diagnostic->SetStringField(TEXT("node_ref"), ExpressionRef);
	}
	if (Expression)
	{
		Diagnostic->SetStringField(TEXT("expression_path"), Expression->GetPathName());
		Diagnostic->SetStringField(TEXT("class_name"), Expression->GetClass()->GetName());
		if (Expression->MaterialExpressionGuid.IsValid())
		{
			Diagnostic->SetStringField(
				TEXT("expression_guid"),
				Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
		}
	}
	OutDiagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic.ToSharedRef()));
}

void FBlueprintHelperMaterialLogicJsonExtractor::AppendOwnershipMetadata(
	const UMaterialExpression* Expression,
	TSharedPtr<FJsonObject> TargetJson) const
{
	if (!Expression || !TargetJson.IsValid())
	{
		return;
	}

	const FString BlockId = ReadOwnershipMetadataValue(Expression, TEXT("BlueprintHelper.BlockId"));
	const FString NodeKey = ReadOwnershipMetadataValue(Expression, TEXT("BlueprintHelper.NodeKey"));
	const FString Ownership = ReadOwnershipMetadataValue(Expression, TEXT("BlueprintHelper.Ownership"));

	if (!BlockId.IsEmpty())
	{
		TargetJson->SetStringField(TEXT("block_id"), BlockId);
	}
	if (!NodeKey.IsEmpty())
	{
		TargetJson->SetStringField(TEXT("node_key"), NodeKey);
	}
	if (!Ownership.IsEmpty())
	{
		TargetJson->SetStringField(TEXT("ownership"), Ownership);
	}
	if (!BlockId.IsEmpty() || Ownership.Equals(TEXT("owned"), ESearchCase::IgnoreCase))
	{
		TargetJson->SetBoolField(TEXT("owned"), true);
	}
}

FString FBlueprintHelperMaterialLogicJsonExtractor::ReadOwnershipMetadataValue(
	const UMaterialExpression* Expression,
	const TCHAR* Key) const
{
	if (!Expression || !Key)
	{
		return FString();
	}

	if (UPackage* Package = Expression->GetPackage())
	{
#if WITH_METADATA
		return FBlueprintHelperVersionCompat::GetPackageMetaData(Package).GetValue(Expression, Key);
#else
		return FString();
#endif
	}
	return FString();
}
