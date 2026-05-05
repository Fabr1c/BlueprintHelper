// BlueprintHelper Service Layer - TaskPlan runtime executor

#include "TaskRuntime/BlueprintHelperTaskRuntimeService.h"
#include "Services/BlueprintHelperAppendBlueprintGraphService.h"
#include "Services/BlueprintHelperMergeBlueprintGraphService.h"
#include "Services/BlueprintHelperPatchBlueprintGraphService.h"
#include "Services/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Services/BlueprintHelperBlueprintVariableService.h"
#include "Services/BlueprintHelperAssetFactoryService.h"
#include "Structure/BlueprintHelperAssetFactoryTypes.h"
#include "Services/BlueprintHelperClassSettingsService.h"
#include "Services/BlueprintHelperCompileAssetService.h"
#include "Services/BlueprintHelperComponentService.h"
#include "Services/BlueprintHelperDataTableService.h"
#include "Services/BlueprintHelperWidgetService.h"
#include "Services/BlueprintHelperAssetBrowseService.h"
#include "Structure/BlueprintHelperSaveAssetTypes.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperClassSettingsTaskPlanAdapter.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperComponentTaskPlanAdapter.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperDataTableTaskPlanAdapter.h"
#include "TaskRuntime/TaskPlanAdapters/BlueprintHelperWidgetTaskPlanAdapter.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FBlueprintHelperToolError MakeTaskRuntimeError(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		FBlueprintHelperToolError Error;
		Error.Code = Code;
		Error.Stage = Stage;
		Error.Message = Message;
		Error.bRetryable = false;
		Error.RollbackResult = EBlueprintHelperRollbackResult::NotNeeded;
		Error.Field = Field;
		return Error;
	}

	FBlueprintHelperToolResultBase MakeFailure(
		const FString& Operation,
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = TEXT(""))
	{
		return FBlueprintHelperToolResultBuilder::Failure(
			Operation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeTaskRuntimeError(Code, Stage, Message, Field));
	}

	TArray<TSharedPtr<FJsonValue>> CopyArrayField(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(FieldName, Array) && Array)
		{
			return *Array;
		}
		return {};
	}

	void CopyObjectFields(
		const TSharedPtr<FJsonObject>& Source,
		const TSharedRef<FJsonObject>& Destination)
	{
		if (!Source.IsValid())
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Source->Values)
		{
			Destination->SetField(Field.Key, Field.Value);
		}
	}

	bool IsGraphWriteTaskPlanOperation(const FString& Operation)
	{
		return Operation == TEXT("append_blueprint_graph") ||
			Operation == TEXT("replace_blueprint_graph") ||
			Operation == TEXT("patch_blueprint_graph") ||
			Operation == TEXT("merge_blueprint_graph");
	}

	FString BuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	FString BuildOpFieldPath(int32 OpIndex, const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex)
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, *Suffix);
	}

	FString ToTaskRuntimeIdSegment(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Ch : Value)
		{
			Result.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
		}
		return Result.IsEmpty() ? FString(TEXT("entry")) : Result;
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

	TSharedPtr<FJsonValue> GetLiteralJsonValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid())
		{
			return MakeShared<FJsonValueNull>();
		}

		if (Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> ObjectValue = Value->AsObject();
			FString Kind;
			if (ObjectValue.IsValid() &&
				ObjectValue->TryGetStringField(TEXT("kind"), Kind) &&
				Kind == TEXT("literal"))
			{
				const TSharedPtr<FJsonValue> LiteralValue = ObjectValue->TryGetField(TEXT("value"));
				return LiteralValue.IsValid() ? LiteralValue : MakeShared<FJsonValueNull>();
			}
		}

		return Value;
	}

	void CopyLiteralArgsToInputs(
		const TSharedPtr<FJsonObject>& ArgsObject,
		const TSharedRef<FJsonObject>& InputsObject)
	{
		if (!ArgsObject.IsValid())
		{
			return;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Arg : ArgsObject->Values)
		{
			InputsObject->SetField(Arg.Key, GetLiteralJsonValue(Arg.Value));
		}
	}

	TSharedRef<FJsonObject> MakeExecLink(
		const FString& FromNodeId,
		const FString& ToNodeId)
	{
		TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetStringField(TEXT("kind"), TEXT("exec"));
		Link->SetStringField(TEXT("from"), FString::Printf(TEXT("%s.then"), *FromNodeId));
		Link->SetStringField(TEXT("to"), FString::Printf(TEXT("%s.execute"), *ToNodeId));
		return Link;
	}

	TSharedRef<FJsonObject> MakeSyntheticDryRunData()
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetArrayField(TEXT("warnings"), {});
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	bool TryReadStepTarget(
		const TSharedPtr<FJsonObject>& StepObject,
		TSharedPtr<FJsonObject>& OutTargetObject,
		FString& OutAssetPath,
		FString& OutGraphName,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step target object is required."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutGraphName);
		if (OutAssetPath.IsEmpty() || OutGraphName.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step_target"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step target requires asset_path and graph."),
				BuildStepFieldPath(TEXT("target")));
			return false;
		}

		OutTargetObject = *TargetObjectPtr;
		return true;
	}

	bool TryAppendGraphWriteStatement(
		const TSharedPtr<FJsonObject>& StatementObject,
		int32 OpIndex,
		int32 StatementIndex,
		const FString& EntryId,
		FString& InOutPreviousExecNodeId,
		TArray<TSharedPtr<FJsonValue>>& Nodes,
		TArray<TSharedPtr<FJsonValue>>& Links,
		FBlueprintHelperToolError& OutError)
	{
		if (!StatementObject.IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_statement"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite statement must be an object."),
				BuildOpFieldPath(OpIndex, FString::Printf(TEXT("body.statements[%d]"), StatementIndex)));
			return false;
		}

		FString StatementKind;
		if (!StatementObject->TryGetStringField(TEXT("kind"), StatementKind))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_statement"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite statement requires kind."),
				BuildOpFieldPath(OpIndex, FString::Printf(TEXT("body.statements[%d].kind"), StatementIndex)));
			return false;
		}

		const FString NodeId = FString::Printf(
			TEXT("%s_stmt_%d"),
			*EntryId,
			StatementIndex + 1);
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("id"), NodeId);

		if (StatementKind == TEXT("call_function"))
		{
			FString FunctionName;
			if (!StatementObject->TryGetStringField(TEXT("name"), FunctionName) || FunctionName.IsEmpty())
			{
				OutError = MakeTaskRuntimeError(
					TEXT("invalid_graph_write_statement"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("call_function statement requires name."),
					BuildOpFieldPath(OpIndex, FString::Printf(TEXT("body.statements[%d].name"), StatementIndex)));
				return false;
			}

			Node->SetStringField(TEXT("kind"), TEXT("call"));
			Node->SetStringField(TEXT("function"), FunctionName);

			const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
			TSharedRef<FJsonObject> InputsObject = MakeShared<FJsonObject>();
			if (StatementObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) &&
				ArgsObjectPtr && ArgsObjectPtr->IsValid())
			{
				CopyLiteralArgsToInputs(*ArgsObjectPtr, InputsObject);
			}
			Node->SetObjectField(TEXT("inputs"), InputsObject);
		}
		else if (StatementKind == TEXT("set_member_variable"))
		{
			FString VariableName;
			if (!StatementObject->TryGetStringField(TEXT("name"), VariableName) || VariableName.IsEmpty())
			{
				OutError = MakeTaskRuntimeError(
					TEXT("invalid_graph_write_statement"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("set_member_variable statement requires name."),
					BuildOpFieldPath(OpIndex, FString::Printf(TEXT("body.statements[%d].name"), StatementIndex)));
				return false;
			}

			Node->SetStringField(TEXT("kind"), TEXT("set"));
			Node->SetStringField(TEXT("var"), VariableName);
			Node->SetStringField(TEXT("value"), JsonValueToString(GetLiteralJsonValue(StatementObject->TryGetField(TEXT("value")))));
		}
		else
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_statement_kind"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite owned_graph_edit currently supports call_function and set_member_variable statements."),
				BuildOpFieldPath(OpIndex, FString::Printf(TEXT("body.statements[%d].kind"), StatementIndex)));
			return false;
		}

		Nodes.Add(MakeShared<FJsonValueObject>(Node));
		Links.Add(MakeShared<FJsonValueObject>(MakeExecLink(InOutPreviousExecNodeId, NodeId)));
		InOutPreviousExecNodeId = NodeId;
		return true;
	}

	bool TryAppendGraphWriteEnsureEntry(
		const TSharedPtr<FJsonObject>& OpObject,
		int32 OpIndex,
		TArray<TSharedPtr<FJsonValue>>& Nodes,
		TArray<TSharedPtr<FJsonValue>>& Links,
		FBlueprintHelperToolError& OutError)
	{
		if (!OpObject.IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite write.ops entry must be an object."),
				BuildOpFieldPath(OpIndex, TEXT("")));
			return false;
		}

		FString OpName;
		OpObject->TryGetStringField(TEXT("op"), OpName);
		if (OpName != TEXT("ensure_entry"))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite owned_graph_edit currently supports ensure_entry operations only."),
				BuildOpFieldPath(OpIndex, TEXT("op")));
			return false;
		}

		FString EntryType;
		OpObject->TryGetStringField(TEXT("entry_type"), EntryType);
		if (EntryType != TEXT("custom_event"))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_ir_entry_type"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite ensure_entry currently supports custom_event entries only."),
				BuildOpFieldPath(OpIndex, TEXT("entry_type")));
			return false;
		}

		FString EntryName;
		if (!OpObject->TryGetStringField(TEXT("name"), EntryName) || EntryName.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite ensure_entry requires name."),
				BuildOpFieldPath(OpIndex, TEXT("name")));
			return false;
		}

		const FString EntryId = FString::Printf(TEXT("%s_entry"), *ToTaskRuntimeIdSegment(EntryName));
		TSharedRef<FJsonObject> EntryNode = MakeShared<FJsonObject>();
		EntryNode->SetStringField(TEXT("id"), EntryId);
		EntryNode->SetStringField(TEXT("kind"), TEXT("custom_event"));
		EntryNode->SetStringField(TEXT("name"), EntryName);
		Nodes.Add(MakeShared<FJsonValueObject>(EntryNode));

		const TSharedPtr<FJsonObject>* BodyObjectPtr = nullptr;
		if (!OpObject->TryGetObjectField(TEXT("body"), BodyObjectPtr) ||
			!BodyObjectPtr || !BodyObjectPtr->IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* StatementsArray = nullptr;
		if (!(*BodyObjectPtr)->TryGetArrayField(TEXT("statements"), StatementsArray) || !StatementsArray)
		{
			return true;
		}

		FString PreviousExecNodeId = EntryId;
		for (int32 StatementIndex = 0; StatementIndex < StatementsArray->Num(); ++StatementIndex)
		{
			const TSharedPtr<FJsonObject> StatementObject =
				(*StatementsArray)[StatementIndex].IsValid()
					? (*StatementsArray)[StatementIndex]->AsObject()
					: nullptr;
			if (!TryAppendGraphWriteStatement(
				StatementObject,
				OpIndex,
				StatementIndex,
				EntryId,
				PreviousExecNodeId,
				Nodes,
				Links,
				OutError))
			{
				return false;
			}
		}

		return true;
	}

	bool TryBuildGraphWriteIrAppendPayload(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedPtr<FJsonObject> TargetObject;
		FString AssetPath;
		FString GraphName;
		if (!TryReadStepTarget(StepObject, TargetObject, AssetPath, GraphName, OutError))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step_write"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("graph_write TaskPlan step requires write object."),
				BuildStepFieldPath(TEXT("write")));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != TEXT("owned_graph_edit"))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_strategy"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite Task Runtime currently supports owned_graph_edit only."),
				BuildStepFieldPath(TEXT("write.strategy")));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ops"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("graph_write TaskPlan step requires write.ops array."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Links;
		for (int32 OpIndex = 0; OpIndex < OpsArray->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject =
				(*OpsArray)[OpIndex].IsValid()
					? (*OpsArray)[OpIndex]->AsObject()
					: nullptr;
			if (!TryAppendGraphWriteEnsureEntry(OpObject, OpIndex, Nodes, Links, OutError))
			{
				return false;
			}
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), GraphName);
		Payload->SetObjectField(TEXT("target"), BridgeTarget);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		FString FeatureName;
		if (TaskPlan.IsValid() && TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName) && !FeatureName.IsEmpty())
		{
			Payload->SetStringField(TEXT("feature_name"), FeatureName);
		}

		Payload->SetArrayField(TEXT("nodes"), Nodes);
		Payload->SetArrayField(TEXT("links"), Links);
		OutPayload = Payload;
		return true;
	}

	bool TryBuildBlueprintVariablePayload(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step_target"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable TaskPlan step target object is required."),
				TEXT("task_plan.steps[0].target"));
			return false;
		}

		FString AssetPath;
		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath);
		if (AssetPath.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step_target"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable TaskPlan step target requires asset_path."),
				TEXT("task_plan.steps[0].target.asset_path"));
			return false;
		}

		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_variable_write"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("blueprint_variable TaskPlan step requires write object."),
				TEXT("task_plan.steps[0].write"));
			return false;
		}

		FString Strategy;
		if (!(*WriteObjectPtr)->TryGetStringField(TEXT("strategy"), Strategy) ||
			Strategy != TEXT("member_variables"))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_variable_strategy"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable Task Runtime currently supports member_variables only."),
				TEXT("task_plan.steps[0].write.strategy"));
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray || OpsArray->Num() == 0)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_variable_ops"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("blueprint_variable TaskPlan step requires write.ops array."),
				TEXT("task_plan.steps[0].write.ops"));
			return false;
		}

		TArray<TSharedPtr<FJsonValue>> Variables;
		for (int32 OpIndex = 0; OpIndex < OpsArray->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject =
				(*OpsArray)[OpIndex].IsValid()
					? (*OpsArray)[OpIndex]->AsObject()
					: nullptr;
			if (!OpObject.IsValid())
			{
				OutError = MakeTaskRuntimeError(
					TEXT("invalid_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("Blueprint variable op must be an object."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex));
				return false;
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			if (OpName != TEXT("ensure_member_variable"))
			{
				OutError = MakeTaskRuntimeError(
					TEXT("unsupported_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("Blueprint variable Task Runtime currently supports ensure_member_variable only."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].op"), OpIndex));
				return false;
			}

			FString VariableName;
			if (!OpObject->TryGetStringField(TEXT("name"), VariableName) || VariableName.IsEmpty())
			{
				OutError = MakeTaskRuntimeError(
					TEXT("invalid_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("ensure_member_variable requires name."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].name"), OpIndex));
				return false;
			}

			const TSharedPtr<FJsonObject>* PinTypeObjectPtr = nullptr;
			if (!OpObject->TryGetObjectField(TEXT("pin_type"), PinTypeObjectPtr) ||
				!PinTypeObjectPtr || !PinTypeObjectPtr->IsValid())
			{
				OutError = MakeTaskRuntimeError(
					TEXT("invalid_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("ensure_member_variable requires pin_type."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].pin_type"), OpIndex));
				return false;
			}

			TSharedRef<FJsonObject> Variable = MakeShared<FJsonObject>();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : OpObject->Values)
			{
				if (Field.Key != TEXT("op"))
				{
					Variable->SetField(Field.Key, Field.Value);
				}
			}
			Variables.Add(MakeShared<FJsonValueObject>(Variable));
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("asset_path"), AssetPath);
		Payload->SetArrayField(TEXT("variables"), Variables);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		OutPayload = Payload;
		return true;
	}

	struct FTaskPlanStepPayloadParts
	{
		TSharedPtr<FJsonObject> TargetObject;
		TSharedPtr<FJsonObject> ArgsObject;
		FString AssetPath;
		FString GraphName;
	};

	bool TryReadStepPayloadParts(
		const TSharedPtr<FJsonObject>& StepObject,
		FTaskPlanStepPayloadParts& OutParts,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) ||
			!TargetObjectPtr || !TargetObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 target 对象。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return false;
		}
		if (!StepObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) ||
			!ArgsObjectPtr || !ArgsObjectPtr->IsValid())
		{
			OutErrorCode = TEXT("invalid_taskplan_step");
			OutErrorMessage = TEXT("TaskPlan step 缺少 args 对象。");
			OutErrorField = TEXT("task_plan.steps[0].args");
			return false;
		}

		(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), OutParts.AssetPath);
		(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), OutParts.GraphName);
		if (OutParts.AssetPath.IsEmpty() || OutParts.GraphName.IsEmpty())
		{
			OutErrorCode = TEXT("invalid_taskplan_step_target");
			OutErrorMessage = TEXT("TaskPlan step target 需要 asset_path 和 graph。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return false;
		}

		OutParts.TargetObject = *TargetObjectPtr;
		OutParts.ArgsObject = *ArgsObjectPtr;
		return true;
	}

	TSharedRef<FJsonObject> BuildBridgeTargetPayload(const FTaskPlanStepPayloadParts& Parts)
	{
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(Parts.TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), Parts.AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), Parts.GraphName);
		return BridgeTarget;
	}

	bool HasExecutionPolicyValidationFields(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if (!TaskPlan.IsValid() ||
			!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
			!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
		{
			return false;
		}

		bool bIgnored = false;
		return (*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bIgnored) ||
			(*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bIgnored);
	}

	bool TryReadExecutionPolicyBool(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TCHAR* FieldName,
		bool& OutValue)
	{
		OutValue = false;
		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		return TaskPlan.IsValid() &&
			TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) &&
			ExecutionPolicyPtr && ExecutionPolicyPtr->IsValid() &&
			(*ExecutionPolicyPtr)->TryGetBoolField(FieldName, OutValue);
	}

	TArray<FString> ReadTargetAssets(const TSharedPtr<FJsonObject>& TaskPlan)
	{
		TArray<FString> Assets;
		const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
		if (!TaskPlan.IsValid() ||
			!TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) ||
			!TargetAssets)
		{
			return Assets;
		}

		for (const TSharedPtr<FJsonValue>& AssetValue : *TargetAssets)
		{
			if (!AssetValue.IsValid() || AssetValue->Type != EJson::String)
			{
				continue;
			}
			const FString AssetPath = AssetValue->AsString();
			if (!AssetPath.IsEmpty())
			{
				Assets.AddUnique(AssetPath);
			}
		}
		return Assets;
	}

	FBlueprintHelperToolResultBase MakeSaveAssetToolResult(
		const FBlueprintHelperAssetBrowseService& Service,
		const FString& AssetPath)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);

		const FBlueprintHelperSaveResult SaveResult = Service.SaveAsset(AssetPath);
		if (!SaveResult.bSuccess)
		{
			FBlueprintHelperToolError Error;
			Error.Code = TEXT("save_failed");
			Error.Stage = EBlueprintHelperToolStage::Execute;
			Error.Message = SaveResult.ErrorMessage;
			Error.bRetryable = true;

			FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("save_asset"),
				TraceId,
				Error);
			Result.CustomTargetJson = Target;
			return Result;
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
			TEXT("save_asset"),
			TraceId);
		Result.bModified = false;
		Result.CustomTargetJson = Target;

		FBlueprintHelperSaveAssetResultData Data;
		Data.SaveResult.bSaved = true;
		Data.SaveResult.bWasDirty = true;
		Result.Data = Data.ToJson();
		return Result;
	}

	TSharedRef<FJsonObject> BuildAppendPayload(
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		FTaskPlanStepPayloadParts Parts;
		if (!TryReadStepPayloadParts(StepObject, Parts, OutErrorCode, OutErrorMessage, OutErrorField))
		{
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> AppendPayload = MakeShared<FJsonObject>();
		AppendPayload->SetObjectField(TEXT("target"), BuildBridgeTargetPayload(Parts));
		AppendPayload->SetBoolField(TEXT("dry_run"), bDryRun);

		FString FeatureName;
		if (Parts.ArgsObject->TryGetStringField(TEXT("feature_name"), FeatureName) && !FeatureName.IsEmpty())
		{
			AppendPayload->SetStringField(TEXT("feature_name"), FeatureName);
		}

		AppendPayload->SetArrayField(TEXT("nodes"), CopyArrayField(Parts.ArgsObject, TEXT("nodes")));
		AppendPayload->SetArrayField(TEXT("links"), CopyArrayField(Parts.ArgsObject, TEXT("links")));
		return AppendPayload;
	}

	TSharedRef<FJsonObject> BuildGraphWritePayload(
		const FString& StepOperation,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		FString& OutErrorCode,
		FString& OutErrorMessage,
		FString& OutErrorField)
	{
		if (StepOperation == TEXT("append_blueprint_graph"))
		{
			return BuildAppendPayload(StepObject, bDryRun, OutErrorCode, OutErrorMessage, OutErrorField);
		}

		FTaskPlanStepPayloadParts Parts;
		if (!TryReadStepPayloadParts(StepObject, Parts, OutErrorCode, OutErrorMessage, OutErrorField))
		{
			return MakeShared<FJsonObject>();
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		CopyObjectFields(Parts.ArgsObject, Payload);
		Payload->SetObjectField(TEXT("target"), BuildBridgeTargetPayload(Parts));

		if (StepOperation == TEXT("replace_blueprint_graph"))
		{
			TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
			const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
			if (Payload->TryGetObjectField(TEXT("options"), OptionsObject) &&
				OptionsObject && OptionsObject->IsValid())
			{
				CopyObjectFields(*OptionsObject, Options);
			}
			Options->SetBoolField(TEXT("dry_run"), bDryRun);
			Payload->SetObjectField(TEXT("options"), Options);
		}
		else
		{
			Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		}

		return Payload;
	}

	TSharedRef<FJsonObject> MakeStepResultJson(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		TSharedRef<FJsonObject> StepJson = MakeShared<FJsonObject>();
		StepJson->SetStringField(TEXT("step_id"), LoweredStep.StepId);
		if (!LoweredStep.Capability.IsEmpty())
		{
			StepJson->SetStringField(TEXT("capability"), LoweredStep.Capability);
		}
		StepJson->SetStringField(TEXT("operation"), LoweredStep.RuntimeOperation);
		if (!LoweredStep.AdapterOperation.IsEmpty())
		{
			StepJson->SetStringField(TEXT("adapter_operation"), LoweredStep.AdapterOperation);
		}
		StepJson->SetStringField(TEXT("status"), ToolStatusToString(StepResult.Status));
		StepJson->SetObjectField(TEXT("result"), StepResult.ToJson());
		return StepJson;
	}

	TSharedRef<FJsonObject> MakePostOperationResultJson(
		const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation)
	{
		TSharedRef<FJsonObject> PostJson = MakeShared<FJsonObject>();
		PostJson->SetStringField(TEXT("operation"), PostOperation.Operation);
		PostJson->SetStringField(TEXT("status"), ToolStatusToString(PostOperation.Result.Status));
		PostJson->SetObjectField(TEXT("result"), PostOperation.Result.ToJson());
		return PostJson;
	}

	bool HasFailedStep(
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords)
	{
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			if (!StepRecord.Result.bOk)
			{
				return true;
			}
		}
		return false;
	}

	bool HasFailedPostOperation(
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords)
	{
		for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
		{
			if (!PostOperation.Result.bOk)
			{
				return true;
			}
		}
		return false;
	}

	TSharedRef<FJsonObject> MakeRuntimeData(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& TaskRunId,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
		bool bDryRun)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRuntimeResult.v1"));
		if (!TaskRunId.IsEmpty())
		{
			Data->SetStringField(TEXT("task_run_id"), TaskRunId);
		}
		if (TaskPlan.IsValid())
		{
			FString TaskPlanSchema;
			if (TaskPlan->TryGetStringField(TEXT("schema"), TaskPlanSchema))
			{
				Data->SetStringField(TEXT("task_plan_schema"), TaskPlanSchema);
			}

			const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
			if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
			{
				Data->SetArrayField(TEXT("target_assets"), *TargetAssets);
			}
		}

		TArray<TSharedPtr<FJsonValue>> Steps;
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			Steps.Add(MakeShared<FJsonValueObject>(MakeStepResultJson(StepRecord.Step, StepRecord.Result)));
		}
		Data->SetArrayField(TEXT("steps"), Steps);

		if (PostOperationRecords.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> PostOperations;
			for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
			{
				PostOperations.Add(MakeShared<FJsonValueObject>(MakePostOperationResultJson(PostOperation)));
			}
			Data->SetArrayField(TEXT("post_operations"), PostOperations);
		}

		if (bDryRun)
		{
			for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
			{
				if (StepRecord.Result.Data.IsValid())
				{
					const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
					if (StepRecord.Result.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) &&
						DryRunObject && DryRunObject->IsValid())
					{
						Data->SetObjectField(TEXT("dry_run"), *DryRunObject);
						break;
					}
				}
			}
		}

		return Data;
	}

	TSharedRef<FJsonObject> MakeRuntimeData(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& TaskRunId,
		const FString& StepId,
		const FString& StepOperation,
		const FBlueprintHelperToolResultBase& StepResult,
		bool bDryRun)
	{
		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
		LoweredStep.StepId = StepId;
		LoweredStep.RuntimeOperation = StepOperation;
		LoweredStep.AdapterOperation = StepOperation;
		TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
		StepRecords.Add({LoweredStep, StepResult});
		return MakeRuntimeData(
			TaskPlan,
			TaskRunId,
			StepRecords,
			{},
			bDryRun);
	}

	TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
		bool bRuntimeFailed = false)
	{
		TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
		Journal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
		Journal->SetStringField(TEXT("task_run_id"), TaskRunId);
		Journal->SetStringField(
			TEXT("status"),
			bRuntimeFailed || HasFailedStep(StepRecords) || HasFailedPostOperation(PostOperationRecords)
				? TEXT("failed")
				: TEXT("completed"));

		if (TaskPlan.IsValid())
		{
			FString TaskType;
			if (TaskPlan->TryGetStringField(TEXT("task_type"), TaskType))
			{
				Journal->SetStringField(TEXT("task_type"), TaskType);
			}
			FString FeatureName;
			if (TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName))
			{
				Journal->SetStringField(TEXT("feature_name"), FeatureName);
			}
			const TArray<TSharedPtr<FJsonValue>>* TargetAssets = nullptr;
			if (TaskPlan->TryGetArrayField(TEXT("target_assets"), TargetAssets) && TargetAssets)
			{
				Journal->SetArrayField(TEXT("target_assets"), *TargetAssets);
			}
		}

		TArray<TSharedPtr<FJsonValue>> Steps;
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			Steps.Add(MakeShared<FJsonValueObject>(MakeStepResultJson(StepRecord.Step, StepRecord.Result)));
		}
		Journal->SetArrayField(TEXT("steps"), Steps);

		if (PostOperationRecords.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> PostOperations;
			for (const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation : PostOperationRecords)
			{
				PostOperations.Add(MakeShared<FJsonValueObject>(MakePostOperationResultJson(PostOperation)));
			}
			Journal->SetArrayField(TEXT("post_operations"), PostOperations);
		}
		return Journal;
	}

	TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
		StepRecords.Add({LoweredStep, StepResult});
		return MakeTaskRunJournal(
			TaskRunId,
			TaskPlan,
			StepRecords,
			{});
	}

	TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const FString& StepId,
		const FString& StepOperation,
		const FBlueprintHelperToolResultBase& StepResult)
	{
		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
		LoweredStep.StepId = StepId;
		LoweredStep.RuntimeOperation = StepOperation;
		LoweredStep.AdapterOperation = StepOperation;
		return MakeTaskRunJournal(TaskRunId, TaskPlan, LoweredStep, StepResult);
	}

	TSharedRef<FJsonObject> MakeRuntimeTarget(
		const FString& AssetPath,
		const FString& TargetType,
		const FString& MemberName = TEXT(""),
		const FString& RowName = TEXT(""),
		const FString& PropertyPath = TEXT(""))
	{
		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		if (!AssetPath.IsEmpty())
		{
			Target->SetStringField(TEXT("asset_path"), AssetPath);
		}
		Target->SetStringField(TEXT("target_type"), TargetType);
		if (!MemberName.IsEmpty())
		{
			Target->SetStringField(TEXT("member_name"), MemberName);
		}
		if (!RowName.IsEmpty())
		{
			Target->SetStringField(TEXT("row_name"), RowName);
		}
		if (!PropertyPath.IsEmpty())
		{
			Target->SetStringField(TEXT("property_path"), PropertyPath);
		}
		return Target;
	}

	bool TryParseAssetFactoryType(const FString& AssetTypeText, EBlueprintHelperAssetType& OutAssetType)
	{
		if (AssetTypeText.Equals(TEXT("blueprint_class"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::BlueprintClass;
			return true;
		}
		if (AssetTypeText.Equals(TEXT("blueprint_interface"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::BlueprintInterface;
			return true;
		}
		if (AssetTypeText.Equals(TEXT("structure"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::Structure;
			return true;
		}
		if (AssetTypeText.Equals(TEXT("input_action"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::InputAction;
			return true;
		}
		if (AssetTypeText.Equals(TEXT("input_mapping_context"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::InputMappingContext;
			return true;
		}
		if (AssetTypeText.Equals(TEXT("data_asset"), ESearchCase::IgnoreCase))
		{
			OutAssetType = EBlueprintHelperAssetType::DataAsset;
			return true;
		}
		OutAssetType = EBlueprintHelperAssetType::Unknown;
		return false;
	}

	EBlueprintHelperAssetCollisionPolicy ParseAssetFactoryCollision(const FString& CollisionText)
	{
		return CollisionText.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase)
			? EBlueprintHelperAssetCollisionPolicy::ReuseIfExists
			: EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	}

	void ApplyAssetFactoryResultData(
		FBlueprintHelperToolResultBase& Result,
		const FBlueprintHelperAssetFactoryData& FactoryData,
		const FString& AssetPath,
		EBlueprintHelperAssetType AssetType)
	{
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("asset"));
		Result.Data = FactoryData.ToJson();

		if (Result.bOk && Result.Status == EBlueprintHelperToolStatus::Applied)
		{
			FBlueprintHelperValidationSummary Validation;
			Validation.bShouldCompile = FBlueprintHelperAssetFactoryService::ShouldCompile(AssetType);
			Validation.bShouldSave = FBlueprintHelperAssetFactoryService::ShouldSave(AssetType);
			Result.Validation = Validation;
		}
	}

	FBlueprintHelperToolResultBase ExecuteAssetFactoryTaskPlanStep(
		const FBlueprintHelperAssetFactoryService& Service,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString AssetTypeText;
		FString ParentClass;
		FString ValueType;
		FString CollisionText;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("asset_type"), AssetTypeText);
			Payload->TryGetStringField(TEXT("parent_class"), ParentClass);
			Payload->TryGetStringField(TEXT("value_type"), ValueType);
			Payload->TryGetStringField(TEXT("collision"), CollisionText);
		}

		EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
		if (!TryParseAssetFactoryType(AssetTypeText, AssetType))
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("unsupported_asset_type"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("Unsupported asset_type: %s"), *AssetTypeText),
				TEXT("task_plan.steps[0].write.ops[0].asset_type"));
		}

		const EBlueprintHelperAssetCollisionPolicy Collision = ParseAssetFactoryCollision(CollisionText);
		const FBlueprintHelperAssetFactoryData FactoryData = Service.CreateAsset(
			AssetPath,
			AssetType,
			ParentClass,
			ValueType,
			Collision);

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result;
		if (FactoryData.Asset.bAlreadyExisted)
		{
			if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists &&
				FactoryData.Collision.bHandled)
			{
				Result = FBlueprintHelperToolResultBuilder::NoOp(TEXT("create_asset"), TraceId);
			}
			else
			{
				Result = FBlueprintHelperToolResultBuilder::Failure(
					TEXT("create_asset"),
					TraceId,
					MakeTaskRuntimeError(
						FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::FailIfExists
							? TEXT("asset_already_exists")
							: TEXT("asset_type_mismatch"),
						EBlueprintHelperToolStage::Preflight,
						TEXT("Asset Factory could not create the requested asset.")));
			}
		}
		else if (!FactoryData.Asset.bCreated)
		{
			Result = FBlueprintHelperToolResultBuilder::Failure(
				TEXT("create_asset"),
				TraceId,
				MakeTaskRuntimeError(
					TEXT("creation_failed"),
					EBlueprintHelperToolStage::Execute,
					TEXT("Failed to create asset.")));
		}
		else
		{
			Result = FBlueprintHelperToolResultBuilder::Applied(TEXT("create_asset"), TraceId);
		}

		ApplyAssetFactoryResultData(Result, FactoryData, AssetPath, AssetType);
		return Result;
	}

	TArray<FString> ReadStringArrayField(
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* FieldName)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(FieldName, Values) || !Values)
		{
			return Result;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Item;
			if (Value.IsValid() && Value->TryGetString(Item))
			{
				Result.Add(Item);
			}
		}
		return Result;
	}

	TMap<FString, FString> ReadStringFieldsObject(const TSharedPtr<FJsonObject>& Payload)
	{
		TMap<FString, FString> Fields;
		const TSharedPtr<FJsonObject>* FieldsObject = nullptr;
		if (!Payload.IsValid() ||
			!Payload->TryGetObjectField(TEXT("fields"), FieldsObject) ||
			!FieldsObject || !FieldsObject->IsValid())
		{
			return Fields;
		}

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*FieldsObject)->Values)
		{
			Fields.Add(Field.Key, JsonValueToString(Field.Value));
		}
		return Fields;
	}

	TArray<FBlueprintHelperClassDefaultPropertySetting> ReadClassDefaultSettings(
		const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FBlueprintHelperClassDefaultPropertySetting> Settings;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("settings"), Values) || !Values)
		{
			return Settings;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}

			FBlueprintHelperClassDefaultPropertySetting Setting;
			Object->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
			Setting.Value = Object->TryGetField(TEXT("value"));
			Settings.Add(MoveTemp(Setting));
		}
		return Settings;
	}

	FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
		const TSharedPtr<FJsonObject>& Payload)
	{
		FBlueprintHelperSetComponentPropertiesRequest Request;
		Request.Mode = EBlueprintHelperComponentPropertyMode::Batch;
		if (!Payload.IsValid())
		{
			return Request;
		}

		Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
		Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *SettingsArray)
			{
				const TSharedPtr<FJsonObject> ItemObject = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
				if (!ItemObject.IsValid())
				{
					continue;
				}

				FBlueprintHelperComponentPropertySetting Setting;
				ItemObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
				Setting.Value = ItemObject->TryGetField(TEXT("value"));
				Request.Settings.Add(MoveTemp(Setting));
			}
		}
		return Request;
	}

	TSharedRef<FJsonObject> MakeBlueprintVariableOpPayload(
		const FString& AssetPath,
		const FString& FunctionName,
		const TSharedPtr<FJsonObject>& OpObject)
	{
		TSharedRef<FJsonObject> OpPayload = MakeShared<FJsonObject>();
		if (OpObject.IsValid())
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : OpObject->Values)
			{
				if (Field.Key != TEXT("op"))
				{
					OpPayload->SetField(Field.Key, Field.Value);
				}
			}
		}
		OpPayload->SetStringField(TEXT("asset_path"), AssetPath);
		if (!FunctionName.IsEmpty())
		{
			OpPayload->SetStringField(TEXT("function_name"), FunctionName);
		}
		return OpPayload;
	}

	FBlueprintHelperToolResultBase ExecuteBlueprintVariableBatchTaskPlanStep(
		const FBlueprintHelperBlueprintVariableService& Service,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString FunctionName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("function_name"), FunctionName);
		}

		const TArray<TSharedPtr<FJsonValue>>* Ops = nullptr;
		if (AssetPath.IsEmpty() ||
			!Payload.IsValid() ||
			!Payload->TryGetArrayField(TEXT("ops"), Ops) ||
			!Ops || Ops->Num() == 0)
		{
			return MakeFailure(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TEXT("invalid_variable_batch_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Blueprint variable batch payload requires asset_path and ops."),
				TEXT("task_plan.steps[0]"));
		}

		int32 AppliedCount = 0;
		int32 NoOpCount = 0;
		for (int32 OpIndex = 0; OpIndex < Ops->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject =
				(*Ops)[OpIndex].IsValid()
					? (*Ops)[OpIndex]->AsObject()
					: nullptr;
			if (!OpObject.IsValid())
			{
				return MakeFailure(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TEXT("invalid_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("Blueprint variable batch op must be an object."),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex));
			}

			FString OpName;
			OpObject->TryGetStringField(TEXT("op"), OpName);
			const TSharedRef<FJsonObject> OpPayload = MakeBlueprintVariableOpPayload(AssetPath, FunctionName, OpObject);

			FBlueprintHelperToolResultBase OpResult;
			if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureMemberVariable)
			{
				OpResult = Service.AddMemberVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberVariableProperties)
			{
				OpResult = Service.SetMemberVariableProperties(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveMemberVariable)
			{
				OpResult = Service.RemoveMemberVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetMemberDefault)
			{
				OpResult = Service.SetMemberDefault(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpEnsureLocalVariable)
			{
				OpResult = Service.AddLocalVariable(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpSetLocalVariableProperties)
			{
				OpResult = Service.SetLocalVariableProperties(OpPayload);
			}
			else if (OpName == FBlueprintHelperBlueprintVariableTaskPlanAdapter::OpRemoveLocalVariable)
			{
				OpResult = Service.RemoveLocalVariable(OpPayload);
			}
			else
			{
				return MakeFailure(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TEXT("unsupported_variable_op"),
					EBlueprintHelperToolStage::ParseInput,
					FString::Printf(TEXT("Unsupported blueprint variable op: %s."), *OpName),
					FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].op"), OpIndex));
			}

			if (!OpResult.bOk)
			{
				return OpResult;
			}

			if (OpResult.Status == EBlueprintHelperToolStatus::Applied)
			{
				++AppliedCount;
			}
			else
			{
				++NoOpCount;
			}
		}

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = AppliedCount > 0
			? FBlueprintHelperToolResultBuilder::Applied(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId)
			: FBlueprintHelperToolResultBuilder::NoOp(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId);

		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("blueprint"));
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.BlueprintVariableBatchResult.v1"));
		Data->SetNumberField(TEXT("requested_count"), Ops->Num());
		Data->SetNumberField(TEXT("applied_count"), AppliedCount);
		Data->SetNumberField(TEXT("no_op_count"), NoOpCount);
		Result.Data = Data;

		if (AppliedCount > 0)
		{
			FBlueprintHelperValidationSummary Validation;
			Validation.bShouldCompile = true;
			Validation.bShouldSave = true;
			Result.Validation = Validation;
		}
		return Result;
	}

	FBlueprintHelperToolResultBase ExecuteComponentTaskPlanStep(
		const FBlueprintHelperComponentService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent)
		{
			FBlueprintHelperAddComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
				Payload->TryGetStringField(TEXT("component_class"), Request.ComponentClass);
				Payload->TryGetStringField(TEXT("parent_component"), Request.ParentComponent);
				Payload->TryGetStringField(TEXT("socket_name"), Request.SocketName);

				FString AttachRule;
				if (Payload->TryGetStringField(TEXT("attach_rule"), AttachRule))
				{
					TryParseAttachRule(AttachRule, Request.AttachRule);
				}
				FString NameCollisionPolicy;
				if (Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy))
				{
					TryParseNameCollisionPolicy(NameCollisionPolicy, Request.NameCollisionPolicy);
				}
			}
			return Service.AddComponent(Request);
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties)
		{
			return Service.SetComponentProperties(ReadComponentPropertiesRequest(Payload));
		}

		if (AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationRemoveComponent)
		{
			FBlueprintHelperRemoveComponentRequest Request;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("asset_path"), Request.AssetPath);
				Payload->TryGetStringField(TEXT("component_name"), Request.ComponentName);
			}
			return Service.RemoveComponent(Request);
		}

		return MakeFailure(
			TEXT("blueprint_component"),
			TEXT("unsupported_component_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported component adapter operation."));
	}

	FBlueprintHelperToolResultBase ExecuteClassSettingsTaskPlanStep(
		const FBlueprintHelperClassSettingsService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
		}

		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::AddImplementedInterfacesOp)
		{
			return Service.AddImplementedInterfaces(AssetPath, ReadStringArrayField(Payload, TEXT("interface_paths")));
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::RemoveImplementedInterfacesOp)
		{
			return Service.RemoveImplementedInterfaces(AssetPath, ReadStringArrayField(Payload, TEXT("interface_paths")));
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::SetClassDefaultPropertiesOp)
		{
			return Service.SetClassDefaultProperties(AssetPath, ReadClassDefaultSettings(Payload));
		}

		return MakeFailure(
			TEXT("blueprint_class_settings"),
			TEXT("unsupported_class_settings_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported class settings adapter operation."));
	}

	FBlueprintHelperToolResultBase MakeWidgetMutationResult(
		const FString& Operation,
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperWidgetMutationResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
			? FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId)
			: FBlueprintHelperToolResultBuilder::Failure(
				Operation,
				TraceId,
				MakeTaskRuntimeError(
					TEXT("widget_operation_failed"),
					EBlueprintHelperToolStage::Execute,
					MutationResult.ErrorMessage));

		FString AssetPath;
		FString WidgetName;
		FString PropertyName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
			Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		}
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("widget"), WidgetName, TEXT(""), PropertyName);

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
		if (!MutationResult.AffectedWidget.IsEmpty())
		{
			Data->SetStringField(TEXT("widget_name"), MutationResult.AffectedWidget);
		}
		if (!PropertyName.IsEmpty())
		{
			Data->SetStringField(TEXT("property_name"), PropertyName);
		}
		Result.Data = Data;
		return Result;
	}

	FBlueprintHelperToolResultBase ExecuteWidgetTaskPlanStep(
		const FBlueprintHelperWidgetService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString ParentName;
		FString WidgetClass;
		FString WidgetName;
		FString PropertyName;
		FString Value;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("parent_name"), ParentName);
			Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
			Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
			Payload->TryGetStringField(TEXT("property_name"), PropertyName);
			Payload->TryGetStringField(TEXT("value"), Value);
		}

		if (AdapterOperation == BlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.AddWidget(AssetPath, ParentName, WidgetClass, WidgetName));
		}
		if (AdapterOperation == BlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value));
		}
		if (AdapterOperation == BlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.RemoveWidget(AssetPath, WidgetName));
		}

		return MakeFailure(
			TEXT("umg_widget"),
			TEXT("unsupported_widget_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported widget adapter operation."));
	}

	FBlueprintHelperToolResultBase MakeDataTableMutationResult(
		const FString& Operation,
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperDataTableMutationResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
			? FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId)
			: FBlueprintHelperToolResultBuilder::Failure(
				Operation,
				TraceId,
				MakeTaskRuntimeError(
					TEXT("data_table_operation_failed"),
					EBlueprintHelperToolStage::Execute,
					MutationResult.ErrorMessage));

		FString AssetPath;
		FString RowName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("row_name"), RowName);
		}
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("data_table_row"), TEXT(""), RowName);

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("DataTableMutation.v1"));
		Data->SetStringField(TEXT("row_name"), MutationResult.AffectedRow.ToString());
		Result.Data = Data;
		return Result;
	}

	FBlueprintHelperToolResultBase ExecuteDataTableTaskPlanStep(
		const FBlueprintHelperDataTableService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString RowName;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("row_name"), RowName);
		}

		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.AddDataTableRow(AssetPath, RowName, ReadStringFieldsObject(Payload)));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.UpdateDataTableRow(AssetPath, RowName, ReadStringFieldsObject(Payload)));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationDeleteRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.DeleteDataTableRow(AssetPath, RowName));
		}

		return MakeFailure(
			TEXT("data_table"),
			TEXT("unsupported_data_table_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported DataTable adapter operation."));
	}
}

