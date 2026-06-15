// BlueprintHelper MaterialGraph execution readback service.

#include "Systems/ToolClusters/MaterialGraph/BlueprintHelperMaterialGraphReadbackService.h"

#include "EdGraph/EdGraphPin.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialEditingLibrary.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpression.h"
#include "Shared/BlueprintHelperVersionCompat.h"

class FBlueprintHelperMaterialGraphReadbackServicePrivate
{
public:
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

		for (int32 InputIndex = 0; InputIndex < FBlueprintHelperVersionCompat::CountMaterialExpressionInputs(Expression); ++InputIndex)
		{
			if (Expression->GetInputName(InputIndex).ToString() == InputName)
			{
				OutInputIndex = InputIndex;
				return Expression->GetInput(InputIndex);
			}
		}
		return nullptr;
	}

	static FBlueprintHelperDiagnosticItem MakeConnectivityDiagnostic(
		const FString& Code,
		const FBlueprintHelperMaterialGraphPlannedConnection& Connection,
		const FString& Message)
	{
		FBlueprintHelperDiagnosticItem Diagnostic;
		Diagnostic.Severity = EBlueprintHelperDiagnosticSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = Message;
		Diagnostic.GraphName = TEXT("MaterialGraph");
		Diagnostic.TargetKey = Connection.bMaterialOutput ? Connection.ToPin : Connection.ToNodeKey;
		Diagnostic.PinName = Connection.bMaterialOutput ? Connection.FromPin : Connection.ToPin;
		Diagnostic.Field = Connection.FieldPath;
		return Diagnostic;
	}

	static void AddConnectivityDiagnostic(
		FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& Code,
		const FBlueprintHelperMaterialGraphPlannedConnection& Connection,
		const FString& Message)
	{
		State.ConnectivityDiagnostics.Add(MakeConnectivityDiagnostic(Code, Connection, Message));
	}

	static bool IsGraphOutputLinked(
		const UMaterialGraph* MaterialGraph,
		EMaterialProperty MaterialProperty,
		const UMaterialExpression* FromExpression,
		int32 ExpectedOutputIndex)
	{
		if (!MaterialGraph || !MaterialGraph->RootNode || !FromExpression || !FromExpression->GraphNode)
		{
			return false;
		}

		int32 MaterialInputIndex = INDEX_NONE;
		for (int32 Index = 0; Index < MaterialGraph->MaterialInputs.Num(); ++Index)
		{
			if (MaterialGraph->MaterialInputs[Index].GetProperty() == MaterialProperty)
			{
				MaterialInputIndex = Index;
				break;
			}
		}
		if (MaterialInputIndex == INDEX_NONE)
		{
			return false;
		}

		UEdGraphPin* InputPin = MaterialGraph->RootNode->GetInputPin(MaterialInputIndex);
		if (!InputPin)
		{
			return false;
		}

		for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
		{
			if (LinkedPin &&
				LinkedPin->GetOwningNode() == FromExpression->GraphNode &&
				LinkedPin->SourceIndex == ExpectedOutputIndex)
			{
				return true;
			}
		}
		return false;
	}

	static bool IsGraphExpressionInputLinked(
		const UMaterialExpression* FromExpression,
		int32 ExpectedOutputIndex,
		const UMaterialExpression* ToExpression,
		int32 InputIndex)
	{
		const UMaterialGraphNode* ToGraphNode = ToExpression
			? Cast<UMaterialGraphNode>(ToExpression->GraphNode)
			: nullptr;
		if (!FromExpression || !FromExpression->GraphNode || !ToGraphNode)
		{
			return false;
		}

		for (UEdGraphPin* InputPin : ToGraphNode->Pins)
		{
			if (!InputPin ||
				InputPin->Direction != EGPD_Input ||
				InputPin->SourceIndex != InputIndex)
			{
				continue;
			}

			for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
			{
				if (LinkedPin &&
					LinkedPin->GetOwningNode() == FromExpression->GraphNode &&
					LinkedPin->SourceIndex == ExpectedOutputIndex)
				{
					return true;
				}
			}
		}
		return false;
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
		if (NormalizedClassName == TEXT("MaterialExpressionTextureSampleParameter"))
		{
			return UMaterialExpressionTextureSampleParameter::StaticClass();
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

	static UMaterialExpression* ResolveExpressionForPinValidation(
		const FBlueprintHelperMaterialGraphExecutionState& State,
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

	static bool IsPlannedConnectionPinValid(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		const FBlueprintHelperMaterialGraphPlannedConnection& Connection,
		FString& OutCode,
		FString& OutMessage)
	{
		UMaterialExpression* FromExpression = ResolveExpressionForPinValidation(State, Connection.FromNodeKey);
		if (!FromExpression)
		{
			OutCode = TEXT("material_connectivity_violation");
			OutMessage = FString::Printf(
				TEXT("MaterialGraph planned source '%s' is not available for pin validation."),
				*Connection.FromNodeKey);
			return false;
		}

		FString EngineFromPin;
		FString PinErrorMessage;
		if (!NormalizeExpressionOutputPin(
			FromExpression,
			Connection.FromPin,
			EngineFromPin,
			PinErrorMessage))
		{
			OutCode = TEXT("material_pin_not_found");
			OutMessage = PinErrorMessage;
			return false;
		}

		if (Connection.bMaterialOutput)
		{
			EMaterialProperty MaterialProperty = MP_BaseColor;
			if (!ResolveMaterialProperty(Connection.ToPin, MaterialProperty))
			{
				OutCode = TEXT("material_property_not_supported");
				OutMessage = FString::Printf(TEXT("Unsupported material output property: %s."), *Connection.ToPin);
				return false;
			}
			return true;
		}

		UMaterialExpression* ToExpression = ResolveExpressionForPinValidation(State, Connection.ToNodeKey);
		int32 InputIndex = INDEX_NONE;
		if (!ToExpression || !FindExpressionInputByName(ToExpression, Connection.ToPin, InputIndex))
		{
			OutCode = TEXT("material_pin_not_found");
			OutMessage = FString::Printf(
				TEXT("Material expression input pin '%s.%s' was not found."),
				*Connection.ToNodeKey,
				*Connection.ToPin);
			return false;
		}
		return true;
	}

	static void ValidatePlannedConnectionPins(FBlueprintHelperMaterialGraphExecutionState& State)
	{
		for (const FBlueprintHelperMaterialGraphPlannedConnection& Connection : State.PlannedConnections)
		{
			FString Code;
			FString Message;
			if (IsPlannedConnectionPinValid(State, Connection, Code, Message))
			{
				continue;
			}
			AddConnectivityDiagnostic(
				State,
				Code.IsEmpty() ? TEXT("material_connectivity_violation") : Code,
				Connection,
				Message.IsEmpty() ? TEXT("MaterialGraph planned connection pin validation failed.") : Message);
		}
	}

	static bool IsMaterialOutputSourceExpression(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		UMaterialExpression* Expression)
	{
		if (!State.Material || !Expression)
		{
			return false;
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
			if (UMaterialEditingLibrary::GetMaterialPropertyInputNode(State.Material, MaterialProperty) == Expression)
			{
				return true;
			}
		}
		return false;
	}

	static bool CurrentMaterialExpressionReachesOutput(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		UMaterialExpression* Expression,
		TSet<UMaterialExpression*>& VisitedExpressions)
	{
		if (!State.Material || !Expression)
		{
			return false;
		}
		if (VisitedExpressions.Contains(Expression))
		{
			return false;
		}
		VisitedExpressions.Add(Expression);

		if (IsMaterialOutputSourceExpression(State, Expression))
		{
			return true;
		}

		for (UMaterialExpression* CandidateExpression : State.Material->GetExpressions())
		{
			if (!CandidateExpression || CandidateExpression == Expression)
			{
				continue;
			}
			for (int32 InputIndex = 0; InputIndex < FBlueprintHelperVersionCompat::CountMaterialExpressionInputs(CandidateExpression); ++InputIndex)
			{
				FExpressionInput* Input = CandidateExpression->GetInput(InputIndex);
				if (Input && Input->Expression == Expression &&
					CurrentMaterialExpressionReachesOutput(State, CandidateExpression, VisitedExpressions))
				{
					return true;
				}
			}
		}
		return false;
	}

	static bool PlannedNodeReachesMaterialOutput(
		const FBlueprintHelperMaterialGraphExecutionState& State,
		const FString& NodeKey,
		TSet<FString>& VisitedNodeKeys)
	{
		if (NodeKey.IsEmpty() || VisitedNodeKeys.Contains(NodeKey))
		{
			return false;
		}
		VisitedNodeKeys.Add(NodeKey);

		for (const FBlueprintHelperMaterialGraphPlannedConnection& Connection : State.PlannedConnections)
		{
			if (Connection.FromNodeKey != NodeKey)
			{
				continue;
			}
			FString PinValidationCode;
			FString PinValidationMessage;
			if (!IsPlannedConnectionPinValid(State, Connection, PinValidationCode, PinValidationMessage))
			{
				continue;
			}
			if (Connection.bMaterialOutput)
			{
				return true;
			}
			if (PlannedNodeReachesMaterialOutput(State, Connection.ToNodeKey, VisitedNodeKeys))
			{
				return true;
			}
			UMaterialExpression* ToExpression = FindExpression(State, Connection.ToNodeKey);
			TSet<UMaterialExpression*> VisitedExpressions;
			if (CurrentMaterialExpressionReachesOutput(State, ToExpression, VisitedExpressions))
			{
				return true;
			}
		}
		return false;
	}
};

void FBlueprintHelperMaterialGraphReadbackService::ValidatePlannedExpressionConsumption(
	FBlueprintHelperMaterialGraphExecutionState& State)
{
	FBlueprintHelperMaterialGraphReadbackServicePrivate::ValidatePlannedConnectionPins(State);

	for (const FString& NodeKey : State.GeneratedExpressionNodeKeys)
	{
		if (NodeKey.IsEmpty() || State.DeletedExpressionNodeKeys.Contains(NodeKey))
		{
			continue;
		}

		TSet<FString> VisitedNodeKeys;
		if (FBlueprintHelperMaterialGraphReadbackServicePrivate::PlannedNodeReachesMaterialOutput(
			State,
			NodeKey,
			VisitedNodeKeys))
		{
			continue;
		}

		FBlueprintHelperMaterialGraphPlannedConnection Connection;
		Connection.FromNodeKey = NodeKey;
		Connection.FieldPath = State.GeneratedExpressionFieldByNodeKey.FindRef(NodeKey);
		FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
			State,
			TEXT("material_unconsumed_expression"),
			Connection,
			FString::Printf(
				TEXT("MaterialGraph generated expression '%s' has no path to a material output."),
				*NodeKey));
	}
}

void FBlueprintHelperMaterialGraphReadbackService::ValidateExecutedConnections(
	FBlueprintHelperMaterialGraphExecutionState& State)
{
	if (!State.Material || State.PlannedConnections.Num() == 0)
	{
		return;
	}

	if (!State.Material->MaterialGraph)
	{
		State.Material->MaterialGraph = CastChecked<UMaterialGraph>(FBlueprintEditorUtils::CreateNewGraph(
			State.Material,
			NAME_None,
			UMaterialGraph::StaticClass(),
			UMaterialGraphSchema::StaticClass()));
	}
	if (State.Material->MaterialGraph)
	{
		State.Material->MaterialGraph->Material = State.Material;
		State.Material->MaterialGraph->RebuildGraph();
	}

	for (const FBlueprintHelperMaterialGraphPlannedConnection& Connection : State.PlannedConnections)
	{
		UMaterialExpression* FromExpression =
			FBlueprintHelperMaterialGraphReadbackServicePrivate::FindExpression(State, Connection.FromNodeKey);
		if (!FromExpression)
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_connectivity_violation"),
				Connection,
				FString::Printf(
					TEXT("MaterialGraph planned connection source '%s' is not available after mutation."),
					*Connection.FromNodeKey));
			continue;
		}

		FString EngineFromPin;
		FString PinErrorMessage;
		if (!FBlueprintHelperMaterialGraphReadbackServicePrivate::NormalizeExpressionOutputPin(
			FromExpression,
			Connection.FromPin,
			EngineFromPin,
			PinErrorMessage))
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_connectivity_violation"),
				Connection,
				PinErrorMessage);
			continue;
		}

		const int32 ExpectedOutputIndex =
			FBlueprintHelperMaterialGraphReadbackServicePrivate::ResolveExpressionOutputIndex(
				FromExpression,
				EngineFromPin);
		if (ExpectedOutputIndex == INDEX_NONE)
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_connectivity_violation"),
				Connection,
				FString::Printf(
					TEXT("MaterialGraph planned source pin '%s' resolved to engine pin '%s', but no output index was found."),
					*Connection.FromPin,
					*EngineFromPin));
			continue;
		}

		if (Connection.bMaterialOutput)
		{
			EMaterialProperty MaterialProperty = MP_BaseColor;
			if (!FBlueprintHelperMaterialGraphReadbackServicePrivate::ResolveMaterialProperty(
				Connection.ToPin,
				MaterialProperty))
			{
				FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
					State,
					TEXT("material_property_not_supported"),
					Connection,
					FString::Printf(TEXT("Unsupported material output property: %s."), *Connection.ToPin));
				continue;
			}

			UMaterialExpression* ActualExpression =
				UMaterialEditingLibrary::GetMaterialPropertyInputNode(State.Material, MaterialProperty);
			const FString ActualPin =
				UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(State.Material, MaterialProperty);
			if (ActualExpression != FromExpression ||
				!FBlueprintHelperMaterialGraphReadbackServicePrivate::AreEnginePinsEquivalent(
					EngineFromPin,
					ActualPin))
			{
				FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
					State,
					TEXT("material_connectivity_violation"),
					Connection,
					FString::Printf(
						TEXT("MaterialGraph expected %s.%s -> $material_output.%s, but readback did not match."),
						*Connection.FromNodeKey,
						*Connection.FromPin,
						*Connection.ToPin));
				continue;
			}

			State.VerifiedConnectionCount++;
			if (!State.Material->MaterialGraph)
			{
				continue;
			}
			if (FBlueprintHelperMaterialGraphReadbackServicePrivate::IsGraphOutputLinked(
				State.Material->MaterialGraph,
				MaterialProperty,
				FromExpression,
				ExpectedOutputIndex))
			{
				State.GraphSyncConnectionCount++;
			}
			else
			{
				FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
					State,
					TEXT("material_graph_sync_violation"),
					Connection,
					FString::Printf(
						TEXT("MaterialGraph data connection %s.%s -> $material_output.%s exists, but graph pins are not linked."),
						*Connection.FromNodeKey,
						*Connection.FromPin,
						*Connection.ToPin));
			}
			continue;
		}

		UMaterialExpression* ToExpression =
			FBlueprintHelperMaterialGraphReadbackServicePrivate::FindExpression(State, Connection.ToNodeKey);
		int32 InputIndex = INDEX_NONE;
		FExpressionInput* ActualInput =
			FBlueprintHelperMaterialGraphReadbackServicePrivate::FindExpressionInputByName(
				ToExpression,
				Connection.ToPin,
				InputIndex);
		if (!ToExpression || !ActualInput)
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_connectivity_violation"),
				Connection,
				FString::Printf(
					TEXT("MaterialGraph expected target input '%s.%s' is not available after mutation."),
					*Connection.ToNodeKey,
					*Connection.ToPin));
			continue;
		}

		if (ActualInput->Expression != FromExpression || ActualInput->OutputIndex != ExpectedOutputIndex)
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_connectivity_violation"),
				Connection,
				FString::Printf(
					TEXT("MaterialGraph expected %s.%s -> %s.%s, but readback did not match."),
					*Connection.FromNodeKey,
					*Connection.FromPin,
					*Connection.ToNodeKey,
					*Connection.ToPin));
			continue;
		}

		State.VerifiedConnectionCount++;
		if (!State.Material->MaterialGraph)
		{
			continue;
		}
		if (FBlueprintHelperMaterialGraphReadbackServicePrivate::IsGraphExpressionInputLinked(
			FromExpression,
			ExpectedOutputIndex,
			ToExpression,
			InputIndex))
		{
			State.GraphSyncConnectionCount++;
		}
		else
		{
			FBlueprintHelperMaterialGraphReadbackServicePrivate::AddConnectivityDiagnostic(
				State,
				TEXT("material_graph_sync_violation"),
				Connection,
				FString::Printf(
					TEXT("MaterialGraph data connection %s.%s -> %s.%s exists, but graph pins are not linked."),
					*Connection.FromNodeKey,
					*Connection.FromPin,
					*Connection.ToNodeKey,
					*Connection.ToPin));
		}
	}
}
