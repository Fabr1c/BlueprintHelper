// BlueprintHelper Service Layer - internal Blueprint function/event signature service.

#include "Services/BlueprintSignature/BlueprintHelperSignatureService.h"

#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "GraphWrite/TextToBlueprintGenerator.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EditablePinBase.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Services/BlueprintHelperBlueprintStructureService.h"

namespace
{
	struct FBlueprintHelperSignaturePinSpec
	{
		FString Name;
		FEdGraphPinType PinType;
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

	FBlueprintHelperToolError MakeSignatureError(
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

	FBlueprintHelperToolResultBase MakeSignatureFailure(
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

	FBlueprintHelperGraphTarget MakeGraphTarget(const FString& AssetPath)
	{
		FBlueprintHelperGraphTarget Target;
		Target.BlueprintPath = AssetPath;
		return Target;
	}

	FBlueprintHelperGraphTarget MakeGraphTarget(const FString& AssetPath, const FString& GraphName)
	{
		FBlueprintHelperGraphTarget Target;
		Target.BlueprintPath = AssetPath;
		Target.GraphName = GraphName;
		return Target;
	}

	bool IsValidNameCollisionPolicy(const FString& Policy)
	{
		return Policy.IsEmpty() ||
			Policy == TEXT("reuse_if_exists") ||
			Policy == TEXT("fail_if_exists");
	}

	FBlueprintHelperValidationSummary MakeSignatureValidation(bool bShouldCompile, bool bShouldSave)
	{
		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = bShouldCompile;
		Validation.bShouldSave = bShouldSave;
		return Validation;
	}

	TSharedRef<FJsonObject> MakeSignatureResultData(
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

	TSharedRef<FJsonObject> MakeSignatureBlockedData(
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

	void SetSignatureResultBool(
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

	void SetSignatureResultNumber(
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

	void SetSignatureResultString(
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

	void SetFunctionTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& FunctionName)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Function;
		Result.Target->Function = FunctionName;
	}

	void SetCustomEventTarget(
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

	void SetRemoveSignatureTarget(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperRemoveSignatureRequest& Request)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = Request.AssetPath;
		Result.Target->Graph = Request.GraphName;
		if (Request.SignatureKind == TEXT("function"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Function;
			Result.Target->Function = Request.SignatureName;
		}
		else if (Request.SignatureKind == TEXT("custom_event"))
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::CustomEvent;
			Result.Target->Event = Request.SignatureName;
		}
		else
		{
			Result.Target->TargetType = EBlueprintHelperTargetType::Asset;
			Result.Target->Event = Request.SignatureName;
		}
	}

	void SetEventDispatcherTarget(
		FBlueprintHelperToolResultBase& Result,
		const FString& AssetPath,
		const FString& DispatcherName)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Property;
		Result.Target->PropertyPath = DispatcherName;
	}

	void SetOverrideEventTarget(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request)
	{
		Result.Target = FBlueprintHelperTargetRef();
		Result.Target->AssetPath = Request.AssetPath;
		Result.Target->TargetType = EBlueprintHelperTargetType::Event;
		Result.Target->Event = Request.EventName;
	}

	bool FunctionExists(
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

	bool BlueprintCanBeListed(
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

	bool TryFindEventDispatcher(
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

	bool EventDispatcherSignatureMatches(
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

	bool TryValidateEventGraphExists(
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

	bool TryBuildSignaturePinSpecs(
		const TArray<TSharedPtr<FJsonValue>>& Params,
		TArray<FBlueprintHelperSignaturePinSpec>& OutPins,
		FString& OutMessage,
		FString& OutField)
	{
		OutPins.Reset();

		for (int32 Index = 0; Index < Params.Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> ParamObject = Params[Index].IsValid() ? Params[Index]->AsObject() : nullptr;
			if (!ParamObject.IsValid())
			{
				OutMessage = TEXT("Signature parameter entries must be objects.");
				OutField = FString::Printf(TEXT("inputs[%d]"), Index);
				return false;
			}

			FString ParamName;
			if (!ParamObject->TryGetStringField(TEXT("name"), ParamName) || ParamName.IsEmpty())
			{
				OutMessage = TEXT("Signature parameter requires name.");
				OutField = FString::Printf(TEXT("inputs[%d].name"), Index);
				return false;
			}

			if (OutPins.ContainsByPredicate([&ParamName](const FBlueprintHelperSignaturePinSpec& Pin)
				{
					return Pin.Name == ParamName;
				}))
			{
				OutMessage = FString::Printf(TEXT("Duplicate signature parameter name: %s."), *ParamName);
				OutField = FString::Printf(TEXT("inputs[%d].name"), Index);
				return false;
			}

			const TSharedPtr<FJsonObject>* PinTypeObject = nullptr;
			if (!ParamObject->TryGetObjectField(TEXT("pin_type"), PinTypeObject) ||
				!PinTypeObject || !PinTypeObject->IsValid())
			{
				OutMessage = FString::Printf(TEXT("Signature parameter %s requires pin_type."), *ParamName);
				OutField = FString::Printf(TEXT("inputs[%d].pin_type"), Index);
				return false;
			}

			FParsedPinType ParsedPinType;
			(*PinTypeObject)->TryGetStringField(TEXT("category"), ParsedPinType.Category);
			(*PinTypeObject)->TryGetStringField(TEXT("sub_category"), ParsedPinType.SubCategory);
			(*PinTypeObject)->TryGetStringField(TEXT("object_path"), ParsedPinType.SubCategoryObjectPath);
			(*PinTypeObject)->TryGetStringField(TEXT("container_type"), ParsedPinType.ContainerType);
			(*PinTypeObject)->TryGetBoolField(TEXT("is_reference"), ParsedPinType.bIsReference);
			(*PinTypeObject)->TryGetBoolField(TEXT("is_const"), ParsedPinType.bIsConst);

			FEdGraphPinType PinType;
			FString ConvertError;
			if (!TextToBlueprintGenerator::ConvertToEdGraphPinType(ParsedPinType, PinType, ConvertError))
			{
				OutMessage = ConvertError.IsEmpty()
					? FString::Printf(TEXT("Unable to convert signature parameter type: %s."), *ParamName)
					: ConvertError;
				OutField = FString::Printf(TEXT("inputs[%d].pin_type"), Index);
				return false;
			}

			FBlueprintHelperSignaturePinSpec PinSpec;
			PinSpec.Name = ParamName;
			PinSpec.PinType = PinType;
			OutPins.Add(PinSpec);
		}

		return true;
	}

	bool PinTypesMatch(const FEdGraphPinType& Left, const FEdGraphPinType& Right)
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
			Left.bIsReference == Right.bIsReference &&
			Left.bIsConst == Right.bIsConst;
	}

	const FUserPinInfo* FindUserDefinedPinInfo(
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

	UK2Node_CustomEvent* FindCustomEventInGraph(UEdGraph* Graph, const FString& EventName)
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

	UK2Node_CustomEvent* FindCustomEventInBlueprint(
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

	bool TryResolveCustomEventTarget(
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
		OutTarget.Graph = Resolver.ResolveGraph(MakeGraphTarget(Request.AssetPath, Request.GraphName), Diag);
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

	bool AddPinsToCustomEvent(
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

	UK2Node_CustomEvent* CreateCustomEventNode(
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
}

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
		return MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_function requires asset_path and function_name."),
			TEXT("task_plan.steps[0].write.ops[0]"));
	}

	if (!IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		return MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("invalid_name_collision_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_function name_collision_policy must be reuse_if_exists or fail_if_exists."),
			TEXT("name_collision_policy"));
	}

	FString ListError;
	const bool bExists = FunctionExists(StructureService, Request.AssetPath, Request.FunctionName, ListError);
	if (!ListError.IsEmpty())
	{
		return MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
	}

	if (bExists && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		return MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("function_already_exists"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Function already exists: %s"), *Request.FunctionName),
			TEXT("function_name"));
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_function"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
		Result.Data = MakeSignatureResultData(true, TEXT("function_result"), false);
		SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), bExists);
		SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
		Result.Validation = MakeSignatureValidation(!bExists, !bExists);
		return Result;
	}

	if (bExists)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_function"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
		Result.Data = MakeSignatureResultData(false, TEXT("function_result"), false);
		SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), true);
		SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
		Result.Validation = MakeSignatureValidation(false, false);
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
	if (!StructureService.AddGraph(MakeGraphTarget(Request.AssetPath), Params, Error))
	{
		return MakeSignatureFailure(
			TEXT("ensure_function"),
			TEXT("function_create_failed"),
			EBlueprintHelperToolStage::Execute,
			Error.IsEmpty() ? TEXT("Failed to create Blueprint function graph.") : Error,
			TEXT("function_name"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_function"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	SetFunctionTarget(Result, Request.AssetPath, Request.FunctionName);
	Result.Data = MakeSignatureResultData(false, TEXT("function_result"), false);
	SetSignatureResultBool(Result.Data, TEXT("function_result"), TEXT("exists"), true);
	SetSignatureResultString(Result.Data, TEXT("function_result"), TEXT("interface_path"), Request.InterfacePath);
	Result.Validation = MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureCustomEvent(
	const FBlueprintHelperEnsureCustomEventSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.EventName.IsEmpty() || Request.GraphName.IsEmpty())
	{
		return MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_custom_event requires asset_path, graph_name, and event_name."),
			TEXT("task_plan.steps[0].write.ops[0]"));
	}

	if (!IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		return MakeSignatureFailure(
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
	if (!TryValidateEventGraphExists(
		StructureService,
		Request.AssetPath,
		Request.GraphName,
		ValidationCode,
		ValidationStage,
		ValidationMessage,
		ValidationField))
	{
		return MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField);
	}

	FBlueprintHelperResolvedCustomEventTarget Target;
	if (!TryResolveCustomEventTarget(
		Request,
		Target,
		ValidationCode,
		ValidationStage,
		ValidationMessage,
		ValidationField))
	{
		return MakeSignatureFailure(
			TEXT("ensure_custom_event"),
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField);
	}

	if (Target.ExistingEvent && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		return MakeSignatureFailure(
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
		SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
		Result.Data = MakeSignatureResultData(true, TEXT("custom_event_result"), true);
		SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), Target.ExistingEvent != nullptr);
		SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), Target.bSignatureMatches);
		SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("missing_inputs"), Target.MissingPins.Num());
		Result.Validation = MakeSignatureValidation(!Target.bSignatureMatches, !Target.bSignatureMatches);
		return Result;
	}

	if (Target.bSignatureMatches)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_custom_event"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
		Result.Data = MakeSignatureResultData(false, TEXT("custom_event_result"), true);
		SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), true);
		SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
		SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("missing_inputs"), 0);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Ensure Custom Event Signature")),
		Target.Blueprint);
	Mutation.Modify(Target.Graph);

