// BlueprintHelper Service Layer - internal Blueprint function/event signature service.

#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/SharedServices/Utils/BlueprintHelperPinTypeSpecUtils.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperBlockIdService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperOwnershipService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureMutationUtils.h"
#include "Systems/ToolClusters/BlueprintSignature/Utils/BlueprintHelperSignatureReferenceContextUtils.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Shared/Safety/BlueprintHelperDependencyAnalysisService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"

class FBlueprintHelperSignatureServiceLocalUtils
{
public:
	struct FBlueprintHelperSignaturePinSpec
	{
		FString Name;
		FEdGraphPinType PinType;
	};

	struct FBlueprintHelperFunctionSignaturePinSnapshot
	{
		FString Name;
		FEdGraphPinType PinType;
		FString Direction;
	};

	struct FBlueprintHelperFunctionSignatureDiff
	{
		bool bMatches = true;
		TArray<TSharedPtr<FJsonValue>> Differences;
	};

	struct FBlueprintHelperResolvedCustomEventTarget
	{
		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		UK2Node_CustomEvent* ExistingEvent = nullptr;
		UEdGraph* ExistingEventGraph = nullptr;
		TArray<FBlueprintHelperSignaturePinSpec> RequestedPins;
		TArray<FBlueprintHelperSignaturePinSpec> MissingPins;
		bool bSignatureMatches = false;
	};

