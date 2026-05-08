// BlueprintHelper Service Layer - Agent semantic import implementation

#include "Shared/Services/BlueprintHelperAgentImportService.h"

#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/Debug/BlueprintHelperCompileService.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperGraphResolver.h"
#include "Systems/ToolClusters/GraphWrite/GraphSupport/BlueprintHelperScopedAssetMutation.h"
#include "Systems/ToolClusters/GraphWrite/NodeHandlers/BlueprintNodeHandler.h"
#include "Systems/ToolClusters/GraphWrite/TextToBlueprintGenerator.h"

#include "Containers/Queue.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CustomEvent.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 ExecLayerSpacingX = 420;
	constexpr int32 ExecNodeSpacingY = 220;
	constexpr int32 DataNodeOffsetX = -260;
	constexpr int32 DataNodeOffsetY = -120;
	constexpr int32 BranchSpacingY = 260;
	constexpr int32 OrphanAreaOffsetY = 900;
	constexpr int32 CommentPadding = 80;

	const TCHAR* AgentImportSchema = TEXT("BlueprintHelper.AgentImportGraph");
	const TCHAR* AgentImportResultSchema = TEXT("BlueprintHelper.AgentImportResult");

	const TSet<FString>& ForbiddenFields()
	{
		static const TSet<FString> Fields = {
			TEXT("Pos"),
			TEXT("PosX"),
			TEXT("PosY"),
			TEXT("NodePosX"),
			TEXT("NodePosY"),
			TEXT("NodeWidth"),
			TEXT("NodeHeight"),
			TEXT("GraphGuid"),
			TEXT("NodeGuid"),
			TEXT("PinGuid"),
			TEXT("PersistentGuid"),
			TEXT("CompilerMessage"),
			TEXT("ErrorType"),
			TEXT("ErrorMsg"),
			TEXT("AdvancedPinDisplay"),
			TEXT("bCommentBubbleVisible"),
			TEXT("CommentBubblePinned")
		};
		return Fields;
	}

	const TSet<FString>& SupportedKinds()
	{
		static const TSet<FString> Kinds = {
			TEXT("event"),
			TEXT("custom_event"),
			TEXT("call"),
			TEXT("get"),
			TEXT("set"),
			TEXT("branch"),
			TEXT("sequence"),
			TEXT("comment")
		};
		return Kinds;
	}

	void AddDiagnostic(
		FBlueprintHelperAgentImportResult& Result,
		EBlueprintHelperAgentImportDiagnosticSeverity Severity,
		const FString& Code,
		const FString& Path,
		const FString& Message,
		const FString& Suggestion = TEXT(""))
	{
		Result.Diagnostics.Add({Severity, Code, Path, Message, Suggestion});
		if (Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Warning)
		{
			++Result.WarningCount;
			Result.Warnings.Add(Message);
		}
		if (Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Error && Result.ErrorCode.IsEmpty())
		{
			++Result.ErrorCount;
			Result.ErrorCode = Code;
			Result.Message = Message;
		}
		else if (Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Error)
		{
			++Result.ErrorCount;
		}
	}

	EBlueprintHelperAgentImportDiagnosticSeverity ConvertGeneratorSeverity(const FString& Severity)
	{
		if (Severity.Equals(TEXT("error"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperAgentImportDiagnosticSeverity::Error;
		}
		if (Severity.Equals(TEXT("warning"), ESearchCase::IgnoreCase))
		{
			return EBlueprintHelperAgentImportDiagnosticSeverity::Warning;
		}
		return EBlueprintHelperAgentImportDiagnosticSeverity::Info;
	}

	void AddGeneratorDiagnosticsToResult(
		const TArray<FBlueprintGeneratorDiagnostic>& GeneratorDiagnostics,
		FBlueprintHelperAgentImportResult& Result)
	{
		for (const FBlueprintGeneratorDiagnostic& Diagnostic : GeneratorDiagnostics)
		{
			AddDiagnostic(
				Result,
				ConvertGeneratorSeverity(Diagnostic.Severity),
				Diagnostic.Code,
				Diagnostic.PinName.IsEmpty()
					? FString::Printf(TEXT("$.nodes.%s"), *Diagnostic.NodeId)
					: FString::Printf(TEXT("$.nodes.%s.%s"), *Diagnostic.NodeId, *Diagnostic.PinName),
				Diagnostic.Message);
		}
	}

	void FinalizeAgentImportStatus(FBlueprintHelperAgentImportResult& Result)
	{
		if (Result.HasErrors())
		{
			Result.bSuccess = false;
			Result.Status = TEXT("failed");
			return;
		}

		Result.bSuccess = true;
		if (Result.CreatedNodeCount == 0 && Result.CreatedLinkCount == 0 && Result.CreatedVariableCount == 0)
		{
			Result.Status = TEXT("no_op");
		}
		else if (Result.WarningCount > 0)
		{
			Result.Status = TEXT("partial_success");
		}
		else
		{
			Result.Status = TEXT("full_success");
		}
	}

	TSet<UEdGraphNode*> CaptureGraphNodeSnapshot(UEdGraph* Graph)
	{
		TSet<UEdGraphNode*> Snapshot;
		if (!Graph)
		{
			return Snapshot;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node)
			{
				Snapshot.Add(Node);
			}
		}
		return Snapshot;
	}

	UK2Node_CustomEvent* FindExistingCustomEventNode(UEdGraph* Graph, const FString& EventName)
	{
		if (!Graph || EventName.IsEmpty())
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

	TSet<FName> CaptureBlueprintVariableSnapshot(UBlueprint* Blueprint)
	{
		TSet<FName> Snapshot;
		if (!Blueprint)
		{
			return Snapshot;
		}

		for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
		{
			Snapshot.Add(Variable.VarName);
		}
		return Snapshot;
	}

	int32 RemoveNodesCreatedAfterSnapshot(UEdGraph* Graph, const TSet<UEdGraphNode*>& Snapshot)
	{
		if (!Graph)
		{
			return 0;
		}

		int32 RemovedCount = 0;
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		for (int32 NodeIndex = Graph->Nodes.Num() - 1; NodeIndex >= 0; --NodeIndex)
		{
			UEdGraphNode* Node = Graph->Nodes[NodeIndex];
			if (!Node || Snapshot.Contains(Node))
			{
				continue;
			}

			if (Blueprint)
			{
				FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			}
			else
			{
				Graph->RemoveNode(Node);
			}
			++RemovedCount;
		}

		if (RemovedCount > 0)
		{
			Graph->NotifyGraphChanged();
		}
		return RemovedCount;
	}

	int32 RemoveVariablesCreatedAfterSnapshot(UBlueprint* Blueprint, const TSet<FName>& Snapshot)
	{
		if (!Blueprint)
		{
			return 0;
		}

		int32 RemovedCount = 0;
		for (int32 Index = Blueprint->NewVariables.Num() - 1; Index >= 0; --Index)
		{
			const FName VariableName = Blueprint->NewVariables[Index].VarName;
			if (Snapshot.Contains(VariableName))
			{
				continue;
			}

			FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableName);
			++RemovedCount;
		}
		return RemovedCount;
	}

	void RollbackAgentImportMutation(
		FBlueprintHelperScopedAssetMutation& Mutation,
		UEdGraph* TargetGraph,
		UBlueprint* Blueprint,
		const TSet<UEdGraphNode*>& NodeSnapshot,
		const TSet<FName>& VariableSnapshot,
		FBlueprintHelperAgentImportResult& Result)
	{
		RemoveNodesCreatedAfterSnapshot(TargetGraph, NodeSnapshot);
		RemoveVariablesCreatedAfterSnapshot(Blueprint, VariableSnapshot);
		Mutation.Rollback();
		Result.bRolledBack = true;
		Result.RollbackCount = 1;
		Result.bSuccess = false;
		Result.Status = TEXT("failed");
		AddDiagnostic(Result,
			EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("strict_import_rolled_back"),
			TEXT("$.options.strict"),
			TEXT("strict AgentImportGraph detected a partial or failed import and rolled back this transaction."));
	}

	FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return TEXT("");
		}

		switch (Value->Type)
		{
		case EJson::String:
			return Value->AsString();
		case EJson::Number:
			return FString::SanitizeFloat(Value->AsNumber());
		case EJson::Boolean:
			return Value->AsBool() ? TEXT("true") : TEXT("false");
		case EJson::Null:
			return TEXT("");
		default:
			break;
		}

		FString Serialized;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
		FJsonSerializer::Serialize(Value.ToSharedRef(), TEXT(""), Writer);
		return Serialized;
	}

	bool SplitEndpoint(const FString& Endpoint, FString& OutNode, FString& OutPin)
	{
		int32 DotIndex = INDEX_NONE;
		if (!Endpoint.FindLastChar(TEXT('.'), DotIndex) || DotIndex <= 0 || DotIndex >= Endpoint.Len() - 1)
		{
			return false;
		}

		OutNode = Endpoint.Left(DotIndex);
		OutPin = Endpoint.Mid(DotIndex + 1);
		return !OutNode.IsEmpty() && !OutPin.IsEmpty();
	}

	bool LooksLikeEndpoint(const FString& Value)
	{
		FString Node;
		FString Pin;
		return SplitEndpoint(Value, Node, Pin);
	}

	FString NormalizeKind(const FString& Kind)
	{
		return Kind.ToLower();
	}

	FString FunctionNameForGenerator(const FString& InFunction)
	{
		FString Function = InFunction.TrimStartAndEnd();
		int32 ColonIndex = INDEX_NONE;
		if (Function.FindLastChar(TEXT(':'), ColonIndex) && ColonIndex < Function.Len() - 1)
		{
			Function = Function.Mid(ColonIndex + 1);
		}
		else if (Function.StartsWith(TEXT("/Script/")))
		{
			int32 DotIndex = INDEX_NONE;
			if (Function.FindLastChar(TEXT('.'), DotIndex) && DotIndex < Function.Len() - 1)
			{
				Function = Function.Mid(DotIndex + 1);
			}
		}
		return Function;
	}

	FParsedPinType PinTypeFromString(const FString& Type)
	{
		FParsedPinType ParsedType;
		ParsedType.Category = Type.IsEmpty() ? TEXT("bool") : Type;
		return ParsedType;
	}

	FParsedPinType PinTypeFromDeclaration(const FBlueprintHelperAgentImportVariableDeclaration& Declaration)
	{
		return PinTypeFromString(Declaration.Type);
	}

	void ScanForbiddenFields(
		const TSharedPtr<FJsonObject>& Object,
		const FString& Path,
		bool bStrict,
		FBlueprintHelperAgentImportResult& Result)
	{
		if (!Object.IsValid())
		{
			return;
		}

		for (const auto& Pair : Object->Values)
		{
			const FString FieldPath = Path.IsEmpty() ? Pair.Key : FString::Printf(TEXT("%s.%s"), *Path, *Pair.Key);
			if (ForbiddenFields().Contains(Pair.Key))
			{
				AddDiagnostic(
					Result,
					bStrict ? EBlueprintHelperAgentImportDiagnosticSeverity::Error : EBlueprintHelperAgentImportDiagnosticSeverity::Warning,
					TEXT("ForbiddenField"),
					FieldPath,
					FString::Printf(TEXT("AgentImportGraph 不接受字。'%s'。"), *Pair.Key),
					TEXT("删除坐标、GUID、Pin 快照或编辑器状态字段，让插件自动生成。"));
			}

			const TSharedPtr<FJsonObject>* ChildObject = nullptr;
			if (Pair.Value->TryGetObject(ChildObject))
			{
				ScanForbiddenFields(*ChildObject, FieldPath, bStrict, Result);
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
			if (Pair.Value->TryGetArray(ArrayValue))
			{
				for (int32 Index = 0; Index < ArrayValue->Num(); ++Index)
				{
					const TSharedPtr<FJsonObject>* ArrayObject = nullptr;
					if ((*ArrayValue)[Index]->TryGetObject(ArrayObject))
					{
						ScanForbiddenFields(*ArrayObject, FString::Printf(TEXT("%s[%d]"), *FieldPath, Index), bStrict, Result);
					}
				}
			}
		}
	}

	void ParseOptions(const TSharedPtr<FJsonObject>& Root, FBlueprintHelperAgentImportOptions& OutOptions)
	{
		const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("options"), OptionsObject) || !OptionsObject || !OptionsObject->IsValid())
		{
			return;
		}

		(*OptionsObject)->TryGetBoolField(TEXT("compile"), OutOptions.bCompile);
		(*OptionsObject)->TryGetBoolField(TEXT("save"), OutOptions.bSave);
		(*OptionsObject)->TryGetBoolField(TEXT("strict"), OutOptions.bStrict);
		(*OptionsObject)->TryGetBoolField(TEXT("dry_run"), OutOptions.bDryRun);
		(*OptionsObject)->TryGetBoolField(TEXT("create_missing_variables"), OutOptions.bCreateMissingVariables);
		(*OptionsObject)->TryGetBoolField(TEXT("reconstruct_existing_nodes"), OutOptions.bReconstructExistingNodes);
	}

	void ParseInputs(const TSharedPtr<FJsonObject>& NodeObject, FBlueprintHelperAgentImportNode& OutNode)
	{
		const TSharedPtr<FJsonObject>* InputsObject = nullptr;
		if (!NodeObject->TryGetObjectField(TEXT("inputs"), InputsObject) || !InputsObject || !InputsObject->IsValid())
		{
			return;
		}

		for (const auto& Pair : (*InputsObject)->Values)
		{
			OutNode.Inputs.Add(Pair.Key, JsonValueToString(Pair.Value));
		}
	}

	void ParseContains(const TSharedPtr<FJsonObject>& NodeObject, FBlueprintHelperAgentImportNode& OutNode)
	{
		const TArray<TSharedPtr<FJsonValue>>* ContainsArray = nullptr;
		if (!NodeObject->TryGetArrayField(TEXT("contains"), ContainsArray) || !ContainsArray)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *ContainsArray)
		{
			FString NodeId;
			if (Value->TryGetString(NodeId) && !NodeId.IsEmpty())
			{
				OutNode.Contains.Add(NodeId);
			}
		}
	}

	bool ParseLinkEndpoint(
		const TSharedPtr<FJsonObject>& LinkObject,
		const FString& EndpointField,
		const FString& NodeField,
		const FString& PinField,
		FString& OutNode,
		FString& OutPin)
	{
		FString Endpoint;
		if (LinkObject->TryGetStringField(*EndpointField, Endpoint) && !Endpoint.IsEmpty())
		{
			return SplitEndpoint(Endpoint, OutNode, OutPin);
		}

		LinkObject->TryGetStringField(*NodeField, OutNode);
		LinkObject->TryGetStringField(*PinField, OutPin);
		return !OutNode.IsEmpty() && !OutPin.IsEmpty();
	}

	void ParseDeclarations(
		const TSharedPtr<FJsonObject>& Root,
		FBlueprintHelperAgentImportParsedRequest& OutRequest)
	{
		const TSharedPtr<FJsonObject>* DeclarationsObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("declarations"), DeclarationsObject) || !DeclarationsObject || !DeclarationsObject->IsValid())
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* VariablesArray = nullptr;
		if (!(*DeclarationsObject)->TryGetArrayField(TEXT("variables"), VariablesArray) || !VariablesArray)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *VariablesArray)
		{
			const TSharedPtr<FJsonObject> VariableObject = Value->AsObject();
			if (!VariableObject.IsValid())
			{
				continue;
			}

			FBlueprintHelperAgentImportVariableDeclaration Declaration;
			VariableObject->TryGetStringField(TEXT("name"), Declaration.Name);
			VariableObject->TryGetStringField(TEXT("type"), Declaration.Type);
			VariableObject->TryGetStringField(TEXT("category"), Declaration.Category);
			VariableObject->TryGetBoolField(TEXT("editable"), Declaration.bEditable);
			if (VariableObject->HasField(TEXT("default")))
			{
				Declaration.DefaultValue = JsonValueToString(VariableObject->Values.FindChecked(TEXT("default")));
			}
			else
			{
				VariableObject->TryGetStringField(TEXT("default_value"), Declaration.DefaultValue);
			}
			if (!Declaration.Name.IsEmpty())
			{
				OutRequest.Variables.Add(Declaration);
			}
		}
	}

	bool ParseRoot(
		const FString& JsonText,
		FBlueprintHelperAgentImportParsedRequest& OutRequest,
		FBlueprintHelperAgentImportResult& Result)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			AddDiagnostic(Result,
				EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("JsonParseFailed"),
				TEXT("$"),
				TEXT("Agent import JSON is not valid JSON."));
			return false;
		}

		ParseOptions(Root, OutRequest.Options);
		ScanForbiddenFields(Root, TEXT("$"), OutRequest.Options.bStrict, Result);

		FString Schema;
		if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != AgentImportSchema)
		{
			AddDiagnostic(Result,
				EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("UnsupportedSchema"),
				TEXT("$.schema"),
				TEXT("schema 必须。BlueprintHelper.AgentImportGraph。"));
		}

		FString Version;
		if (!Root->TryGetStringField(TEXT("version"), Version) || Version != TEXT("1.0"))
		{
			AddDiagnostic(Result,
				EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("UnsupportedVersion"),
				TEXT("$.version"),
				TEXT("version 必须。1.0。"));
		}

		Root->TryGetStringField(TEXT("target_blueprint"), OutRequest.Target.BlueprintPath);
		Root->TryGetStringField(TEXT("target_graph"), OutRequest.Target.GraphName);
		if (OutRequest.Target.BlueprintPath.IsEmpty())
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("MissingTargetBlueprint"), TEXT("$.target_blueprint"),
				TEXT("AgentImportGraph 必须显式指定 target_blueprint。"));
		}
		if (OutRequest.Target.GraphName.IsEmpty())
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("MissingTargetGraph"), TEXT("$.target_graph"),
				TEXT("AgentImportGraph 必须显式指定 target_graph。"));
		}

		FString Mode;
		if (!Root->TryGetStringField(TEXT("mode"), Mode) || !Mode.Equals(TEXT("append"), ESearchCase::IgnoreCase))
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("UnsupportedMode"), TEXT("$.mode"),
				TEXT("第一阶段只支持 mode=append。"));
		}

		FString Layout;
		if (Root->TryGetStringField(TEXT("layout"), Layout) && !Layout.IsEmpty())
		{
			if (Layout.Equals(TEXT("auto"), ESearchCase::IgnoreCase))
			{
				OutRequest.Layout = EBlueprintHelperAgentLayoutStrategy::Auto;
			}
			else if (Layout.Equals(TEXT("append_right"), ESearchCase::IgnoreCase))
			{
				OutRequest.Layout = EBlueprintHelperAgentLayoutStrategy::AppendRight;
			}
			else
			{
				AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
					TEXT("UnsupportedLayout"), TEXT("$.layout"),
					TEXT("第一阶段只支持 layout=auto 或 append_right。"));
			}
		}

		ParseDeclarations(Root, OutRequest);

		TSet<FString> NodeIds;
		const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
		if (!Root->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("MissingNodes"), TEXT("$.nodes"),
				TEXT("AgentImportGraph 必须包含 nodes 数组。"));
		}
		else
		{
			for (int32 Index = 0; Index < NodesArray->Num(); ++Index)
			{
				const TSharedPtr<FJsonObject> NodeObject = (*NodesArray)[Index]->AsObject();
				if (!NodeObject.IsValid())
				{
					continue;
				}

				FBlueprintHelperAgentImportNode Node;
				NodeObject->TryGetStringField(TEXT("id"), Node.Id);
				NodeObject->TryGetStringField(TEXT("kind"), Node.Kind);
				Node.Kind = NormalizeKind(Node.Kind);
				NodeObject->TryGetStringField(TEXT("label"), Node.Label);
				NodeObject->TryGetStringField(TEXT("function"), Node.Function);
				NodeObject->TryGetStringField(TEXT("event"), Node.EventName);
				if (Node.EventName.IsEmpty())
				{
					NodeObject->TryGetStringField(TEXT("event_name"), Node.EventName);
				}
				NodeObject->TryGetStringField(TEXT("name"), Node.CustomEventName);
				NodeObject->TryGetStringField(TEXT("var"), Node.VariableName);
				NodeObject->TryGetStringField(TEXT("type"), Node.VariableType);
				NodeObject->TryGetStringField(TEXT("condition"), Node.Condition);
				NodeObject->TryGetStringField(TEXT("text"), Node.CommentText);
				if (NodeObject->HasField(TEXT("value")))
				{
					Node.Value = JsonValueToString(NodeObject->Values.FindChecked(TEXT("value")));
				}

				ParseInputs(NodeObject, Node);
				ParseContains(NodeObject, Node);

				const FString Path = FString::Printf(TEXT("$.nodes[%d]"), Index);
				if (Node.Id.IsEmpty())
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("MissingNodeId"), Path + TEXT(".id"),
						TEXT("节点缺少 id。"));
				}
				else if (NodeIds.Contains(Node.Id))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("DuplicateNodeId"), Path + TEXT(".id"),
						FString::Printf(TEXT("节点 id 重复。s。"), *Node.Id));
				}
				else
				{
					NodeIds.Add(Node.Id);
				}

				if (!SupportedKinds().Contains(Node.Kind))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnknownNodeKind"), Path + TEXT(".kind"),
						FString::Printf(TEXT("不支持的节点 kind。s。"), *Node.Kind));
				}

				if (Node.Kind == TEXT("branch") && !Node.Condition.IsEmpty())
				{
					if (LooksLikeEndpoint(Node.Condition))
					{
						FBlueprintHelperAgentImportLink ConditionLink;
						ConditionLink.Kind = TEXT("data");
						SplitEndpoint(Node.Condition, ConditionLink.FromNode, ConditionLink.FromPin);
						ConditionLink.ToNode = Node.Id;
						ConditionLink.ToPin = TEXT("condition");
						ConditionLink.Path = Path + TEXT(".condition");
						OutRequest.Links.Add(ConditionLink);
					}
					else
					{
						Node.Inputs.FindOrAdd(TEXT("condition")) = Node.Condition;
					}
				}

				OutRequest.Nodes.Add(Node);
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* LinksArray = nullptr;
		if (Root->TryGetArrayField(TEXT("links"), LinksArray) && LinksArray)
		{
			for (int32 Index = 0; Index < LinksArray->Num(); ++Index)
			{
				const TSharedPtr<FJsonObject> LinkObject = (*LinksArray)[Index]->AsObject();
				if (!LinkObject.IsValid())
				{
					continue;
				}

				FBlueprintHelperAgentImportLink Link;
				Link.Path = FString::Printf(TEXT("$.links[%d]"), Index);
				LinkObject->TryGetStringField(TEXT("kind"), Link.Kind);
				Link.Kind = Link.Kind.ToLower();
				if (Link.Kind != TEXT("exec") && Link.Kind != TEXT("data"))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnsupportedLinkKind"), Link.Path + TEXT(".kind"),
						FString::Printf(TEXT("不支持的连线 kind。s。"), *Link.Kind));
				}

				if (!ParseLinkEndpoint(LinkObject, TEXT("from"), TEXT("from_node"), TEXT("from_pin"), Link.FromNode, Link.FromPin))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("InvalidLinkEndpoint"), Link.Path + TEXT(".from"),
						TEXT("连线来源必须。node.pin 。from_node/from_pin。"));
				}
				if (!ParseLinkEndpoint(LinkObject, TEXT("to"), TEXT("to_node"), TEXT("to_pin"), Link.ToNode, Link.ToPin))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("InvalidLinkEndpoint"), Link.Path + TEXT(".to"),
						TEXT("连线目标必须。node.pin 。to_node/to_pin。"));
				}

				if (!Link.FromNode.IsEmpty() && !NodeIds.Contains(Link.FromNode))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("InvalidLinkEndpoint"), Link.Path + TEXT(".from"),
						FString::Printf(TEXT("连线引用了不存在的来源节点：%s。"), *Link.FromNode));
				}
				if (!Link.ToNode.IsEmpty() && !NodeIds.Contains(Link.ToNode))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("InvalidLinkEndpoint"), Link.Path + TEXT(".to"),
						FString::Printf(TEXT("连线引用了不存在的目标节点：%s。"), *Link.ToNode));
				}

				OutRequest.Links.Add(Link);
			}
		}

		return !Result.HasErrors();
	}

	float CalculateAppendBaseX(UEdGraph* TargetGraph)
	{
		float MaxX = 0.0f;
		bool bHasNodes = false;
		if (!TargetGraph)
		{
			return 0.0f;
		}

		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			bHasNodes = true;
			MaxX = FMath::Max(MaxX, static_cast<float>(Node->NodePosX + Node->NodeWidth));
		}

		return bHasNodes ? MaxX + ExecLayerSpacingX : 0.0f;
	}

	void ApplyAutoLayout(UEdGraph* TargetGraph, FBlueprintHelperAgentImportParsedRequest& Request)
	{
		const float BaseX = (Request.Layout == EBlueprintHelperAgentLayoutStrategy::AppendRight || (TargetGraph && TargetGraph->Nodes.Num() > 0))
			? CalculateAppendBaseX(TargetGraph)
			: 0.0f;

		TMap<FString, int32> NodeIndexById;
		for (int32 Index = 0; Index < Request.Nodes.Num(); ++Index)
		{
			NodeIndexById.Add(Request.Nodes[Index].Id, Index);
		}

		TMap<FString, TArray<FBlueprintHelperAgentImportLink>> OutExecLinks;
		TMap<FString, int32> IncomingExecCount;
		for (const FBlueprintHelperAgentImportLink& Link : Request.Links)
		{
			if (Link.Kind != TEXT("exec"))
			{
				continue;
			}
			OutExecLinks.FindOrAdd(Link.FromNode).Add(Link);
			IncomingExecCount.FindOrAdd(Link.ToNode)++;
		}

		TMap<FString, int32> LayerByNode;
		TQueue<FString> Queue;
		for (const FBlueprintHelperAgentImportNode& Node : Request.Nodes)
		{
			if (Node.Kind == TEXT("event") || Node.Kind == TEXT("custom_event") || !IncomingExecCount.Contains(Node.Id))
			{
				if (!LayerByNode.Contains(Node.Id))
				{
					LayerByNode.Add(Node.Id, 0);
					Queue.Enqueue(Node.Id);
				}
			}
		}

		FString CurrentId;
		while (Queue.Dequeue(CurrentId))
		{
			const int32 CurrentLayer = LayerByNode.FindRef(CurrentId);
			const TArray<FBlueprintHelperAgentImportLink>* Links = OutExecLinks.Find(CurrentId);
			if (!Links)
			{
				continue;
			}

			for (const FBlueprintHelperAgentImportLink& Link : *Links)
			{
				const int32 NextLayer = CurrentLayer + 1;
				if (!LayerByNode.Contains(Link.ToNode) || LayerByNode[Link.ToNode] < NextLayer)
				{
					LayerByNode.FindOrAdd(Link.ToNode) = NextLayer;
					Queue.Enqueue(Link.ToNode);
				}
			}
		}

		TMap<int32, int32> LayerRowCounts;
		for (FBlueprintHelperAgentImportNode& Node : Request.Nodes)
		{
			if (Node.Kind == TEXT("comment"))
			{
				continue;
			}

			int32 Layer = LayerByNode.Contains(Node.Id) ? LayerByNode[Node.Id] : 0;
			int32 Row = LayerRowCounts.FindOrAdd(Layer)++;
			const int32 SpacingY = Node.Kind == TEXT("branch") ? BranchSpacingY : ExecNodeSpacingY;
			Node.Position = FVector2D(BaseX + Layer * ExecLayerSpacingX, Row * SpacingY);
		}

		for (const FBlueprintHelperAgentImportLink& Link : Request.Links)
		{
			if (Link.Kind != TEXT("data") || !NodeIndexById.Contains(Link.FromNode) || !NodeIndexById.Contains(Link.ToNode))
			{
				continue;
			}

			FBlueprintHelperAgentImportNode& FromNode = Request.Nodes[NodeIndexById[Link.FromNode]];
			const FBlueprintHelperAgentImportNode& ToNode = Request.Nodes[NodeIndexById[Link.ToNode]];
			if (!LayerByNode.Contains(FromNode.Id))
			{
				FromNode.Position = ToNode.Position + FVector2D(DataNodeOffsetX, DataNodeOffsetY);
			}
		}

		int32 OrphanRow = 0;
		for (FBlueprintHelperAgentImportNode& Node : Request.Nodes)
		{
			if (Node.Kind == TEXT("comment"))
			{
				continue;
			}
			if (!LayerByNode.Contains(Node.Id))
			{
				Node.Position = FVector2D(BaseX, OrphanAreaOffsetY + OrphanRow * ExecNodeSpacingY);
				++OrphanRow;
			}
		}

		for (FBlueprintHelperAgentImportNode& Node : Request.Nodes)
		{
			if (Node.Kind != TEXT("comment"))
			{
				continue;
			}

			bool bHasBounds = false;
			float MinX = 0.0f;
			float MinY = 0.0f;
			float MaxX = 0.0f;
			float MaxY = 0.0f;
			for (const FString& ContainedId : Node.Contains)
			{
				const int32* Index = NodeIndexById.Find(ContainedId);
				if (!Index)
				{
					continue;
				}
				const FVector2D Pos = Request.Nodes[*Index].Position;
				if (!bHasBounds)
				{
					MinX = MaxX = Pos.X;
					MinY = MaxY = Pos.Y;
					bHasBounds = true;
				}
				else
				{
					MinX = FMath::Min(MinX, Pos.X);
					MinY = FMath::Min(MinY, Pos.Y);
					MaxX = FMath::Max(MaxX, Pos.X);
					MaxY = FMath::Max(MaxY, Pos.Y);
				}
			}

			if (bHasBounds)
			{
				Node.Position = FVector2D(MinX - CommentPadding, MinY - CommentPadding);
				Node.Size = FVector2D((MaxX - MinX) + CommentPadding * 2 + 260.0f, (MaxY - MinY) + CommentPadding * 2 + 160.0f);
			}
			else
			{
				Node.Position = FVector2D(BaseX, OrphanAreaOffsetY + OrphanRow * ExecNodeSpacingY);
				++OrphanRow;
			}
		}
	}

	bool BlueprintHasMemberVariable(UBlueprint* Blueprint, const FString& Name)
	{
		if (!Blueprint || Name.IsEmpty())
		{
			return false;
		}

		const FName VarName(*Name);
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (Var.VarName == VarName)
			{
				return true;
			}
		}

		return (Blueprint->SkeletonGeneratedClass && Blueprint->SkeletonGeneratedClass->FindPropertyByName(VarName))
			|| (Blueprint->GeneratedClass && Blueprint->GeneratedClass->FindPropertyByName(VarName))
			|| (Blueprint->ParentClass && Blueprint->ParentClass->FindPropertyByName(VarName));
	}

	const FBlueprintHelperAgentImportVariableDeclaration* FindDeclaration(
		const FBlueprintHelperAgentImportParsedRequest& Request,
		const FString& Name)
	{
		for (const FBlueprintHelperAgentImportVariableDeclaration& Declaration : Request.Variables)
		{
			if (Declaration.Name.Equals(Name, ESearchCase::IgnoreCase))
			{
				return &Declaration;
			}
		}
		return nullptr;
	}

	bool CanCreateVariableForNode(
		const FBlueprintHelperAgentImportParsedRequest& Request,
		const FBlueprintHelperAgentImportNode& Node)
	{
		return Request.Options.bCreateMissingVariables
			&& (!Node.VariableType.IsEmpty() || FindDeclaration(Request, Node.VariableName) != nullptr);
	}

	bool PreflightRuntimeReferences(
		UBlueprint* Blueprint,
		const FBlueprintHelperAgentImportParsedRequest& Request,
		FBlueprintHelperAgentImportResult& Result)
	{
		for (int32 Index = 0; Index < Request.Variables.Num(); ++Index)
		{
			const FBlueprintHelperAgentImportVariableDeclaration& Declaration = Request.Variables[Index];
			FEdGraphPinType PinType;
			FString Error;
			if (!TextToBlueprintGenerator::ConvertToEdGraphPinType(PinTypeFromDeclaration(Declaration), PinType, Error))
			{
				AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
					TEXT("InvalidVariableType"),
					FString::Printf(TEXT("$.declarations.variables[%d].type"), Index),
					Error);
			}
		}

		for (int32 Index = 0; Index < Request.Nodes.Num(); ++Index)
		{
			const FBlueprintHelperAgentImportNode& Node = Request.Nodes[Index];
			const FString Path = FString::Printf(TEXT("$.nodes[%d]"), Index);
			if (Node.Kind == TEXT("call"))
			{
				if (Node.Function.StartsWith(TEXT("$")))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnknownSymbol"), Path + TEXT(".function"),
						FString::Printf(TEXT("第一阶段不支。symbols。s。"), *Node.Function));
					continue;
				}

				const FString FunctionName = FunctionNameForGenerator(Node.Function);
				if (FunctionName.IsEmpty() || !TextToBlueprintGenerator::FindFunctionByName(FunctionName))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnknownFunction"), Path + TEXT(".function"),
						FString::Printf(TEXT("无法解析函数。s。"), *Node.Function),
						TEXT("使用原生函数名，例如 PrintString，或 /Script/Engine.KismetSystemLibrary:PrintString。"));
				}
			}
			else if (Node.Kind == TEXT("get") || Node.Kind == TEXT("set"))
			{
				if (Node.VariableName.IsEmpty())
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnknownVariable"), Path + TEXT(".var"),
						TEXT("变量节点缺少 var 字段。"));
					continue;
				}

				if (!BlueprintHasMemberVariable(Blueprint, Node.VariableName) && !CanCreateVariableForNode(Request, Node))
				{
					AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
						TEXT("UnknownVariable"), Path + TEXT(".var"),
						FString::Printf(TEXT("变量不存在且缺少可创建的类型声明。s。"), *Node.VariableName),
						TEXT("。declarations.variables 中声明变量，或在节点上提。type 字段。"));
				}
			}
		}

		return !Result.HasErrors();
	}

	int32 CreateDeclaredVariables(
		UBlueprint* Blueprint,
		const FBlueprintHelperAgentImportParsedRequest& Request,
		FBlueprintHelperAgentImportResult& Result)
	{
		if (!Blueprint)
		{
			return 0;
		}

		int32 CreatedCount = 0;
		auto CreateVariable = [&](const FString& Name, const FString& Type, const FString& DefaultValue, const FString& Category, const FString& Path)
		{
			if (BlueprintHasMemberVariable(Blueprint, Name))
			{
				return;
			}

			FEdGraphPinType PinType;
			FString Error;
			if (!TextToBlueprintGenerator::ConvertToEdGraphPinType(PinTypeFromString(Type), PinType, Error))
			{
				AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
					TEXT("InvalidVariableType"), Path, Error);
				return;
			}

			if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*Name), PinType))
			{
				AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
					TEXT("CreateVariableFailed"), Path,
					FString::Printf(TEXT("创建成员变量失败。s。"), *Name));
				return;
			}

			for (FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (Var.VarName == FName(*Name))
				{
					Var.DefaultValue = DefaultValue;
					break;
				}
			}

			if (!Category.IsEmpty())
			{
				FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, FName(*Name), nullptr, FText::FromString(Category));
			}

			++CreatedCount;
		};

		for (int32 Index = 0; Index < Request.Variables.Num(); ++Index)
		{
			const FBlueprintHelperAgentImportVariableDeclaration& Declaration = Request.Variables[Index];
			CreateVariable(
				Declaration.Name,
				Declaration.Type,
				Declaration.DefaultValue,
				Declaration.Category,
				FString::Printf(TEXT("$.declarations.variables[%d]"), Index));
		}

		if (Request.Options.bCreateMissingVariables)
		{
			for (int32 Index = 0; Index < Request.Nodes.Num(); ++Index)
			{
				const FBlueprintHelperAgentImportNode& Node = Request.Nodes[Index];
				if ((Node.Kind != TEXT("get") && Node.Kind != TEXT("set")) || Node.VariableName.IsEmpty() || BlueprintHasMemberVariable(Blueprint, Node.VariableName))
				{
					continue;
				}

				if (const FBlueprintHelperAgentImportVariableDeclaration* Declaration = FindDeclaration(Request, Node.VariableName))
				{
					CreateVariable(Declaration->Name, Declaration->Type, Declaration->DefaultValue, Declaration->Category, FString::Printf(TEXT("$.nodes[%d].var"), Index));
				}
				else if (!Node.VariableType.IsEmpty())
				{
					CreateVariable(Node.VariableName, Node.VariableType, Node.Value, TEXT("Agent"), FString::Printf(TEXT("$.nodes[%d].var"), Index));
				}
			}
		}

		if (CreatedCount > 0)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		return CreatedCount;
	}

	EParsedBlueprintNodeType NodeTypeForKind(const FString& Kind)
	{
		if (Kind == TEXT("event")) return EParsedBlueprintNodeType::Event;
		if (Kind == TEXT("custom_event")) return EParsedBlueprintNodeType::CustomEvent;
		if (Kind == TEXT("call")) return EParsedBlueprintNodeType::CallFunction;
		if (Kind == TEXT("get")) return EParsedBlueprintNodeType::VariableGet;
		if (Kind == TEXT("set")) return EParsedBlueprintNodeType::VariableSet;
		if (Kind == TEXT("branch")) return EParsedBlueprintNodeType::Branch;
		if (Kind == TEXT("sequence")) return EParsedBlueprintNodeType::Sequence;
		return EParsedBlueprintNodeType::Unknown;
	}

	FParsedNode ToParsedNode(
		const FBlueprintHelperAgentImportParsedRequest& Request,
		const FBlueprintHelperAgentImportNode& Node)
	{
		FParsedNode ParsedNode;
		ParsedNode.Id = Node.Id;
		ParsedNode.NodeType = NodeTypeForKind(Node.Kind);
		ParsedNode.X = Node.Position.X;
		ParsedNode.Y = Node.Position.Y;
		ParsedNode.SourceType = Node.Kind;

		if (Node.Kind == TEXT("call"))
		{
			ParsedNode.SourceType = TEXT("K2Node_CallFunction");
			ParsedNode.FunctionName = FunctionNameForGenerator(Node.Function);
			ParsedNode.DefaultValues = Node.Inputs;
		}
		else if (Node.Kind == TEXT("event"))
		{
			ParsedNode.SourceType = TEXT("K2Node_Event");
			ParsedNode.EventReference.EventName = Node.EventName;
		}
		else if (Node.Kind == TEXT("custom_event"))
		{
			ParsedNode.SourceType = TEXT("K2Node_CustomEvent");
			ParsedNode.EventReference.EventName = Node.CustomEventName;
		}
		else if (Node.Kind == TEXT("get") || Node.Kind == TEXT("set"))
		{
			ParsedNode.SourceType = Node.Kind == TEXT("get") ? TEXT("K2Node_VariableGet") : TEXT("K2Node_VariableSet");
			ParsedNode.VariableReference.VariableName = Node.VariableName;
			ParsedNode.VariableReference.ScopeType = TEXT("member");
			ParsedNode.VariableReference.bSelfContext = true;
			ParsedNode.VariableReference.bEnsureExists = false;
			if (!Node.VariableType.IsEmpty())
			{
				ParsedNode.VariableReference.PinType = PinTypeFromString(Node.VariableType);
			}
			else if (const FBlueprintHelperAgentImportVariableDeclaration* Declaration = FindDeclaration(Request, Node.VariableName))
			{
				ParsedNode.VariableReference.PinType = PinTypeFromDeclaration(*Declaration);
			}
			if (Node.Kind == TEXT("set") && !Node.Value.IsEmpty())
			{
				ParsedNode.DefaultValues.Add(TEXT("value"), Node.Value);
			}
		}
		else if (Node.Kind == TEXT("branch"))
		{
			ParsedNode.SourceType = TEXT("K2Node_IfThenElse");
			ParsedNode.DefaultValues = Node.Inputs;
		}
		else if (Node.Kind == TEXT("sequence"))
		{
			ParsedNode.SourceType = TEXT("K2Node_ExecutionSequence");
			ParsedNode.DefaultValues = Node.Inputs;
		}

		return ParsedNode;
	}

	FString AvailablePinsSummary(UK2Node* Node)
	{
		if (!Node)
		{
			return TEXT("");
		}

		TArray<FString> PinNames;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				PinNames.Add(Pin->PinName.ToString());
			}
		}
		return FString::Join(PinNames, TEXT(", "));
	}
}