FBlueprintHelperTaskRuntimeService::FBlueprintHelperTaskRuntimeService(
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettingsService,
	const FBlueprintHelperWidgetService& InWidgetService,
	const FBlueprintHelperDataTableService& InDataTableService,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService)
	: AppendGraphService(InAppendGraphService)
	, ReplaceGraphService(InReplaceGraphService)
	, PatchGraphService(InPatchGraphService)
	, MergeGraphService(InMergeGraphService)
	, VariableService(InVariableService)
	, AssetFactoryService(InAssetFactoryService)
	, ComponentService(InComponentService)
	, ClassSettingsService(InClassSettingsService)
	, WidgetService(InWidgetService)
	, DataTableService(InDataTableService)
	, CompileAssetService(InCompileAssetService)
	, AssetBrowseService(InAssetBrowseService)
{
}

FBlueprintHelperValidationSummary FBlueprintHelperTaskRuntimeService::BuildRuntimeValidation(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperValidationSummary& BaseValidation)
{
	FBlueprintHelperValidationSummary RuntimeValidation = BaseValidation;

	const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
	if (!TaskPlan.IsValid() ||
		!TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr) ||
		!ExecutionPolicyPtr || !ExecutionPolicyPtr->IsValid())
	{
		return RuntimeValidation;
	}

	bool bShouldCompile = false;
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_compile"), bShouldCompile))
	{
		RuntimeValidation.bShouldCompile = bShouldCompile;
	}

	bool bShouldSave = false;
	if ((*ExecutionPolicyPtr)->TryGetBoolField(TEXT("should_save"), bShouldSave))
	{
		RuntimeValidation.bShouldSave = bShouldSave;
	}

	return RuntimeValidation;
}