	UK2Node_CustomEvent* EventNode = Target.ExistingEvent;
	FString Error;
	if (EventNode)
	{
		Mutation.Modify(EventNode);
		if (!AddPinsToCustomEvent(EventNode, Target.MissingPins, Error))
		{
			Mutation.Rollback();
			return MakeSignatureFailure(
				TEXT("ensure_custom_event"),
				TEXT("custom_event_update_failed"),
				EBlueprintHelperToolStage::Execute,
				Error.IsEmpty() ? TEXT("Failed to update custom event signature.") : Error,
				TEXT("inputs"));
		}
	}
	else
	{
		EventNode = CreateCustomEventNode(Target.Graph, Request.EventName, Target.RequestedPins, Error);
		if (!EventNode)
		{
			Mutation.Rollback();
			return MakeSignatureFailure(
				TEXT("ensure_custom_event"),
				TEXT("custom_event_create_failed"),
				EBlueprintHelperToolStage::Execute,
				Error.IsEmpty() ? TEXT("Failed to create custom event signature.") : Error,
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

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_custom_event"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	SetCustomEventTarget(Result, Request.AssetPath, Request.GraphName, Request.EventName);
	Result.Data = MakeSignatureResultData(false, TEXT("custom_event_result"), true);
	SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("exists"), true);
	SetSignatureResultBool(Result.Data, TEXT("custom_event_result"), TEXT("signature_matches"), true);
	SetSignatureResultNumber(Result.Data, TEXT("custom_event_result"), TEXT("added_inputs"), EventNode == Target.ExistingEvent ? Target.MissingPins.Num() : Target.RequestedPins.Num());
	Result.Validation = MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::RemoveSignature(
	const FBlueprintHelperRemoveSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.SignatureName.IsEmpty() || Request.SignatureKind.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("remove_signature requires asset_path, signature_kind, and signature_name."),
			TEXT("remove_signature"));
		SetRemoveSignatureTarget(Result, Request);
		Result.Data = MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("invalid_signature_payload"),
			TEXT("remove_signature requires asset_path, signature_kind, and signature_name."),
			TEXT("remove_signature"));
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.SignatureKind != TEXT("function") && Request.SignatureKind != TEXT("custom_event"))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("signature_remove_unsupported_kind"),
			EBlueprintHelperToolStage::Preflight,
			TEXT("remove_signature currently has preflight placeholders for function and custom_event only."),
			TEXT("signature_kind"));
		SetRemoveSignatureTarget(Result, Request);
		Result.Data = MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("signature_remove_unsupported_kind"),
			TEXT("remove_signature currently has preflight placeholders for function and custom_event only."),
			TEXT("signature_kind"));
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FString ListError;
	if (!BlueprintCanBeListed(StructureService, Request.AssetPath, ListError))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("remove_signature"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		SetRemoveSignatureTarget(Result, Request);
		Result.Data = MakeSignatureBlockedData(
			Request.bDryRun,
			TEXT("target_blueprint_not_found"),
			ListError,
			TEXT("asset_path"));
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	if (Request.SignatureKind == TEXT("custom_event"))
	{
		if (Request.GraphName.IsEmpty())
		{
			FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
				TEXT("remove_signature"),
				TEXT("invalid_signature_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("remove_signature custom_event requires graph_name."),
				TEXT("graph_name"));
			SetRemoveSignatureTarget(Result, Request);
			Result.Data = MakeSignatureBlockedData(
				Request.bDryRun,
				TEXT("invalid_signature_payload"),
				TEXT("remove_signature custom_event requires graph_name."),
				TEXT("graph_name"));
			Result.Validation = MakeSignatureValidation(false, false);
			return Result;
		}

		FString ValidationCode;
		EBlueprintHelperToolStage ValidationStage = EBlueprintHelperToolStage::Preflight;
		FString ValidationMessage;
		FString ValidationField;
		if (!TryValidateEventGraphExists(
			StructureService,
			Request.AssetPath,
			Request.GraphName,
			ValidationCode,
			ValidationStage,
			ValidationMessage,
			ValidationField))
		{
			FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
				TEXT("remove_signature"),
				ValidationCode,
				ValidationStage,
				ValidationMessage,
				ValidationField);
			SetRemoveSignatureTarget(Result, Request);
			Result.Data = MakeSignatureBlockedData(
				Request.bDryRun,
				ValidationCode,
				ValidationMessage,
				ValidationField);
			Result.Validation = MakeSignatureValidation(false, false);
			return Result;
		}
	}

	FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
		TEXT("remove_signature"),
		TEXT("signature_remove_unsupported"),
		Request.bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Preflight,
		TEXT("Signature removal is not wired for safe execution yet."),
		TEXT("remove_signature"));
	SetRemoveSignatureTarget(Result, Request);
	Result.Data = MakeSignatureBlockedData(
		Request.bDryRun,
		TEXT("signature_remove_unsupported"),
		TEXT("Signature removal is not wired for safe execution yet."),
		TEXT("remove_signature"));
	Result.Validation = MakeSignatureValidation(false, false);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureEventDispatcher(
	const FBlueprintHelperEnsureEventDispatcherSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.DispatcherName.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_event_dispatcher requires asset_path and dispatcher_name."),
			TEXT("ensure_event_dispatcher"));
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	if (!IsValidNameCollisionPolicy(Request.NameCollisionPolicy))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_name_collision_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_event_dispatcher name_collision_policy must be reuse_if_exists or fail_if_exists."),
			TEXT("name_collision_policy"));
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	TArray<FBlueprintHelperSignaturePinSpec> RequestedPins;
	FString PinMessage;
	FString PinField;
	if (!TryBuildSignaturePinSpecs(Request.Inputs, RequestedPins, PinMessage, PinField))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			PinMessage,
			PinField);
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperEventDispatcherInfo ExistingDispatcher;
	FString ListError;
	const bool bExists = TryFindEventDispatcher(
		StructureService,
		Request.AssetPath,
		Request.DispatcherName,
		ExistingDispatcher,
		ListError);
	if (!bExists && !ListError.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	if (bExists && Request.NameCollisionPolicy == TEXT("fail_if_exists"))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("event_dispatcher_already_exists"),
			EBlueprintHelperToolStage::Preflight,
			FString::Printf(TEXT("Event dispatcher already exists: %s"), *Request.DispatcherName),
			TEXT("dispatcher_name"));
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	if (bExists)
	{
		FString MismatchMessage;
		if (!EventDispatcherSignatureMatches(ExistingDispatcher, RequestedPins, MismatchMessage))
		{
			FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
				TEXT("ensure_event_dispatcher"),
				TEXT("event_dispatcher_signature_change_unsupported"),
				EBlueprintHelperToolStage::Preflight,
				MismatchMessage,
				TEXT("inputs"));
			SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
			Result.Data = MakeSignatureBlockedData(
				Request.bDryRun,
				TEXT("event_dispatcher_signature_change_unsupported"),
				MismatchMessage,
				TEXT("inputs"),
				TEXT("event_dispatcher_result"));
			Result.Validation = MakeSignatureValidation(false, false);
			return Result;
		}
	}

	if (Request.bDryRun)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			TEXT("ensure_event_dispatcher"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Data = MakeSignatureResultData(true, TEXT("event_dispatcher_result"), false);
		SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), bExists);
		Result.Validation = MakeSignatureValidation(!bExists, !bExists);
		return Result;
	}

	if (bExists)
	{
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::NoOp(
			TEXT("ensure_event_dispatcher"),
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Data = MakeSignatureResultData(false, TEXT("event_dispatcher_result"), false);
		SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), Request.DispatcherName);
	if (Request.Inputs.Num() > 0)
	{
		Params->SetArrayField(TEXT("params"), Request.Inputs);
	}

	FString Error;
	if (!StructureService.AddEventDispatcher(MakeGraphTarget(Request.AssetPath), Params, Error))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_event_dispatcher"),
			TEXT("event_dispatcher_create_failed"),
			EBlueprintHelperToolStage::Execute,
			Error.IsEmpty() ? TEXT("Failed to create event dispatcher signature.") : Error,
			TEXT("dispatcher_name"));
		SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Applied(
		TEXT("ensure_event_dispatcher"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	SetEventDispatcherTarget(Result, Request.AssetPath, Request.DispatcherName);
	Result.Data = MakeSignatureResultData(false, TEXT("event_dispatcher_result"), false);
	SetSignatureResultBool(Result.Data, TEXT("event_dispatcher_result"), TEXT("exists"), true);
	SetSignatureResultNumber(Result.Data, TEXT("event_dispatcher_result"), TEXT("added_inputs"), RequestedPins.Num());
	Result.Validation = MakeSignatureValidation(true, true);
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperSignatureService::EnsureOverrideEvent(
	const FBlueprintHelperEnsureOverrideEventSignatureRequest& Request) const
{
	if (Request.AssetPath.IsEmpty() || Request.EventName.IsEmpty())
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("invalid_signature_payload"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_override_event requires asset_path and event_name."),
			TEXT("ensure_override_event"));
		SetOverrideEventTarget(Result, Request);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	const FString EventKind = Request.EventKind.IsEmpty() ? TEXT("native_event") : Request.EventKind;
	if (EventKind != TEXT("native_event") && EventKind != TEXT("override_event"))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("invalid_override_event_kind"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("ensure_override_event event_kind must be native_event or override_event."),
			TEXT("event_kind"));
		SetOverrideEventTarget(Result, Request);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FString ListError;
	if (!BlueprintCanBeListed(StructureService, Request.AssetPath, ListError))
	{
		FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
			TEXT("ensure_override_event"),
			TEXT("target_blueprint_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			ListError,
			TEXT("asset_path"));
		SetOverrideEventTarget(Result, Request);
		Result.Validation = MakeSignatureValidation(false, false);
		return Result;
	}

	FBlueprintHelperToolResultBase Result = MakeSignatureFailure(
		TEXT("ensure_override_event"),
		TEXT("override_event_signature_unsupported"),
		Request.bDryRun ? EBlueprintHelperToolStage::DryRun : EBlueprintHelperToolStage::Preflight,
		TEXT("Override/native event signature creation is not wired for safe execution yet."),
		TEXT("ensure_override_event"));
	SetOverrideEventTarget(Result, Request);
	Result.Data = MakeSignatureBlockedData(
		Request.bDryRun,
		TEXT("override_event_signature_unsupported"),
		TEXT("Override/native event signature creation is not wired for safe execution yet."),
		TEXT("ensure_override_event"),
		TEXT("override_event_result"));
	Result.Validation = MakeSignatureValidation(false, false);
	return Result;
}
