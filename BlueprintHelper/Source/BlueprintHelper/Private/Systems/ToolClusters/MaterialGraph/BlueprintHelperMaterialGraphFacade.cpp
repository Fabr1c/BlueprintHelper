// BlueprintHelper MaterialGraph facade.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphFacade.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Misc/PackageName.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialExpressionCandidateCacheService.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialExpressionSelectorResolver.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphCompileService.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphConnectionValidator.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphOwnershipService.h"
#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphReadbackService.h"
#include "UObject/Package.h"

class FBlueprintHelperMaterialGraphFacadePrivate
{
public:
	static void SetInvalid(
		FBlueprintHelperMaterialGraphPreflightResult& Result,
		const FString& Code,
		const FString& Message,
		const FString& FieldPath)
	{
		Result.bValid = false;
		Result.ErrorCode = Code;
		Result.ErrorMessage = Message;
		Result.FieldPath = FieldPath;
	}

	static bool ReadWriteOps(
		const TSharedPtr<FJsonObject>& StepObject,
		const TArray<TSharedPtr<FJsonValue>>*& OutOps,
		FBlueprintHelperMaterialGraphPreflightResult& Result)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr ||
			!WriteObjectPtr->IsValid())
		{
			SetInvalid(
				Result,
				TEXT("invalid_taskplan_step_write"),
				TEXT("MaterialGraph preflight requires write object."),
				TEXT("write"));
			return false;
		}

		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OutOps) || !OutOps || OutOps->Num() == 0)
		{
			SetInvalid(
				Result,
				TEXT("material_graph_schema_invalid"),
				TEXT("MaterialGraph preflight requires write.ops."),
				TEXT("write.ops"));
			return false;
		}
		return true;
	}

	static void RecordBlockId(
		const TSharedPtr<FJsonObject>& OpObject,
		FBlueprintHelperMaterialGraphPreflightResult& Result)
	{
		FString BlockId;
		if (OpObject.IsValid() &&
			OpObject->TryGetStringField(TEXT("block_id"), BlockId) &&
			!BlockId.IsEmpty())
		{
			Result.BlockIds.AddUnique(BlockId);
		}
	}

	static void RecordNodeKey(
		const TSharedPtr<FJsonObject>& OpObject,
		FBlueprintHelperMaterialGraphPreflightResult& Result)
	{
		FString NodeKey;
		if (OpObject.IsValid() &&
			OpObject->TryGetStringField(TEXT("node_key"), NodeKey) &&
			!NodeKey.IsEmpty())
		{
			Result.NodeKeys.AddUnique(NodeKey);
		}
	}

	static FBlueprintHelperToolError MakeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field)
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
		const FString& Field)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			TEXT("material_graph_edit"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeError(Code, Stage, Message, Field));
	}

	static FBlueprintHelperToolResultBase MakeCandidateRequiredFailure(
		const FString& Query,
		const FString& Field,
		const FString& AssetPath)
	{
		FBlueprintHelperToolResultBase Result = MakeFailure(
			TEXT("material_expression_candidate_confirmation_required"),
			EBlueprintHelperToolStage::DryRun,
			TEXT("MaterialGraph query selector requires candidate_id confirmation before execute."),
			Field);
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialExpressionCandidates.v1"));
		Data->SetStringField(TEXT("asset_path"), AssetPath);
		Data->SetStringField(TEXT("query"), Query);
		Data->SetStringField(
			TEXT("fingerprint"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::BuildCandidateCacheFingerprint(AssetPath, Query));
		Data->SetStringField(
			TEXT("schema_fingerprint"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::GetCandidateSchemaFingerprint());
		Data->SetStringField(
			TEXT("class_catalog_fingerprint"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::GetMaterialExpressionClassActionSnapshotFingerprint());
		Data->SetStringField(
			TEXT("class_catalog_revision"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::GetMaterialExpressionClassActionSnapshotRevision());
		Data->SetNumberField(
			TEXT("expires_in_seconds"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::GetCandidateTtlSeconds());
		Data->SetArrayField(
			TEXT("candidates"),
			FBlueprintHelperMaterialExpressionCandidateCacheService::BuildAndCacheCandidates(Query, AssetPath));
		Result.Data = Data;
		return Result;
	}

	static bool TryReadTargetAssetPath(
		const TSharedPtr<FJsonObject>& Object,
		FString& OutAssetPath)
	{
		if (!Object.IsValid())
		{
			return false;
		}

		if (Object->TryGetStringField(TEXT("asset_path"), OutAssetPath) && !OutAssetPath.IsEmpty())
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (Object->TryGetObjectField(TEXT("target"), TargetObjectPtr) &&
			TargetObjectPtr &&
			TargetObjectPtr->IsValid() &&
			(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath) &&
			!OutAssetPath.IsEmpty())
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (Object->TryGetObjectField(TEXT("write"), WriteObjectPtr) &&
			WriteObjectPtr &&
			WriteObjectPtr->IsValid())
		{
			return TryReadTargetAssetPath(*WriteObjectPtr, OutAssetPath);
		}
		return false;
	}

	static FString NormalizeObjectPath(const FString& AssetPath)
	{
		FString Path = AssetPath;
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty() || Path.Contains(TEXT(".")))
		{
			return Path;
		}
		if (FPackageName::IsValidLongPackageName(Path))
		{
			return FString::Printf(TEXT("%s.%s"), *Path, *FPackageName::GetShortName(Path));
		}
		return Path;
	}

	static UMaterial* LoadMaterial(
		const FString& AssetPath,
		FBlueprintHelperToolResultBase& OutFailure)
	{
		const FString ObjectPath = NormalizeObjectPath(AssetPath);
		UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
		if (!Object)
		{
			OutFailure = MakeFailure(
				TEXT("material_asset_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Material asset was not found: %s."), *AssetPath),
				TEXT("target.asset_path"));
			return nullptr;
		}

		UMaterial* Material = Cast<UMaterial>(Object);
		if (!Material)
		{
			OutFailure = MakeFailure(
				TEXT("material_asset_type_mismatch"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Asset is not a UMaterial: %s."), *AssetPath),
				TEXT("target.asset_path"));
			return nullptr;
		}
		return Material;
	}

	static UTexture* LoadTexture(const FString& TexturePath)
	{
		if (TexturePath.TrimStartAndEnd().IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UTexture>(nullptr, *NormalizeObjectPath(TexturePath));
	}

	static UClass* ResolveExpressionClass(const FString& ClassName)
	{
		const FString NormalizedClassName = ClassName.StartsWith(TEXT("U"))
			? ClassName.RightChop(1)
			: ClassName;
		if (NormalizedClassName == TEXT("MaterialExpressionConstant"))
		{
			return UMaterialExpressionConstant::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionScalarParameter"))
		{
			return UMaterialExpressionScalarParameter::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionVectorParameter"))
		{
			return UMaterialExpressionVectorParameter::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionTextureObjectParameter"))
		{
			return UMaterialExpressionTextureObjectParameter::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionTextureSample"))
		{
			return UMaterialExpressionTextureSample::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionAdd"))
		{
			return UMaterialExpressionAdd::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionMultiply"))
		{
			return UMaterialExpressionMultiply::StaticClass();
		}
		if (NormalizedClassName == TEXT("MaterialExpressionStaticSwitchParameter"))
		{
			return UMaterialExpressionStaticSwitchParameter::StaticClass();
		}
		return nullptr;
	}

	static bool ResolveMaterialProperty(const FString& PinName, EMaterialProperty& OutProperty)
	{
		if (PinName == TEXT("BaseColor"))
		{
			OutProperty = MP_BaseColor;
			return true;
		}
		if (PinName == TEXT("Metallic"))
		{
			OutProperty = MP_Metallic;
			return true;
		}
		if (PinName == TEXT("Specular"))
		{
			OutProperty = MP_Specular;
			return true;
		}
		if (PinName == TEXT("Roughness"))
		{
			OutProperty = MP_Roughness;
			return true;
		}
		if (PinName == TEXT("EmissiveColor"))
		{
			OutProperty = MP_EmissiveColor;
			return true;
		}
		if (PinName == TEXT("Opacity"))
		{
			OutProperty = MP_Opacity;
			return true;
		}
		if (PinName == TEXT("OpacityMask"))
		{
			OutProperty = MP_OpacityMask;
			return true;
		}
		if (PinName == TEXT("Normal"))
		{
			OutProperty = MP_Normal;
			return true;
		}
		if (PinName == TEXT("WorldPositionOffset"))
		{
			OutProperty = MP_WorldPositionOffset;
			return true;
		}
		return false;
	}

	static void RecordPlannedConnection(
		FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& FromNodeKey,
		const FString& FromPin,
		const FString& ToNodeKey,
		const FString& ToPin,
		const FString& FieldPath)
	{
		FBlueprintHelperMaterialGraphPlannedConnection Connection;
		Connection.FromNodeKey = FromNodeKey;
		Connection.FromPin = FromPin;
		Connection.ToNodeKey = ToNodeKey;
		Connection.ToPin = ToPin;
		Connection.FieldPath = FieldPath;
		Connection.bMaterialOutput = ToNodeKey == TEXT("$material_output");
		State.PlannedConnections.Add(Connection);
		State.RequestedConnectionCount++;
	}

	static bool ReadDouble(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		double& OutValue)
	{
		return Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue);
	}

	static float ReadFloatField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		float DefaultValue)
	{
		double Number = 0.0;
		if (ReadDouble(Object, FieldName, Number))
		{
			return static_cast<float>(Number);
		}
		return DefaultValue;
	}

	static FString GetEngineOutputPinName(const FExpressionOutput& Output)
	{
		return Output.OutputName.IsNone() ? FString() : Output.OutputName.ToString();
	}

	static FString GetOutputPinName(const FExpressionOutput& Output)
	{
		const FString EnginePinName = GetEngineOutputPinName(Output);
		if (!EnginePinName.IsEmpty())
		{
			return EnginePinName;
		}
		if (Output.Mask)
		{
			if (Output.MaskR && Output.MaskG && Output.MaskB && Output.MaskA)
			{
				return TEXT("RGBA");
			}
			if (Output.MaskR && Output.MaskG && Output.MaskB && !Output.MaskA)
			{
				return TEXT("RGB");
			}
			if (Output.MaskR && !Output.MaskG && !Output.MaskB && !Output.MaskA)
			{
				return TEXT("R");
			}
			if (!Output.MaskR && Output.MaskG && !Output.MaskB && !Output.MaskA)
			{
				return TEXT("G");
			}
			if (!Output.MaskR && !Output.MaskG && Output.MaskB && !Output.MaskA)
			{
				return TEXT("B");
			}
			if (!Output.MaskR && !Output.MaskG && !Output.MaskB && Output.MaskA)
			{
				return TEXT("A");
			}
		}

		return FString();
	}

	static int32 ResolveExpressionOutputIndex(
		const UMaterialExpression* Expression,
		const FString& EnginePinName)
	{
		if (!Expression)
		{
			return INDEX_NONE;
		}

		for (int32 OutputIndex = 0; OutputIndex < Expression->Outputs.Num(); ++OutputIndex)
		{
			if (GetEngineOutputPinName(Expression->Outputs[OutputIndex]) == EnginePinName)
			{
				return OutputIndex;
			}
		}
		return INDEX_NONE;
	}

	static bool AreEnginePinsEquivalent(
		const FString& ExpectedEnginePin,
		const FString& ActualEnginePin)
	{
		return ExpectedEnginePin == ActualEnginePin;
	}

	static FString DescribeExpressionOutputs(UMaterialExpression* Expression)
	{
		if (!Expression)
		{
			return FString();
		}

		TArray<FString> OutputNames;
		for (const FExpressionOutput& Output : Expression->GetOutputs())
		{
			const FString OutputName = GetOutputPinName(Output);
			OutputNames.Add(OutputName.IsEmpty() ? TEXT("<default>") : OutputName);
		}
		return FString::Join(OutputNames, TEXT(", "));
	}

	static bool NormalizeExpressionOutputPin(
		UMaterialExpression* Expression,
		const FString& AgentFacingPin,
		FString& OutEnginePin,
		FString& OutErrorMessage)
	{
		if (!Expression)
		{
			OutErrorMessage = TEXT("Material expression is missing.");
			return false;
		}

		const TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
		if (Outputs.Num() == 1 && GetEngineOutputPinName(Outputs[0]).IsEmpty())
		{
			if (AgentFacingPin == TEXT("Value") ||
				AgentFacingPin == TEXT("Result") ||
				AgentFacingPin == TEXT("RGB"))
			{
				OutEnginePin = FString();
				return true;
			}
		}

		for (const FExpressionOutput& Output : Expression->GetOutputs())
		{
			const FString OutputName = GetOutputPinName(Output);
			if (OutputName == AgentFacingPin)
			{
				const FString EnginePinName = GetEngineOutputPinName(Output);
				if (EnginePinName.IsEmpty() && AgentFacingPin != TEXT("RGB"))
				{
					continue;
				}
				OutEnginePin = EnginePinName;
				return true;
			}
		}

		OutErrorMessage = FString::Printf(
			TEXT("Material expression output pin '%s' was not found. Available outputs: %s."),
			*AgentFacingPin,
		*DescribeExpressionOutputs(Expression));
		return false;
	}

	static FExpressionInput* FindExpressionInputByName(
		UMaterialExpression* Expression,
		const FString& InputName,
		int32& OutInputIndex)
	{
		OutInputIndex = INDEX_NONE;
		if (!Expression)
		{
			return nullptr;
		}

		for (int32 InputIndex = 0; InputIndex < Expression->CountInputs(); ++InputIndex)
		{
			if (Expression->GetInputName(InputIndex).ToString() == InputName)
			{
				OutInputIndex = InputIndex;
				return Expression->GetInput(InputIndex);
			}
		}
		return nullptr;
	}

	static TArray<FBlueprintHelperDiagnosticItem> BuildAllDiagnostics(
		const FBlueprintHelperMaterialGraphExecutionState& State)
	{
		TArray<FBlueprintHelperDiagnosticItem> Diagnostics = State.CompileDiagnostics;
		Diagnostics.Append(State.ConnectivityDiagnostics);
		return Diagnostics;
	}

	static UMaterialExpression* ResolveExpressionForPinValidation(
		FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& NodeKey)
	{
		if (UMaterialExpression* Expression = FindExpression(State, NodeKey))
		{
			return Expression;
		}

		const FString ClassName = State.ExpressionClassNameByNodeKey.FindRef(NodeKey);
		if (ClassName.IsEmpty())
		{
			return nullptr;
		}

		UClass* ExpressionClass = ResolveExpressionClass(ClassName);
		return ExpressionClass ? ExpressionClass->GetDefaultObject<UMaterialExpression>() : nullptr;
	}

	static FLinearColor ReadLinearColor(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		const FLinearColor& DefaultValue)
	{
		const TSharedPtr<FJsonObject>* ColorObjectPtr = nullptr;
		if (!Object.IsValid() ||
			!Object->TryGetObjectField(FieldName, ColorObjectPtr) ||
			!ColorObjectPtr || !ColorObjectPtr->IsValid())
		{
			return DefaultValue;
		}

		return FLinearColor(
			ReadFloatField(*ColorObjectPtr, TEXT("r"), DefaultValue.R),
			ReadFloatField(*ColorObjectPtr, TEXT("g"), DefaultValue.G),
			ReadFloatField(*ColorObjectPtr, TEXT("b"), DefaultValue.B),
			ReadFloatField(*ColorObjectPtr, TEXT("a"), DefaultValue.A));
	}

	static void ApplyGenericFields(
		UMaterialExpression* Expression,
		const TSharedPtr<FJsonObject>& Properties)
	{
		if (!Expression || !Properties.IsValid())
		{
			return;
		}

		FString Desc;
		if (Properties->TryGetStringField(TEXT("desc"), Desc))
		{
			Expression->Desc = Desc;
		}
	}

	static void ApplyParameterFields(
		UMaterialExpressionParameter* Parameter,
		const TSharedPtr<FJsonObject>& Properties)
	{
		if (!Parameter || !Properties.IsValid())
		{
			return;
		}
		FString ParameterName;
		if (Properties->TryGetStringField(TEXT("parameter_name"), ParameterName) && !ParameterName.IsEmpty())
		{
			Parameter->ParameterName = FName(*ParameterName);
		}
		FString Group;
		if (Properties->TryGetStringField(TEXT("group"), Group))
		{
			Parameter->Group = FName(*Group);
		}
	}

	static void ApplyTextureParameterFields(
		UMaterialExpressionTextureSampleParameter* Parameter,
		const TSharedPtr<FJsonObject>& Properties)
	{
		if (!Parameter || !Properties.IsValid())
		{
			return;
		}
		FString ParameterName;
		if (Properties->TryGetStringField(TEXT("parameter_name"), ParameterName) && !ParameterName.IsEmpty())
		{
			Parameter->ParameterName = FName(*ParameterName);
		}
		FString Group;
		if (Properties->TryGetStringField(TEXT("group"), Group))
		{
			Parameter->Group = FName(*Group);
		}
	}

	static bool ApplyProperties(
		UMaterialExpression* Expression,
		const TSharedPtr<FJsonObject>& Properties,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		if (!Expression || !Properties.IsValid())
		{
			return true;
		}

		Expression->Modify();
		ApplyGenericFields(Expression, Properties);
		if (UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			Constant->R = ReadFloatField(Properties, TEXT("default_value"), Constant->R);
			return true;
		}
		if (UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			ApplyParameterFields(Scalar, Properties);
			Scalar->DefaultValue = ReadFloatField(Properties, TEXT("default_value"), Scalar->DefaultValue);
			return true;
		}
		if (UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			ApplyParameterFields(Vector, Properties);
			Vector->DefaultValue = ReadLinearColor(Properties, TEXT("default_value"), Vector->DefaultValue);
			return true;
		}
		if (UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression))
		{
			FString TexturePath;
			if (Properties->TryGetStringField(TEXT("texture"), TexturePath) && !TexturePath.IsEmpty())
			{
				UTexture* Texture = LoadTexture(TexturePath);
				if (!Texture)
				{
					OutErrorCode = TEXT("material_texture_not_found");
					OutErrorMessage = FString::Printf(TEXT("Texture asset was not found: %s."), *TexturePath);
					return false;
				}
				TextureBase->Texture = Texture;
				TextureBase->AutoSetSampleType();
			}
			if (UMaterialExpressionTextureSampleParameter* TextureParameter =
				Cast<UMaterialExpressionTextureSampleParameter>(Expression))
			{
				ApplyTextureParameterFields(TextureParameter, Properties);
			}
			return true;
		}
		if (UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expression))
		{
			Add->ConstA = ReadFloatField(Properties, TEXT("const_a"), Add->ConstA);
			Add->ConstB = ReadFloatField(Properties, TEXT("const_b"), Add->ConstB);
			return true;
		}
		if (UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Expression))
		{
			Multiply->ConstA = ReadFloatField(Properties, TEXT("const_a"), Multiply->ConstA);
			Multiply->ConstB = ReadFloatField(Properties, TEXT("const_b"), Multiply->ConstB);
			return true;
		}
		if (UMaterialExpressionStaticSwitchParameter* StaticSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			ApplyParameterFields(StaticSwitch, Properties);
			bool bDefaultValue = StaticSwitch->DefaultValue != 0;
			if (Properties->TryGetBoolField(TEXT("default_value"), bDefaultValue))
			{
				StaticSwitch->DefaultValue = bDefaultValue ? 1 : 0;
			}
			return true;
		}
		return true;
	}

	static void WriteOwnershipMetadata(
		UMaterialExpression* Expression,
		const FString& BlockId,
		const FString& NodeKey)
	{
		if (!Expression)
		{
			return;
		}
		if (!Expression->MaterialExpressionGuid.IsValid())
		{
			Expression->UpdateMaterialExpressionGuid(true, true);
		}
#if WITH_METADATA
		if (UPackage* Package = Expression->GetPackage())
		{
			if (!BlockId.IsEmpty())
			{
				Package->GetMetaData().SetValue(
					Expression,
					FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey(),
					*BlockId);
			}
			if (!NodeKey.IsEmpty())
			{
				Package->GetMetaData().SetValue(
					Expression,
					FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey(),
					*NodeKey);
			}
			Package->GetMetaData().SetValue(
				Expression,
				FBlueprintHelperMaterialGraphOwnershipService::OwnershipMetadataKey(),
				FBlueprintHelperMaterialGraphOwnershipService::OwnedMetadataValue());
		}
#endif
	}

	static FString ReadOwnershipMetadata(
		const UMaterialExpression* Expression,
		const TCHAR* Key)
	{
		if (!Expression || !Key)
		{
			return FString();
		}
#if WITH_METADATA
		if (UPackage* Package = Expression->GetPackage())
		{
			return Package->GetMetaData().GetValue(Expression, Key);
		}
#endif
		return FString();
	}

	static bool IsOwnedExpression(const UMaterialExpression* Expression)
	{
		const FString Ownership = ReadOwnershipMetadata(
			Expression,
			FBlueprintHelperMaterialGraphOwnershipService::OwnershipMetadataKey());
		const FString NodeKey = ReadOwnershipMetadata(
			Expression,
			FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey());
		return Ownership.Equals(
			FBlueprintHelperMaterialGraphOwnershipService::OwnedMetadataValue(),
			ESearchCase::IgnoreCase) ||
			!NodeKey.IsEmpty();
	}

	static void IndexOwnedExpressions(FBlueprintHelperMaterialGraphExecutionState& State)
	{
		if (!State.Material)
		{
			return;
		}

		for (UMaterialExpression* Expression : State.Material->GetExpressions())
		{
			if (!Expression || !IsOwnedExpression(Expression))
			{
				continue;
			}

			const FString NodeKey = ReadOwnershipMetadata(
				Expression,
				FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey());
			const FString BlockId = ReadOwnershipMetadata(
				Expression,
				FBlueprintHelperMaterialGraphOwnershipService::BlockIdMetadataKey());
			if (!NodeKey.IsEmpty())
			{
				State.ExpressionsByNodeKey.Add(NodeKey, Expression);
				State.NodeKeyByExpression.Add(Expression, NodeKey);
			}
			if (!BlockId.IsEmpty())
			{
				State.BlockIdByExpression.Add(Expression, BlockId);
			}
		}
	}

	static TSet<FString> CollectTouchedBlockIds(const TArray<TSharedPtr<FJsonValue>>& Ops)
	{
		TSet<FString> BlockIds;
		for (const TSharedPtr<FJsonValue>& OpValue : Ops)
		{
			const TSharedPtr<FJsonObject> OpObject = OpValue.IsValid() ? OpValue->AsObject() : nullptr;
			if (!OpObject.IsValid())
			{
				continue;
			}

			FString BlockId;
			if (OpObject->TryGetStringField(TEXT("block_id"), BlockId) && !BlockId.IsEmpty())
			{
				BlockIds.Add(BlockId);
			}
		}
		return BlockIds;
	}

	static void RecordDeletedExpression(
		FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& NodeKey,
		const FString& BlockId,
		UMaterialExpression* Expression)
	{
		State.DeletedExpressionCount++;
		if (!NodeKey.IsEmpty())
		{
			State.DeletedExpressionNodeKeys.Add(NodeKey);
		}
		TSharedRef<FJsonObject> Json = MakeExpressionJson(
			NodeKey,
			BlockId,
			FString(),
			const_cast<UMaterialExpression*>(Expression));
		Json->SetBoolField(TEXT("exists"), Expression != nullptr);
		Json->SetStringField(TEXT("target_kind"), TEXT("material_expression"));
		TArray<TSharedPtr<FJsonValue>> InputLinks;
		TArray<TSharedPtr<FJsonValue>> OutputLinks;
		if (Expression && State.Material)
		{
			for (int32 InputIndex = 0; InputIndex < Expression->CountInputs(); ++InputIndex)
			{
				const FExpressionInput* Input = Expression->GetInput(InputIndex);
				if (!Input || !Input->Expression)
				{
					continue;
				}
				TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
				Link->SetStringField(TEXT("kind"), TEXT("material_expression_input"));
				Link->SetStringField(
					TEXT("from_node_key"),
					ReadOwnershipMetadata(Input->Expression, FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey()));
				Link->SetStringField(
					TEXT("from_expression_guid"),
					Input->Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
				Link->SetStringField(TEXT("from_engine_pin"), GetEngineOutputPinName(Input->Expression->GetOutputs().IsValidIndex(Input->OutputIndex)
					? Input->Expression->GetOutputs()[Input->OutputIndex]
					: FExpressionOutput()));
				Link->SetStringField(TEXT("to_node_key"), NodeKey);
				Link->SetStringField(TEXT("to_pin"), Expression->GetInputName(InputIndex).ToString());
				InputLinks.Add(MakeShared<FJsonValueObject>(Link));
			}

			const EMaterialProperty MaterialProperties[] =
			{
				MP_BaseColor,
				MP_Metallic,
				MP_Specular,
				MP_Roughness,
				MP_EmissiveColor,
				MP_Opacity,
				MP_OpacityMask,
				MP_Normal,
				MP_WorldPositionOffset
			};
			for (EMaterialProperty MaterialProperty : MaterialProperties)
			{
				if (UMaterialEditingLibrary::GetMaterialPropertyInputNode(State.Material, MaterialProperty) != Expression)
				{
					continue;
				}
				TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
				Link->SetStringField(TEXT("kind"), TEXT("material_output_link"));
				Link->SetStringField(TEXT("from_node_key"), NodeKey);
				Link->SetStringField(
					TEXT("from_expression_guid"),
					Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
				Link->SetStringField(
					TEXT("from_engine_pin"),
					UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(State.Material, MaterialProperty));
				Link->SetStringField(TEXT("to_node_key"), TEXT("$material_output"));
				Link->SetStringField(TEXT("to_pin"), FMaterialAttributeDefinitionMap::GetAttributeName(MaterialProperty));
				OutputLinks.Add(MakeShared<FJsonValueObject>(Link));
			}

			for (UMaterialExpression* Candidate : State.Material->GetExpressions())
			{
				if (!Candidate || Candidate == Expression)
				{
					continue;
				}
				for (int32 InputIndex = 0; InputIndex < Candidate->CountInputs(); ++InputIndex)
				{
					const FExpressionInput* Input = Candidate->GetInput(InputIndex);
					if (!Input || Input->Expression != Expression)
					{
						continue;
					}
					TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
					Link->SetStringField(TEXT("kind"), TEXT("material_expression_link"));
					Link->SetStringField(TEXT("from_node_key"), NodeKey);
					Link->SetStringField(
						TEXT("from_expression_guid"),
						Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
					TArray<FExpressionOutput>& ExpressionOutputs = Expression->GetOutputs();
					Link->SetStringField(TEXT("from_engine_pin"), GetEngineOutputPinName(ExpressionOutputs.IsValidIndex(Input->OutputIndex)
						? ExpressionOutputs[Input->OutputIndex]
						: FExpressionOutput()));
					Link->SetStringField(
						TEXT("to_node_key"),
						ReadOwnershipMetadata(Candidate, FBlueprintHelperMaterialGraphOwnershipService::NodeKeyMetadataKey()));
					Link->SetStringField(
						TEXT("to_expression_guid"),
						Candidate->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
					Link->SetStringField(TEXT("to_pin"), Candidate->GetInputName(InputIndex).ToString());
					OutputLinks.Add(MakeShared<FJsonValueObject>(Link));
				}
			}
		}
		Json->SetArrayField(TEXT("input_links"), InputLinks);
		Json->SetArrayField(TEXT("output_links"), OutputLinks);
		State.DeletedExpressions.Add(MakeShared<FJsonValueObject>(Json));
	}

	static void RemoveExpressionFromIndexes(
		FBlueprintHelperMaterialGraphExecutionState& State,
		UMaterialExpression* Expression,
		const FString& NodeKey)
	{
		if (!NodeKey.IsEmpty())
		{
			State.ExpressionsByNodeKey.Remove(NodeKey);
		}
		State.NodeKeyByExpression.Remove(Expression);
		State.BlockIdByExpression.Remove(Expression);
	}

	static bool DeleteOwnedExpression(
		FBlueprintHelperMaterialGraphExecutionState& State,
		UMaterialExpression* Expression,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage)
	{
		if (!Expression)
		{
			OutErrorCode = TEXT("material_owned_anchor_violation");
			OutErrorMessage = TEXT("Owned material expression was not found.");
			return false;
		}
		if (!IsOwnedExpression(Expression))
		{
			OutErrorCode = TEXT("material_owned_anchor_violation");
			OutErrorMessage = TEXT("Material expression is not BlueprintHelper-owned.");
			return false;
		}

		const FString NodeKey = State.NodeKeyByExpression.FindRef(Expression);
		const FString BlockId = State.BlockIdByExpression.FindRef(Expression);
		RecordDeletedExpression(State, NodeKey, BlockId, Expression);
		if (!bDryRun)
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(State.Material, Expression);
			RemoveExpressionFromIndexes(State, Expression, NodeKey);
		}
		return true;
	}

	static FBlueprintHelperToolResultBase DeleteTouchedBlocks(
		FBlueprintHelperMaterialGraphExecutionState& State,
		const TSet<FString>& BlockIds,
		bool bDryRun)
	{
		if (BlockIds.Num() == 0)
		{
			return FBlueprintHelperToolResultBase();
		}

		TArray<UMaterialExpression*> ToDelete;
		for (const TPair<UMaterialExpression*, FString>& Pair : State.BlockIdByExpression)
		{
			if (Pair.Key && BlockIds.Contains(Pair.Value))
			{
				ToDelete.Add(Pair.Key);
			}
		}

		for (UMaterialExpression* Expression : ToDelete)
		{
			FString ErrorCode;
			FString ErrorMessage;
			if (!DeleteOwnedExpression(State, Expression, bDryRun, ErrorCode, ErrorMessage))
			{
				return MakeFailure(
					ErrorCode.IsEmpty() ? TEXT("material_owned_anchor_violation") : ErrorCode,
					EBlueprintHelperToolStage::Preflight,
					ErrorMessage.IsEmpty() ? TEXT("Failed to delete owned MaterialGraph block expression.") : ErrorMessage,
					TEXT("material_strategy"));
			}
		}
		return FBlueprintHelperToolResultBase();
	}

	static TSharedRef<FJsonObject> BuildExpressionPropertiesJson(const UMaterialExpression* Expression)
	{
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		if (!Expression)
		{
			return Properties;
		}

		if (const UMaterialExpressionConstant* Constant = Cast<UMaterialExpressionConstant>(Expression))
		{
			Properties->SetNumberField(TEXT("value"), Constant->R);
		}
		if (const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			Properties->SetStringField(TEXT("parameter_name"), Scalar->ParameterName.ToString());
			Properties->SetStringField(TEXT("group"), Scalar->Group.ToString());
			Properties->SetNumberField(TEXT("default_value"), Scalar->DefaultValue);
		}
		if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			Properties->SetStringField(TEXT("parameter_name"), Vector->ParameterName.ToString());
			Properties->SetStringField(TEXT("group"), Vector->Group.ToString());
			TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
			Value->SetNumberField(TEXT("r"), Vector->DefaultValue.R);
			Value->SetNumberField(TEXT("g"), Vector->DefaultValue.G);
			Value->SetNumberField(TEXT("b"), Vector->DefaultValue.B);
			Value->SetNumberField(TEXT("a"), Vector->DefaultValue.A);
			Properties->SetObjectField(TEXT("default_value"), Value);
		}
		if (const UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression))
		{
			Properties->SetStringField(TEXT("texture"), TextureBase->Texture ? TextureBase->Texture->GetPathName() : FString());
		}
		if (const UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expression))
		{
			Properties->SetNumberField(TEXT("const_a"), Add->ConstA);
			Properties->SetNumberField(TEXT("const_b"), Add->ConstB);
		}
		if (const UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Expression))
		{
			Properties->SetNumberField(TEXT("const_a"), Multiply->ConstA);
			Properties->SetNumberField(TEXT("const_b"), Multiply->ConstB);
		}
		if (const UMaterialExpressionStaticSwitchParameter* StaticSwitch = Cast<UMaterialExpressionStaticSwitchParameter>(Expression))
		{
			Properties->SetStringField(TEXT("parameter_name"), StaticSwitch->ParameterName.ToString());
			Properties->SetStringField(TEXT("group"), StaticSwitch->Group.ToString());
			Properties->SetBoolField(TEXT("default_value"), StaticSwitch->DefaultValue != 0);
		}
		return Properties;
	}

	static TSharedRef<FJsonObject> MakeExpressionJson(
		const FString& NodeKey,
		const FString& BlockId,
		const FString& SelectorId,
		UMaterialExpression* Expression)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("node_key"), NodeKey);
		if (!BlockId.IsEmpty())
		{
			Json->SetStringField(TEXT("block_id"), BlockId);
		}
		if (!SelectorId.IsEmpty())
		{
			Json->SetStringField(TEXT("selector"), SelectorId);
		}
		if (Expression)
		{
			Json->SetStringField(TEXT("class_name"), Expression->GetClass()->GetName());
			Json->SetStringField(
				TEXT("expression_guid"),
				Expression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
			Json->SetObjectField(TEXT("properties"), BuildExpressionPropertiesJson(Expression));
		}
		return Json;
	}

	static bool ReadEndpoint(
		const TSharedPtr<FJsonObject>& OpObject,
		const TCHAR* FieldName,
		FString& OutNodeKey,
		FString& OutPin)
	{
		const TSharedPtr<FJsonObject>* EndpointPtr = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetObjectField(FieldName, EndpointPtr) ||
			!EndpointPtr || !EndpointPtr->IsValid())
		{
			return false;
		}
		(*EndpointPtr)->TryGetStringField(TEXT("node_key"), OutNodeKey);
		(*EndpointPtr)->TryGetStringField(TEXT("pin"), OutPin);
		return !OutNodeKey.IsEmpty() && !OutPin.IsEmpty();
	}

	static UMaterialExpression* FindExpression(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& NodeKey)
	{
		if (UMaterialExpression* const* Found = State.ExpressionsByNodeKey.Find(NodeKey))
		{
			return *Found;
		}
		return nullptr;
	}

	static void AttachConnectivityJson(
		const TSharedRef<FJsonObject>& Data,
		const TArray<FBlueprintHelperDiagnosticItem>& Diagnostics)
	{
		if (Diagnostics.Num() == 0)
		{
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Violations;
		for (const FBlueprintHelperDiagnosticItem& Diagnostic : Diagnostics)
		{
			TSharedRef<FJsonObject> Violation = MakeShared<FJsonObject>();
			Violation->SetStringField(TEXT("code"), Diagnostic.Code);
			Violation->SetStringField(TEXT("node_id"), Diagnostic.TargetKey);
			Violation->SetStringField(TEXT("message"), Diagnostic.Message);
			if (!Diagnostic.Field.IsEmpty())
			{
				Violation->SetStringField(TEXT("field"), Diagnostic.Field);
			}
			if (!Diagnostic.PinName.IsEmpty())
			{
				Violation->SetStringField(TEXT("pin"), Diagnostic.PinName);
			}
			Violations.Add(MakeShared<FJsonValueObject>(Violation));
		}

		TSharedRef<FJsonObject> Connectivity = MakeShared<FJsonObject>();
		Connectivity->SetArrayField(TEXT("violations"), Violations);
		Data->SetObjectField(TEXT("connectivity"), Connectivity);
	}

	static FBlueprintHelperToolResultBase BuildResult(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		bool bDryRun)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = bDryRun
			? FBlueprintHelperToolResultBuilder::DryRun(TEXT("material_graph_edit"), TraceId)
			: FBlueprintHelperToolResultBuilder::Applied(TEXT("material_graph_edit"), TraceId);

		Result.bModified = !bDryRun &&
			(State.CreatedExpressionCount > 0 ||
				State.UpdatedPropertyCount > 0 ||
				State.DeletedExpressionCount > 0 ||
				State.ExpressionConnectionCount > 0 ||
				State.MaterialOutputConnectionCount > 0);

		FBlueprintHelperTargetRef Target;
		Target.AssetPath = State.AssetPath;
		Target.AssetClass = TEXT("Material");
		Target.TargetType = EBlueprintHelperTargetType::Asset;
		Target.Graph = TEXT("MaterialGraph");
		Result.Target = Target;

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialGraphEditResult.v1"));
		Data->SetStringField(TEXT("asset_path"), State.AssetPath);
		Data->SetStringField(TEXT("graph_name"), TEXT("MaterialGraph"));
		Data->SetStringField(TEXT("material_strategy"), State.MaterialStrategy);
		Data->SetBoolField(TEXT("dry_run"), bDryRun);
		Data->SetNumberField(TEXT("created_expressions"), State.CreatedExpressionCount);
		Data->SetNumberField(TEXT("updated_properties"), State.UpdatedPropertyCount);
		Data->SetNumberField(TEXT("deleted_expressions"), State.DeletedExpressionCount);
		Data->SetNumberField(TEXT("expression_connections"), State.ExpressionConnectionCount);
		Data->SetNumberField(TEXT("material_output_connections"), State.MaterialOutputConnectionCount);
		Data->SetNumberField(TEXT("requested_connections"), State.RequestedConnectionCount);
		Data->SetNumberField(TEXT("verified_connections"), State.VerifiedConnectionCount);
		Data->SetNumberField(TEXT("graph_sync_connections"), State.GraphSyncConnectionCount);
		Data->SetNumberField(TEXT("connectivity_violation_count"), State.ConnectivityDiagnostics.Num());
		Data->SetArrayField(TEXT("created_expression_refs"), State.CreatedExpressions);
		Data->SetArrayField(TEXT("updated_property_refs"), State.UpdatedProperties);
		Data->SetArrayField(TEXT("deleted_expression_refs"), State.DeletedExpressions);
		Data->SetArrayField(TEXT("connections"), State.Connections);
		Data->SetArrayField(TEXT("material_outputs"), State.MaterialOutputs);
		Data->SetArrayField(
			TEXT("connectivity_diagnostics"),
			BlueprintHelperDiagnosticItemsToJsonArray(State.ConnectivityDiagnostics));
		AttachConnectivityJson(Data, State.ConnectivityDiagnostics);
		TSharedRef<FJsonObject> Readback = MakeShared<FJsonObject>();
		Readback->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.MaterialGraphReadback.v1"));
		Readback->SetStringField(TEXT("asset_path"), State.AssetPath);
		Readback->SetStringField(TEXT("graph_name"), TEXT("MaterialGraph"));
		Readback->SetArrayField(TEXT("created_expression_refs"), State.CreatedExpressions);
		Readback->SetArrayField(TEXT("updated_property_refs"), State.UpdatedProperties);
		Readback->SetArrayField(TEXT("deleted_expression_refs"), State.DeletedExpressions);
		Readback->SetArrayField(TEXT("connections"), State.Connections);
		Readback->SetArrayField(TEXT("material_outputs"), State.MaterialOutputs);
		Data->SetObjectField(TEXT("material_readback"), Readback);
		Data->SetObjectField(TEXT("readback"), Readback);
		const TArray<FBlueprintHelperDiagnosticItem> AllDiagnostics = BuildAllDiagnostics(State);
		Data->SetArrayField(TEXT("diagnostics"), BlueprintHelperDiagnosticItemsToJsonArray(AllDiagnostics));
		Data->SetArrayField(TEXT("compiler_results"), BlueprintHelperDiagnosticItemsToJsonArray(State.CompileDiagnostics));
		Data->SetObjectField(
			TEXT("compile_result"),
			FBlueprintHelperMaterialGraphCompileService::BuildCompileResultJson(State.CompileDiagnostics));
		Result.Data = Data;

		const bool bHasBlockingCompileErrors =
			FBlueprintHelperMaterialGraphCompileService::HasBlockingCompileErrors(State.CompileDiagnostics);
		const bool bHasConnectivityViolations = State.ConnectivityDiagnostics.Num() > 0;
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = true;
		Validation.bShouldSave = Result.bModified;
		Validation.bCompiled = !bDryRun;
		Validation.bCompileSuccess = !bDryRun && !bHasBlockingCompileErrors && !bHasConnectivityViolations;
		Result.Validation = Validation;

		if (!bDryRun && Result.bModified)
		{
			FBlueprintHelperReviewSummary Review;
			Review.bReviewRequired = true;
			Review.ReviewStatus = EBlueprintHelperReviewStatus::Pending;
			Review.ReviewReason = TEXT("material_graph_write");
			Review.ReviewGrouping = { TEXT("asset_path"), TEXT("block_id"), TEXT("node_key") };
			Result.Review = Review;
		}
		if (!bDryRun && bHasBlockingCompileErrors)
		{
			Result.bOk = false;
			Result.Status = EBlueprintHelperToolStatus::Failed;
			Result.Error = FBlueprintHelperMaterialGraphCompileService::BuildBlockingCompileError(
				State.CompileDiagnostics);
		}
		else if (bHasConnectivityViolations)
		{
			const FBlueprintHelperDiagnosticItem& FirstDiagnostic = State.ConnectivityDiagnostics[0];
			Result.bOk = false;
			Result.Status = EBlueprintHelperToolStatus::Failed;
			Result.Error = MakeError(
				FirstDiagnostic.Code.IsEmpty() ? TEXT("material_connectivity_violation") : FirstDiagnostic.Code,
				bDryRun ? EBlueprintHelperToolStage::Preflight : EBlueprintHelperToolStage::Execute,
				FirstDiagnostic.Message.IsEmpty()
					? TEXT("MaterialGraph connectivity validation failed.")
					: FirstDiagnostic.Message,
				FirstDiagnostic.Field);
		}
		return Result;
	}
};

FBlueprintHelperMaterialGraphPreflightResult FBlueprintHelperMaterialGraphFacade::Preflight(
	const FBlueprintHelperMaterialGraphPreflightInput& Input) const
{
	FBlueprintHelperMaterialGraphPreflightResult Result;
	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	if (!FBlueprintHelperMaterialGraphFacadePrivate::ReadWriteOps(Input.StepObject, Ops, Result))
	{
		return Result;
	}
	FString AssetPath;
	FBlueprintHelperMaterialGraphFacadePrivate::TryReadTargetAssetPath(Input.StepObject, AssetPath);

	for (int32 Index = 0; Index < Ops->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> OpObject = (*Ops)[Index].IsValid()
			? (*Ops)[Index]->AsObject()
			: nullptr;
		if (!OpObject.IsValid())
		{
			FBlueprintHelperMaterialGraphFacadePrivate::SetInvalid(
				Result,
				TEXT("material_graph_schema_invalid"),
				TEXT("MaterialGraph ops must be objects."),
				FString::Printf(TEXT("write.ops[%d]"), Index));
			return Result;
		}

		FString OpName;
		OpObject->TryGetStringField(TEXT("op"), OpName);
		FBlueprintHelperMaterialGraphFacadePrivate::RecordBlockId(OpObject, Result);
		FBlueprintHelperMaterialGraphFacadePrivate::RecordNodeKey(OpObject, Result);

		if (OpName == TEXT("spawn_material_expression") ||
			OpName == TEXT("resolve_material_expression"))
		{
			const TSharedPtr<FJsonValue> SelectorValue = OpObject->TryGetField(TEXT("selector"));
			const FBlueprintHelperMaterialSelectorResolution Resolution =
				FBlueprintHelperMaterialExpressionSelectorResolver::ResolveSelector(SelectorValue, AssetPath);
			if (!Resolution.bResolved)
			{
				FBlueprintHelperMaterialGraphFacadePrivate::SetInvalid(
					Result,
					Resolution.ErrorCode,
					Resolution.ErrorMessage,
					FString::Printf(TEXT("write.ops[%d].selector"), Index));
				return Result;
			}
			if (Resolution.bRequiresCandidateSearch)
			{
				Result.Diagnostics.Add(TEXT("material_expression_candidate_search_required"));
			}
		}
		else if (OpName == TEXT("connect_material_expression") ||
			OpName == TEXT("connect_material_property"))
		{
			const FBlueprintHelperMaterialGraphValidationResult LinkValidation =
				FBlueprintHelperMaterialGraphConnectionValidator::ValidateLink(
					OpObject,
					FString::Printf(TEXT("write.ops[%d]"), Index));
			if (!LinkValidation.bValid)
			{
				FBlueprintHelperMaterialGraphFacadePrivate::SetInvalid(
					Result,
					LinkValidation.ErrorCode,
					LinkValidation.ErrorMessage,
					LinkValidation.FieldPath);
				return Result;
			}
		}
	}

	Result.Diagnostics.Add(FBlueprintHelperMaterialGraphCompileService::BuildCompilePendingDiagnostic());
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperMaterialGraphFacade::Execute(
	const FBlueprintHelperMaterialGraphExecutionInput& Input) const
{
	if (!Input.bDryRun)
	{
		FBlueprintHelperMaterialGraphExecutionInput DryRunInput = Input;
		DryRunInput.bDryRun = true;
		const FBlueprintHelperToolResultBase DryRunResult = Execute(DryRunInput);
		if (!DryRunResult.bOk)
		{
			return DryRunResult;
		}
	}

	const TSharedPtr<FJsonObject> Payload = Input.Payload;
	const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
	if (!Payload.IsValid() ||
		!Payload->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
		!TargetObjectPtr || !TargetObjectPtr->IsValid())
	{
		return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
			TEXT("material_graph_schema_invalid"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("material_graph_edit requires target object."),
			TEXT("target"));
	}

	FBlueprintHelperMaterialGraphExecutionState State;
	(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), State.AssetPath);
	if (State.AssetPath.IsEmpty())
	{
		return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
			TEXT("material_graph_schema_invalid"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("material_graph_edit target requires asset_path."),
			TEXT("target.asset_path"));
	}

	Payload->TryGetStringField(TEXT("material_strategy"), State.MaterialStrategy);
	if (State.MaterialStrategy.IsEmpty())
	{
		State.MaterialStrategy = TEXT("append_new_owned_graph");
	}

	FBlueprintHelperToolResultBase LoadFailure;
	State.Material = FBlueprintHelperMaterialGraphFacadePrivate::LoadMaterial(State.AssetPath, LoadFailure);
	if (!State.Material)
	{
		return LoadFailure;
	}

	const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
	if (!Payload->TryGetArrayField(TEXT("ops"), Ops) || !Ops || Ops->Num() == 0)
	{
		return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
			TEXT("material_graph_schema_invalid"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("material_graph_edit requires ops array."),
			TEXT("ops"));
	}

	if (!Input.bDryRun)
	{
		State.Material->Modify();
	}
	FBlueprintHelperMaterialGraphFacadePrivate::IndexOwnedExpressions(State);

	if (State.MaterialStrategy == TEXT("replace_owned_graph"))
	{
		const TSet<FString> TouchedBlockIds =
			FBlueprintHelperMaterialGraphFacadePrivate::CollectTouchedBlockIds(*Ops);
		FBlueprintHelperToolResultBase DeleteFailure =
			FBlueprintHelperMaterialGraphFacadePrivate::DeleteTouchedBlocks(
				State,
				TouchedBlockIds,
				Input.bDryRun);
		if (DeleteFailure.Error.IsSet())
		{
			return DeleteFailure;
		}
	}
	else if (State.MaterialStrategy != TEXT("append_new_owned_graph") &&
		State.MaterialStrategy != TEXT("patch_owned_graph") &&
		State.MaterialStrategy != TEXT("merge_owned_graph"))
	{
		return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
			TEXT("material_graph_strategy_not_supported"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Unsupported MaterialGraph strategy: %s."), *State.MaterialStrategy),
			TEXT("material_strategy"));
	}

	for (int32 Index = 0; Index < Ops->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> OpObject = (*Ops)[Index].IsValid()
			? (*Ops)[Index]->AsObject()
			: nullptr;
		if (!OpObject.IsValid())
		{
			return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
				TEXT("material_graph_schema_invalid"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("MaterialGraph ops must be objects."),
				FString::Printf(TEXT("ops[%d]"), Index));
		}

		FString OpName;
		OpObject->TryGetStringField(TEXT("op"), OpName);
		if (OpName == TEXT("begin_material_graph_edit"))
		{
			continue;
		}
		if (OpName == TEXT("begin_material_block"))
		{
			OpObject->TryGetStringField(TEXT("block_id"), State.CurrentBlockId);
			continue;
		}
		if (OpName == TEXT("resolve_material_expression"))
		{
			if (Input.bDryRun)
			{
				FString Query;
				const TSharedPtr<FJsonValue> SelectorValue = OpObject->TryGetField(TEXT("selector"));
				const TSharedPtr<FJsonObject> SelectorObject = SelectorValue.IsValid() ? SelectorValue->AsObject() : nullptr;
				if (SelectorObject.IsValid())
				{
					SelectorObject->TryGetStringField(TEXT("query"), Query);
				}
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeCandidateRequiredFailure(
					Query,
					FString::Printf(TEXT("ops[%d].selector"), Index),
					State.AssetPath);
			}
			return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
				TEXT("material_expression_candidate_confirmation_required"),
				EBlueprintHelperToolStage::Preflight,
				TEXT("MaterialGraph query selectors require candidate confirmation before execute."),
				FString::Printf(TEXT("ops[%d].selector"), Index));
		}
		if (OpName == TEXT("spawn_material_expression"))
		{
			FString NodeKey;
			if (!OpObject->TryGetStringField(TEXT("node_key"), NodeKey) || NodeKey.IsEmpty())
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_graph_schema_invalid"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("spawn_material_expression requires node_key."),
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}

			const TSharedPtr<FJsonValue> SelectorValue = OpObject->TryGetField(TEXT("selector"));
			const FBlueprintHelperMaterialSelectorResolution Resolution =
				FBlueprintHelperMaterialExpressionSelectorResolver::ResolveSelector(SelectorValue, State.AssetPath);
			if (!Resolution.bResolved || Resolution.ClassName.IsEmpty())
			{
				const bool bNeedsCandidate = Resolution.bRequiresCandidateSearch;
				const FString ErrorCode = !Resolution.ErrorCode.IsEmpty()
					? Resolution.ErrorCode
					: (bNeedsCandidate
						? TEXT("material_expression_candidate_confirmation_required")
						: TEXT("material_expression_candidate_expired"));
				const FString ErrorMessage = !Resolution.ErrorMessage.IsEmpty()
					? Resolution.ErrorMessage
					: (bNeedsCandidate
						? TEXT("MaterialGraph query selectors require candidate confirmation before execute.")
						: TEXT("MaterialGraph candidate_id does not resolve to the active preview cache."));
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					ErrorCode,
					Input.bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Preflight,
					ErrorMessage,
					FString::Printf(TEXT("ops[%d].selector"), Index));
			}

			const FString BlockId = [&OpObject, &State]()
			{
				FString Value;
				OpObject->TryGetStringField(TEXT("block_id"), Value);
				return Value.IsEmpty() ? State.CurrentBlockId : Value;
			}();

			if (Input.bDryRun)
			{
				if (State.ExpressionsByNodeKey.Contains(NodeKey) &&
					State.MaterialStrategy != TEXT("replace_owned_graph"))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_owned_anchor_violation"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("MaterialGraph node_key '%s' already exists in owned metadata."), *NodeKey),
						FString::Printf(TEXT("ops[%d].node_key"), Index));
				}
				State.CreatedExpressionCount++;
				State.CreatedExpressions.Add(MakeShared<FJsonValueObject>(
					FBlueprintHelperMaterialGraphFacadePrivate::MakeExpressionJson(
						NodeKey,
						BlockId,
						Resolution.SelectorId,
						nullptr)));
				State.ExpressionsByNodeKey.Add(NodeKey, nullptr);
				State.ExpressionClassNameByNodeKey.Add(NodeKey, Resolution.ClassName);
				State.GeneratedExpressionNodeKeys.Add(NodeKey);
				State.GeneratedExpressionFieldByNodeKey.Add(NodeKey, FString::Printf(TEXT("ops[%d]"), Index));
				continue;
			}

			UClass* ExpressionClass = FBlueprintHelperMaterialGraphFacadePrivate::ResolveExpressionClass(Resolution.ClassName);
			if (!ExpressionClass)
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_expression_selector_unknown"),
					EBlueprintHelperToolStage::Preflight,
					FString::Printf(TEXT("MaterialGraph candidate class is outside the P0 execute allowlist: %s."), *Resolution.ClassName),
					FString::Printf(TEXT("ops[%d].selector"), Index));
			}
			if (State.ExpressionsByNodeKey.Contains(NodeKey))
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_owned_anchor_violation"),
					EBlueprintHelperToolStage::Preflight,
					FString::Printf(TEXT("MaterialGraph node_key '%s' already exists in owned metadata."), *NodeKey),
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}

			UMaterialExpression* Expression = UMaterialEditingLibrary::CreateMaterialExpression(
				State.Material,
				TSubclassOf<UMaterialExpression>(ExpressionClass),
				State.CreatedExpressionCount * 240,
				0);
			if (!Expression)
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_graph_write_failed"),
					EBlueprintHelperToolStage::Execute,
					TEXT("UE failed to create material expression."),
					FString::Printf(TEXT("ops[%d]"), Index));
			}

			const TSharedPtr<FJsonObject>* PropertiesObjectPtr = nullptr;
			if (OpObject->TryGetObjectField(TEXT("properties"), PropertiesObjectPtr) &&
				PropertiesObjectPtr && PropertiesObjectPtr->IsValid())
			{
				FString PropertyErrorCode;
				FString PropertyErrorMessage;
				if (!FBlueprintHelperMaterialGraphFacadePrivate::ApplyProperties(
					Expression,
					*PropertiesObjectPtr,
					PropertyErrorCode,
					PropertyErrorMessage))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						PropertyErrorCode.IsEmpty() ? TEXT("material_expression_property_invalid") : PropertyErrorCode,
						EBlueprintHelperToolStage::Execute,
						PropertyErrorMessage.IsEmpty() ? TEXT("Material expression property assignment failed.") : PropertyErrorMessage,
						FString::Printf(TEXT("ops[%d].properties"), Index));
				}
				State.UpdatedPropertyCount++;
			}

			FBlueprintHelperMaterialGraphFacadePrivate::WriteOwnershipMetadata(Expression, BlockId, NodeKey);
			State.ExpressionsByNodeKey.Add(NodeKey, Expression);
			State.NodeKeyByExpression.Add(Expression, NodeKey);
			if (!BlockId.IsEmpty())
			{
				State.BlockIdByExpression.Add(Expression, BlockId);
			}
			State.CreatedExpressionCount++;
			State.CreatedExpressions.Add(MakeShared<FJsonValueObject>(
				FBlueprintHelperMaterialGraphFacadePrivate::MakeExpressionJson(
					NodeKey,
					BlockId,
					Resolution.SelectorId,
					Expression)));
			State.GeneratedExpressionNodeKeys.Add(NodeKey);
			State.GeneratedExpressionFieldByNodeKey.Add(NodeKey, FString::Printf(TEXT("ops[%d]"), Index));
			continue;
		}
		if (OpName == TEXT("delete_owned_material_expression"))
		{
			FString NodeKey;
			OpObject->TryGetStringField(TEXT("node_key"), NodeKey);
			if (NodeKey.IsEmpty())
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_graph_schema_invalid"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("delete_owned_material_expression requires node_key."),
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}
			UMaterialExpression* Expression = FBlueprintHelperMaterialGraphFacadePrivate::FindExpression(State, NodeKey);
			FString DeleteErrorCode;
			FString DeleteErrorMessage;
			if (!FBlueprintHelperMaterialGraphFacadePrivate::DeleteOwnedExpression(
				State,
				Expression,
				Input.bDryRun,
				DeleteErrorCode,
				DeleteErrorMessage))
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					DeleteErrorCode.IsEmpty() ? TEXT("material_owned_anchor_violation") : DeleteErrorCode,
					EBlueprintHelperToolStage::Preflight,
					DeleteErrorMessage.IsEmpty() ? TEXT("Failed to delete owned MaterialGraph expression.") : DeleteErrorMessage,
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}
			continue;
		}
		if (OpName == TEXT("set_material_expression_properties"))
		{
			FString NodeKey;
			OpObject->TryGetStringField(TEXT("node_key"), NodeKey);
			if (NodeKey.IsEmpty())
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_graph_schema_invalid"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("set_material_expression_properties requires node_key."),
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}
			if (Input.bDryRun)
			{
				State.UpdatedPropertyCount++;
				TSharedRef<FJsonObject> Ref = MakeShared<FJsonObject>();
				Ref->SetStringField(TEXT("node_key"), NodeKey);
				if (UMaterialExpression* DryRunExpression = FBlueprintHelperMaterialGraphFacadePrivate::FindExpression(State, NodeKey))
				{
					Ref->SetStringField(
						TEXT("expression_guid"),
						DryRunExpression->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
					if (const FString* BlockId = State.BlockIdByExpression.Find(DryRunExpression))
					{
						Ref->SetStringField(TEXT("block_id"), *BlockId);
					}
				}
				State.UpdatedProperties.Add(MakeShared<FJsonValueObject>(Ref));
				continue;
			}
			UMaterialExpression* Expression = FBlueprintHelperMaterialGraphFacadePrivate::FindExpression(State, NodeKey);
			if (!Expression)
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_owned_anchor_violation"),
					EBlueprintHelperToolStage::Preflight,
					FString::Printf(TEXT("No owned material expression exists for node_key '%s' in this edit batch."), *NodeKey),
					FString::Printf(TEXT("ops[%d].node_key"), Index));
			}
			const TSharedPtr<FJsonObject>* PropertiesObjectPtr = nullptr;
			if (!OpObject->TryGetObjectField(TEXT("properties"), PropertiesObjectPtr) ||
				!PropertiesObjectPtr || !PropertiesObjectPtr->IsValid())
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_graph_schema_invalid"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("set_material_expression_properties requires properties object."),
				FString::Printf(TEXT("ops[%d].properties"), Index));
			}
			const FString BlockId = State.BlockIdByExpression.FindRef(Expression);
			const TSharedRef<FJsonObject> Before = FBlueprintHelperMaterialGraphFacadePrivate::MakeExpressionJson(
				NodeKey,
				BlockId,
				FString(),
				Expression);
			FString PropertyErrorCode;
			FString PropertyErrorMessage;
			if (!FBlueprintHelperMaterialGraphFacadePrivate::ApplyProperties(
				Expression,
				*PropertiesObjectPtr,
				PropertyErrorCode,
				PropertyErrorMessage))
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					PropertyErrorCode.IsEmpty() ? TEXT("material_expression_property_invalid") : PropertyErrorCode,
					EBlueprintHelperToolStage::Execute,
					PropertyErrorMessage.IsEmpty() ? TEXT("Material expression property assignment failed.") : PropertyErrorMessage,
					FString::Printf(TEXT("ops[%d].properties"), Index));
			}
			State.UpdatedPropertyCount++;
			TSharedRef<FJsonObject> Ref = FBlueprintHelperMaterialGraphFacadePrivate::MakeExpressionJson(
				NodeKey,
				BlockId,
				FString(),
				Expression);
			Ref->SetObjectField(TEXT("before"), Before);
			Ref->SetObjectField(
				TEXT("after"),
				FBlueprintHelperMaterialGraphFacadePrivate::MakeExpressionJson(NodeKey, BlockId, FString(), Expression));
			State.UpdatedProperties.Add(MakeShared<FJsonValueObject>(Ref));
			continue;
		}
		if (OpName == TEXT("connect_material_expression") || OpName == TEXT("connect_material_property"))
		{
			FString FromNodeKey;
			FString FromPin;
			FString ToNodeKey;
			FString ToPin;
			if (!FBlueprintHelperMaterialGraphFacadePrivate::ReadEndpoint(OpObject, TEXT("from"), FromNodeKey, FromPin) ||
				!FBlueprintHelperMaterialGraphFacadePrivate::ReadEndpoint(OpObject, TEXT("to"), ToNodeKey, ToPin))
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_connection_schema_invalid"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("Material connection ops require from/to endpoints with node_key and pin."),
					FString::Printf(TEXT("ops[%d]"), Index));
			}

			if (Input.bDryRun)
			{
				if (!State.ExpressionsByNodeKey.Contains(FromNodeKey))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_owned_anchor_violation"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("No owned material expression exists for from.node_key '%s' in this edit batch."), *FromNodeKey),
						FString::Printf(TEXT("ops[%d].from.node_key"), Index));
				}
				if (ToNodeKey == TEXT("$material_output"))
				{
					EMaterialProperty MaterialProperty = MP_BaseColor;
					if (!FBlueprintHelperMaterialGraphFacadePrivate::ResolveMaterialProperty(ToPin, MaterialProperty))
					{
						return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
							TEXT("material_property_not_supported"),
							EBlueprintHelperToolStage::Preflight,
							FString::Printf(TEXT("Unsupported material output property: %s."), *ToPin),
							FString::Printf(TEXT("ops[%d].to.pin"), Index));
					}
				}
				UMaterialExpression* FromExpression =
					FBlueprintHelperMaterialGraphFacadePrivate::ResolveExpressionForPinValidation(State, FromNodeKey);
				FString EngineFromPin;
				FString PinErrorMessage;
				if (!FBlueprintHelperMaterialGraphFacadePrivate::NormalizeExpressionOutputPin(
					FromExpression,
					FromPin,
					EngineFromPin,
					PinErrorMessage))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_pin_not_found"),
						EBlueprintHelperToolStage::Preflight,
						PinErrorMessage,
						FString::Printf(TEXT("ops[%d].from.pin"), Index));
				}
				if (ToNodeKey == TEXT("$material_output"))
				{
					EMaterialProperty MaterialProperty = MP_BaseColor;
					if (!FBlueprintHelperMaterialGraphFacadePrivate::ResolveMaterialProperty(ToPin, MaterialProperty))
					{
						return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
							TEXT("material_property_not_supported"),
							EBlueprintHelperToolStage::Preflight,
							FString::Printf(TEXT("Unsupported material output property: %s."), *ToPin),
							FString::Printf(TEXT("ops[%d].to.pin"), Index));
					}
				}
				else if (!State.ExpressionsByNodeKey.Contains(ToNodeKey))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_owned_anchor_violation"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("No owned material expression exists for to.node_key '%s' in this edit batch."), *ToNodeKey),
						FString::Printf(TEXT("ops[%d].to.node_key"), Index));
				}
				if (ToNodeKey == TEXT("$material_output"))
				{
					State.MaterialOutputConnectionCount++;
				}
				else
				{
					State.ExpressionConnectionCount++;
				}
				FBlueprintHelperMaterialGraphFacadePrivate::RecordPlannedConnection(
					State,
					FromNodeKey,
					FromPin,
					ToNodeKey,
					ToPin,
					FString::Printf(TEXT("ops[%d]"), Index));
				TSharedRef<FJsonObject> Connection = MakeShared<FJsonObject>();
				Connection->SetStringField(
					TEXT("kind"),
					ToNodeKey == TEXT("$material_output") ? TEXT("material_output_link") : TEXT("material_expression_link"));
				Connection->SetStringField(TEXT("from_node_key"), FromNodeKey);
				Connection->SetStringField(TEXT("from_pin"), FromPin);
				Connection->SetStringField(TEXT("to_node_key"), ToNodeKey);
				Connection->SetStringField(TEXT("to_pin"), ToPin);
				State.Connections.Add(MakeShared<FJsonValueObject>(Connection));
				if (ToNodeKey == TEXT("$material_output"))
				{
					TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
					Output->SetStringField(TEXT("property"), ToPin);
					Output->SetStringField(TEXT("source_node_key"), FromNodeKey);
					Output->SetStringField(TEXT("source_pin"), FromPin);
					Output->SetStringField(TEXT("kind"), TEXT("material_output_link"));
					State.MaterialOutputs.Add(MakeShared<FJsonValueObject>(Output));
				}
				continue;
			}

			UMaterialExpression* FromExpression = FBlueprintHelperMaterialGraphFacadePrivate::FindExpression(State, FromNodeKey);
			if (!FromExpression)
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_owned_anchor_violation"),
					EBlueprintHelperToolStage::Preflight,
					FString::Printf(TEXT("No owned material expression exists for from.node_key '%s' in this edit batch."), *FromNodeKey),
					FString::Printf(TEXT("ops[%d].from.node_key"), Index));
			}

			bool bConnected = false;
			FString EngineFromPin;
			FString PinErrorMessage;
			if (!FBlueprintHelperMaterialGraphFacadePrivate::NormalizeExpressionOutputPin(
				FromExpression,
				FromPin,
				EngineFromPin,
				PinErrorMessage))
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_pin_not_found"),
					EBlueprintHelperToolStage::Preflight,
					PinErrorMessage,
					FString::Printf(TEXT("ops[%d].from.pin"), Index));
			}
			if (ToNodeKey == TEXT("$material_output"))
			{
				EMaterialProperty MaterialProperty = MP_BaseColor;
				if (!FBlueprintHelperMaterialGraphFacadePrivate::ResolveMaterialProperty(ToPin, MaterialProperty))
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_property_not_supported"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("Unsupported material output property: %s."), *ToPin),
						FString::Printf(TEXT("ops[%d].to.pin"), Index));
				}
				bConnected = UMaterialEditingLibrary::ConnectMaterialProperty(FromExpression, EngineFromPin, MaterialProperty);
				if (bConnected)
				{
					State.MaterialOutputConnectionCount++;
				}
			}
			else
			{
				UMaterialExpression* ToExpression = FBlueprintHelperMaterialGraphFacadePrivate::FindExpression(State, ToNodeKey);
				if (!ToExpression)
				{
					return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
						TEXT("material_owned_anchor_violation"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("No owned material expression exists for to.node_key '%s' in this edit batch."), *ToNodeKey),
						FString::Printf(TEXT("ops[%d].to.node_key"), Index));
				}
				bConnected = UMaterialEditingLibrary::ConnectMaterialExpressions(
					FromExpression,
					EngineFromPin,
					ToExpression,
					ToPin);
				if (bConnected)
				{
					State.ExpressionConnectionCount++;
				}
			}

			if (!bConnected)
			{
				return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
					TEXT("material_pin_not_found"),
					EBlueprintHelperToolStage::Execute,
					TEXT("UE failed to connect material expression pins."),
					FString::Printf(TEXT("ops[%d]"), Index));
			}

			FBlueprintHelperMaterialGraphFacadePrivate::RecordPlannedConnection(
				State,
				FromNodeKey,
				FromPin,
				ToNodeKey,
				ToPin,
				FString::Printf(TEXT("ops[%d]"), Index));
			TSharedRef<FJsonObject> Connection = MakeShared<FJsonObject>();
			Connection->SetStringField(
				TEXT("kind"),
				ToNodeKey == TEXT("$material_output") ? TEXT("material_output_link") : TEXT("material_expression_link"));
			Connection->SetStringField(TEXT("from_node_key"), FromNodeKey);
			Connection->SetStringField(TEXT("from_pin"), FromPin);
			Connection->SetStringField(TEXT("to_node_key"), ToNodeKey);
			Connection->SetStringField(TEXT("to_pin"), ToPin);
			State.Connections.Add(MakeShared<FJsonValueObject>(Connection));
			if (ToNodeKey == TEXT("$material_output"))
			{
				TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
				Output->SetStringField(TEXT("property"), ToPin);
				Output->SetStringField(TEXT("source_node_key"), FromNodeKey);
				Output->SetStringField(TEXT("source_pin"), FromPin);
				Output->SetStringField(TEXT("kind"), TEXT("material_output_link"));
				State.MaterialOutputs.Add(MakeShared<FJsonValueObject>(Output));
			}
			continue;
		}
		if (OpName == TEXT("compile_material"))
		{
			if (!Input.bDryRun)
			{
				UMaterialEditingLibrary::RecompileMaterial(State.Material);
			}
			continue;
		}

		return FBlueprintHelperMaterialGraphFacadePrivate::MakeFailure(
			TEXT("material_graph_runtime_not_implemented"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Unsupported MaterialGraph op in runtime: %s."), *OpName),
			FString::Printf(TEXT("ops[%d].op"), Index));
	}

	if (!Input.bDryRun)
	{
		UMaterialEditingLibrary::RecompileMaterial(State.Material);
		FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);
		FBlueprintHelperMaterialGraphReadbackService::ValidateExecutedConnections(State);
		State.CompileDiagnostics = FBlueprintHelperMaterialGraphCompileService::CollectExpressionDiagnostics(
			State.Material,
			State.NodeKeyByExpression);
		State.Material->PostEditChange();
		State.Material->MarkPackageDirty();
	}
	else
	{
		FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(State);
	}

	return FBlueprintHelperMaterialGraphFacadePrivate::BuildResult(State, Input.bDryRun);
}