bool FBlueprintHelperAgentImportResult::HasErrors() const
{
	for (const FBlueprintHelperAgentImportDiagnostic& Diagnostic : Diagnostics)
	{
		if (Diagnostic.Severity == EBlueprintHelperAgentImportDiagnosticSeverity::Error)
		{
			return true;
		}
	}
	return false;
}

FString FBlueprintHelperAgentImportResult::GetSummaryText() const
{
	if (bSuccess)
	{
		return FString::Printf(TEXT("AgentImportGraph 导入成功：状态 %s，节点 %d，连线 %d，变量 %d。"),
			*Status, CreatedNodeCount, CreatedLinkCount, CreatedVariableCount);
	}

	return Message.IsEmpty()
		? FString::Printf(TEXT("AgentImportGraph 导入失败：状态 %s，错误 %d。"), *Status, ErrorCount)
		: Message;
}

FBlueprintHelperAgentImportService::FBlueprintHelperAgentImportService(
	const FBlueprintHelperGraphResolver& InResolver,
	const FBlueprintHelperCompileService& InCompileService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService)
	: Resolver(InResolver)
	, CompileService(InCompileService)
	, AssetBrowseService(InAssetBrowseService)
{
}

FBlueprintHelperAgentImportResult FBlueprintHelperAgentImportService::Import(const FBlueprintHelperAgentImportRequest& Request) const
{
	FBlueprintHelperAgentImportResult Result;

	FBlueprintHelperAgentImportParsedRequest ParsedRequest;
	if (!ParseRoot(Request.JsonText, ParsedRequest, Result))
	{
		return Result;
	}

	FBlueprintHelperDiagnosticSet TargetDiagnostics;
	UEdGraph* TargetGraph = Resolver.ResolveGraph(ParsedRequest.Target, TargetDiagnostics);
	for (const FBlueprintHelperDiagnosticItem& Item : TargetDiagnostics.Items)
	{
		AddDiagnostic(Result,
			Item.Severity == EBlueprintHelperDiagnosticSeverity::Error
				? EBlueprintHelperAgentImportDiagnosticSeverity::Error
				: EBlueprintHelperAgentImportDiagnosticSeverity::Warning,
			!Item.Code.IsEmpty()
				? Item.Code
				: (Item.Severity == EBlueprintHelperDiagnosticSeverity::Error ? TEXT("invalid_target_graph") : TEXT("target_warning")),
			Item.Field.IsEmpty() ? TEXT("$.target_graph") : FString::Printf(TEXT("$.%s"), *Item.Field),
			Item.Message);
	}
	if (!TargetGraph || Result.HasErrors())
	{
		return Result;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	if (!Blueprint)
	{
		AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
			TEXT("InvalidTargetBlueprint"), TEXT("$.target_blueprint"),
			TEXT("无法从目标图表解析蓝图对象。"));
		return Result;
	}

	ApplyAutoLayout(TargetGraph, ParsedRequest);

	if (!PreflightRuntimeReferences(Blueprint, ParsedRequest, Result))
	{
		return Result;
	}

	Result.bDryRun = ParsedRequest.Options.bDryRun;
	if (ParsedRequest.Options.bDryRun)
	{
		Result.CreatedNodeCount = ParsedRequest.Nodes.Num();
		Result.CreatedLinkCount = ParsedRequest.Links.Num();
		Result.CreatedVariableCount = ParsedRequest.Variables.Num();
		FinalizeAgentImportStatus(Result);
		Result.Message = TEXT("AgentImportGraph dry_run 校验通过。");
		return Result;
	}

	const TSet<UEdGraphNode*> NodeSnapshot = CaptureGraphNodeSnapshot(TargetGraph);
	const TSet<FName> VariableSnapshot = CaptureBlueprintVariableSnapshot(Blueprint);

	FBlueprintHelperScopedAssetMutation Mutation(
		FText::FromString(TEXT("BlueprintHelper Import Agent Graph")),
		Blueprint);
	Mutation.Modify(TargetGraph);

	auto FinishMutationErrors = [&]() -> FBlueprintHelperAgentImportResult
	{
		if (ParsedRequest.Options.bStrict)
		{
			RollbackAgentImportMutation(Mutation, TargetGraph, Blueprint, NodeSnapshot, VariableSnapshot, Result);
			return Result;
		}

		if (Result.CreatedNodeCount > 0 || Result.CreatedLinkCount > 0 || Result.CreatedVariableCount > 0)
		{
			Result.bSuccess = true;
			Result.Status = TEXT("partial_success");
			TargetGraph->NotifyGraphChanged();
			Mutation.Commit();
			Result.Message = Result.GetSummaryText();
			return Result;
		}

		Mutation.Rollback();
		Result.bSuccess = false;
		Result.Status = TEXT("failed");
		return Result;
	};

	Result.CreatedVariableCount = CreateDeclaredVariables(Blueprint, ParsedRequest, Result);
	if (Result.HasErrors())
	{
		return FinishMutationErrors();
	}

	TMap<FString, UEdGraphNode*> IdToNode;
	TMap<FString, UK2Node*> IdToK2Node;
	TMap<FString, FParsedNode> IdToParsedNode;
	TSet<FString> ReusedNodeIds;

	for (int32 Index = 0; Index < ParsedRequest.Nodes.Num(); ++Index)
	{
		const FBlueprintHelperAgentImportNode& AgentNode = ParsedRequest.Nodes[Index];
		if (ParsedRequest.Options.bReconstructExistingNodes && AgentNode.Kind == TEXT("custom_event"))
		{
			UK2Node_CustomEvent* ExistingEvent = FindExistingCustomEventNode(TargetGraph, AgentNode.CustomEventName);
			if (!ExistingEvent)
			{
				AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
					TEXT("custom_event_entry_not_found"), FString::Printf(TEXT("$.nodes[%d].name"), Index),
					FString::Printf(TEXT("Custom Event '%s' must already exist when reconstruct_existing_nodes is enabled."), *AgentNode.CustomEventName));
				continue;
			}

			IdToNode.Add(AgentNode.Id, ExistingEvent);
			IdToK2Node.Add(AgentNode.Id, ExistingEvent);
			ReusedNodeIds.Add(AgentNode.Id);
			continue;
		}

		if (AgentNode.Kind == TEXT("comment"))
		{
			UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(TargetGraph);
			TargetGraph->AddNode(CommentNode, true, false);
			CommentNode->CreateNewGuid();
			CommentNode->NodePosX = static_cast<int32>(AgentNode.Position.X);
			CommentNode->NodePosY = static_cast<int32>(AgentNode.Position.Y);
			CommentNode->NodeWidth = static_cast<int32>(AgentNode.Size.X);
			CommentNode->NodeHeight = static_cast<int32>(AgentNode.Size.Y);
			CommentNode->NodeComment = AgentNode.CommentText;
			IdToNode.Add(AgentNode.Id, CommentNode);
			++Result.CreatedNodeCount;
			continue;
		}

		const FParsedNode ParsedNode = ToParsedNode(ParsedRequest, AgentNode);
		IBlueprintNodeHandler* Handler = FBlueprintNodeHandlerRegistry::Get().FindHandler(ParsedNode.NodeType);
		if (!Handler)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("UnknownNodeKind"), FString::Printf(TEXT("$.nodes[%d].kind"), Index),
				FString::Printf(TEXT("没有可处理节点 kind 的 handler：%s。"), *AgentNode.Kind));
			continue;
		}

		FString SpawnError;
		UK2Node* SpawnedNode = Handler->Spawn(TargetGraph, ParsedNode, SpawnError);
		if (!SpawnedNode)
		{
			const FString Code = AgentNode.Kind == TEXT("call") ? TEXT("UnknownFunction") : TEXT("CreateNodeFailed");
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				Code, FString::Printf(TEXT("$.nodes[%d]"), Index),
				SpawnError.IsEmpty() ? TEXT("节点创建失败。") : SpawnError);
			continue;
		}

		IdToNode.Add(AgentNode.Id, SpawnedNode);
		IdToK2Node.Add(AgentNode.Id, SpawnedNode);
		IdToParsedNode.Add(AgentNode.Id, ParsedNode);
		++Result.CreatedNodeCount;
	}

	if (Result.HasErrors())
	{
		return FinishMutationErrors();
	}

	for (const auto& Pair : IdToK2Node)
	{
		if (Pair.Value)
		{
			if (ReusedNodeIds.Contains(Pair.Key))
			{
				continue;
			}
			if (TargetGraph->GetSchema())
			{
				TargetGraph->GetSchema()->ReconstructNode(*Pair.Value);
			}
			if (const FParsedNode* ParsedNode = IdToParsedNode.Find(Pair.Key))
			{
				AddGeneratorDiagnosticsToResult(
					TextToBlueprintGenerator::ApplyDefaultValues(Pair.Value, ParsedNode->DefaultValues, Pair.Key),
					Result);
			}
		}
	}

	if (Result.HasErrors())
	{
		return FinishMutationErrors();
	}

	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	for (const FBlueprintHelperAgentImportLink& Link : ParsedRequest.Links)
	{
		UK2Node* const* FromNodePtr = IdToK2Node.Find(Link.FromNode);
		UK2Node* const* ToNodePtr = IdToK2Node.Find(Link.ToNode);
		if (!FromNodePtr || !ToNodePtr || !*FromNodePtr || !*ToNodePtr)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("link_node_not_found"), Link.Path,
				FString::Printf(TEXT("连线端点必须引用可连线的 K2 节点。s -> %s。"), *Link.FromNode, *Link.ToNode));
			continue;
		}

		UEdGraphPin* FromPin = TextToBlueprintGenerator::FindPinByAlias(*FromNodePtr, Link.FromPin);
		UEdGraphPin* ToPin = TextToBlueprintGenerator::FindPinByAlias(*ToNodePtr, Link.ToPin);
		if (!FromPin)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("link_pin_not_found"), Link.Path + TEXT(".from"),
				FString::Printf(TEXT("Link source pin '%s.%s' was not found."), *Link.FromNode, *Link.FromPin),
				FString::Printf(TEXT("Available pins: %s"), *AvailablePinsSummary(*FromNodePtr)));
			continue;
		}
		if (!ToPin)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("link_pin_not_found"), Link.Path + TEXT(".to"),
				FString::Printf(TEXT("Link target pin '%s.%s' was not found."), *Link.ToNode, *Link.ToPin),
				FString::Printf(TEXT("Available pins: %s"), *AvailablePinsSummary(*ToNodePtr)));
			continue;
		}

		if (!Schema || !Schema->TryCreateConnection(FromPin, ToPin))
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("link_connection_rejected"), Link.Path,
				FString::Printf(TEXT("无法连接 '%s.%s' 到 '%s.%s'。"), *Link.FromNode, *Link.FromPin, *Link.ToNode, *Link.ToPin));
			continue;
		}

		++Result.CreatedLinkCount;
	}

	if (Result.HasErrors())
	{
		return FinishMutationErrors();
	}

	TargetGraph->NotifyGraphChanged();
	Blueprint->MarkPackageDirty();

	if (ParsedRequest.Options.bCompile)
	{
		const FBlueprintHelperCompileResult CompileResult = CompileService.Compile(ParsedRequest.Target);
		Result.bCompiled = CompileResult.bSuccess;
		if (!CompileResult.bSuccess)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("CompileFailed"), TEXT("$.options.compile"),
				TEXT("导入后蓝图编译失败。"));
			return FinishMutationErrors();
		}
	}

	FinalizeAgentImportStatus(Result);
	if (!Result.bSuccess)
	{
		return FinishMutationErrors();
	}

	Mutation.Commit();

	if (ParsedRequest.Options.bSave)
	{
		const FBlueprintHelperSaveResult SaveResult = AssetBrowseService.SaveAsset(ParsedRequest.Target.BlueprintPath);
		Result.bSaved = SaveResult.bSuccess;
		if (!SaveResult.bSuccess)
		{
			AddDiagnostic(Result, EBlueprintHelperAgentImportDiagnosticSeverity::Error,
				TEXT("SaveFailed"), TEXT("$.options.save"),
				SaveResult.ErrorMessage.IsEmpty() ? TEXT("保存资产失败。") : SaveResult.ErrorMessage);
			Result.bSuccess = false;
			Result.Status = TEXT("failed");
			return Result;
		}
	}

	Result.Message = Result.GetSummaryText();
	return Result;
}