	static FBlueprintHelperToolError MakeSignatureError(
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

	static FBlueprintHelperToolResultBase MakeSignatureFailure(
		const FString& Operation,
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field)
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeSignatureError(Code, Stage, Message, Field));
	}

	static FBlueprintHelperGraphTarget MakeGraphTarget(const FString& AssetPath)
	{
		FBlueprintHelperGraphTarget Target;
		Target.BlueprintPath = AssetPath;
		return Target;
	}

	static FBlueprintHelperGraphTarget MakeGraphTarget(const FString& AssetPath, const FString& GraphName)
	{
		FBlueprintHelperGraphTarget Target;
		Target.BlueprintPath = AssetPath;
		Target.GraphName = GraphName;
		return Target;
	}

	static bool IsValidNameCollisionPolicy(const FString& Policy)
	{
		return Policy.IsEmpty() ||
			Policy == TEXT("reuse_if_exists") ||
			Policy == TEXT("fail_if_exists");
	}

	static FBlueprintHelperValidationSummary MakeSignatureValidation(bool bShouldCompile, bool bShouldSave)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = bShouldCompile;
		Validation.bShouldSave = bShouldSave;
		return Validation;
	}

	static TSharedRef<FJsonObject> MakeSignatureResultData(
		bool bDryRun,
		const TCHAR* ResultField,
		bool bDeferredToGraphWrite)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintSignature.v1"));
		if (bDryRun)
		{
			Data->SetBoolField(TEXT("dry_run"), true);
		}

		TSharedRef<FJsonObject> InnerResult = MakeShared<FJsonObject>();
		InnerResult->SetBoolField(TEXT("success"), true);
		if (bDeferredToGraphWrite)
		{
			InnerResult->SetBoolField(TEXT("deferred_to_graph_write"), true);
		}
		Data->SetObjectField(FString(ResultField), InnerResult);
		return Data;
	}

	static TSharedRef<FJsonObject> MakeSignatureBlockedData(
		bool bDryRun,
		const FString& Code,
		const FString& Message,
		const FString& Field,
		const TCHAR* ResultField = TEXT("remove_signature_result"))
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintSignature.v1"));

		if (bDryRun)
		{
			Data->SetBoolField(TEXT("dry_run"), true);
		}

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), false);

		TArray<TSharedPtr<FJsonValue>> BlockedBy;
		BlockedBy.Add(MakeShared<FJsonValueString>(Code));
		DryRun->SetArrayField(TEXT("blocked_by"), BlockedBy);

		TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
		Issue->SetStringField(TEXT("code"), Code);
		Issue->SetStringField(TEXT("message"), Message);
		Issue->SetStringField(TEXT("source"), Field);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueObject>(Issue));
		DryRun->SetArrayField(TEXT("errors"), Errors);

		Data->SetObjectField(TEXT("dry_run"), DryRun);

		TSharedRef<FJsonObject> BlockedResult = MakeShared<FJsonObject>();
		BlockedResult->SetBoolField(TEXT("success"), false);
		BlockedResult->SetBoolField(TEXT("supported"), false);
		Data->SetObjectField(FString(ResultField), BlockedResult);
		return Data;
	}

	static void SetSignatureResultBool(
		const TSharedPtr<FJsonObject>& Data,
		const TCHAR* ResultField,
		const TCHAR* FieldName,
		bool bValue)
	{
		const TSharedPtr<FJsonObject>* InnerResult = nullptr;
		if (Data.IsValid() &&
			Data->TryGetObjectField(ResultField, InnerResult) &&
			InnerResult && InnerResult->IsValid())
		{
			(*InnerResult)->SetBoolField(FieldName, bValue);
		}
	}

	static void SetSignatureResultNumber(
		const TSharedPtr<FJsonObject>& Data,
		const TCHAR* ResultField,
		const TCHAR* FieldName,
		int32 Value)
	{
		const TSharedPtr<FJsonObject>* InnerResult = nullptr;
		if (Data.IsValid() &&
			Data->TryGetObjectField(ResultField, InnerResult) &&
			InnerResult && InnerResult->IsValid())
		{
			(*InnerResult)->SetNumberField(FieldName, Value);
		}
	}

	static void SetSignatureResultString(
		const TSharedPtr<FJsonObject>& Data,
		const TCHAR* ResultField,
		const TCHAR* FieldName,
		const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return;
		}

		const TSharedPtr<FJsonObject>* InnerResult = nullptr;
		if (Data.IsValid() &&
			Data->TryGetObjectField(ResultField, InnerResult) &&
			InnerResult && InnerResult->IsValid())
		{
			(*InnerResult)->SetStringField(FieldName, Value);
		}
	}

	static void SetFunctionTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& FunctionName)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Function;
		Result.Target->Function = FunctionName;
	}

	static void SetCustomEventTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& GraphName,
		const FString& EventName)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::CustomEvent;
		Result.Target->Graph = GraphName;
		Result.Target->Event = EventName;
	}

	static void SetRemoveSignatureTarget(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperRemoveSignatureRequest& Request)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = Request.AssetPath;
		Result.Target->Graph = Request.GraphName;
		if (Request.SignatureKind == TEXT("function") || Request.SignatureKind == TEXT("interface_function"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Function;
			Result.Target->Function = Request.SignatureName;
		}
		else if (Request.SignatureKind == TEXT("custom_event") || Request.SignatureKind == TEXT("interface_event"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::CustomEvent;
			Result.Target->Event = Request.SignatureName;
		}
		else if (Request.SignatureKind == TEXT("event_dispatcher"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Property;
			Result.Target->PropertyPath = Request.SignatureName;
		}
		else if (Request.SignatureKind == TEXT("override_event") || Request.SignatureKind == TEXT("native_event"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Event;
			Result.Target->Event = Request.SignatureName;
		}
		else
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Asset;
			Result.Target->Event = Request.SignatureName;
		}
	}

	static void SetEventDispatcherTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& DispatcherName)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Property;
		Result.Target->PropertyPath = DispatcherName;
	}

	static void SetOverrideEventTarget(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = Request.AssetPath;
		Result.Target->Graph = Request.GraphName;
		Result.Target->TargetType = EBlueprintHelperTargetType::Event;
		Result.Target->Event = Request.EventName;
	}

	static bool FunctionExists(
		const FBlueprintHelperBlueprintStructureService& StructureService,
		const FString& AssetPath,
		const FString& FunctionName,
		FString& OutError)
	{
		const FBlueprintHelperListGraphsResult Graphs = StructureService.ListGraphs(MakeGraphTarget(AssetPath));
		if (!Graphs.bSuccess)
		{
			OutError = Graphs.ErrorMessage.IsEmpty()
				? FString::Printf(TEXT("Unable to list Blueprint graphs for %s."), *AssetPath)
				: Graphs.ErrorMessage;
			return false;
		}

		for (const FBlueprintHelperGraphInfo& Graph : Graphs.Graphs)
		{
			if (Graph.GraphType == TEXT("Function") && Graph.Name == FunctionName)
			{
				return true;
			}
		}
		return false;
	}

	static bool BlueprintCanBeListed(
		const FBlueprintHelperBlueprintStructureService& StructureService,
		const FString& AssetPath,
		FString& OutError)
	{
		const FBlueprintHelperListGraphsResult Graphs = StructureService.ListGraphs(MakeGraphTarget(AssetPath));
		if (Graphs.bSuccess)
		{
			return true;
		}
		OutError = Graphs.ErrorMessage.IsEmpty()
			? FString::Printf(TEXT("Unable to list Blueprint graphs for %s."), *AssetPath)
			: Graphs.ErrorMessage;
		return false;
	}

	static bool TryFindEventDispatcher(
		const FBlueprintHelperBlueprintStructureService& StructureService,
		const FString& AssetPath,
		const FString& DispatcherName,
		FBlueprintHelperEventDispatcherInfo& OutDispatcher,
		FString& OutError)
	{
		const FBlueprintHelperListDispatchersResult Dispatchers = StructureService.ListEventDispatchers(MakeGraphTarget(AssetPath));
		if (!Dispatchers.bSuccess)
		{
			OutError = Dispatchers.ErrorMessage.IsEmpty()
				? FString::Printf(TEXT("Unable to list Blueprint event dispatchers for %s."), *AssetPath)
				: Dispatchers.ErrorMessage;
			return false;
		}

		for (const FBlueprintHelperEventDispatcherInfo& Dispatcher : Dispatchers.Dispatchers)
		{
			if (Dispatcher.Name.Equals(DispatcherName, ESearchCase::IgnoreCase))
			{
				OutDispatcher = Dispatcher;
				return true;
			}
		}
		return false;
	}

	static bool EventDispatcherSignatureMatches(
		const FBlueprintHelperEventDispatcherInfo& ExistingDispatcher,
		const TArray<FBlueprintHelperSignaturePinSpec>& RequestedPins,
		FString& OutMessage)
	{
		for (const FBlueprintHelperSignaturePinSpec& RequestedPin : RequestedPins)
		{
			bool bFound = false;
			for (const FString& ExistingParam : ExistingDispatcher.Params)
			{
				FString ExistingName;
				FString ExistingCategory;
				if (!ExistingParam.Split(TEXT(":"), &ExistingName, &ExistingCategory))
				{
					ExistingName = ExistingParam;
					ExistingCategory.Reset();
				}

				if (!ExistingName.Equals(RequestedPin.Name, ESearchCase::IgnoreCase))
				{
					continue;
				}

				bFound = ExistingCategory.IsEmpty() || ExistingCategory == RequestedPin.PinType.PinCategory.ToString();
				break;
			}

			if (!bFound)
			{
				OutMessage = FString::Printf(
					TEXT("Existing event dispatcher signature does not contain requested input: %s."),
					*RequestedPin.Name);
				return false;
			}
		}
		return true;
	}

	static bool EventDispatcherSignatureExactlyMatches(
		const FBlueprintHelperEventDispatcherInfo& ExistingDispatcher,
		const TArray<FBlueprintHelperSignaturePinSpec>& RequestedPins,
		FString& OutMessage)
	{
		if (!EventDispatcherSignatureMatches(ExistingDispatcher, RequestedPins, OutMessage))
		{
			return false;
		}
		if (ExistingDispatcher.Params.Num() != RequestedPins.Num())
		{
			OutMessage = FString::Printf(
				TEXT("Existing event dispatcher signature has %d inputs but requested %d."),
				ExistingDispatcher.Params.Num(),
				RequestedPins.Num());
			return false;
		}
		return true;
	}

	static bool TryValidateEventGraphExists(
		const FBlueprintHelperBlueprintStructureService& StructureService,
		const FString& AssetPath,
		const FString& GraphName,
		FString& OutCode,
		EBlueprintHelperToolStage& OutStage,
		FString& OutMessage,
		FString& OutField)
	{
		const FBlueprintHelperListGraphsResult Graphs = StructureService.ListGraphs(MakeGraphTarget(AssetPath));
		if (!Graphs.bSuccess)
		{
			OutCode = TEXT("target_blueprint_not_found");
			OutStage = EBlueprintHelperToolStage::ResolveTarget;
			OutMessage = Graphs.ErrorMessage.IsEmpty()
				? FString::Printf(TEXT("Unable to list Blueprint graphs for %s."), *AssetPath)
				: Graphs.ErrorMessage;
			OutField = TEXT("asset_path");
			return false;
		}

		for (const FBlueprintHelperGraphInfo& Graph : Graphs.Graphs)
		{
			if (Graph.Name == GraphName)
			{
				if (Graph.GraphType != TEXT("EventGraph"))
				{
					OutCode = TEXT("target_graph_type_invalid");
					OutStage = EBlueprintHelperToolStage::Preflight;
					OutMessage = FString::Printf(
						TEXT("Graph is not an EventGraph and cannot host custom event signatures: %s."),
						*GraphName);
					OutField = TEXT("graph_name");
					return false;
				}
				return true;
			}
		}

		OutCode = TEXT("target_graph_not_found");
		OutStage = EBlueprintHelperToolStage::ResolveTarget;
		OutMessage = FString::Printf(TEXT("Blueprint graph not found: %s."), *GraphName);
		OutField = TEXT("graph_name");
		return false;
	}

	static bool TryBuildSignaturePinSpecs(
		const TArray<TSharedPtr<FJsonValue>>& Params,
		TArray<FBlueprintHelperSignaturePinSpec>& OutPins,
		FString& OutMessage,
		FString& OutField,
		const TCHAR* FieldPrefix = TEXT("inputs"),
		FString* OutCode = nullptr)
	{
		OutPins.Reset();
		if (OutCode)
		{
			OutCode->Reset();
		}

		for (int32 Index = 0; Index < Params.Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> ParamObject = Params[Index].IsValid() ? Params[Index]->AsObject() : nullptr;
			if (!ParamObject.IsValid())
			{
				if (OutCode)
				{
					*OutCode = TEXT("invalid_signature_payload");
				}
				OutMessage = TEXT("Signature parameter entries must be objects.");
				OutField = FString::Printf(TEXT("%s[%d]"), FieldPrefix, Index);
				return false;
			}

			FString ParamName;
			if (!ParamObject->TryGetStringField(TEXT("name"), ParamName) || ParamName.IsEmpty())
			{
				if (OutCode)
				{
					*OutCode = TEXT("invalid_signature_payload");
				}
				OutMessage = TEXT("Signature parameter requires name.");
				OutField = FString::Printf(TEXT("%s[%d].name"), FieldPrefix, Index);
				return false;
			}

			if (OutPins.ContainsByPredicate([&ParamName](const FBlueprintHelperSignaturePinSpec& Pin)
				{
					return Pin.Name == ParamName;
				}))
			{
				if (OutCode)
				{
					*OutCode = TEXT("duplicate_signature_pin_name");
				}
				OutMessage = FString::Printf(TEXT("Duplicate signature parameter name: %s."), *ParamName);
				OutField = FString::Printf(TEXT("%s[%d].name"), FieldPrefix, Index);
				return false;
			}

			const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
			if (!ParamObject->TryGetObjectField(TEXT("pin_type"), PinTypeObject) ||
				!PinTypeObject || !PinTypeObject->IsValid())
			{
				if (OutCode)
				{
					*OutCode = TEXT("invalid_pin_type");
				}
				OutMessage = FString::Printf(TEXT("Signature parameter %s requires pin_type."), *ParamName);
				OutField = FString::Printf(TEXT("%s[%d].pin_type"), FieldPrefix, Index);
				return false;
			}

			FEdGraphPinType PinType;
			FBlueprintHelperPinTypeSpecError PinTypeError;
			if (!FBlueprintHelperPinTypeSpecUtils::TryConvertPinTypeObject(
				*PinTypeObject,
				PinType,
				PinTypeError,
				FString::Printf(TEXT("%s[%d].pin_type"), FieldPrefix, Index)))
			{
				if (OutCode)
				{
					*OutCode = PinTypeError.Code.IsEmpty() ? TEXT("invalid_pin_type") : PinTypeError.Code;
				}
				OutMessage = PinTypeError.Message.IsEmpty()
					? FString::Printf(TEXT("Unable to convert signature parameter type: %s."), *ParamName)
					: PinTypeError.Message;
				OutField = PinTypeError.FieldPath.IsEmpty()
					? FString::Printf(TEXT("%s[%d].pin_type"), FieldPrefix, Index)
					: PinTypeError.FieldPath;
				return false;
			}

			FBlueprintHelperSignaturePinSpec PinSpec;
			PinSpec.Name = ParamName;
			PinSpec.PinType = PinType;
			OutPins.Add(PinSpec);
		}

		return true;
	}

	static bool PinTerminalTypesMatch(const FEdGraphTerminalType& Left, const FEdGraphTerminalType& Right)
	{
		const FString LeftObjectPath = Left.TerminalSubCategoryObject.IsValid()
			? Left.TerminalSubCategoryObject->GetPathName()
			: TEXT("");
		const FString RightObjectPath = Right.TerminalSubCategoryObject.IsValid()
			? Right.TerminalSubCategoryObject->GetPathName()
			: TEXT("");

		return Left.TerminalCategory == Right.TerminalCategory &&
			Left.TerminalSubCategory == Right.TerminalSubCategory &&
			LeftObjectPath == RightObjectPath;
	}

	static bool PinTypesMatch(const FEdGraphPinType& Left, const FEdGraphPinType& Right)
	{
		const FString LeftObjectPath = Left.PinSubCategoryObject.IsValid()
			? Left.PinSubCategoryObject->GetPathName()
			: TEXT("");
		const FString RightObjectPath = Right.PinSubCategoryObject.IsValid()
			? Right.PinSubCategoryObject->GetPathName()
			: TEXT("");

		return Left.PinCategory == Right.PinCategory &&
			Left.PinSubCategory == Right.PinSubCategory &&
			LeftObjectPath == RightObjectPath &&
			Left.ContainerType == Right.ContainerType &&
			PinTerminalTypesMatch(Left.PinValueType, Right.PinValueType) &&
			Left.bIsReference == Right.bIsReference &&
			Left.bIsConst == Right.bIsConst;
	}

	static TSharedRef<FJsonObject> PinTerminalTypeToJson(const FEdGraphTerminalType& PinType)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		if (!PinType.TerminalCategory.IsNone())
		{
			Json->SetStringField(TEXT("category"), PinType.TerminalCategory.ToString());
		}
		if (!PinType.TerminalSubCategory.IsNone())
		{
			Json->SetStringField(TEXT("sub_category"), PinType.TerminalSubCategory.ToString());
		}
		if (PinType.TerminalSubCategoryObject.IsValid())
		{
			Json->SetStringField(TEXT("object_path"), PinType.TerminalSubCategoryObject->GetPathName());
		}
		return Json;
	}

	static TSharedRef<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
		if (!PinType.PinSubCategory.IsNone())
		{
			Json->SetStringField(TEXT("sub_category"), PinType.PinSubCategory.ToString());
		}
		if (PinType.PinSubCategoryObject.IsValid())
		{
			Json->SetStringField(TEXT("object_path"), PinType.PinSubCategoryObject->GetPathName());
		}
		if (PinType.ContainerType == EPinContainerType::Array)
		{
			Json->SetStringField(TEXT("container_type"), TEXT("array"));
		}
		else if (PinType.ContainerType == EPinContainerType::Set)
		{
			Json->SetStringField(TEXT("container_type"), TEXT("set"));
		}
		else if (PinType.ContainerType == EPinContainerType::Map)
		{
			Json->SetStringField(TEXT("container_type"), TEXT("map"));
			Json->SetObjectField(TEXT("value_type"), PinTerminalTypeToJson(PinType.PinValueType));
		}
		if (PinType.bIsReference)
		{
			Json->SetBoolField(TEXT("is_reference"), true);
		}
		if (PinType.bIsConst)
		{
			Json->SetBoolField(TEXT("is_const"), true);
		}
		return Json;
	}

	static UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				return Graph;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				return Entry;
			}
		}
		return nullptr;
	}

	static UK2Node_FunctionResult* FindFunctionResult(UEdGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionResult* Result = Cast<UK2Node_FunctionResult>(Node))
			{
				return Result;
			}
		}
		return nullptr;
	}

	static void AppendFunctionUserPinSnapshots(
		const TArray<TSharedPtr<FUserPinInfo>>& Pins,
		const FString& Direction,
		TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& OutSnapshots)
	{
		for (const TSharedPtr<FUserPinInfo>& Pin : Pins)
		{
			if (!Pin.IsValid())
			{
				continue;
			}

			FBlueprintHelperFunctionSignaturePinSnapshot Snapshot;
			Snapshot.Name = Pin->PinName.ToString();
			Snapshot.PinType = Pin->PinType;
			Snapshot.Direction = Direction;
			OutSnapshots.Add(Snapshot);
		}
	}

	static bool BuildExistingFunctionSignature(
		UBlueprint* Blueprint,
		const FString& FunctionName,
		TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& OutInputs,
		TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& OutOutputs,
		FString& OutError)
	{
		OutInputs.Reset();
		OutOutputs.Reset();
		OutError.Reset();

		UEdGraph* FunctionGraph = FindFunctionGraph(Blueprint, FunctionName);
		if (!FunctionGraph)
		{
			OutError = FString::Printf(TEXT("Function graph not found: %s."), *FunctionName);
			return false;
		}

		UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
		if (!Entry)
		{
			OutError = FString::Printf(TEXT("Function entry node not found: %s."), *FunctionName);
			return false;
		}

		AppendFunctionUserPinSnapshots(Entry->UserDefinedPins, TEXT("input"), OutInputs);
		if (UK2Node_FunctionResult* Result = FindFunctionResult(FunctionGraph))
		{
			AppendFunctionUserPinSnapshots(Result->UserDefinedPins, TEXT("output"), OutOutputs);
		}
		return true;
	}

	static const FBlueprintHelperFunctionSignaturePinSnapshot* FindFunctionSnapshotPin(
		const TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& Pins,
		const FString& PinName)
	{
		for (const FBlueprintHelperFunctionSignaturePinSnapshot& Pin : Pins)
		{
			if (Pin.Name == PinName)
			{
				return &Pin;
			}
		}
		return nullptr;
	}

	static void AddFunctionSignatureDifference(
		FBlueprintHelperFunctionSignatureDiff& Diff,
		const FString& PinName,
		const FString& Direction,
		const FEdGraphPinType* Expected,
		const FEdGraphPinType* Actual,
		const FString& Reason)
	{
		TSharedRef<FJsonObject> Difference = MakeShared<FJsonObject>();
		Difference->SetStringField(TEXT("pin_name"), PinName);
		Difference->SetStringField(TEXT("direction"), Direction);
		Difference->SetObjectField(TEXT("expected"), Expected ? PinTypeToJson(*Expected) : MakeShared<FJsonObject>());
		Difference->SetObjectField(TEXT("actual"), Actual ? PinTypeToJson(*Actual) : MakeShared<FJsonObject>());
		Difference->SetStringField(TEXT("reason"), Reason);
		Diff.Differences.Add(MakeShared<FJsonValueObject>(Difference));
		Diff.bMatches = false;
	}

	static void CompareFunctionSignatureDirection(
		const TArray<FBlueprintHelperSignaturePinSpec>& ExpectedPins,
		const TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& ActualPins,
		const FString& Direction,
		FBlueprintHelperFunctionSignatureDiff& Diff)
	{
		for (const FBlueprintHelperSignaturePinSpec& ExpectedPin : ExpectedPins)
		{
			const FBlueprintHelperFunctionSignaturePinSnapshot* ActualPin = FindFunctionSnapshotPin(ActualPins, ExpectedPin.Name);
			if (!ActualPin)
			{
				AddFunctionSignatureDifference(
					Diff,
					ExpectedPin.Name,
					Direction,
					&ExpectedPin.PinType,
					nullptr,
					TEXT("missing_pin"));
				continue;
			}

			if (!PinTypesMatch(ExpectedPin.PinType, ActualPin->PinType))
			{
				AddFunctionSignatureDifference(
					Diff,
					ExpectedPin.Name,
					Direction,
					&ExpectedPin.PinType,
					&ActualPin->PinType,
					TEXT("pin_type_mismatch"));
			}
		}

		for (const FBlueprintHelperFunctionSignaturePinSnapshot& ActualPin : ActualPins)
		{
			if (!ExpectedPins.ContainsByPredicate([&ActualPin](const FBlueprintHelperSignaturePinSpec& ExpectedPin)
				{
					return ExpectedPin.Name == ActualPin.Name;
				}))
			{
				AddFunctionSignatureDifference(
					Diff,
					ActualPin.Name,
					Direction,
					nullptr,
					&ActualPin.PinType,
					TEXT("extra_pin"));
			}
		}
	}

	static FBlueprintHelperFunctionSignatureDiff CompareFunctionSignature(
		const TArray<FBlueprintHelperSignaturePinSpec>& ExpectedInputs,
		const TArray<FBlueprintHelperSignaturePinSpec>& ExpectedOutputs,
		const TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& ActualInputs,
		const TArray<FBlueprintHelperFunctionSignaturePinSnapshot>& ActualOutputs)
	{
		FBlueprintHelperFunctionSignatureDiff Diff;
		CompareFunctionSignatureDirection(ExpectedInputs, ActualInputs, TEXT("input"), Diff);
		CompareFunctionSignatureDirection(ExpectedOutputs, ActualOutputs, TEXT("output"), Diff);
		return Diff;
	}

	static const FUserPinInfo* FindUserDefinedPinInfo(
		UK2Node_CustomEvent* EventNode,
		const FString& PinName)
	{
		if (!EventNode)
		{
			return nullptr;
		}

		for (const TSharedPtr<FUserPinInfo>& Pin : EventNode->UserDefinedPins)
		{
			if (Pin.IsValid() && Pin->PinName == FName(*PinName))
			{
				return Pin.Get();
			}
		}
		return nullptr;
	}

	static UK2Node_CustomEvent* FindCustomEventInGraph(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (CustomEvent && CustomEvent->CustomFunctionName.ToString().Equals(EventName, ESearchCase::IgnoreCase))
			{
				return CustomEvent;
			}
		}
		return nullptr;
	}

	static UK2Node_CustomEvent* FindCustomEventInBlueprint(
		UBlueprint* Blueprint,
		const FString& EventName,
		UEdGraph*& OutGraph)
	{
		OutGraph = nullptr;
		if (!Blueprint)
		{
			return nullptr;
		}

		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (UK2Node_CustomEvent* EventNode = FindCustomEventInGraph(Graph, EventName))
			{
				OutGraph = Graph;
				return EventNode;
			}
		}
		return nullptr;
	}

	static UEdGraph* CreateCustomEventUbergraphPage(
		UBlueprint* Blueprint,
		const FString& GraphName,
		FString& OutError)
	{
		if (!Blueprint)
		{
			OutError = TEXT("Target Blueprint is invalid.");
			return nullptr;
		}

		const FName TargetGraphName(*GraphName);
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph && Graph->GetFName() == TargetGraphName)
			{
				return Graph;
			}
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == TargetGraphName)
			{
				OutError = FString::Printf(TEXT("Target graph name conflicts with an existing function graph: %s."), *GraphName);
				return nullptr;
			}
		}
		for (UEdGraph* Graph : Blueprint->MacroGraphs)
		{
			if (Graph && Graph->GetFName() == TargetGraphName)
			{
				OutError = FString::Printf(TEXT("Target graph name conflicts with an existing macro graph: %s."), *GraphName);
				return nullptr;
			}
		}

		UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
			Blueprint,
			TargetGraphName,
			UEdGraph::StaticClass(),
			UEdGraphSchema_K2::StaticClass());
		if (!NewGraph)
		{
			OutError = FString::Printf(TEXT("Failed to create custom event graph: %s."), *GraphName);
			return nullptr;
		}

		FBlueprintEditorUtils::AddUbergraphPage(Blueprint, NewGraph);
		return NewGraph;
	}

	static bool TryResolveCustomEventTarget(
		const FBlueprintHelperEnsureCustomEventSignatureRequest& Request,
		FBlueprintHelperResolvedCustomEventTarget& OutTarget,
		FString& OutCode,
		EBlueprintHelperToolStage& OutStage,
		FString& OutMessage,
		FString& OutField)
	{
		FString PinMessage;
		FString PinField;
		if (!TryBuildSignaturePinSpecs(Request.Inputs, OutTarget.RequestedPins, PinMessage, PinField))
		{
			OutCode = TEXT("invalid_signature_payload");
			OutStage = EBlueprintHelperToolStage::ParseInput;
			OutMessage = PinMessage;
			OutField = PinField;
			return false;
		}

		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperDiagnosticSet Diag;
		OutTarget.Graph = Resolver.ResolveGraph(
			MakeGraphTarget(Request.AssetPath, Request.GraphName),
			Diag,
			FBlueprintHelperResolvePolicy::Mutation());
		if (!OutTarget.Graph || Diag.HasErrors())
		{
			OutCode = TEXT("target_graph_not_found");
			OutStage = EBlueprintHelperToolStage::ResolveTarget;
			OutMessage = Diag.Items.Num() > 0
				? Diag.Items[0].Message
				: FString::Printf(TEXT("Blueprint graph not found: %s."), *Request.GraphName);
			OutField = TEXT("graph_name");
			return false;
		}

		OutTarget.Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(OutTarget.Graph);
		if (!OutTarget.Blueprint)
		{
			OutCode = TEXT("target_blueprint_not_found");
			OutStage = EBlueprintHelperToolStage::ResolveTarget;
			OutMessage = TEXT("Unable to resolve Blueprint owning the target graph.");
			OutField = TEXT("asset_path");
			return false;
		}

		OutTarget.ExistingEvent = FindCustomEventInBlueprint(
			OutTarget.Blueprint,
			Request.EventName,
			OutTarget.ExistingEventGraph);
		if (OutTarget.ExistingEvent && OutTarget.ExistingEventGraph != OutTarget.Graph)
		{
			OutCode = TEXT("custom_event_already_exists");
			OutStage = EBlueprintHelperToolStage::Preflight;
			OutMessage = FString::Printf(
				TEXT("Custom event already exists in graph %s: %s."),
				OutTarget.ExistingEventGraph ? *OutTarget.ExistingEventGraph->GetName() : TEXT("<unknown>"),
				*Request.EventName);
			OutField = TEXT("event_name");
			return false;
		}

		OutTarget.MissingPins.Reset();
		bool bAllPinsMatch = true;
		if (OutTarget.ExistingEvent)
		{
			for (const FBlueprintHelperSignaturePinSpec& RequestedPin : OutTarget.RequestedPins)
			{
				const FUserPinInfo* ExistingPin = FindUserDefinedPinInfo(OutTarget.ExistingEvent, RequestedPin.Name);
				if (!ExistingPin)
				{
					OutTarget.MissingPins.Add(RequestedPin);
					bAllPinsMatch = false;
					continue;
				}

				if (!PinTypesMatch(ExistingPin->PinType, RequestedPin.PinType))
				{
					OutCode = TEXT("custom_event_signature_mismatch");
					OutStage = EBlueprintHelperToolStage::Preflight;
					OutMessage = FString::Printf(
						TEXT("Custom event pin type mismatch for %s.%s."),
						*Request.EventName,
						*RequestedPin.Name);
					OutField = TEXT("inputs");
					return false;
				}
			}
		}
		else
		{
			OutTarget.MissingPins = OutTarget.RequestedPins;
			bAllPinsMatch = false;
		}

		OutTarget.bSignatureMatches = OutTarget.ExistingEvent && bAllPinsMatch;
		return true;
	}

	static bool AddPinsToCustomEvent(
		UK2Node_CustomEvent* EventNode,
		const TArray<FBlueprintHelperSignaturePinSpec>& Pins,
		FString& OutError)
	{
		if (!EventNode)
		{
			OutError = TEXT("Custom event node is invalid.");
			return false;
		}

		for (const FBlueprintHelperSignaturePinSpec& Pin : Pins)
		{
			if (!EventNode->CreateUserDefinedPin(*Pin.Name, Pin.PinType, EGPD_Output))
			{
				OutError = FString::Printf(TEXT("Failed to add custom event pin: %s."), *Pin.Name);
				return false;
			}
		}

		EventNode->ReconstructNode();
		return true;
	}

	static UK2Node_CustomEvent* CreateCustomEventNode(
		UEdGraph* Graph,
		const FString& EventName,
		const TArray<FBlueprintHelperSignaturePinSpec>& Pins,
		FString& OutError)
	{
		if (!Graph)
		{
			OutError = TEXT("Target graph is invalid.");
			return nullptr;
		}

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		if (!EventNode)
		{
			OutError = TEXT("Failed to allocate custom event node.");
			return nullptr;
		}

		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->NodePosX = 0;
		EventNode->NodePosY = 0;
		EventNode->AllocateDefaultPins();

		if (!AddPinsToCustomEvent(EventNode, Pins, OutError))
		{
			return nullptr;
		}

		return EventNode;
	}

	static void RecordGeneratedCustomEventForLayout(UEdGraph* Graph, UK2Node_CustomEvent* EventNode)
	{
		if (!Graph || !EventNode)
		{
			return;
		}

		TArray<UEdGraphNode*> GeneratedNodes;
		GeneratedNodes.Add(EventNode);
		FBlueprintHelperGraphLayoutCoordinator::RecordGeneratedNodes(Graph, GeneratedNodes);
	}

	static bool WriteCreatedCustomEventOwnership(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		UK2Node_CustomEvent* EventNode,
		const FString& EventName,
		FString& OutError)
	{
		if (!Blueprint || !Graph || !EventNode || EventName.IsEmpty())
		{
			OutError = TEXT("custom_event_ownership_target_invalid");
			return false;
		}

		const FBlueprintHelperBlockIdService BlockIdService;
		const FString BlockRef = BlockIdService.MakeBlockRef(Blueprint, Graph, EventName);
		FString BlockId = BlockIdService.MakeFullBlockId(Graph->GetName(), BlockRef);
		if (BlockId.IsEmpty())
		{
			BlockId = BlockRef;
		}
		if (BlockId.IsEmpty())
		{
			OutError = TEXT("custom_event_ownership_block_id_empty");
			return false;
		}

		const FBlueprintHelperOwnershipService OwnershipService;
		return OwnershipService.WriteNodeOwnership(Blueprint, EventNode, BlockId, EventName, OutError);
	}

	static FName ResolveNativeOrOverrideEventName(const FString& InEventName)
	{
		const FString Lower = InEventName.ToLower();
		if (Lower == TEXT("beginplay") || Lower == TEXT("receivebeginplay"))
		{
			return FName(TEXT("ReceiveBeginPlay"));
		}
		if (Lower == TEXT("tick") || Lower == TEXT("receivetick"))
		{
			return FName(TEXT("ReceiveTick"));
		}
		if (Lower == TEXT("endplay") || Lower == TEXT("receiveendplay"))
		{
			return FName(TEXT("ReceiveEndPlay"));
		}
		if (Lower == TEXT("anydamage") || Lower == TEXT("receiveanydamage"))
		{
			return FName(TEXT("ReceiveAnyDamage"));
		}
		if (Lower == TEXT("actorbeginoverlap") || Lower == TEXT("receiveactorbeginoverlap"))
		{
			return FName(TEXT("ReceiveActorBeginOverlap"));
		}
		if (Lower == TEXT("actorendoverlap") || Lower == TEXT("receiveactorendoverlap"))
		{
			return FName(TEXT("ReceiveActorEndOverlap"));
		}
		if (Lower == TEXT("actorhit") || Lower == TEXT("receivehit") || Lower == TEXT("hit"))
		{
			return FName(TEXT("ReceiveHit"));
		}
		return FName(*InEventName);
	}

	static UFunction* ResolveNativeOrOverrideEventDeclarationFunction(UFunction* EventFunction)
	{
		while (EventFunction && EventFunction->GetSuperFunction())
		{
			EventFunction = EventFunction->GetSuperFunction();
		}
		return EventFunction;
	}

	static UClass* ResolveNativeOrOverrideEventSignatureClass(UFunction* EventFunction, UClass* FallbackSignatureClass)
	{
		UClass* EventSignatureClass = EventFunction ? EventFunction->GetOwnerClass() : nullptr;
		if (!EventSignatureClass)
		{
			EventSignatureClass = FallbackSignatureClass;
		}
		return EventSignatureClass ? EventSignatureClass->GetAuthoritativeClass() : nullptr;
	}

	static UEdGraph* ResolveOverrideEventGraph(
		const FBlueprintHelperBlueprintStructureService& StructureService,
		const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request,
		FString& OutGraphName,
		FString& OutCode,
		EBlueprintHelperToolStage& OutStage,
		FString& OutMessage,
		FString& OutField)
	{
		OutGraphName = Request.GraphName.IsEmpty() ? TEXT("EventGraph") : Request.GraphName;

		FString ValidationCode;
		EBlueprintHelperToolStage ValidationStage = EBlueprintHelperToolStage::Preflight;
		FString ValidationMessage;
		FString ValidationField;
		if (!TryValidateEventGraphExists(
			StructureService,
			Request.AssetPath,
			OutGraphName,
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField))
		{
			OutCode = ValidationCode;
			OutStage = ValidationStage;
			OutMessage = ValidationMessage;
			OutField = ValidationField;
			return nullptr;
		}

		FBlueprintHelperGraphResolver Resolver;
		FBlueprintHelperDiagnosticSet Diag;
		UEdGraph* Graph = Resolver.ResolveGraph(
			MakeGraphTarget(Request.AssetPath, OutGraphName),
			Diag,
			FBlueprintHelperResolvePolicy::Mutation());
		if (!Graph || Diag.HasErrors())
		{
			OutCode = TEXT("target_graph_not_found");
			OutStage = EBlueprintHelperToolStage::ResolveTarget;
			OutMessage = Diag.Items.Num() > 0
				? Diag.Items[0].Message
				: FString::Printf(TEXT("Blueprint graph not found: %s."), *OutGraphName);
			OutField = TEXT("graph_name");
			return nullptr;
		}

		return Graph;
	}

	static UK2Node_Event* CreateOverrideEventNode(
		UEdGraph* Graph,
		UFunction* EventFunction,
		UClass* SignatureClass,
		FString& OutError)
	{
		if (!Graph)
		{
			OutError = TEXT("Target graph is invalid.");
			return nullptr;
		}
		if (!EventFunction)
		{
			OutError = TEXT("Override event function is invalid.");
			return nullptr;
		}

		UClass* EventSignatureClass = SignatureClass ? SignatureClass->GetAuthoritativeClass() : nullptr;
		if (!EventSignatureClass)
		{
			EventSignatureClass = EventFunction->GetOwnerClass();
			EventSignatureClass = EventSignatureClass ? EventSignatureClass->GetAuthoritativeClass() : nullptr;
		}
		if (!EventSignatureClass)
		{
			OutError = TEXT("Override event signature class is invalid.");
			return nullptr;
		}

		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		if (!EventNode)
		{
			OutError = TEXT("Failed to allocate override event node.");
			return nullptr;
		}

		EventNode->SetFlags(RF_Transactional);
		EventNode->NodePosX = 0;
		EventNode->NodePosY = 0;
		Graph->AddNode(EventNode, true, false);
		EventNode->CreateNewGuid();
		EventNode->PostPlacedNewNode();
		EventNode->EventReference.SetExternalMember(EventFunction->GetFName(), EventSignatureClass);
		EventNode->bOverrideFunction = true;
		EventNode->AllocateDefaultPins();
		return EventNode;
	}

};