bool FBlueprintHelperTaskRuntimeService::TryLowerTaskPlanStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TSharedPtr<FJsonObject>& StepObject,
	bool bDryRun,
	FBlueprintHelperTaskRuntimeLoweredStep& OutLoweredStep,
	FBlueprintHelperToolError& OutError)
{
	OutLoweredStep = FBlueprintHelperTaskRuntimeLoweredStep();
	OutError = FBlueprintHelperToolError();

	if (!StepObject.IsValid())
	{
		OutError = MakeTaskRuntimeError(
			TEXT("invalid_taskplan_step"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("TaskPlan step must be an object."),
			TEXT("task_plan.steps[0]"));
		return false;
	}

	StepObject->TryGetStringField(TEXT("step_id"), OutLoweredStep.StepId);
	if (OutLoweredStep.StepId.IsEmpty())
	{
		OutLoweredStep.StepId = TEXT("step_001");
	}

	FString Capability;
	StepObject->TryGetStringField(TEXT("capability"), Capability);
	if (Capability == TEXT("graph_write"))
	{
		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_operation_field"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				TEXT("task_plan.steps[0].operation"));
			return false;
		}

		TSharedPtr<FJsonObject> Payload;
		if (!TryBuildGraphWriteIrAppendPayload(TaskPlan, StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = TEXT("graph_write");
		OutLoweredStep.RuntimeOperation = TEXT("graph_write");
		OutLoweredStep.AdapterOperation = TEXT("append_blueprint_graph");
		OutLoweredStep.Payload = Payload;
		return true;
	}

	if (Capability == TEXT("blueprint_variable"))
	{
		FBlueprintHelperBlueprintVariableTaskPlanPayload BuiltPayload;
		if (!FBlueprintHelperBlueprintVariableTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			StepObject,
			bDryRun,
			BuiltPayload,
			OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = BuiltPayload.Capability;
		OutLoweredStep.RuntimeOperation = BuiltPayload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = BuiltPayload.AdapterOperation;
		OutLoweredStep.Payload = BuiltPayload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = BuiltPayload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability)
	{
		FString AdapterOperation;
		if (StepObject->TryGetStringField(TEXT("operation"), AdapterOperation))
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_asset_factory_operation_field"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("asset_factory IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				TEXT("task_plan.steps[0].operation"));
			return false;
		}

		TSharedPtr<FJsonObject> Payload;
		if (!FBlueprintHelperAssetFactoryTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			TaskPlan, StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability;
		OutLoweredStep.RuntimeOperation = FBlueprintHelperAssetFactoryTaskPlanAdapter::SupportedCapability;
		OutLoweredStep.AdapterOperation = FBlueprintHelperAssetFactoryTaskPlanAdapter::AdapterOperation;
		OutLoweredStep.Payload = Payload;
		OutLoweredStep.bAdapterDryRunSupported = false;
		return true;
	}

	if (Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent)
	{
		FBlueprintHelperComponentTaskPlanPayload Payload;
		if (!FBlueprintHelperComponentTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = Payload.Capability;
		OutLoweredStep.RuntimeOperation = Payload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = Payload.AdapterOperation;
		OutLoweredStep.Payload = Payload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = Payload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperClassSettingsTaskPlanAdapter::CapabilityName)
	{
		return FBlueprintHelperClassSettingsTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == BlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
	{
		FBlueprintHelperWidgetTaskPlanPayload Payload;
		if (!FBlueprintHelperWidgetTaskPlanAdapter::TryBuildPayloadFromTaskPlanStep(
			TaskPlan, StepObject, bDryRun, Payload, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = Payload.Capability;
		OutLoweredStep.RuntimeOperation = Payload.RuntimeOperation;
		OutLoweredStep.AdapterOperation = Payload.AdapterOperation;
		OutLoweredStep.Payload = Payload.Payload;
		OutLoweredStep.bAdapterDryRunSupported = Payload.bAdapterDryRunSupported;
		return true;
	}

	if (Capability == FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable)
	{
		return FBlueprintHelperDataTableTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (!Capability.IsEmpty())
	{
		OutError = MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_capability"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime does not support this TaskPlan capability yet."),
			TEXT("task_plan.steps[0].capability"));
		return false;
	}

	FString StepOperation;
	StepObject->TryGetStringField(TEXT("operation"), StepOperation);
	if (!IsGraphWriteTaskPlanOperation(StepOperation))
	{
		OutError = MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime currently supports graph write TaskPlan steps only."),
			TEXT("task_plan.steps[0].operation"));
		return false;
	}

	FString StepPayloadErrorCode;
	FString StepPayloadErrorMessage;
	FString StepPayloadErrorField;
	TSharedRef<FJsonObject> StepPayload = BuildGraphWritePayload(
		StepOperation,
		StepObject,
		bDryRun,
		StepPayloadErrorCode,
		StepPayloadErrorMessage,
		StepPayloadErrorField);
	if (!StepPayloadErrorCode.IsEmpty())
	{
		OutError = MakeTaskRuntimeError(
			StepPayloadErrorCode,
			EBlueprintHelperToolStage::ParseInput,
			StepPayloadErrorMessage,
			StepPayloadErrorField);
		return false;
	}

	OutLoweredStep.RuntimeOperation = StepOperation;
	OutLoweredStep.AdapterOperation = StepOperation;
	OutLoweredStep.Payload = StepPayload;
	return true;
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForStep(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FString& TaskRunId,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult,
	bool bDryRun)
{
	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	StepRecords.Add({LoweredStep, StepResult});
	return MakeRuntimeData(TaskPlan, TaskRunId, StepRecords, {}, bDryRun);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForSteps(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FString& TaskRunId,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
	bool bDryRun)
{
	return MakeRuntimeData(TaskPlan, TaskRunId, StepRecords, PostOperationRecords, bDryRun);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForStep(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult)
{
	return MakeTaskRunJournal(TaskRunId, TaskPlan, LoweredStep, StepResult);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords)
{
	return MakeTaskRunJournal(TaskRunId, TaskPlan, StepRecords, PostOperationRecords);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::PreviewTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	return RunTaskPlan(Payload, true);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::ExecuteTaskPlan(
	const TSharedPtr<FJsonObject>& Payload) const
{
	return RunTaskPlan(Payload, false);
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::GetTaskRunJournal(
	const FString& TaskRunId) const
{
	const TSharedPtr<FJsonObject>* FoundJournal = TaskRunJournals.Find(TaskRunId);
	if (!FoundJournal || !FoundJournal->IsValid())
	{
		return MakeFailure(
			TEXT("get_task_run_journal"),
			TEXT("task_run_journal_not_found"),
			EBlueprintHelperToolStage::ResolveTarget,
			FString::Printf(TEXT("TaskRunJournal not found: %s"), *TaskRunId),
			TEXT("task_run_id"));
	}

	FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Completed(
		TEXT("get_task_run_journal"),
		FBlueprintHelperToolResultBuilder::GenerateTraceId());
	Result.Data = *FoundJournal;
	return Result;
}

FBlueprintHelperToolResultBase FBlueprintHelperTaskRuntimeService::RunTaskPlan(
	const TSharedPtr<FJsonObject>& Payload,
	bool bDryRun) const
{
	const FString RuntimeOperation = bDryRun ? TEXT("preview_task_plan") : TEXT("execute_task_plan");
	if (!Payload.IsValid())
	{
		return MakeFailure(RuntimeOperation, TEXT("invalid_taskplan_payload"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload is required."), TEXT("payload"));
	}

	const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) || !TaskPlanPtr || !TaskPlanPtr->IsValid())
	{
		return MakeFailure(RuntimeOperation, TEXT("missing_task_plan"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload.task_plan is required."), TEXT("payload.task_plan"));
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
	if (!(*TaskPlanPtr)->TryGetArrayField(TEXT("steps"), StepsArray) || !StepsArray || StepsArray->Num() == 0)
	{
		return MakeFailure(RuntimeOperation, TEXT("unsupported_taskplan_step_count"),
			EBlueprintHelperToolStage::ParseInput, TEXT("Task Runtime requires at least one TaskPlan step."),
			TEXT("task_plan.steps"));
	}

	const FString TaskRunId = bDryRun
		? TEXT("")
		: FString::Printf(TEXT("task_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PostOperationRecords;
	FBlueprintHelperValidationSummary BaseValidation;
	bool bSawStepValidation = false;

	auto NormalizeErrorField = [](FBlueprintHelperToolError& Error, int32 StepIndex)
	{
		if (StepIndex != 0 && Error.Field.Contains(TEXT("task_plan.steps[0]")))
		{
			Error.Field = Error.Field.Replace(
				TEXT("task_plan.steps[0]"),
				*FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex));
		}
	};

	auto BuildFailureResult = [&](const FBlueprintHelperToolError& Error) -> FBlueprintHelperToolResultBase
	{
		FBlueprintHelperToolResultBase RuntimeResult = FBlueprintHelperToolResultBuilder::Failure(
			RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			Error);

		if (bSawStepValidation || HasExecutionPolicyValidationFields(*TaskPlanPtr))
		{
			RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
		}

		if (StepRecords.Num() > 0 || PostOperationRecords.Num() > 0)
		{
			RuntimeResult.Data = BuildRuntimeDataForSteps(
				*TaskPlanPtr,
				TaskRunId,
				StepRecords,
				PostOperationRecords,
				bDryRun);
		}

		if (!bDryRun && !TaskRunId.IsEmpty() && (StepRecords.Num() > 0 || PostOperationRecords.Num() > 0))
		{
			TaskRunJournals.Add(TaskRunId, MakeTaskRunJournal(
				TaskRunId,
				*TaskPlanPtr,
				StepRecords,
				PostOperationRecords,
				true));
		}

		return RuntimeResult;
	};

	auto ExecuteLoweredStep = [&](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) -> FBlueprintHelperToolResultBase
	{
		if (bDryRun && !LoweredStep.bAdapterDryRunSupported)
		{
			FBlueprintHelperToolResultBase StepResult = FBlueprintHelperToolResultBuilder::DryRun(
				LoweredStep.AdapterOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId());
			StepResult.Data = MakeSyntheticDryRunData();
			return StepResult;
		}

		if (LoweredStep.AdapterOperation == TEXT("append_blueprint_graph"))
		{
			return AppendGraphService.Execute(LoweredStep.Payload.ToSharedRef());
		}
		if (LoweredStep.AdapterOperation == TEXT("replace_blueprint_graph"))
		{
			return ReplaceGraphService.Execute(LoweredStep.Payload.ToSharedRef());
		}
		if (LoweredStep.AdapterOperation == TEXT("patch_blueprint_graph"))
		{
			return PatchGraphService.Execute(LoweredStep.Payload.ToSharedRef());
		}
		if (LoweredStep.AdapterOperation == TEXT("merge_blueprint_graph"))
		{
			return MergeGraphService.Execute(LoweredStep.Payload.ToSharedRef());
		}
		if (LoweredStep.AdapterOperation == TEXT("add_blueprint_member_variables"))
		{
			return VariableService.AddMemberVariables(LoweredStep.Payload.ToSharedRef());
		}
		if (LoweredStep.AdapterOperation == FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch)
		{
			return ExecuteBlueprintVariableBatchTaskPlanStep(VariableService, LoweredStep.Payload);
		}
		if (LoweredStep.AdapterOperation == FBlueprintHelperAssetFactoryTaskPlanAdapter::AdapterOperation)
		{
			return ExecuteAssetFactoryTaskPlanStep(AssetFactoryService, LoweredStep.Payload);
		}
		if (LoweredStep.Capability == FBlueprintHelperComponentTaskPlanAdapter::CapabilityBlueprintComponent)
		{
			return ExecuteComponentTaskPlanStep(ComponentService, LoweredStep.AdapterOperation, LoweredStep.Payload);
		}
		if (LoweredStep.Capability == FBlueprintHelperClassSettingsTaskPlanAdapter::CapabilityName)
		{
			return ExecuteClassSettingsTaskPlanStep(ClassSettingsService, LoweredStep.AdapterOperation, LoweredStep.Payload);
		}
		if (LoweredStep.Capability == BlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
		{
			return ExecuteWidgetTaskPlanStep(WidgetService, LoweredStep.AdapterOperation, LoweredStep.Payload);
		}
		if (LoweredStep.Capability == FBlueprintHelperDataTableTaskPlanAdapter::CapabilityDataTable)
		{
			return ExecuteDataTableTaskPlanStep(DataTableService, LoweredStep.AdapterOperation, LoweredStep.Payload);
		}

		return FBlueprintHelperToolResultBuilder::Failure(
			LoweredStep.RuntimeOperation.IsEmpty() ? RuntimeOperation : LoweredStep.RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			MakeTaskRuntimeError(
				TEXT("unsupported_taskplan_adapter_operation"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Task Runtime lowering produced an unsupported adapter operation."),
				TEXT("task_plan.steps[0]")));
	};

	for (int32 StepIndex = 0; StepIndex < StepsArray->Num(); ++StepIndex)
	{
		const TSharedPtr<FJsonObject> StepObject =
			(*StepsArray)[StepIndex].IsValid()
				? (*StepsArray)[StepIndex]->AsObject()
				: nullptr;
		if (!StepObject.IsValid())
		{
			return BuildFailureResult(MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step must be an object."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex)));
		}

		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
		FBlueprintHelperToolError LoweringError;
		if (!TryLowerTaskPlanStep(*TaskPlanPtr, StepObject, bDryRun, LoweredStep, LoweringError))
		{
			NormalizeErrorField(LoweringError, StepIndex);
			return BuildFailureResult(LoweringError);
		}

		if (!LoweredStep.Payload.IsValid())
		{
			return BuildFailureResult(MakeTaskRuntimeError(
				TEXT("invalid_taskplan_lowered_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Task Runtime lowering did not produce a payload."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex)));
		}

		FBlueprintHelperToolResultBase StepResult = ExecuteLoweredStep(LoweredStep);
		StepRecords.Add({LoweredStep, StepResult});

		if (StepResult.Validation.IsSet())
		{
			BaseValidation.bShouldCompile = BaseValidation.bShouldCompile || StepResult.Validation->bShouldCompile;
			BaseValidation.bShouldSave = BaseValidation.bShouldSave || StepResult.Validation->bShouldSave;
			bSawStepValidation = true;
		}

		if (!StepResult.bOk)
		{
			return BuildFailureResult(
				StepResult.Error.IsSet()
					? *StepResult.Error
					: MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step failed.")));
		}
	}

	if (!bDryRun)
	{
		bool bShouldCompile = false;
		bool bShouldSave = false;
		const bool bHasCompilePolicy = TryReadExecutionPolicyBool(*TaskPlanPtr, TEXT("should_compile"), bShouldCompile);
		const bool bHasSavePolicy = TryReadExecutionPolicyBool(*TaskPlanPtr, TEXT("should_save"), bShouldSave);
		if ((bHasCompilePolicy && bShouldCompile) || (bHasSavePolicy && bShouldSave))
		{
			const TArray<FString> TargetAssets = ReadTargetAssets(*TaskPlanPtr);
			if (TargetAssets.Num() == 0)
			{
				return BuildFailureResult(MakeTaskRuntimeError(
					TEXT("missing_target_assets_for_post_operation"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("TaskPlan execution_policy compile/save requires target_assets."),
					TEXT("task_plan.target_assets")));
			}

			if (bHasCompilePolicy && bShouldCompile)
			{
				for (const FString& AssetPath : TargetAssets)
				{
					TSharedRef<FJsonObject> CompilePayload = MakeShared<FJsonObject>();
					CompilePayload->SetStringField(TEXT("asset_path"), AssetPath);
					FBlueprintHelperToolResultBase CompileResult = CompileAssetService.Execute(CompilePayload);
					PostOperationRecords.Add({TEXT("compile_blueprint_asset"), CompileResult});
					if (!CompileResult.bOk)
					{
						return BuildFailureResult(
							CompileResult.Error.IsSet()
								? *CompileResult.Error
								: MakeTaskRuntimeError(TEXT("task_compile_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan compile post operation failed.")));
					}
				}
			}

			if (bHasSavePolicy && bShouldSave)
			{
				for (const FString& AssetPath : TargetAssets)
				{
					FBlueprintHelperToolResultBase SaveResult = MakeSaveAssetToolResult(AssetBrowseService, AssetPath);
					PostOperationRecords.Add({TEXT("save_asset"), SaveResult});
					if (!SaveResult.bOk)
					{
						return BuildFailureResult(
							SaveResult.Error.IsSet()
								? *SaveResult.Error
								: MakeTaskRuntimeError(TEXT("task_save_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan save post operation failed.")));
					}
				}
			}
		}
	}

	FBlueprintHelperToolResultBase RuntimeResult = bDryRun
		? FBlueprintHelperToolResultBuilder::DryRun(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId())
		: FBlueprintHelperToolResultBuilder::Applied(RuntimeOperation, FBlueprintHelperToolResultBuilder::GenerateTraceId());

	for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
	{
		if (StepRecord.Result.Target.IsSet())
		{
			RuntimeResult.Target = StepRecord.Result.Target;
			break;
		}
	}

	if (bSawStepValidation || HasExecutionPolicyValidationFields(*TaskPlanPtr))
	{
		RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
	}

	RuntimeResult.Data = BuildRuntimeDataForSteps(
		*TaskPlanPtr,
		TaskRunId,
		StepRecords,
		PostOperationRecords,
		bDryRun);

	if (!bDryRun && !TaskRunId.IsEmpty())
	{
		TaskRunJournals.Add(TaskRunId, BuildTaskRunJournalForSteps(
			TaskRunId,
			*TaskPlanPtr,
			StepRecords,
			PostOperationRecords));
	}

	return RuntimeResult;
}