FBlueprintHelperSignatureService::FBlueprintHelperSignatureService(
	const FBlueprintHelperBlueprintStructureService& InStructureService)
	: StructureService(InStructureService)
{
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureFunction(
	const FBlueprintHelperEnsureFunctionSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.FunctionName.IsEmpty())
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_function requires asset_path and function_name."),
			TEXT("task_plan.steps[0].write.ops[0]"));
	}

	if (!FBlueprintHelperSignatureServiceLocalUtils::IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("invalid_name_collision_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_function name_collision_policy must be reuse_if_exists or fail_if_exists."),
			TEXT("name_collision_policy"));
	}

	FString ListError;
	const bool bExists = FBlueprintHelperSignatureServiceLocalUtils::FunctionExists(StructureService, Request.AssetPath, Request.FunctionName, ListError);
	if (!ListError.IsEmpty())
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
	}

	if (bExists && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("function_already_exists"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Function already exists: %s"), *Request.FunctionName),
			TEXT("function_name"));
	}

	TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperSignaturePinSpec> ExpectedInputs;
	TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperSignaturePinSpec> ExpectedOutputs;
	FString PinMessage;
	FString PinField;
	FString PinCode;
	if (!FBlueprintHelperSignatureServiceLocalUtils::TryBuildSignaturePinSpecs(
		Request.Inputs,
		ExpectedInputs,
		PinMessage,
		PinField,
		TEXT("inputs"),
		&PinCode))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			PinCode.IsEmpty() ? TEXT("invalid_pin_type") : PinCode,
			EBlueprintHelperToolStage::ParseInput,
			PinMessage,
			PinField);
	}
	if (!FBlueprintHelperSignatureServiceLocalUtils::TryBuildSignaturePinSpecs(
		Request.Outputs,
		ExpectedOutputs,
		PinMessage,
		PinField,
		TEXT("outputs"),
		&PinCode))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			PinCode.IsEmpty() ? TEXT("invalid_pin_type") : PinCode,
			EBlueprintHelperToolStage::ParseInput,
			PinMessage,
			PinField);
	}

	if (bExists)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Request.AssetPath);
		if (!Blueprint)
		{
			return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_function"),
				TEXT("target_blueprint_not_found"),
				EBlueprintHelperToolStage::ResolveTarget,
				FString::Printf(TEXT("Blueprint asset not found: %s."), *Request.AssetPath),
				TEXT("asset_path"));
		}

		TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperFunctionSignaturePinSnapshot> ActualInputs;
		TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperFunctionSignaturePinSnapshot> ActualOutputs;
		FString SnapshotError;
		if (!FBlueprintHelperSignatureServiceLocalUtils::BuildExistingFunctionSignature(
			Blueprint,
			Request.FunctionName,
			ActualInputs,
			ActualOutputs,
			SnapshotError))
		{
			return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_function"),
				TEXT("function_signature_snapshot_failed"),
				EBlueprintHelperToolStage::Preflight,
				SnapshotError.IsEmpty()
					? FString::Printf(TEXT("Unable to inspect existing function signature: %s."), *Request.FunctionName)
					: SnapshotError,
				TEXT("function_name"));
		}

		const FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperFunctionSignatureDiff Diff =
			FBlueprintHelperSignatureServiceLocalUtils::CompareFunctionSignature(
				ExpectedInputs,
				ExpectedOutputs,
				ActualInputs,
				ActualOutputs);
		if (!Diff.bMatches)
		{
			FBlueprintHelperToolError Error =
				FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureError(
					TEXT("function_signature_mismatch"),
					EBlueprintHelperToolStage::Preflight,
					FString::Printf(TEXT("Function signature mismatch: %s."), *Request.FunctionName),
					TEXT("inputs"));
			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("ensure_function"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId(),
				Error);
			FBlueprintHelperSignatureServiceLocalUtils::SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(Request.bDryRun, TEXT("function_result"), false);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("success"), false);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("signature_matches"), false);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
			Result.Data->SetArrayField(TEXT("signature_differences"), Diff.Differences);
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
			return Result;
		}
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_function"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("function_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), bExists);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(!bExists, !bExists);
		return Result;
	}

	if (bExists)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_function"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("function_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), Request.FunctionName);
	Params->SetStringField(TEXT("graph_type"), TEXT("Function"));
	Params->SetBoolField(TEXT("is_pure"), Request.bIsPure);
	if (Request.Inputs.Num() > 0)
	{
		Params->SetArrayField(TEXT("inputs"), Request.Inputs);
	}
	if (Request.Outputs.Num() > 0)
	{
		Params->SetArrayField(TEXT("outputs"), Request.Outputs);
	}
	if (!Request.InterfacePath.IsEmpty())
	{
		Params->SetStringField(TEXT("interface_path"), Request.InterfacePath);
	}

	FString Error;
	if (!StructureService.AddGraph(FBlueprintHelperSignatureServiceLocalUtils::MakeGraphTarget(Request.AssetPath), Params, Error))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("function_create_failed"),
			EBlueprintHelperToolStage::Execute,
			Error.IsEmpty() ? TEXT("Failed to create Blueprint function graph.") : Error,
			TEXT("function_name"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_function"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperSignatureServiceLocalUtils::SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
	Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("function_result"), false);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
	Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
	return Result;
}
FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureCustomEvent(
	const FBlueprintHelperEnsureCustomEventSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.EventName.IsEmpty() || Request.GraphName.IsEmpty())
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_custom_event requires asset_path, graph_name, and event_name."),
			TEXT("task_plan.steps[0].write.ops[0]"));
	}

	if (!FBlueprintHelperSignatureServiceLocalUtils::IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			TEXT("invalid_name_collision_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_custom_event name_collision_policy must be reuse_if_exists or fail_if_exists."),
			TEXT("name_collision_policy"));
	}

	FString ValidationCode;
	EBlueprintHelperToolStage ValidationStage = EBlueprintHelperToolStage::Preflight;
	FString ValidationMessage;
	FString ValidationField;
	if (!FBlueprintHelperSignatureServiceLocalUtils::TryValidateEventGraphExists(
		StructureService,
		Request.AssetPath,
		Request.GraphName,
		ValidationCode,
		ValidationStage,
		ValidationMessage,
		ValidationField))
	{
		if (ValidationCode == TEXT("target_graph_not_found"))
		{
			UEdGraph* ExistingEventGraph = nullptr;
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Request.AssetPath);
			if (Blueprint)
			{
				if (FBlueprintHelperSignatureServiceLocalUtils::FindCustomEventInBlueprint(Blueprint, Request.EventName, ExistingEventGraph))
				{
					return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
						TEXT("ensure_custom_event"),
						TEXT("custom_event_already_exists"),
						EBlueprintHelperToolStage::Preflight,
						FString::Printf(TEXT("Custom event already exists in graph %s: %s."),
							ExistingEventGraph ? *ExistingEventGraph->GetName() : TEXT("<unknown>"),
							*Request.EventName),
						TEXT("event_name"));
				}
			}
			else
			{
				return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_custom_event"),
					TEXT("target_blueprint_not_found"),
					EBlueprintHelperToolStage::ResolveTarget,
					FString::Printf(TEXT("Blueprint asset not found: %s."), *Request.AssetPath),
					TEXT("asset_path"));
			}

			if (Request.bDryRun)
			{
				FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
					TEXT("ensure_custom_event"),
					FBlueprintHelperToolResultBuilder::GenerateTraceId());
				FBlueprintHelperSignatureServiceLocalUtils::SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
				Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("custom_event_result"), true);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), false);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("added_inputs"), 0);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_path"), Request.InterfacePath);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperSignaturePinSpec> RequestedPins;
			FString PinMessage;
			FString PinField;
			if (!FBlueprintHelperSignatureServiceLocalUtils::TryBuildSignaturePinSpecs(Request.Inputs, RequestedPins, PinMessage, PinField))
			{
				return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_custom_event"),
					TEXT("invalid_signature_payload"),
					EBlueprintHelperToolStage::ParseInput,
					PinMessage,
					PinField);
			}

			FBlueprintHelperScopedAssetMutation Mutation(
				FText::FromString(TEXT("BlueprintHelper Ensure Custom Event Signature")),
				Blueprint);
			FString CreateGraphError;
			UEdGraph* NewGraph = FBlueprintHelperSignatureServiceLocalUtils::CreateCustomEventUbergraphPage(
				Blueprint,
				Request.GraphName,
				CreateGraphError);
			if (!NewGraph)
			{
				Mutation.Rollback();
				return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_custom_event"),
					TEXT("custom_event_graph_create_failed"),
					EBlueprintHelperToolStage::Execute,
					CreateGraphError.IsEmpty() ? TEXT("Failed to create target custom event graph.") : CreateGraphError,
					TEXT("graph_name"));
			}
			Mutation.Modify(NewGraph);

			FString CreateEventError;
			UK2Node_CustomEvent* EventNode = FBlueprintHelperSignatureServiceLocalUtils::CreateCustomEventNode(
				NewGraph,
				Request.EventName,
				RequestedPins,
				CreateEventError);
			if (!EventNode)
			{
				Mutation.Rollback();
				return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_custom_event"),
					TEXT("custom_event_create_failed"),
					EBlueprintHelperToolStage::Execute,
					CreateEventError.IsEmpty() ? TEXT("Failed to create custom event signature.") : CreateEventError,
					TEXT("event_name"));
			}
			if (!FBlueprintHelperSignatureServiceLocalUtils::WriteCreatedCustomEventOwnership(
				Blueprint,
				NewGraph,
				EventNode,
				Request.EventName,
				CreateEventError))
			{
				Mutation.Rollback();
				return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_custom_event"),
					TEXT("custom_event_ownership_write_failed"),
					EBlueprintHelperToolStage::Execute,
					CreateEventError.IsEmpty() ? TEXT("Failed to write custom event ownership metadata.") : CreateEventError,
					TEXT("event_name"));
			}

			NewGraph->NotifyGraphChanged();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			Mutation.Commit();
			FBlueprintHelperSignatureServiceLocalUtils::RecordGeneratedCustomEventForLayout(NewGraph, EventNode);

			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
				TEXT("ensure_custom_event"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			FBlueprintHelperSignatureServiceLocalUtils::SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("custom_event_result"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("added_inputs"), RequestedPins.Num());
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_path"), Request.InterfacePath);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
			return Result;
		}

		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField);
	}

	FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperResolvedCustomEventTarget Target;
	if (!FBlueprintHelperSignatureServiceLocalUtils::TryResolveCustomEventTarget(
		Request,
		Target,
		ValidationCode,
		ValidationStage,
		ValidationMessage,
		ValidationField))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField);
	}

	if (Target.ExistingEvent && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			TEXT("custom_event_already_exists"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Custom event already exists: %s"), *Request.EventName),
			TEXT("event_name"));
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_custom_event"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("custom_event_result"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), Target.ExistingEvent != nullptr);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), Target.bSignatureMatches);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("missing_inputs"), Target.MissingPins.Num());
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_path"), Request.InterfacePath);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(!Target.bSignatureMatches, !Target.bSignatureMatches);
		return Result;
	}

	if (Target.bSignatureMatches)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_custom_event"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("custom_event_result"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("missing_inputs"), 0);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_path"), Request.InterfacePath);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Ensure Custom Event Signature")),
		Target.Blueprint);
	Mutation.Modify(Target.Graph);

	UK2Node_CustomEvent* EventNode = Target.ExistingEvent;
	bool bCreatedEventNode = false;
	FString Error;
	if (EventNode)
	{
		Mutation.Modify(EventNode);
		if (!FBlueprintHelperSignatureServiceLocalUtils::AddPinsToCustomEvent(EventNode, Target.MissingPins, Error))
		{
			Mutation.Rollback();
			return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_custom_event"),
				TEXT("custom_event_update_failed"),
				EBlueprintHelperToolStage::Execute,
				Error.IsEmpty() ? TEXT("Failed to update custom event signature.") : Error,
				TEXT("inputs"));
		}
	}
	else
	{
		EventNode = FBlueprintHelperSignatureServiceLocalUtils::CreateCustomEventNode(Target.Graph, Request.EventName, Target.RequestedPins, Error);
		bCreatedEventNode = EventNode != nullptr;
		if (!EventNode)
		{
			Mutation.Rollback();
			return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_custom_event"),
				TEXT("custom_event_create_failed"),
				EBlueprintHelperToolStage::Execute,
				Error.IsEmpty() ? TEXT("Failed to create custom event signature.") : Error,
				TEXT("event_name"));
		}

		if (!FBlueprintHelperSignatureServiceLocalUtils::WriteCreatedCustomEventOwnership(
			Target.Blueprint,
			Target.Graph,
			EventNode,
			Request.EventName,
			Error))
		{
			Mutation.Rollback();
			return FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_custom_event"),
				TEXT("custom_event_ownership_write_failed"),
				EBlueprintHelperToolStage::Execute,
				Error.IsEmpty() ? TEXT("Failed to write custom event ownership metadata.") : Error,
				TEXT("event_name"));
		}
	}

	if (Target.Graph)
	{
		Target.Graph->NotifyGraphChanged();
	}
	if (Target.Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Target.Blueprint);
	}
	Mutation.Commit();
	if (bCreatedEventNode)
	{
		FBlueprintHelperSignatureServiceLocalUtils::RecordGeneratedCustomEventForLayout(Target.Graph, EventNode);
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_custom_event"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperSignatureServiceLocalUtils::SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
	Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("custom_event_result"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("added_inputs"), EventNode == Target.ExistingEvent ? Target.MissingPins.Num() : Target.RequestedPins.Num());
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_path"), Request.InterfacePath);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("custom_event_result"), TEXT("interface_entry_kind"), Request.InterfaceEntryKind);
	Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::RemoveSignature(
	const FBlueprintHelperRemoveSignatureRequest& Request) const
{
	const FString ExecutePolicy = Request.ExecutePolicy.IsEmpty() ? TEXT("blocked_preflight") : Request.ExecutePolicy;
	if (Request.AssetPath.IsEmpty() || Request.SignatureName.IsEmpty() || Request.SignatureKind.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("remove_signature requires asset_path, signature_kind, and signature_name."),
			TEXT("remove_signature"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("invalid_signature_payload"),
			TEXT("remove_signature requires asset_path, signature_kind, and signature_name."),
			TEXT("remove_signature"));
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (ExecutePolicy != TEXT("blocked_preflight") && ExecutePolicy != TEXT("execute_if_unreferenced"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("invalid_signature_remove_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("remove_signature execute_policy must be blocked_preflight or execute_if_unreferenced."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("invalid_signature_remove_policy"),
			TEXT("remove_signature execute_policy must be blocked_preflight or execute_if_unreferenced."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (!Request.bRequireReferenceContext)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("invalid_signature_remove_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("remove_signature require_reference_context must be true in this slice."),
			TEXT("require_reference_context"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("invalid_signature_remove_policy"),
			TEXT("remove_signature require_reference_context must be true in this slice."),
			TEXT("require_reference_context"));
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("requires_reference_context"), false);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.SignatureKind != TEXT("function") &&
		Request.SignatureKind != TEXT("interface_function") &&
		Request.SignatureKind != TEXT("custom_event") &&
		Request.SignatureKind != TEXT("interface_event") &&
		Request.SignatureKind != TEXT("event_dispatcher") &&
		Request.SignatureKind != TEXT("override_event") &&
		Request.SignatureKind != TEXT("native_event"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_remove_unsupported_kind"),
			EBlueprintHelperToolStage::Preflight,
			TEXT("remove_signature supports function, interface_function, custom_event, interface_event, event_dispatcher, override_event, and native_event policies."),
			TEXT("signature_kind"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("signature_remove_unsupported_kind"),
			TEXT("remove_signature supports function, interface_function, custom_event, interface_event, event_dispatcher, override_event, and native_event policies."),
			TEXT("signature_kind"));
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FString ListError;
	if (!FBlueprintHelperSignatureServiceLocalUtils::BlueprintCanBeListed(StructureService, Request.AssetPath, ListError))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("target_blueprint_not_found"),
			ListError,
			TEXT("asset_path"));
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.SignatureKind == TEXT("custom_event") || Request.SignatureKind == TEXT("interface_event"))
	{
		if (Request.GraphName.IsEmpty())
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("remove_signature"),
				TEXT("invalid_signature_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("remove_signature custom_event/interface_event requires graph_name."),
				TEXT("graph_name"));
			FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
				Request.bDryRun,
				TEXT("invalid_signature_payload"),
				TEXT("remove_signature custom_event/interface_event requires graph_name."),
				TEXT("graph_name"));
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
			return Result;
		}

		FString ValidationCode;
		EBlueprintHelperToolStage ValidationStage = EBlueprintHelperToolStage::Preflight;
		FString ValidationMessage;
		FString ValidationField;
		if (!FBlueprintHelperSignatureServiceLocalUtils::TryValidateEventGraphExists(
			StructureService,
			Request.AssetPath,
			Request.GraphName,
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField))
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("remove_signature"),
				ValidationCode,
				ValidationStage,
				ValidationMessage,
				ValidationField);
			FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
				Request.bDryRun,
				ValidationCode,
				ValidationMessage,
				ValidationField);
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
			return Result;
		}
	}

	if (ExecutePolicy == TEXT("blocked_preflight"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_remove_blocked_by_policy"),
			Request.bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Preflight,
			TEXT("Signature removal requires execute_policy=execute_if_unreferenced."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("signature_remove_blocked_by_policy"),
			TEXT("Signature removal requires execute_policy=execute_if_unreferenced."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_kind"), Request.SignatureKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_name"), Request.SignatureName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("requires_reference_context"), Request.bRequireReferenceContext);
		FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextHint(Result.Data, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperReferenceContextPack ReferenceContext;
	FString ReferenceContextError;
	if (!FBlueprintHelperSignatureReferenceContextUtils::TryBuildSignatureReferenceContext(
		Request.AssetPath,
		FBlueprintHelperSignatureReferenceContextUtils::ReferenceContextTargetTypeForSignatureKind(Request.SignatureKind),
		Request.SignatureName,
		Request.GraphName,
		ReferenceContext,
		ReferenceContextError))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_reference_context_failed"),
			EBlueprintHelperToolStage::Preflight,
			ReferenceContextError,
			TEXT("reference_context"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("signature_reference_context_failed"),
			ReferenceContextError,
			TEXT("reference_context"));
		FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextHint(Result.Data, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (!FBlueprintHelperSignatureReferenceContextUtils::IsReferenceContextSafeForMutation(ReferenceContext))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_remove_blocked_by_references"),
			EBlueprintHelperToolStage::Preflight,
			TEXT("Signature removal is blocked by reference context."),
			TEXT("reference_context"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("signature_remove_blocked_by_references"),
			TEXT("Signature removal is blocked by reference context."),
			TEXT("reference_context"));
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_kind"), Request.SignatureKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_name"), Request.SignatureName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("requires_reference_context"), Request.bRequireReferenceContext);
		FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextSummary(Result.Data, Request, ReferenceContext);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("remove_signature"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("remove_signature_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_kind"), Request.SignatureKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_name"), Request.SignatureName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("can_execute"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("requires_reference_context"), Request.bRequireReferenceContext);
		FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextSummary(Result.Data, Request, ReferenceContext);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	UBlueprint* Blueprint = FBlueprintHelperSignatureMutationUtils::LoadSignatureBlueprint(Request.AssetPath);
	if (!Blueprint)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("Unable to load Blueprint: %s."), *Request.AssetPath),
			TEXT("asset_path"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Remove Signature")),
		Blueprint);

	bool bRemoved = false;
	FString RemoveError;
	if (!FBlueprintHelperSignatureMutationUtils::RemoveSignatureDirect(Blueprint, Request, bRemoved, RemoveError))
	{
		Mutation.Rollback();
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_remove_failed"),
			EBlueprintHelperToolStage::Execute,
			RemoveError.IsEmpty() ? TEXT("Failed to remove signature.") : RemoveError,
			TEXT("remove_signature"));
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (!bRemoved)
	{
		Mutation.Rollback();
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("remove_signature"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("remove_signature_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_kind"), Request.SignatureKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_name"), Request.SignatureName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("removed"), false);
		FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextSummary(Result.Data, Request, ReferenceContext);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("remove_signature"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperSignatureServiceLocalUtils::SetRemoveSignatureTarget(Result, Request);
	Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("remove_signature_result"), false);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_kind"), Request.SignatureKind);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("signature_name"), Request.SignatureName);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("remove_signature_result"), TEXT("execute_policy"), ExecutePolicy);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("remove_signature_result"), TEXT("removed"), true);
	FBlueprintHelperSignatureReferenceContextUtils::AttachRemoveSignatureReferenceContextSummary(Result.Data, Request, ReferenceContext);
	Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureEventDispatcher(
	const FBlueprintHelperEnsureEventDispatcherSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.DispatcherName.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_event_dispatcher requires asset_path and dispatcher_name."),
			TEXT("ensure_event_dispatcher"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (!FBlueprintHelperSignatureServiceLocalUtils::IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_name_collision_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_event_dispatcher name_collision_policy must be reuse_if_exists or fail_if_exists."),
			TEXT("name_collision_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	const FString SignatureMismatchPolicy = Request.SignatureMismatchPolicy.IsEmpty() ? TEXT("block") : Request.SignatureMismatchPolicy;
	if (SignatureMismatchPolicy != TEXT("block") && SignatureMismatchPolicy != TEXT("migrate_if_unreferenced"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_event_dispatcher_mutation_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_event_dispatcher signature_mismatch_policy must be block or migrate_if_unreferenced."),
			TEXT("signature_mismatch_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	TArray<FBlueprintHelperSignatureServiceLocalUtils::FBlueprintHelperSignaturePinSpec> RequestedPins;
	FString PinMessage;
	FString PinField;
	if (!FBlueprintHelperSignatureServiceLocalUtils::TryBuildSignaturePinSpecs(Request.Inputs, RequestedPins, PinMessage, PinField))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			PinMessage,
			PinField);
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperEventDispatcherInfo ExistingDispatcher;
	FString ListError;
	const bool bExists = FBlueprintHelperSignatureServiceLocalUtils::TryFindEventDispatcher(
		StructureService,
		Request.AssetPath,
		Request.DispatcherName,
		ExistingDispatcher,
		ListError);
	if (!bExists && !ListError.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (bExists && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("event_dispatcher_already_exists"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Event dispatcher already exists: %s"), *Request.DispatcherName),
			TEXT("dispatcher_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (bExists)
	{
		FString MismatchMessage;
		const bool bSignatureExactlyMatches = FBlueprintHelperSignatureServiceLocalUtils::EventDispatcherSignatureExactlyMatches(
			ExistingDispatcher,
			RequestedPins,
			MismatchMessage);
		if (!bSignatureExactlyMatches && SignatureMismatchPolicy == TEXT("block"))
		{
			FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
				TEXT("ensure_event_dispatcher"),
				TEXT("event_dispatcher_signature_change_unsupported"),
				EBlueprintHelperToolStage::Preflight,
				MismatchMessage,
				TEXT("inputs"));
			FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
				Request.bDryRun,
				TEXT("event_dispatcher_signature_change_unsupported"),
				MismatchMessage,
				TEXT("inputs"),
				TEXT("event_dispatcher_result"));
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
			return Result;
		}

		if (!bSignatureExactlyMatches && SignatureMismatchPolicy == TEXT("migrate_if_unreferenced"))
		{
			FBlueprintHelperReferenceContextPack ReferenceContext;
			FString ReferenceContextError;
			if (!FBlueprintHelperSignatureReferenceContextUtils::TryBuildSignatureReferenceContext(
				Request.AssetPath,
				TEXT("event_dispatcher"),
				Request.DispatcherName,
				FString(),
				ReferenceContext,
				ReferenceContextError))
			{
				FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_event_dispatcher"),
					TEXT("signature_reference_context_failed"),
					EBlueprintHelperToolStage::Preflight,
					ReferenceContextError,
					TEXT("reference_context"));
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			if (!FBlueprintHelperSignatureReferenceContextUtils::IsReferenceContextSafeForMutation(ReferenceContext))
			{
				FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_event_dispatcher"),
					TEXT("event_dispatcher_signature_migration_blocked_by_references"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("Event dispatcher signature migration is blocked by reference context."),
					TEXT("reference_context"));
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
					Request.bDryRun,
					TEXT("event_dispatcher_signature_migration_blocked_by_references"),
					TEXT("Event dispatcher signature migration is blocked by reference context."),
					TEXT("reference_context"),
					TEXT("event_dispatcher_result"));
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_matches"), false);
				const TSharedPtr<FJsonObject>* EventDispatcherResult = nullptr;
				if (Result.Data.IsValid() &&
					Result.Data->TryGetObjectField(TEXT("event_dispatcher_result"), EventDispatcherResult) &&
					EventDispatcherResult && EventDispatcherResult->IsValid())
				{
					(*EventDispatcherResult)->SetObjectField(TEXT("reference_context_request"),
						FBlueprintHelperSignatureReferenceContextUtils::MakeReferenceContextRequestJson(
							Request.AssetPath,
							TEXT("event_dispatcher"),
							Request.DispatcherName,
							FString()));
				}
				FBlueprintHelperSignatureReferenceContextUtils::AttachReferenceContextSummary(Result.Data, TEXT("event_dispatcher_result"), ReferenceContext);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			if (Request.bDryRun)
			{
				FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
					TEXT("ensure_event_dispatcher"),
					FBlueprintHelperToolResultBuilder::GenerateTraceId());
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("event_dispatcher_result"), false);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_matches"), false);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("can_migrate"), true);
				FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
				const TSharedPtr<FJsonObject>* EventDispatcherResult = nullptr;
				if (Result.Data.IsValid() &&
					Result.Data->TryGetObjectField(TEXT("event_dispatcher_result"), EventDispatcherResult) &&
					EventDispatcherResult && EventDispatcherResult->IsValid())
				{
					(*EventDispatcherResult)->SetObjectField(TEXT("reference_context_request"),
						FBlueprintHelperSignatureReferenceContextUtils::MakeReferenceContextRequestJson(
							Request.AssetPath,
							TEXT("event_dispatcher"),
							Request.DispatcherName,
							FString()));
				}
				FBlueprintHelperSignatureReferenceContextUtils::AttachReferenceContextSummary(Result.Data, TEXT("event_dispatcher_result"), ReferenceContext);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
				return Result;
			}

			UBlueprint* Blueprint = FBlueprintHelperSignatureMutationUtils::LoadSignatureBlueprint(Request.AssetPath);
			if (!Blueprint)
			{
				FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_event_dispatcher"),
					TEXT("target_blueprint_not_found"),
					EBlueprintHelperToolStage::ResolveTarget,
					FString::Printf(TEXT("Unable to load Blueprint: %s."), *Request.AssetPath),
					TEXT("asset_path"));
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			FBlueprintHelperScopedAssetMutation Mutation(
				FText::FromString(TEXT("BlueprintHelper Migrate Event Dispatcher Signature")),
				Blueprint);
			bool bRemoved = false;
			FString RemoveError;
			if (!FBlueprintHelperSignatureMutationUtils::RemoveEventDispatcherSignatureDirect(Blueprint, Request.DispatcherName, bRemoved, RemoveError))
			{
				Mutation.Rollback();
				FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_event_dispatcher"),
					TEXT("event_dispatcher_migration_remove_failed"),
					EBlueprintHelperToolStage::Execute,
					RemoveError.IsEmpty() ? TEXT("Failed to remove existing event dispatcher signature.") : RemoveError,
					TEXT("dispatcher_name"));
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("name"), Request.DispatcherName);
			if (Request.Inputs.Num() > 0)
			{
				Params->SetArrayField(TEXT("params"), Request.Inputs);
			}

			FString AddError;
			if (!StructureService.AddEventDispatcher(FBlueprintHelperSignatureServiceLocalUtils::MakeGraphTarget(Request.AssetPath), Params, AddError))
			{
				Mutation.Rollback();
				FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
					TEXT("ensure_event_dispatcher"),
					TEXT("event_dispatcher_migration_create_failed"),
					EBlueprintHelperToolStage::Execute,
					AddError.IsEmpty() ? TEXT("Failed to recreate event dispatcher signature.") : AddError,
					TEXT("dispatcher_name"));
				FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
				Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
				return Result;
			}

			Mutation.Commit();

			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
				TEXT("ensure_event_dispatcher"),
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
			Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("event_dispatcher_result"), false);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_matches"), true);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("migrated"), bRemoved);
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("event_dispatcher_result"), TEXT("added_inputs"), RequestedPins.Num());
			FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
			const TSharedPtr<FJsonObject>* EventDispatcherResult = nullptr;
			if (Result.Data.IsValid() &&
				Result.Data->TryGetObjectField(TEXT("event_dispatcher_result"), EventDispatcherResult) &&
				EventDispatcherResult && EventDispatcherResult->IsValid())
			{
				(*EventDispatcherResult)->SetObjectField(TEXT("reference_context_request"),
					FBlueprintHelperSignatureReferenceContextUtils::MakeReferenceContextRequestJson(
						Request.AssetPath,
						TEXT("event_dispatcher"),
						Request.DispatcherName,
						FString()));
			}
			FBlueprintHelperSignatureReferenceContextUtils::AttachReferenceContextSummary(Result.Data, TEXT("event_dispatcher_result"), ReferenceContext);
			Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
			return Result;
		}
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_event_dispatcher"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("event_dispatcher_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), bExists);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(!bExists, !bExists);
		return Result;
	}

	if (bExists)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_event_dispatcher"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("event_dispatcher_result"), false);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), Request.DispatcherName);
	if (Request.Inputs.Num() > 0)
	{
		Params->SetArrayField(TEXT("params"), Request.Inputs);
	}

	FString Error;
	if (!StructureService.AddEventDispatcher(FBlueprintHelperSignatureServiceLocalUtils::MakeGraphTarget(Request.AssetPath), Params, Error))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("event_dispatcher_create_failed"),
			EBlueprintHelperToolStage::Execute,
			Error.IsEmpty() ? TEXT("Failed to create event dispatcher signature.") : Error,
			TEXT("dispatcher_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_event_dispatcher"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperSignatureServiceLocalUtils::SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
	Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("event_dispatcher_result"), false);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultNumber(Result.Data, TEXT("event_dispatcher_result"), TEXT("added_inputs"), RequestedPins.Num());
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("event_dispatcher_result"), TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
	Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureOverrideEvent(
	const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.EventName.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_override_event requires asset_path and event_name."),
			TEXT("ensure_override_event"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	const FString EventKind = Request.EventKind.IsEmpty() ? TEXT("native_event") : Request.EventKind;
	if (EventKind != TEXT("native_event") && EventKind != TEXT("override_event"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("invalid_override_event_kind"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_override_event event_kind must be native_event or override_event."),
			TEXT("event_kind"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	const FString ExecutePolicy = Request.ExecutePolicy.IsEmpty() ? TEXT("blocked_preflight") : Request.ExecutePolicy;
	if (ExecutePolicy != TEXT("blocked_preflight") && ExecutePolicy != TEXT("create_if_missing"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("invalid_override_event_execute_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_override_event execute_policy must be blocked_preflight or create_if_missing."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FString ListError;
	if (!FBlueprintHelperSignatureServiceLocalUtils::BlueprintCanBeListed(StructureService, Request.AssetPath, ListError))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FString GraphName;
	FString ValidationCode;
	EBlueprintHelperToolStage ValidationStage = EBlueprintHelperToolStage::Preflight;
	FString ValidationMessage;
	FString ValidationField;
	UEdGraph* Graph = FBlueprintHelperSignatureServiceLocalUtils::ResolveOverrideEventGraph(
		StructureService,
		Request,
		GraphName,
		ValidationCode,
		ValidationStage,
		ValidationMessage,
		ValidationField);
	if (!Graph)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField);
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			TEXT("Unable to resolve Blueprint owning the target graph."),
			TEXT("asset_path"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	const FName ResolvedEventName = FBlueprintHelperSignatureServiceLocalUtils::ResolveNativeOrOverrideEventName(Request.EventName);
	UFunction* EventFunction = nullptr;
	UClass* const SignatureClass = FBlueprintEditorUtils::GetOverrideFunctionClass(
		Blueprint,
		ResolvedEventName,
		&EventFunction);
	if (!SignatureClass || !EventFunction)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("override_event_function_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("Override/native event function not found: %s."), *ResolvedEventName.ToString()),
			TEXT("event_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	UFunction* const EventDeclarationFunction =
		FBlueprintHelperSignatureServiceLocalUtils::ResolveNativeOrOverrideEventDeclarationFunction(EventFunction);
	UClass* const EventSignatureClass =
		FBlueprintHelperSignatureServiceLocalUtils::ResolveNativeOrOverrideEventSignatureClass(EventDeclarationFunction, SignatureClass);
	if (!EventDeclarationFunction || !EventSignatureClass)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("override_event_function_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("Override/native event function declaration not found: %s."), *ResolvedEventName.ToString()),
			TEXT("event_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (!UEdGraphSchema_K2::FunctionCanBePlacedAsEvent(EventDeclarationFunction))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("override_event_not_placeable"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Function cannot be placed as an event: %s."), *ResolvedEventName.ToString()),
			TEXT("event_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	UK2Node_Event* ExistingEvent = FBlueprintEditorUtils::FindOverrideForFunction(
		Blueprint,
		EventSignatureClass,
		ResolvedEventName);

	if (ExecutePolicy == TEXT("blocked_preflight"))
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("override_event_signature_blocked_by_policy"),
			Request.bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Preflight,
			TEXT("Override/native event creation requires execute_policy=create_if_missing."),
			TEXT("execute_policy"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("override_event_signature_blocked_by_policy"),
			TEXT("Override/native event creation requires execute_policy=create_if_missing."),
			TEXT("execute_policy"),
			TEXT("override_event_result"));
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_kind"), EventKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("graph_name"), GraphName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_function"), ResolvedEventName.ToString());
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("override_event_result"), TEXT("exists"), ExistingEvent != nullptr);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_override_event"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(true, TEXT("override_event_result"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("override_event_result"), TEXT("exists"), ExistingEvent != nullptr);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_kind"), EventKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("graph_name"), GraphName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_function"), ResolvedEventName.ToString());
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(ExistingEvent == nullptr, ExistingEvent == nullptr);
		return Result;
	}

	if (ExistingEvent)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_override_event"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("override_event_result"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("override_event_result"), TEXT("exists"), true);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_kind"), EventKind);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("execute_policy"), ExecutePolicy);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("graph_name"), GraphName);
		FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_function"), ResolvedEventName.ToString());
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Ensure Override Event Signature")),
		Blueprint);
	Mutation.Modify(Graph);

	FString Error;
	UK2Node_Event* EventNode = FBlueprintHelperSignatureServiceLocalUtils::CreateOverrideEventNode(
		Graph,
		EventDeclarationFunction,
		EventSignatureClass,
		Error);
	if (!EventNode)
	{
		Mutation.Rollback();
		FBlueprintHelperToolResultBase Result = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("override_event_create_failed"),
			EBlueprintHelperToolStage::Execute,
			Error.IsEmpty() ? TEXT("Failed to create override/native event node.") : Error,
			TEXT("event_name"));
		FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
		Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(false, false);
		return Result;
	}

	Graph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	Mutation.Commit();

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_override_event"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	FBlueprintHelperSignatureServiceLocalUtils::SetOverrideEventTarget(Result, Request);
	Result.Data = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureResultData(false, TEXT("override_event_result"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("override_event_result"), TEXT("exists"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultBool(Result.Data, TEXT("override_event_result"), TEXT("created"), true);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_kind"), EventKind);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("execute_policy"), ExecutePolicy);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("graph_name"), GraphName);
	FBlueprintHelperSignatureServiceLocalUtils::SetSignatureResultString(Result.Data, TEXT("override_event_result"), TEXT("event_function"), ResolvedEventName.ToString());
	Result.Validation = FBlueprintHelperSignatureServiceLocalUtils::MakeSignatureValidation(true, true);
	return Result;
}
