// BlueprintHelper Service Layer - TaskPlan runtime executor

#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeService.h"

#include "Systems/ToolClusters/GraphWrite/BlueprintHelperAppendBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperMergeBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperPatchBlueprintGraphService.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintHelperReplaceBlueprintGraphService.h"
#include "Systems/ToolClusters/BlueprintVariables/BlueprintHelperBlueprintVariableService.h"
#include "Shared/Services/BlueprintHelperBlueprintStructureService.h"
#include "Systems/ToolClusters/BlueprintSignature/BlueprintHelperSignatureService.h"
#include "Systems/ToolClusters/AssetFactory/BlueprintHelperAssetFactoryService.h"
#include "Shared/AssetFactory/BlueprintHelperAssetFactoryTypes.h"
#include "Systems/ToolClusters/BlueprintClassSettings/BlueprintHelperClassSettingsService.h"
#include "Systems/Debug/BlueprintHelperCompileAssetService.h"
#include "Systems/Debug/BlueprintHelperDebugEntryService.h"
#include "Systems/ToolClusters/BlueprintComponent/BlueprintHelperComponentService.h"
#include "Systems/ToolClusters/ObjectProperty/BlueprintHelperPropertyReflectionService.h"
#include "Systems/ToolClusters/DataTable/BlueprintHelperDataTableService.h"
#include "Systems/ToolClusters/UMGWidget/BlueprintHelperWidgetService.h"
#include "Systems/Debug/BlueprintHelperAssetBrowseService.h"
#include "Systems/GraphLayout/BlueprintHelperGraphLayoutCoordinator.h"
#include "Systems/Review/BlueprintHelperReviewBaselineSnapshotService.h"
#include "Systems/Review/BlueprintHelperReviewStoreService.h"
#include "Shared/Debug/BlueprintHelperSaveAssetTypes.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/AssetFactory/BlueprintHelperAssetFactoryTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintVariables/BlueprintHelperBlueprintVariableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintClassSettings/BlueprintHelperClassSettingsTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintComponent/BlueprintHelperComponentTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataAssetObjectProperty/BlueprintHelperObjectPropertyTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/DataTable/BlueprintHelperDataTableTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/BlueprintSignature/BlueprintHelperSignatureTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/TaskPlanAdapters/UMGWidget/BlueprintHelperWidgetTaskPlanAdapter.h"
#include "Runtime/TaskRuntime/BlueprintHelperTaskRuntimeClusterHub.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeClusterExecutionUtils.h"
#include "Runtime/TaskRuntime/Utils/BlueprintHelperTaskRuntimeTimingUtils.h"
#include "Systems/ToolClusters/GraphWrite/FunctionResolution/BlueprintHelperCallFunctionResolver.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/Crc.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

class FBlueprintHelperTaskRuntimeServiceLocalUtils
{
public:
	static FBlueprintHelperToolError MakeTaskRuntimeError(
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

	static FBlueprintHelperToolResultBase MakeFailure(
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

	struct FScopedBlueprintHelperReviewContext
	{
		FScopedBlueprintHelperReviewContext(
			bool bInActive,
			const FString& ArchiveSessionId,
			const FString& TaskRunId)
			: bActive(bInActive)
		{
			(void)ArchiveSessionId;
			(void)TaskRunId;
		}

		~FScopedBlueprintHelperReviewContext()
		{
			if (bActive)
			{
			}
		}

		bool bActive = false;
	};

	struct FScopedBlueprintHelperGraphLayoutTask
	{
		explicit FScopedBlueprintHelperGraphLayoutTask(bool bInActive)
			: bActive(bInActive)
		{
		}

		~FScopedBlueprintHelperGraphLayoutTask()
		{
			if (bActive && !bCompleted)
			{
				FBlueprintHelperGraphLayoutCoordinator::DiscardPendingTaskLayouts();
			}
		}

		void FlushAndComplete()
		{
			if (!bActive || bCompleted)
			{
				return;
			}

			FBlueprintHelperGraphLayoutCoordinator::FlushPendingTaskLayouts();
			bCompleted = true;
		}

		bool bActive = false;
		bool bCompleted = false;
	};

	enum class EBlueprintHelperReviewBaselineDirtyAssetPolicy : uint8
	{
		Block,
		SaveBeforeArchive,
		AllowStaleDiskSnapshot
	};

	struct FBlueprintHelperReviewBaselinePolicyEvaluation
	{
		EBlueprintHelperReviewBaselineDirtyAssetPolicy Policy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
		FString PolicyString = TEXT("block");
		FString SnapshotTrust = TEXT("fresh_disk_copy");
		TArray<FString> DirtyTargetAssets;
		TArray<FString> SavedBeforeArchiveAssets;
		TArray<FString> Warnings;
		TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PreArchiveSaveOperations;
	};

	static FString MakeTaskRuntimeReviewRefSegment(const FString& RawValue)
	{
		FString Segment;
		Segment.Reserve(RawValue.Len());
		for (const TCHAR Ch : RawValue)
		{
			Segment.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
		}
		while (Segment.Contains(TEXT("__")))
		{
			Segment.ReplaceInline(TEXT("__"), TEXT("_"));
		}
		return Segment.IsEmpty() ? FString(TEXT("target")) : Segment;
	}

	static TArray<FString> CaptureReviewBaselineSnapshots(
		const FString& ArchiveSessionId,
		const TArray<FString>& AssetPaths)
	{
		TArray<FString> SnapshotRefs;
		if (ArchiveSessionId.IsEmpty())
		{
			return SnapshotRefs;
		}

		const FString SnapshotDir = FPaths::ProjectSavedDir()
			/ TEXT("BlueprintHelper")
			/ TEXT("Review")
			/ TEXT("Snapshots")
			/ ArchiveSessionId;
		IFileManager::Get().MakeDirectory(*SnapshotDir, true);

		for (const FString& AssetPath : AssetPaths)
		{
			if (AssetPath.IsEmpty())
			{
				continue;
			}

			const FString PackageFilePath = FPackageName::LongPackageNameToFilename(
				AssetPath,
				FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().FileExists(*PackageFilePath))
			{
				continue;
			}

			const FString SnapshotFileName = MakeTaskRuntimeReviewRefSegment(AssetPath) + TEXT(".uasset");
			const FString SnapshotPath = SnapshotDir / SnapshotFileName;
			if (IFileManager::Get().Copy(*SnapshotPath, *PackageFilePath, true, true) == COPY_OK)
			{
				SnapshotRefs.Add(FString::Printf(
					TEXT("review://archive/%s/baseline/%s"),
					*ArchiveSessionId,
					*SnapshotFileName));
			}
		}

		return SnapshotRefs;
	}

	static void PopulateTaskRuntimeReviewTargetSnapshots(
		FBlueprintHelperWriteReviewEvidence& Evidence,
		bool bBeforeSnapshot)
	{
		FBlueprintHelperReviewBaselineSnapshotService SnapshotService;
		for (FBlueprintHelperReviewAtomicTarget& Target : Evidence.AtomicTargets)
		{
			FString SnapshotJson;
			FString SnapshotHash;
			FString SnapshotError;
			if (!SnapshotService.CaptureTargetSnapshot(Target, SnapshotJson, SnapshotHash, SnapshotError))
			{
				continue;
			}

			if (bBeforeSnapshot)
			{
				Target.BeforeSnapshotJson = SnapshotJson;
				Target.BaselineHash = SnapshotHash;
			}
			else
			{
				Target.AfterSnapshotJson = SnapshotJson;
				Target.RecordedAfterHash = SnapshotHash;
			}
		}
	}

	static TArray<TSharedPtr<FJsonValue>> CopyArrayField(
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

	static void CopyObjectFields(
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

	static bool IsGraphWriteTaskPlanOperation(const FString& Operation)
	{
		return Operation == TEXT("append_blueprint_graph") ||
			Operation == TEXT("replace_blueprint_graph") ||
			Operation == TEXT("patch_blueprint_graph") ||
			Operation == TEXT("merge_blueprint_graph");
	}

	static FString BuildStepFieldPath(const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString(TEXT("task_plan.steps[0]"))
			: FString::Printf(TEXT("task_plan.steps[0].%s"), *Suffix);
	}

	static FString BuildOpFieldPath(int32 OpIndex, const FString& Suffix)
	{
		return Suffix.IsEmpty()
			? FString::Printf(TEXT("task_plan.steps[0].write.ops[%d]"), OpIndex)
			: FString::Printf(TEXT("task_plan.steps[0].write.ops[%d].%s"), OpIndex, *Suffix);
	}

	static FString ToTaskRuntimeIdSegment(const FString& Value)
	{
		FString Result;
		Result.Reserve(Value.Len());
		for (const TCHAR Ch : Value)
		{
			Result.AppendChar(FChar::IsAlnum(Ch) || Ch == TCHAR('_') ? Ch : TCHAR('_'));
		}
		return Result.IsEmpty() ? FString(TEXT("entry")) : Result;
	}

	static FString JsonValueToString(const TSharedPtr<FJsonValue>& Value)
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
			{
				const double NumberValue = Value->AsNumber();
				const double RoundedValue = FMath::RoundToDouble(NumberValue);
				if (FMath::IsNearlyEqual(NumberValue, RoundedValue))
				{
					return FString::Printf(TEXT("%.0f"), RoundedValue);
				}
				return FString::SanitizeFloat(NumberValue);
			}
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

	static TSharedPtr<FJsonValue> GetLiteralJsonValue(const TSharedPtr<FJsonValue>& Value)
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

	static void CopyLiteralArgsToInputs(
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

	static TSharedRef<FJsonObject> MakeExecLink(
		const FString& FromNodeId,
		const FString& ToNodeId)
	{
		TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
		Link->SetStringField(TEXT("kind"), TEXT("exec"));
		Link->SetStringField(TEXT("from"), FString::Printf(TEXT("%s.then"), *FromNodeId));
		Link->SetStringField(TEXT("to"), FString::Printf(TEXT("%s.execute"), *ToNodeId));
		return Link;
	}

	static TSharedRef<FJsonObject> MakeSyntheticDryRunData()
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("synthetic"));
		DryRun->SetStringField(TEXT("validated_scope"), TEXT("taskplan_lowering_only"));
		DryRun->SetStringField(TEXT("limitation"), TEXT("Adapter service dry-run is not implemented; target asset state and service preflight were not validated."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueString>(
			TEXT("Synthetic preview only validates TaskPlan lowering. Execute may still fail in the underlying service preflight.")));
		DryRun->SetArrayField(TEXT("warnings"), MoveTemp(Warnings));
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	struct FResolvedCallFunctionRuntimeFact
	{
		FString StepId;
		FString StatementPath;
		FString Query;
		FString StableId;
		FString NativeName;
		FString DisplayName;
		FString OwnerClassPath;
	};

	struct FCallFunctionStatementRef
	{
		TSharedPtr<FJsonObject> StatementObject;
		FString StatementPath;
		FString NamePath;
		FString Query;
	};

	static UBlueprint* ResolveTaskRuntimeBlueprint(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}
		if (UBlueprint* Existing = FindObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Existing;
		}
		return Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *AssetPath));
	}

	static UEdGraph* ResolveTaskRuntimeGraph(UBlueprint* Blueprint, const FString& GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		auto FindInGraphs = [&GraphName](const TArray<TObjectPtr<UEdGraph>>& Graphs) -> UEdGraph*
		{
			for (UEdGraph* Graph : Graphs)
			{
				if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
				{
					return Graph;
				}
			}
			return nullptr;
		};

		if (UEdGraph* Graph = FindInGraphs(Blueprint->UbergraphPages))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindInGraphs(Blueprint->FunctionGraphs))
		{
			return Graph;
		}
		if (UEdGraph* Graph = FindInGraphs(Blueprint->MacroGraphs))
		{
			return Graph;
		}
		return Blueprint->UbergraphPages.Num() > 0 ? Blueprint->UbergraphPages[0] : nullptr;
	}

	static void CollectCallFunctionStatements(
		const TArray<TSharedPtr<FJsonValue>>& StatementValues,
		const FString& StatementsPath,
		TArray<FCallFunctionStatementRef>& OutStatements)
	{
		for (int32 StatementIndex = 0; StatementIndex < StatementValues.Num(); ++StatementIndex)
		{
			const TSharedPtr<FJsonObject> StatementObject = StatementValues[StatementIndex].IsValid()
				? StatementValues[StatementIndex]->AsObject()
				: nullptr;
			if (!StatementObject.IsValid())
			{
				continue;
			}

			const FString StatementPath = FString::Printf(TEXT("%s[%d]"), *StatementsPath, StatementIndex);
			FString Kind;
			StatementObject->TryGetStringField(TEXT("kind"), Kind);
			if (Kind.Equals(TEXT("call"), ESearchCase::IgnoreCase) ||
				Kind.Equals(TEXT("call_function"), ESearchCase::IgnoreCase))
			{
				FString Query;
				bool bHasName = StatementObject->TryGetStringField(TEXT("name"), Query) && !Query.TrimStartAndEnd().IsEmpty();
				if (!bHasName)
				{
					bHasName = false;
					StatementObject->TryGetStringField(TEXT("target"), Query);
				}

				Query.TrimStartAndEndInline();
				if (!Query.IsEmpty())
				{
					FCallFunctionStatementRef Ref;
					Ref.StatementObject = StatementObject;
					Ref.StatementPath = StatementPath;
					Ref.NamePath = StatementPath + (bHasName ? TEXT(".name") : TEXT(".target"));
					Ref.Query = Query;
					OutStatements.Add(MoveTemp(Ref));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* ThenStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("then"), ThenStatements) && ThenStatements)
			{
				CollectCallFunctionStatements(*ThenStatements, StatementPath + TEXT(".then"), OutStatements);
			}

			const TArray<TSharedPtr<FJsonValue>>* ElseStatements = nullptr;
			if (StatementObject->TryGetArrayField(TEXT("else"), ElseStatements) && ElseStatements)
			{
				CollectCallFunctionStatements(*ElseStatements, StatementPath + TEXT(".else"), OutStatements);
			}
		}
	}

	static TArray<TSharedPtr<FJsonValue>> MakeResolvedCallFunctionFactArray(
		const TArray<FResolvedCallFunctionRuntimeFact>& Facts)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FResolvedCallFunctionRuntimeFact& Fact : Facts)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
			if (!Fact.StepId.IsEmpty())
			{
				Json->SetStringField(TEXT("step_id"), Fact.StepId);
			}
			Json->SetStringField(TEXT("statement_path"), Fact.StatementPath);
			Json->SetStringField(TEXT("query"), Fact.Query);
			Json->SetStringField(TEXT("stable_id"), Fact.StableId);
			Json->SetStringField(TEXT("native_name"), Fact.NativeName);
			Json->SetStringField(TEXT("display_name"), Fact.DisplayName);
			if (!Fact.OwnerClassPath.IsEmpty())
			{
				Json->SetStringField(TEXT("owner_class"), Fact.OwnerClassPath);
			}
			Values.Add(MakeShared<FJsonValueObject>(Json));
		}
		return Values;
	}

	static void AttachRuntimeFacts(
		TSharedPtr<FJsonObject> Data,
		const TArray<FResolvedCallFunctionRuntimeFact>& ResolvedCallFunctionFacts)
	{
		if (!Data.IsValid() || ResolvedCallFunctionFacts.Num() == 0)
		{
			return;
		}

		TSharedRef<FJsonObject> RuntimeFacts = MakeShared<FJsonObject>();
		RuntimeFacts->SetArrayField(
			TEXT("resolved_call_functions"),
			MakeResolvedCallFunctionFactArray(ResolvedCallFunctionFacts));
		Data->SetObjectField(TEXT("runtime_facts"), RuntimeFacts);
	}

	static TSharedRef<FJsonObject> MakeCompactCallFunctionCandidateJson(
		const FBlueprintHelperCallFunctionCandidateInfo& Candidate)
	{
		TSharedRef<FJsonObject> CandidateJson = MakeShared<FJsonObject>();
		CandidateJson->SetStringField(TEXT("stable_id"), Candidate.StableId);
		CandidateJson->SetStringField(TEXT("display_name"), Candidate.DisplayName);
		CandidateJson->SetStringField(TEXT("owner_class"), Candidate.OwnerClassPath);
		CandidateJson->SetStringField(TEXT("native_name"), Candidate.NativeFunctionName);
		return CandidateJson;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeCandidateFunctionGroupsJson(
		const FString& Query,
		const TArray<FBlueprintHelperCallFunctionCandidateInfo>& Candidates)
	{
		TArray<TSharedPtr<FJsonValue>> Groups;
		if (Candidates.Num() == 0)
		{
			return Groups;
		}

		TSharedRef<FJsonObject> Group = MakeShared<FJsonObject>();
		Group->SetStringField(TEXT("query"), Query);

		TArray<TSharedPtr<FJsonValue>> CandidateValues;
		for (const FBlueprintHelperCallFunctionCandidateInfo& Candidate : Candidates)
		{
			CandidateValues.Add(MakeShared<FJsonValueObject>(MakeCompactCallFunctionCandidateJson(Candidate)));
		}
		Group->SetArrayField(TEXT("candidates"), CandidateValues);
		Groups.Add(MakeShared<FJsonValueObject>(Group));
		return Groups;
	}

	static TSharedRef<FJsonObject> MakeCallFunctionResolutionBlockedData(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Query,
		const FString& Path,
		const TArray<FBlueprintHelperCallFunctionCandidateInfo>& Candidates)
	{
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
		DryRun->SetBoolField(TEXT("can_execute"), false);
		DryRun->SetArrayField(TEXT("conflicts"), {});

		TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
		Issue->SetStringField(TEXT("code"), Code);
		Issue->SetStringField(TEXT("stage"), ToolStageToString(Stage));
		Issue->SetStringField(TEXT("path"), Path);
		Issue->SetStringField(TEXT("source"), Path);
		Issue->SetStringField(TEXT("target"), Query);
		if (!Message.IsEmpty())
		{
			Issue->SetStringField(TEXT("message"), Message);
		}
		const TArray<TSharedPtr<FJsonValue>> CandidateGroups = MakeCandidateFunctionGroupsJson(Query, Candidates);
		if (CandidateGroups.Num() > 0)
		{
			Issue->SetArrayField(TEXT("candidate_functions"), CandidateGroups);
		}

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueObject>(Issue));
		DryRun->SetArrayField(TEXT("errors"), Errors);
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		return Data;
	}

	static void PopulateCallFunctionResolveContext(
		FBlueprintHelperCallFunctionResolveRequest& Request,
		UBlueprint* Blueprint,
		UEdGraph* Graph)
	{
		Request.Blueprint = Blueprint;
		Request.Graph = Graph;
		Request.Context.Blueprint = Blueprint;
		Request.Context.Graph = Graph;
		Request.Context.Schema = Graph ? Graph->GetSchema() : nullptr;
		Request.Context.SelfClass = Blueprint
			? (Blueprint->GeneratedClass ? Blueprint->GeneratedClass.Get() : Blueprint->SkeletonGeneratedClass.Get())
			: nullptr;
		Request.Context.GraphKind = Graph && Graph->GetClass() ? Graph->GetClass()->GetName() : FString();
		Request.Context.ArgumentNames = Request.ArgumentNames;
		Request.Context.ArgumentTypes = Request.ArgumentTypes;
		Request.Context.TargetObjectType = Request.TargetObjectType;
	}

	static bool TryResolveTaskRuntimeCallFunctions(
		const TSharedPtr<FJsonObject>& StepObject,
		int32 StepIndex,
		const FString& StepId,
		bool bDryRun,
		TArray<FResolvedCallFunctionRuntimeFact>& OutResolvedFacts,
		FBlueprintHelperToolError& OutError,
		TSharedPtr<FJsonObject>& OutBlockedStepData)
	{
		const TSharedPtr<FJsonObject>* WriteObjectPtr = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetObjectField(TEXT("write"), WriteObjectPtr) ||
			!WriteObjectPtr || !WriteObjectPtr->IsValid())
		{
			return true;
		}

		const TArray<TSharedPtr<FJsonValue>>* OpsArray = nullptr;
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray)
		{
			return true;
		}

		const TSharedPtr<FJsonObject>* TargetObjectPtr = nullptr;
		FString AssetPath;
		FString GraphName;
		if (StepObject->TryGetObjectField(TEXT("target"), TargetObjectPtr) &&
			TargetObjectPtr && TargetObjectPtr->IsValid())
		{
			(*TargetObjectPtr)->TryGetStringField(TEXT("asset_path"), AssetPath);
			(*TargetObjectPtr)->TryGetStringField(TEXT("graph"), GraphName);
		}

		UBlueprint* Blueprint = nullptr;
		UEdGraph* Graph = nullptr;
		bool bTriedResolveBlueprint = false;

		for (int32 OpIndex = 0; OpIndex < OpsArray->Num(); ++OpIndex)
		{
			const TSharedPtr<FJsonObject> OpObject =
				(*OpsArray)[OpIndex].IsValid()
					? (*OpsArray)[OpIndex]->AsObject()
					: nullptr;
			if (!OpObject.IsValid())
			{
				continue;
			}

			TSharedPtr<FJsonObject> LogicSpec;
			FString LogicSpecPath;
			const TSharedPtr<FJsonObject>* BodyObjectPtr = nullptr;
			if (OpObject->TryGetObjectField(TEXT("body"), BodyObjectPtr) &&
				BodyObjectPtr && BodyObjectPtr->IsValid())
			{
				LogicSpec = *BodyObjectPtr;
				LogicSpecPath = FString::Printf(TEXT("write.ops[%d].body"), OpIndex);
			}
			else if (OpObject->TryGetObjectField(TEXT("logic_spec"), BodyObjectPtr) &&
				BodyObjectPtr && BodyObjectPtr->IsValid())
			{
				LogicSpec = *BodyObjectPtr;
				LogicSpecPath = FString::Printf(TEXT("write.ops[%d].logic_spec"), OpIndex);
			}

			const TArray<TSharedPtr<FJsonValue>>* StatementValues = nullptr;
			if (!LogicSpec.IsValid() ||
				!LogicSpec->TryGetArrayField(TEXT("statements"), StatementValues) ||
				!StatementValues)
			{
				continue;
			}

			TArray<FCallFunctionStatementRef> CallStatements;
			CollectCallFunctionStatements(
				*StatementValues,
				LogicSpecPath + TEXT(".statements"),
				CallStatements);
			if (CallStatements.IsEmpty())
			{
				continue;
			}

			if (!bTriedResolveBlueprint)
			{
				Blueprint = ResolveTaskRuntimeBlueprint(AssetPath);
				Graph = ResolveTaskRuntimeGraph(Blueprint, GraphName);
				bTriedResolveBlueprint = true;
			}
			if (!Blueprint)
			{
				return true;
			}

			for (const FCallFunctionStatementRef& CallStatement : CallStatements)
			{
				FBlueprintHelperCallFunctionResolveRequest ResolveRequest;
				ResolveRequest.Query = CallStatement.Query;
				CallStatement.StatementObject->TryGetStringField(TEXT("search_mode"), ResolveRequest.SearchMode);
				CallStatement.StatementObject->TryGetStringField(TEXT("ambiguity"), ResolveRequest.AmbiguityPolicy);
				CallStatement.StatementObject->TryGetStringField(TEXT("ambiguity_policy"), ResolveRequest.AmbiguityPolicy);
				const TArray<TSharedPtr<FJsonValue>>* CategoryPriorityValues = nullptr;
				if (CallStatement.StatementObject->TryGetArrayField(TEXT("category_priority"), CategoryPriorityValues) &&
					CategoryPriorityValues)
				{
					for (const TSharedPtr<FJsonValue>& Value : *CategoryPriorityValues)
					{
						if (Value.IsValid())
						{
							ResolveRequest.CategoryPriority.Add(Value->AsString());
						}
					}
				}
				const TSharedPtr<FJsonObject>* ArgsObjectPtr = nullptr;
				if (CallStatement.StatementObject->TryGetObjectField(TEXT("args"), ArgsObjectPtr) &&
					ArgsObjectPtr && ArgsObjectPtr->IsValid())
				{
					(*ArgsObjectPtr)->Values.GetKeys(ResolveRequest.ArgumentNames);
				}
				PopulateCallFunctionResolveContext(ResolveRequest, Blueprint, Graph);

				const FBlueprintHelperCallFunctionResolveResult ResolveResult =
					FBlueprintHelperCallFunctionResolver::Resolve(ResolveRequest);
				if (!ResolveResult.IsResolved())
				{
					const FString Code = ResolveResult.ErrorCode.IsEmpty()
						? TEXT("function_call_not_found")
						: ResolveResult.ErrorCode;
					const FString Message = ResolveResult.Message.IsEmpty()
						? FString::Printf(TEXT("call_function resolve failed: %s"), *CallStatement.Query)
						: ResolveResult.Message;
					const EBlueprintHelperToolStage Stage = bDryRun
						? EBlueprintHelperToolStage::DryRun
						: EBlueprintHelperToolStage::Execute;
					OutError = MakeTaskRuntimeError(
						Code,
						Stage,
						Message,
						FString::Printf(TEXT("task_plan.steps[%d].%s"), StepIndex, *CallStatement.NamePath));
					OutBlockedStepData = MakeCallFunctionResolutionBlockedData(
						Code,
						Stage,
						Message,
						CallStatement.Query,
						CallStatement.NamePath,
						ResolveResult.CandidateFunctions);
					return false;
				}

				FResolvedCallFunctionRuntimeFact Fact;
				Fact.StepId = StepId;
				Fact.StatementPath = CallStatement.StatementPath;
				Fact.Query = CallStatement.Query;
				Fact.StableId = ResolveResult.Selected.StableId;
				Fact.NativeName = ResolveResult.Selected.NativeFunctionName;
				Fact.DisplayName = ResolveResult.Selected.DisplayName;
				Fact.OwnerClassPath = ResolveResult.Selected.OwnerClassPath;
				OutResolvedFacts.Add(MoveTemp(Fact));
			}
		}

		return true;
	}

	static bool TryReadStepTarget(
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

	static bool TryBuildGraphWriteEnsureEntryLogicSpec(
		const TSharedPtr<FJsonObject>& OpObject,
		int32 OpIndex,
		TSharedPtr<FJsonObject>& OutLogicSpec,
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
		TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("kind"), TEXT("custom_event"));
		EntryObject->SetStringField(TEXT("name"), EntryName);
		EntryObject->SetStringField(TEXT("id"), EntryId);

		TSharedRef<FJsonObject> LogicSpec = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* LogicSpecObjectPtr = nullptr;
		const TSharedPtr<FJsonObject>* BodyObjectPtr = nullptr;
		if (OpObject->TryGetObjectField(TEXT("logic_spec"), LogicSpecObjectPtr) &&
			LogicSpecObjectPtr && LogicSpecObjectPtr->IsValid())
		{
			CopyObjectFields(*LogicSpecObjectPtr, LogicSpec);
		}
		else if (OpObject->TryGetObjectField(TEXT("body"), BodyObjectPtr) &&
			BodyObjectPtr && BodyObjectPtr->IsValid())
		{
			CopyObjectFields(*BodyObjectPtr, LogicSpec);
		}

		if (!LogicSpec->HasField(TEXT("statements")))
		{
			LogicSpec->SetArrayField(TEXT("statements"), {});
		}
		LogicSpec->SetStringField(TEXT("schema"), TEXT("BlueprintLogicSpec.v2"));
		if (!LogicSpec->HasTypedField<EJson::Object>(TEXT("entry")))
		{
			LogicSpec->SetObjectField(TEXT("entry"), EntryObject);
		}

		OutLogicSpec = LogicSpec;
		return true;
	}

	static bool TryBuildGraphWriteIrAppendPayload(
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

		if (OpsArray->Num() != 1)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_op_batch"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("SemanticIR append graph_write supports one ensure_entry operation per TaskPlan step."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		const TSharedPtr<FJsonObject> OpObject =
			(*OpsArray)[0].IsValid()
				? (*OpsArray)[0]->AsObject()
				: nullptr;
		TSharedPtr<FJsonObject> LogicSpec;
		if (!TryBuildGraphWriteEnsureEntryLogicSpec(OpObject, 0, LogicSpec, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), GraphName);
		Payload->SetObjectField(TEXT("target"), BridgeTarget);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);
		Payload->SetBoolField(TEXT("allow_existing_graph"), true);
		if (ReadStepDependsOn(StepObject).Num() > 0)
		{
			Payload->SetBoolField(TEXT("reuse_existing_entries"), true);
		}

		FString FeatureName;
		if (TaskPlan.IsValid() && TaskPlan->TryGetStringField(TEXT("task_name"), FeatureName) && !FeatureName.IsEmpty())
		{
			Payload->SetStringField(TEXT("feature_name"), FeatureName);
		}

		Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);
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

	static bool TryReadStepPayloadParts(
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
			OutErrorMessage = TEXT("TaskPlan step target 需。asset_path 。graph。");
			OutErrorField = TEXT("task_plan.steps[0].target");
			return false;
		}

		OutParts.TargetObject = *TargetObjectPtr;
		OutParts.ArgsObject = *ArgsObjectPtr;
		return true;
	}

	static TSharedRef<FJsonObject> BuildBridgeTargetPayload(const FTaskPlanStepPayloadParts& Parts)
	{
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(Parts.TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), Parts.AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), Parts.GraphName);
		return BridgeTarget;
	}

	static bool HasExecutionPolicyValidationFields(const TSharedPtr<FJsonObject>& TaskPlan)
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

	static bool TryReadExecutionPolicyBool(
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

	static bool TryReadReviewBaselineDirtyAssetPolicy(
		const TSharedPtr<FJsonObject>& TaskPlan,
		EBlueprintHelperReviewBaselineDirtyAssetPolicy& OutPolicy,
		FString& OutPolicyString,
		FBlueprintHelperToolError& OutError)
	{
		OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
		OutPolicyString = TEXT("block");

		const TSharedPtr<FJsonObject>* ExecutionPolicyPtr = nullptr;
		if (!TaskPlan.IsValid()
			|| !TaskPlan->TryGetObjectField(TEXT("execution_policy"), ExecutionPolicyPtr)
			|| !ExecutionPolicyPtr
			|| !ExecutionPolicyPtr->IsValid())
		{
			return true;
		}

		FString RawPolicy;
		if (!(*ExecutionPolicyPtr)->TryGetStringField(TEXT("review_baseline_dirty_asset_policy"), RawPolicy)
			|| RawPolicy.IsEmpty())
		{
			return true;
		}

		RawPolicy.TrimStartAndEndInline();
		if (RawPolicy.Equals(TEXT("block"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block;
			OutPolicyString = TEXT("block");
			return true;
		}
		if (RawPolicy.Equals(TEXT("save_before_archive"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::SaveBeforeArchive;
			OutPolicyString = TEXT("save_before_archive");
			return true;
		}
		if (RawPolicy.Equals(TEXT("allow_stale_disk_snapshot"), ESearchCase::IgnoreCase))
		{
			OutPolicy = EBlueprintHelperReviewBaselineDirtyAssetPolicy::AllowStaleDiskSnapshot;
			OutPolicyString = TEXT("allow_stale_disk_snapshot");
			return true;
		}

		OutError = MakeTaskRuntimeError(
			TEXT("invalid_taskplan_execution_policy"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("execution_policy.review_baseline_dirty_asset_policy must be block, save_before_archive, or allow_stale_disk_snapshot."),
			TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
		OutError.Actual = RawPolicy;
		return false;
	}

	static TArray<FString> ReadTargetAssets(const TSharedPtr<FJsonObject>& TaskPlan)
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

	static TArray<FString> FindDirtyTargetAssets(const TArray<FString>& TargetAssets)
	{
		TArray<FString> DirtyAssets;
		for (const FString& AssetPath : TargetAssets)
		{
			if (AssetPath.IsEmpty())
			{
				continue;
			}

			UObject* Asset = StaticFindObject(UObject::StaticClass(), nullptr, *AssetPath);
			if (!Asset)
			{
				Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
			}
			if (!Asset)
			{
				continue;
			}

			UPackage* Package = Asset->GetOutermost();
			if (Package && Package->IsDirty())
			{
				DirtyAssets.AddUnique(AssetPath);
			}
		}
		return DirtyAssets;
	}

	static bool EvaluateReviewBaselinePolicy(
		const TSharedPtr<FJsonObject>& TaskPlan,
		bool bDryRun,
		const FBlueprintHelperAssetBrowseService& AssetBrowseService,
		FBlueprintHelperReviewBaselinePolicyEvaluation& OutEvaluation,
		FBlueprintHelperToolError& OutError)
	{
		OutEvaluation = FBlueprintHelperReviewBaselinePolicyEvaluation();
		if (!TryReadReviewBaselineDirtyAssetPolicy(
			TaskPlan,
			OutEvaluation.Policy,
			OutEvaluation.PolicyString,
			OutError))
		{
			return false;
		}

		const TArray<FString> TargetAssets = ReadTargetAssets(TaskPlan);
		OutEvaluation.DirtyTargetAssets = FindDirtyTargetAssets(TargetAssets);
		if (OutEvaluation.DirtyTargetAssets.Num() == 0)
		{
			OutEvaluation.SnapshotTrust = TEXT("fresh_disk_copy");
			return true;
		}

		if (OutEvaluation.Policy == EBlueprintHelperReviewBaselineDirtyAssetPolicy::Block)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("review_baseline_dirty_target_assets"),
				EBlueprintHelperToolStage::Preflight,
				TEXT("Review baseline requires saved target assets or review_baseline_dirty_asset_policy=save_before_archive."),
				TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
			OutError.Actual = FString::Join(OutEvaluation.DirtyTargetAssets, TEXT(","));
			return false;
		}

		if (OutEvaluation.Policy == EBlueprintHelperReviewBaselineDirtyAssetPolicy::SaveBeforeArchive)
		{
			OutEvaluation.SnapshotTrust = TEXT("saved_before_archive");
			const TArray<FString> OriginallyDirtyTargetAssets = OutEvaluation.DirtyTargetAssets;
			if (bDryRun)
			{
				OutEvaluation.Warnings.Add(TEXT("review_baseline_dirty_targets_would_save_before_archive"));
				return true;
			}

			for (const FString& DirtyAsset : OutEvaluation.DirtyTargetAssets)
			{
				const FBlueprintHelperToolResultBase SaveResult = MakeSaveAssetToolResult(AssetBrowseService, DirtyAsset);
				OutEvaluation.PreArchiveSaveOperations.Add({TEXT("save_before_archive"), SaveResult});
				if (!SaveResult.bOk)
				{
					OutError = SaveResult.Error.IsSet()
						? *SaveResult.Error
						: MakeTaskRuntimeError(
							TEXT("review_baseline_save_before_archive_failed"),
							EBlueprintHelperToolStage::Preflight,
							FString::Printf(TEXT("Failed to save dirty target before archive: %s"), *DirtyAsset),
							TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
					OutError.Code = TEXT("review_baseline_save_before_archive_failed");
					OutError.Stage = EBlueprintHelperToolStage::Preflight;
					OutError.Field = TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy");
					return false;
				}
				OutEvaluation.SavedBeforeArchiveAssets.Add(DirtyAsset);
			}
			const TArray<FString> RemainingDirtyTargetAssets = FindDirtyTargetAssets(TargetAssets);
			if (RemainingDirtyTargetAssets.Num() > 0)
			{
				OutError = MakeTaskRuntimeError(
					TEXT("review_baseline_save_before_archive_failed"),
					EBlueprintHelperToolStage::Preflight,
					TEXT("One or more target assets remained dirty after save_before_archive."),
					TEXT("task_plan.execution_policy.review_baseline_dirty_asset_policy"));
				OutError.Actual = FString::Join(RemainingDirtyTargetAssets, TEXT(","));
				return false;
			}
			OutEvaluation.DirtyTargetAssets = OriginallyDirtyTargetAssets;
			return true;
		}

		OutEvaluation.SnapshotTrust = TEXT("stale_disk_copy");
		OutEvaluation.Warnings.Add(TEXT("Review baseline snapshot copied from disk while target asset was dirty in editor. Snapshot is diagnostic evidence only and must not be used as authoritative rollback baseline."));
		return true;
	}

	static FBlueprintHelperToolResultBase MakeSaveAssetToolResult(
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

	static TSharedRef<FJsonObject> BuildAppendPayload(
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

	static TSharedRef<FJsonObject> BuildGraphWritePayload(
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

	static bool TryReadRequiredGraphWriteOpObject(
		const TSharedPtr<FJsonObject>& OpObject,
		const FString& FieldName,
		const FString& FieldPath,
		TSharedPtr<FJsonObject>& OutObject,
		FBlueprintHelperToolError& OutError)
	{
		const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetObjectField(FieldName, ObjectPtr) ||
			!ObjectPtr || !ObjectPtr->IsValid())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("GraphWrite structural op requires %s object."), *FieldName),
				FieldPath);
			return false;
		}

		OutObject = *ObjectPtr;
		return true;
	}

	static TSharedRef<FJsonObject> BuildGraphWriteIrTargetPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName)
	{
		TSharedRef<FJsonObject> BridgeTarget = MakeShared<FJsonObject>();
		CopyObjectFields(TargetObject, BridgeTarget);
		BridgeTarget->SetStringField(TEXT("asset_path"), AssetPath);
		BridgeTarget->SetStringField(TEXT("graph"), GraphName);
		return BridgeTarget;
	}

	static bool TryBuildGraphWriteIrReplacePayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		FString ReplaceScope;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetStringField(TEXT("replace_scope"), ReplaceScope) ||
			ReplaceScope.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("replace_body requires replace_scope."),
				BuildOpFieldPath(0, TEXT("replace_scope")));
			return false;
		}

		TSharedPtr<FJsonObject> Selector;
		TSharedPtr<FJsonObject> LogicSpec;
		if (!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("selector"), BuildOpFieldPath(0, TEXT("selector")), Selector, OutError) ||
			!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("logic_spec"), BuildOpFieldPath(0, TEXT("logic_spec")), LogicSpec, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BridgeTarget = BuildGraphWriteIrTargetPayload(TargetObject, AssetPath, GraphName);
		BridgeTarget->SetStringField(TEXT("replace_scope"), ReplaceScope);
		Payload->SetObjectField(TEXT("target"), BridgeTarget);
		Payload->SetObjectField(TEXT("selector"), Selector);
		Payload->SetObjectField(TEXT("logic_spec"), LogicSpec);

		TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* OptionsObject = nullptr;
		if (OpObject->TryGetObjectField(TEXT("options"), OptionsObject) &&
			OptionsObject && OptionsObject->IsValid())
		{
			CopyObjectFields(*OptionsObject, Options);
		}
		Options->SetBoolField(TEXT("dry_run"), bDryRun);
		Payload->SetObjectField(TEXT("options"), Options);

		OutPayload = Payload;
		return true;
	}

	static FString DefaultPatchScopeForGraphWriteOp(const FString& OpName)
	{
		if (OpName == TEXT("set_node_comment"))
		{
			return TEXT("node_comment");
		}
		if (OpName == TEXT("set_node_position"))
		{
			// DEPRECATED_LAYOUT: TaskPlan/GraphWrite node_position exists for legacy compatibility only.
			// New placement must be handled after graph writes by the UE-side GraphLayout system.
			return TEXT("node_position");
		}
		return TEXT("pin_default");
	}

	static bool TryBuildGraphWriteIrPatchPayload(
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedPtr<FJsonObject>& OpObject,
		const FString& OpName,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		TSharedPtr<FJsonObject> PatchedRef;
		TSharedPtr<FJsonObject> Patch;
		if (!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("patched_ref"), BuildOpFieldPath(0, TEXT("patched_ref")), PatchedRef, OutError) ||
			!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("patch"), BuildOpFieldPath(0, TEXT("patch")), Patch, OutError))
		{
			return false;
		}

		FString PatchScope;
		if (!OpObject->TryGetStringField(TEXT("patch_scope"), PatchScope) || PatchScope.IsEmpty())
		{
			PatchScope = DefaultPatchScopeForGraphWriteOp(OpName);
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BridgeTarget = BuildGraphWriteIrTargetPayload(TargetObject, AssetPath, GraphName);
		BridgeTarget->SetStringField(TEXT("patch_scope"), PatchScope);
		Payload->SetObjectField(TEXT("target"), BridgeTarget);
		Payload->SetStringField(TEXT("patch_type"), OpName);
		Payload->SetObjectField(TEXT("patched_ref"), PatchedRef);
		Payload->SetObjectField(TEXT("patch"), Patch);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		const TSharedPtr<FJsonObject>* ExpectedOldState = nullptr;
		if (OpObject->TryGetObjectField(TEXT("expected_old_state"), ExpectedOldState) &&
			ExpectedOldState && ExpectedOldState->IsValid())
		{
			Payload->SetObjectField(TEXT("expected_old_state"), *ExpectedOldState);
		}

		OutPayload = Payload;
		return true;
	}

	static bool TryBuildGraphWriteIrMergePayload(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& TargetObject,
		const FString& AssetPath,
		const FString& GraphName,
		const TSharedPtr<FJsonObject>& OpObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FBlueprintHelperToolError& OutError)
	{
		FString MergeScope;
		FString InsertStrategy;
		if (!OpObject.IsValid() ||
			!OpObject->TryGetStringField(TEXT("merge_scope"), MergeScope) ||
			MergeScope.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("insert_flow requires merge_scope."),
				BuildOpFieldPath(0, TEXT("merge_scope")));
			return false;
		}
		if (!OpObject->TryGetStringField(TEXT("insert_strategy"), InsertStrategy) ||
			InsertStrategy.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("insert_flow requires insert_strategy."),
				BuildOpFieldPath(0, TEXT("insert_strategy")));
			return false;
		}

		TSharedPtr<FJsonObject> Anchor;
		TSharedPtr<FJsonObject> Inserted;
		if (!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("anchor"), BuildOpFieldPath(0, TEXT("anchor")), Anchor, OutError) ||
			!TryReadRequiredGraphWriteOpObject(OpObject, TEXT("inserted"), BuildOpFieldPath(0, TEXT("inserted")), Inserted, OutError))
		{
			return false;
		}

		TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> BridgeTarget = BuildGraphWriteIrTargetPayload(TargetObject, AssetPath, GraphName);
		BridgeTarget->SetStringField(TEXT("merge_scope"), MergeScope);
		BridgeTarget->SetStringField(TEXT("insert_strategy"), InsertStrategy);
		Payload->SetObjectField(TEXT("target"), BridgeTarget);
		Payload->SetObjectField(TEXT("anchor"), Anchor);
		Payload->SetObjectField(TEXT("inserted"), Inserted);
		Payload->SetBoolField(TEXT("dry_run"), bDryRun);

		bool bAllowCompileBeforeCall = false;
		if (TryReadExecutionPolicyBool(TaskPlan, TEXT("should_compile"), bAllowCompileBeforeCall))
		{
			Payload->SetBoolField(TEXT("allow_compile_before_call"), bAllowCompileBeforeCall);
		}

		const TArray<TSharedPtr<FJsonValue>>* SequenceOrder = nullptr;
		if (OpObject->TryGetArrayField(TEXT("sequence_order"), SequenceOrder) && SequenceOrder)
		{
			Payload->SetArrayField(TEXT("sequence_order"), *SequenceOrder);
		}

		OutPayload = Payload;
		return true;
	}

	static bool TryBuildGraphWriteIrPayload(
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TSharedPtr<FJsonObject>& StepObject,
		bool bDryRun,
		TSharedPtr<FJsonObject>& OutPayload,
		FString& OutAdapterOperation,
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
		if (!(*WriteObjectPtr)->TryGetArrayField(TEXT("ops"), OpsArray) || !OpsArray || OpsArray->Num() == 0)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ops"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("graph_write TaskPlan step requires non-empty write.ops array."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		const TSharedPtr<FJsonObject> FirstOpObject =
			(*OpsArray)[0].IsValid()
				? (*OpsArray)[0]->AsObject()
				: nullptr;
		FString OpName;
		if (!FirstOpObject.IsValid() ||
			!FirstOpObject->TryGetStringField(TEXT("op"), OpName) ||
			OpName.IsEmpty())
		{
			OutError = MakeTaskRuntimeError(
				TEXT("invalid_graph_write_ir_op"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite write.ops entry requires op."),
				BuildOpFieldPath(0, TEXT("op")));
			return false;
		}

		if (OpName == TEXT("ensure_entry"))
		{
			OutAdapterOperation = TEXT("append_blueprint_graph");
			return TryBuildGraphWriteIrAppendPayload(TaskPlan, StepObject, bDryRun, OutPayload, OutError);
		}

		if (OpsArray->Num() != 1)
		{
			OutError = MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_ir_op_batch"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite replace/patch/merge IR supports one structural op per TaskPlan step."),
				BuildStepFieldPath(TEXT("write.ops")));
			return false;
		}

		if (OpName == TEXT("replace_body"))
		{
			OutAdapterOperation = TEXT("replace_blueprint_graph");
			return TryBuildGraphWriteIrReplacePayload(TargetObject, AssetPath, GraphName, FirstOpObject, bDryRun, OutPayload, OutError);
		}
		if (OpName == TEXT("set_pin_default") ||
			OpName == TEXT("set_node_comment") ||
			OpName == TEXT("set_node_position"))
		{
			// DEPRECATED_LAYOUT: set_node_position should be removed from normal TaskPlan/GraphWrite flow
			// once GraphLayout owns configurable placement.
			OutAdapterOperation = TEXT("patch_blueprint_graph");
			return TryBuildGraphWriteIrPatchPayload(TargetObject, AssetPath, GraphName, FirstOpObject, OpName, bDryRun, OutPayload, OutError);
		}
		if (OpName == TEXT("insert_flow"))
		{
			OutAdapterOperation = TEXT("merge_blueprint_graph");
			return TryBuildGraphWriteIrMergePayload(TaskPlan, TargetObject, AssetPath, GraphName, FirstOpObject, bDryRun, OutPayload, OutError);
		}

		OutError = MakeTaskRuntimeError(
			TEXT("unsupported_graph_write_ir_op"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("GraphWrite Task Runtime does not support this structural op."),
			BuildOpFieldPath(0, TEXT("op")));
		return false;
	}

	static TSharedRef<FJsonObject> MakeStepResultJson(
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

	static TSharedRef<FJsonObject> MakePostOperationResultJson(
		const FBlueprintHelperTaskRuntimePostOperationRecord& PostOperation)
	{
		TSharedRef<FJsonObject> PostJson = MakeShared<FJsonObject>();
		PostJson->SetStringField(TEXT("operation"), PostOperation.Operation);
		PostJson->SetStringField(TEXT("status"), ToolStatusToString(PostOperation.Result.Status));
		PostJson->SetObjectField(TEXT("result"), PostOperation.Result.ToJson());
		return PostJson;
	}

	static bool HasFailedStep(
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

	static bool HasFailedPostOperation(
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

	enum class EBlueprintHelperTaskJournalStepStatus : uint8
	{
		Completed,
		Failed,
		Blocked,
		Skipped
	};

	static const TCHAR* TaskJournalStepStatusToString(EBlueprintHelperTaskJournalStepStatus Status)
	{
		switch (Status)
		{
		case EBlueprintHelperTaskJournalStepStatus::Completed:
			return TEXT("completed");
		case EBlueprintHelperTaskJournalStepStatus::Failed:
			return TEXT("failed");
		case EBlueprintHelperTaskJournalStepStatus::Blocked:
			return TEXT("blocked");
		case EBlueprintHelperTaskJournalStepStatus::Skipped:
			return TEXT("skipped");
		default:
			return TEXT("unknown");
		}
	}

	static TArray<FString> ReadStepDependsOn(const TSharedPtr<FJsonObject>& StepObject)
	{
		TArray<FString> DependsOn;
		const TArray<TSharedPtr<FJsonValue>>* DependsOnValues = nullptr;
		if (!StepObject.IsValid() ||
			!StepObject->TryGetArrayField(TEXT("depends_on"), DependsOnValues) ||
			!DependsOnValues)
		{
			return DependsOn;
		}

		for (const TSharedPtr<FJsonValue>& DependsOnValue : *DependsOnValues)
		{
			if (!DependsOnValue.IsValid())
			{
				continue;
			}

			const FString StepId = DependsOnValue->AsString();
			if (!StepId.IsEmpty())
			{
				DependsOn.Add(StepId);
			}
		}

		return DependsOn;
	}

	static TArray<TSharedPtr<FJsonValue>> MakeStringArray(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const FString& Value : Values)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	static TSharedPtr<FJsonObject> MakeReviewBaselinePolicyJson(
		const FBlueprintHelperReviewBaselinePolicyEvaluation& BaselinePolicy)
	{
		TSharedPtr<FJsonObject> Baseline = MakeShared<FJsonObject>();
		Baseline->SetStringField(TEXT("dirty_asset_policy"), BaselinePolicy.PolicyString);
		Baseline->SetStringField(TEXT("snapshot_trust"), BaselinePolicy.SnapshotTrust);
		Baseline->SetArrayField(TEXT("dirty_target_assets"), MakeStringArray(BaselinePolicy.DirtyTargetAssets));
		Baseline->SetArrayField(TEXT("saved_before_archive_assets"), MakeStringArray(BaselinePolicy.SavedBeforeArchiveAssets));
		Baseline->SetArrayField(TEXT("warnings"), MakeStringArray(BaselinePolicy.Warnings));

		TArray<TSharedPtr<FJsonValue>> PreArchiveSaveOperations;
		for (const FBlueprintHelperTaskRuntimePostOperationRecord& Operation : BaselinePolicy.PreArchiveSaveOperations)
		{
			TSharedRef<FJsonObject> OperationJson = MakeShared<FJsonObject>();
			OperationJson->SetStringField(TEXT("operation"), Operation.Operation);
			OperationJson->SetStringField(TEXT("status"), Operation.Result.bOk ? TEXT("ok") : TEXT("failed"));
			if (Operation.Result.Target.IsSet())
			{
				OperationJson->SetObjectField(TEXT("target"), Operation.Result.Target->ToJson());
			}
			if (Operation.Result.Error.IsSet())
			{
				OperationJson->SetStringField(TEXT("error_code"), Operation.Result.Error->Code);
				OperationJson->SetStringField(TEXT("error_message"), Operation.Result.Error->Message);
			}
			PreArchiveSaveOperations.Add(MakeShared<FJsonValueObject>(OperationJson));
		}
		Baseline->SetArrayField(TEXT("pre_archive_save_operations"), PreArchiveSaveOperations);
		return Baseline;
	}

	static FString GetTaskPlanStepId(const TSharedPtr<FJsonObject>& StepObject, int32 StepIndex)
	{
		FString StepId;
		if (StepObject.IsValid() && StepObject->TryGetStringField(TEXT("step_id"), StepId) && !StepId.IsEmpty())
		{
			return StepId;
		}

		return FString::Printf(TEXT("step_%03d"), StepIndex + 1);
	}

	static FString GetTaskPlanStepOperation(const TSharedPtr<FJsonObject>& StepObject)
	{
		FString Capability;
		if (StepObject.IsValid() &&
			StepObject->TryGetStringField(TEXT("capability"), Capability) &&
			!Capability.IsEmpty())
		{
			return Capability;
		}

		FString Operation;
		if (StepObject.IsValid() &&
			StepObject->TryGetStringField(TEXT("operation"), Operation) &&
			!Operation.IsEmpty())
		{
			return Operation;
		}

		return TEXT("unknown");
	}

	static TSharedRef<FJsonObject> MakeTaskJournalRecoveryJson()
	{
		TSharedRef<FJsonObject> Recovery = MakeShared<FJsonObject>();
		Recovery->SetStringField(
			TEXT("recommended_action"),
			TEXT("inspect_task_result_then_submit_followup_taskspec"));
		Recovery->SetBoolField(TEXT("safe_to_retry"), false);
		Recovery->SetBoolField(TEXT("rollback_available"), false);
		TArray<TSharedPtr<FJsonValue>> Notes;
		Notes.Add(MakeShared<FJsonValueString>(
			TEXT("TaskRuntime does not perform global rollback after partial failure.")));
		Notes.Add(MakeShared<FJsonValueString>(
			TEXT("Review failed and blocked steps, then submit a follow-up TaskSpec for the remaining intended changes.")));
		Recovery->SetArrayField(TEXT("notes"), Notes);
		return Recovery;
	}

	static TSharedRef<FJsonObject> MakeTaskJournalStepJson(
		const TSharedPtr<FJsonObject>& PlannedStep,
		const FBlueprintHelperTaskRuntimeStepRecord* ExecutedStep,
		const FString& StepId,
		EBlueprintHelperTaskJournalStepStatus StepStatus,
		const TArray<FString>& DependsOn,
		const TArray<FString>& BlockedByStepIds,
		const FString& BlockedReason)
	{
		TSharedRef<FJsonObject> StepJson = MakeShared<FJsonObject>();
		StepJson->SetStringField(TEXT("step_id"), StepId);

		FString Capability;
		if (ExecutedStep && !ExecutedStep->Step.Capability.IsEmpty())
		{
			Capability = ExecutedStep->Step.Capability;
		}
		else if (PlannedStep.IsValid())
		{
			PlannedStep->TryGetStringField(TEXT("capability"), Capability);
		}
		if (!Capability.IsEmpty())
		{
			StepJson->SetStringField(TEXT("capability"), Capability);
		}

		const FString Operation = ExecutedStep
			? (ExecutedStep->Step.RuntimeOperation.IsEmpty() ? GetTaskPlanStepOperation(PlannedStep) : ExecutedStep->Step.RuntimeOperation)
			: GetTaskPlanStepOperation(PlannedStep);
		StepJson->SetStringField(TEXT("operation"), Operation);

		if (ExecutedStep && !ExecutedStep->Step.AdapterOperation.IsEmpty())
		{
			StepJson->SetStringField(TEXT("adapter_operation"), ExecutedStep->Step.AdapterOperation);
		}

		if (DependsOn.Num() > 0)
		{
			StepJson->SetArrayField(TEXT("depends_on"), MakeStringArray(DependsOn));
		}

		StepJson->SetStringField(TEXT("status"), TaskJournalStepStatusToString(StepStatus));

		if (ExecutedStep)
		{
			StepJson->SetObjectField(TEXT("result"), ExecutedStep->Result.ToJson());
		}
		else
		{
			StepJson->SetField(TEXT("error"), MakeShared<FJsonValueNull>());
		}

		if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
		{
			StepJson->SetArrayField(TEXT("blocked_by_step_ids"), MakeStringArray(BlockedByStepIds));
			StepJson->SetStringField(TEXT("blocked_reason"), BlockedReason);
		}

		return StepJson;
	}

	static TSharedRef<FJsonObject> MakeRuntimeData(
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

	static TSharedRef<FJsonObject> MakeRuntimeData(
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

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
		const FString& TaskRunId,
		const TSharedPtr<FJsonObject>& TaskPlan,
		const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
		const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
		bool bRuntimeFailed = false,
		const TSharedPtr<FJsonObject>& ReviewBaseline = nullptr)
	{
		TSharedRef<FJsonObject> Journal = MakeShared<FJsonObject>();
		Journal->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.TaskRunJournal.v1"));
		Journal->SetStringField(TEXT("task_run_id"), TaskRunId);

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

		if (ReviewBaseline.IsValid())
		{
			Journal->SetObjectField(TEXT("review_baseline"), ReviewBaseline);
		}

		TMap<FString, const FBlueprintHelperTaskRuntimeStepRecord*> StepRecordsById;
		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			StepRecordsById.Add(StepRecord.Step.StepId, &StepRecord);
		}

		TArray<TSharedPtr<FJsonValue>> Steps;
		TSet<FString> ConsumedStepIds;
		TMap<FString, EBlueprintHelperTaskJournalStepStatus> JournalStepStatuses;
		bool bHasBlockedJournalStep = false;
		bool bHasFailedJournalStep = false;
		const TArray<TSharedPtr<FJsonValue>>* PlannedSteps = nullptr;
		if (TaskPlan.IsValid() &&
			TaskPlan->TryGetArrayField(TEXT("steps"), PlannedSteps) &&
			PlannedSteps)
		{
			for (int32 StepIndex = 0; StepIndex < PlannedSteps->Num(); ++StepIndex)
			{
				const TSharedPtr<FJsonObject> PlannedStep =
					(*PlannedSteps)[StepIndex].IsValid()
						? (*PlannedSteps)[StepIndex]->AsObject()
						: nullptr;
				const FString StepId = GetTaskPlanStepId(PlannedStep, StepIndex);
				const TArray<FString> DependsOn = ReadStepDependsOn(PlannedStep);

				const FBlueprintHelperTaskRuntimeStepRecord* const* ExecutedStepPtr = StepRecordsById.Find(StepId);
				const FBlueprintHelperTaskRuntimeStepRecord* ExecutedStep = ExecutedStepPtr ? *ExecutedStepPtr : nullptr;

				EBlueprintHelperTaskJournalStepStatus StepStatus = EBlueprintHelperTaskJournalStepStatus::Skipped;
				TArray<FString> BlockedByStepIds;
				FString BlockedReason;
				if (ExecutedStep)
				{
					StepStatus = ExecutedStep->Result.bOk
						? EBlueprintHelperTaskJournalStepStatus::Completed
						: EBlueprintHelperTaskJournalStepStatus::Failed;
					ConsumedStepIds.Add(StepId);
				}
				else
				{
					bool bBlockedByFailedDependency = false;
					bool bBlockedByBlockedDependency = false;
					for (const FString& DependsOnStepId : DependsOn)
					{
						const EBlueprintHelperTaskJournalStepStatus* DependsOnStatus = JournalStepStatuses.Find(DependsOnStepId);
						if (!DependsOnStatus)
						{
							continue;
						}

						if (*DependsOnStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
						{
							bBlockedByFailedDependency = true;
							BlockedByStepIds.Add(DependsOnStepId);
						}
						else if (*DependsOnStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
						{
							bBlockedByBlockedDependency = true;
							BlockedByStepIds.Add(DependsOnStepId);
						}
					}

					if (BlockedByStepIds.Num() > 0)
					{
						StepStatus = EBlueprintHelperTaskJournalStepStatus::Blocked;
						BlockedReason = bBlockedByFailedDependency
							? TEXT("dependency_failed")
							: TEXT("dependency_blocked");
					}
				}

				if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
				{
					bHasFailedJournalStep = true;
				}
				else if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Blocked)
				{
					bHasBlockedJournalStep = true;
				}

				JournalStepStatuses.Add(StepId, StepStatus);
				Steps.Add(MakeShared<FJsonValueObject>(MakeTaskJournalStepJson(
					PlannedStep,
					ExecutedStep,
					StepId,
					StepStatus,
					DependsOn,
					BlockedByStepIds,
					BlockedReason)));
			}
		}

		for (const FBlueprintHelperTaskRuntimeStepRecord& StepRecord : StepRecords)
		{
			if (ConsumedStepIds.Contains(StepRecord.Step.StepId))
			{
				continue;
			}

			const EBlueprintHelperTaskJournalStepStatus StepStatus = StepRecord.Result.bOk
				? EBlueprintHelperTaskJournalStepStatus::Completed
				: EBlueprintHelperTaskJournalStepStatus::Failed;
			if (StepStatus == EBlueprintHelperTaskJournalStepStatus::Failed)
			{
				bHasFailedJournalStep = true;
			}

			Steps.Add(MakeShared<FJsonValueObject>(MakeTaskJournalStepJson(
				nullptr,
				&StepRecord,
				StepRecord.Step.StepId,
				StepStatus,
				{},
				{},
				TEXT(""))));
		}
		Journal->SetArrayField(TEXT("steps"), Steps);

		const bool bHasPartialFailure =
			bRuntimeFailed ||
			bHasFailedJournalStep ||
			bHasBlockedJournalStep ||
			HasFailedPostOperation(PostOperationRecords);
		Journal->SetStringField(
			TEXT("status"),
			bHasPartialFailure ? TEXT("partial_failure") : TEXT("completed"));
		if (bHasPartialFailure)
		{
			Journal->SetObjectField(TEXT("recovery"), MakeTaskJournalRecoveryJson());
		}

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

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
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

	static TSharedRef<FJsonObject> MakeTaskRunJournal(
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

	static TSharedRef<FJsonObject> MakeRuntimeTarget(
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

	static FString MakePlannedComponentKey(
		const FString& AssetPath,
		const FString& ComponentName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *ComponentName);
	}

	static FString MakePlannedWidgetKey(
		const FString& AssetPath,
		const FString& WidgetName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *WidgetName);
	}

	static FString MakePlannedDataTableRowKey(
		const FString& AssetPath,
		const FString& RowName)
	{
		return FString::Printf(TEXT("%s\n%s"), *AssetPath, *RowName);
	}

	static bool TryReadComponentPayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutComponentName)
	{
		OutAssetPath.Empty();
		OutComponentName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("component_name"), OutComponentName);
		return !OutAssetPath.IsEmpty() && !OutComponentName.IsEmpty();
	}

	static bool TryReadWidgetPayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutWidgetName)
	{
		OutAssetPath.Empty();
		OutWidgetName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("widget_name"), OutWidgetName);
		return !OutAssetPath.IsEmpty() && !OutWidgetName.IsEmpty();
	}

	static bool TryReadDataTablePayloadIdentity(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		FString& OutAssetPath,
		FString& OutRowName)
	{
		OutAssetPath.Empty();
		OutRowName.Empty();
		if (!LoweredStep.Payload.IsValid())
		{
			return false;
		}

		LoweredStep.Payload->TryGetStringField(TEXT("asset_path"), OutAssetPath);
		LoweredStep.Payload->TryGetStringField(TEXT("row_name"), OutRowName);
		return !OutAssetPath.IsEmpty() && !OutRowName.IsEmpty();
	}

	static bool IsPlannedComponentPropertyDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedComponentKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationSetComponentProperties ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("component_not_found"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString ComponentName;
		return TryReadComponentPayloadIdentity(LoweredStep, AssetPath, ComponentName) &&
			PlannedComponentKeys.Contains(MakePlannedComponentKey(AssetPath, ComponentName));
	}

	static bool IsPlannedWidgetPropertyDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedWidgetKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("widget_operation_failed"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString WidgetName;
		return TryReadWidgetPayloadIdentity(LoweredStep, AssetPath, WidgetName) &&
			PlannedWidgetKeys.Contains(MakePlannedWidgetKey(AssetPath, WidgetName));
	}

	static bool IsPlannedDataTableRowUpdateDryRun(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
		const FBlueprintHelperToolResultBase& StepResult,
		const TSet<FString>& PlannedDataTableRowKeys)
	{
		if (LoweredStep.AdapterOperation != FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow ||
			StepResult.bOk ||
			!StepResult.Error.IsSet() ||
			StepResult.Error->Code != TEXT("data_table_operation_failed"))
		{
			return false;
		}

		bool bDryRun = false;
		if (!LoweredStep.Payload.IsValid() ||
			!LoweredStep.Payload->TryGetBoolField(TEXT("dry_run"), bDryRun) ||
			!bDryRun)
		{
			return false;
		}

		FString AssetPath;
		FString RowName;
		return TryReadDataTablePayloadIdentity(LoweredStep, AssetPath, RowName) &&
			PlannedDataTableRowKeys.Contains(MakePlannedDataTableRowKey(AssetPath, RowName));
	}

	static FBlueprintHelperToolResultBase MakePlannedComponentPropertyDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString ComponentName;
		TryReadComponentPayloadIdentity(LoweredStep, AssetPath, ComponentName);

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());

		TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
		if (!AssetPath.IsEmpty())
		{
			Target->SetStringField(TEXT("asset_path"), AssetPath);
		}
		Target->SetStringField(TEXT("target_type"), TEXT("component"));
		if (!ComponentName.IsEmpty())
		{
			Target->SetStringField(TEXT("component_name"), ComponentName);
		}
		Result.CustomTargetJson = Target;

		int32 SettingsCount = 0;
		const TArray<TSharedPtr<FJsonValue>>* Settings = nullptr;
		if (LoweredStep.Payload.IsValid() &&
			LoweredStep.Payload->TryGetArrayField(TEXT("settings"), Settings) &&
			Settings)
		{
			SettingsCount = Settings->Num();
		}

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_component"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), SettingsCount);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), SettingsCount);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("Component property validation is deferred because the component is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_component_property_validation_deferred"));
		Warning->SetStringField(TEXT("target"), ComponentName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The component does not exist in the asset during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintComponent.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static FBlueprintHelperToolResultBase MakePlannedWidgetPropertyDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString WidgetName;
		TryReadWidgetPayloadIdentity(LoweredStep, AssetPath, WidgetName);

		FString PropertyName;
		if (LoweredStep.Payload.IsValid())
		{
			LoweredStep.Payload->TryGetStringField(TEXT("property_name"), PropertyName);
		}

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("widget"), WidgetName, TEXT(""), PropertyName);

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_widget"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), 1);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), 1);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("Widget property validation is deferred because the widget is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_widget_property_validation_deferred"));
		Warning->SetStringField(TEXT("target"), WidgetName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The widget does not exist in the asset during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("WidgetMutation.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		if (!WidgetName.IsEmpty())
		{
			Data->SetStringField(TEXT("widget_name"), WidgetName);
		}
		if (!PropertyName.IsEmpty())
		{
			Data->SetStringField(TEXT("property_name"), PropertyName);
		}
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static FBlueprintHelperToolResultBase MakePlannedDataTableRowUpdateDryRunResult(
		const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep)
	{
		FString AssetPath;
		FString RowName;
		TryReadDataTablePayloadIdentity(LoweredStep, AssetPath, RowName);

		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::DryRun(
			LoweredStep.AdapterOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId());
		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("data_table_row"), TEXT(""), RowName);

		int32 FieldCount = 0;
		const TSharedPtr<FJsonObject>* Fields = nullptr;
		if (LoweredStep.Payload.IsValid() &&
			LoweredStep.Payload->TryGetObjectField(TEXT("fields"), Fields) &&
			Fields && Fields->IsValid())
		{
			FieldCount = (*Fields)->Values.Num();
		}

		TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
		DryRun->SetStringField(TEXT("preview_kind"), TEXT("task_runtime_planned_data_table_row"));
		DryRun->SetBoolField(TEXT("can_execute"), true);
		DryRun->SetStringField(TEXT("result"), TEXT("passed"));
		DryRun->SetNumberField(TEXT("would_change_count"), FieldCount);
		DryRun->SetNumberField(TEXT("would_create_count"), 0);
		DryRun->SetNumberField(TEXT("would_update_count"), FieldCount);
		DryRun->SetNumberField(TEXT("would_remove_count"), 0);
		DryRun->SetNumberField(TEXT("would_no_op_count"), 0);
		DryRun->SetStringField(
			TEXT("limitation"),
			TEXT("DataTable row update validation is deferred because the row is created by an earlier dry-run step."));

		TSharedRef<FJsonObject> Warning = MakeShared<FJsonObject>();
		Warning->SetStringField(TEXT("code"), TEXT("planned_datatable_row_update_validation_deferred"));
		Warning->SetStringField(TEXT("target"), RowName);
		Warning->SetStringField(
			TEXT("message"),
			TEXT("The DataTable row does not exist during preview, but a prior TaskPlan step plans to create it."));
		TArray<TSharedPtr<FJsonValue>> Warnings;
		Warnings.Add(MakeShared<FJsonValueObject>(Warning));
		DryRun->SetArrayField(TEXT("warnings"), Warnings);
		DryRun->SetArrayField(TEXT("conflicts"), {});
		DryRun->SetArrayField(TEXT("errors"), {});

		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("DataTableMutation.v1"));
		Data->SetObjectField(TEXT("dry_run"), DryRun);
		Data->SetStringField(TEXT("row_name"), RowName);
		Result.Data = Data;

		FBlueprintHelperValidationSummary Validation;
		Validation.bShouldCompile = false;
		Validation.bShouldSave = false;
		Result.Validation = Validation;
		return Result;
	}

	static EBlueprintHelperAssetCollisionPolicy ParseAssetFactoryCollision(const FString& CollisionText)
	{
		return CollisionText.Equals(TEXT("reuse_if_exists"), ESearchCase::IgnoreCase)
			? EBlueprintHelperAssetCollisionPolicy::ReuseIfExists
			: EBlueprintHelperAssetCollisionPolicy::FailIfExists;
	}

	static void ApplyAssetFactoryResultData(
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

	static TArray<FBlueprintHelperAssetFactoryFieldSpec> ReadAssetFactoryFieldsArray(
		const TSharedPtr<FJsonObject>& Payload)
	{
		TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Payload.IsValid() || !Payload->TryGetArrayField(TEXT("fields"), Values) || !Values)
		{
			return Fields;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> FieldObject = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!FieldObject.IsValid())
			{
				continue;
			}

			FString Name;
			FString Type;
			if (!FieldObject->TryGetStringField(TEXT("name"), Name) ||
				!FieldObject->TryGetStringField(TEXT("type"), Type))
			{
				continue;
			}

			FBlueprintHelperAssetFactoryFieldSpec Field(Name, Type);
			if (FieldObject->HasField(TEXT("default_value")))
			{
				FString DefaultValue;
				if (!FieldObject->TryGetStringField(TEXT("default_value"), DefaultValue))
				{
					DefaultValue = JsonValueToString(FieldObject->TryGetField(TEXT("default_value")));
				}
				Field.DefaultValue = DefaultValue;
				Field.bHasDefaultValue = true;
			}
			Fields.Add(Field);
		}
		return Fields;
	}

	static FBlueprintHelperToolResultBase ExecuteAssetFactoryTaskPlanStep(
		const FBlueprintHelperAssetFactoryService& Service,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString AssetTypeText;
		FString ParentClass;
		FString ValueType;
		FString RowStruct;
		FString DataAssetClass;
		FString CollisionText;
		TArray<FBlueprintHelperAssetFactoryFieldSpec> Fields;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("asset_type"), AssetTypeText);
			Payload->TryGetStringField(TEXT("parent_class"), ParentClass);
			Payload->TryGetStringField(TEXT("value_type"), ValueType);
			Payload->TryGetStringField(TEXT("row_struct"), RowStruct);
			Payload->TryGetStringField(TEXT("data_asset_class"), DataAssetClass);
			Payload->TryGetStringField(TEXT("collision"), CollisionText);
			Fields = ReadAssetFactoryFieldsArray(Payload);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		EBlueprintHelperAssetType AssetType = EBlueprintHelperAssetType::Unknown;
		if (!FBlueprintHelperAssetFactoryService::TryNormalizeAssetTypeAndParent(AssetTypeText, ParentClass, AssetType))
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("unsupported_asset_type"),
				EBlueprintHelperToolStage::ParseInput,
				FString::Printf(TEXT("Unsupported asset_type: %s"), *AssetTypeText),
				TEXT("task_plan.steps[0].write.ops[0].asset_type"));
		}

		if (AssetType == EBlueprintHelperAssetType::DataTable && RowStruct.TrimStartAndEnd().IsEmpty())
		{
			return MakeFailure(
				TEXT("create_asset"),
				TEXT("missing_row_struct"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("asset_type=data_table requires row_struct."),
				TEXT("task_plan.steps[0].write.ops[0].row_struct"));
		}

		const EBlueprintHelperAssetCollisionPolicy Collision = ParseAssetFactoryCollision(CollisionText);
		const FBlueprintHelperAssetFactoryData FactoryData = Service.CreateAsset(
			AssetPath,
			AssetType,
			ParentClass,
			ValueType,
			RowStruct,
			DataAssetClass,
			Fields,
			Collision,
			bDryRun);

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result;
		if (FactoryData.Asset.bAlreadyExisted)
		{
			if (FactoryData.Collision.Policy == EBlueprintHelperAssetCollisionPolicy::ReuseIfExists &&
				FactoryData.Collision.bHandled)
			{
				Result = bDryRun
					? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
					: FBlueprintHelperToolResultBuilder::NoOp(TEXT("create_asset"), TraceId);
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
			Result = bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(TEXT("create_asset"), TraceId)
				: FBlueprintHelperToolResultBuilder::Failure(
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
		if (bDryRun && Result.Data.IsValid())
		{
			Result.Data->SetBoolField(TEXT("dry_run"), true);
		}
		return Result;
	}

	static TArray<FString> ReadTaskRuntimeStringArrayField(
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

	static TMap<FString, FString> ReadTaskRuntimeStringFieldsObject(const TSharedPtr<FJsonObject>& Payload)
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

	static TArray<FBlueprintHelperClassDefaultPropertySetting> ReadTaskRuntimeClassDefaultSettings(
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

	static FBlueprintHelperSetComponentPropertiesRequest ReadComponentPropertiesRequest(
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
		Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

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

	static TSharedRef<FJsonObject> MakeBlueprintVariableOpPayload(
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

	static FBlueprintHelperToolResultBase ExecuteBlueprintVariableBatchTaskPlanStep(
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
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
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
		int32 DryRunCount = 0;
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
			OpPayload->SetBoolField(TEXT("dry_run"), bDryRun);

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
			else if (OpResult.Status == EBlueprintHelperToolStatus::DryRun)
			{
				++DryRunCount;
			}
			else
			{
				++NoOpCount;
			}
		}

		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = (bDryRun || DryRunCount > 0)
			? FBlueprintHelperToolResultBuilder::DryRun(
				FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
				TraceId)
			: (AppliedCount > 0
				? FBlueprintHelperToolResultBuilder::Applied(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TraceId)
				: FBlueprintHelperToolResultBuilder::NoOp(
					FBlueprintHelperBlueprintVariableTaskPlanAdapter::AdapterOperationVariableBatch,
					TraceId));

		Result.CustomTargetJson = MakeRuntimeTarget(AssetPath, TEXT("blueprint"));
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("schema"), TEXT("BlueprintHelper.BlueprintVariableBatchResult.v1"));
		Data->SetNumberField(TEXT("requested_count"), Ops->Num());
		Data->SetNumberField(TEXT("applied_count"), AppliedCount);
		Data->SetNumberField(TEXT("dry_run_count"), DryRunCount);
		Data->SetNumberField(TEXT("no_op_count"), NoOpCount);
		Result.Data = Data;

		if (!bDryRun && AppliedCount > 0)
		{
			FBlueprintHelperValidationSummary Validation;
			Validation.bShouldCompile = true;
			Validation.bShouldSave = true;
			Result.Validation = Validation;
		}
		return Result;
	}

	static FBlueprintHelperToolResultBase ExecuteComponentTaskPlanStep(
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
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);

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
				Payload->TryGetBoolField(TEXT("dry_run"), Request.bDryRun);
			}
			return Service.RemoveComponent(Request);
		}

		return MakeFailure(
			TEXT("blueprint_component"),
			TEXT("unsupported_component_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported component adapter operation."));
	}

	static FBlueprintHelperToolResultBase ExecuteClassSettingsTaskPlanStep(
		const FBlueprintHelperClassSettingsService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::AddImplementedInterfacesOp)
		{
			return Service.AddImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::RemoveImplementedInterfacesOp)
		{
			return Service.RemoveImplementedInterfaces(AssetPath, ReadTaskRuntimeStringArrayField(Payload, TEXT("interface_paths")), bDryRun);
		}
		if (AdapterOperation == FBlueprintHelperClassSettingsTaskPlanAdapter::SetClassDefaultPropertiesOp)
		{
			return Service.SetClassDefaultProperties(AssetPath, ReadTaskRuntimeClassDefaultSettings(Payload), bDryRun);
		}

		return MakeFailure(
			TEXT("blueprint_class_settings"),
			TEXT("unsupported_class_settings_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported class settings adapter operation."));
	}

	static FBlueprintHelperToolResultBase MakeWidgetMutationResult(
		const FString& Operation,
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperWidgetMutationResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
			? (MutationResult.bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId)
				: FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId))
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
		Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
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

	static FBlueprintHelperToolResultBase ExecuteWidgetTaskPlanStep(
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
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("parent_name"), ParentName);
			Payload->TryGetStringField(TEXT("widget_class"), WidgetClass);
			Payload->TryGetStringField(TEXT("widget_name"), WidgetName);
			Payload->TryGetStringField(TEXT("property_name"), PropertyName);
			Payload->TryGetStringField(TEXT("value"), Value);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		if (AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.AddWidget(AssetPath, ParentName, WidgetClass, WidgetName, bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::SetWidgetProperty)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.SetWidgetProperty(AssetPath, WidgetName, PropertyName, Value, bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::RemoveWidget)
		{
			return MakeWidgetMutationResult(AdapterOperation, Payload,
				Service.RemoveWidget(AssetPath, WidgetName, bDryRun));
		}

		return MakeFailure(
			TEXT("umg_widget"),
			TEXT("unsupported_widget_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported widget adapter operation."));
	}

	static FBlueprintHelperToolResultBase MakeDataTableMutationResult(
		const FString& Operation,
		const TSharedPtr<FJsonObject>& Payload,
		const FBlueprintHelperDataTableMutationResult& MutationResult)
	{
		const FString TraceId = FBlueprintHelperToolResultBuilder::GenerateTraceId();
		FBlueprintHelperToolResultBase Result = MutationResult.bSuccess
			? (MutationResult.bDryRun
				? FBlueprintHelperToolResultBuilder::DryRun(Operation, TraceId)
				: FBlueprintHelperToolResultBuilder::Applied(Operation, TraceId))
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
		Data->SetBoolField(TEXT("dry_run"), MutationResult.bDryRun);
		Data->SetStringField(TEXT("row_name"), MutationResult.AffectedRow.ToString());
		Result.Data = Data;
		return Result;
	}

	static FBlueprintHelperToolResultBase ExecuteDataTableTaskPlanStep(
		const FBlueprintHelperDataTableService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FString AssetPath;
		FString RowName;
		bool bDryRun = false;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("row_name"), RowName);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
		}

		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.AddDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationUpdateRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.UpdateDataTableRow(AssetPath, RowName, ReadTaskRuntimeStringFieldsObject(Payload), bDryRun));
		}
		if (AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationDeleteRow)
		{
			return MakeDataTableMutationResult(AdapterOperation, Payload,
				Service.DeleteDataTableRow(AssetPath, RowName, bDryRun));
		}

		return MakeFailure(
			TEXT("data_table"),
			TEXT("unsupported_data_table_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported DataTable adapter operation."));
	}

	static bool TryBuildObjectPropertyRequest(
		const TSharedPtr<FJsonObject>& Payload,
		FBlueprintHelperSetObjectPropertiesRequest& OutRequest,
		FString& OutError)
	{
		OutRequest = FBlueprintHelperSetObjectPropertiesRequest();
		OutError.Reset();
		if (!Payload.IsValid())
		{
			OutError = TEXT("object_property adapter payload is required.");
			return false;
		}

		Payload->TryGetStringField(TEXT("asset_path"), OutRequest.AssetPath);
		Payload->TryGetBoolField(TEXT("dry_run"), OutRequest.bDryRun);
		if (OutRequest.AssetPath.IsEmpty())
		{
			OutError = TEXT("object_property adapter payload requires asset_path.");
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* SettingsArray = nullptr;
		if (Payload->TryGetArrayField(TEXT("settings"), SettingsArray) && SettingsArray)
		{
			for (const TSharedPtr<FJsonValue>& SettingValue : *SettingsArray)
			{
				const TSharedPtr<FJsonObject> SettingObject = SettingValue.IsValid()
					? SettingValue->AsObject()
					: nullptr;
				if (!SettingObject.IsValid())
				{
					OutError = TEXT("object_property settings entries must be objects.");
					return false;
				}

				FBlueprintHelperObjectPropertySetting Setting;
				SettingObject->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
				if (Setting.PropertyPath.IsEmpty())
				{
					OutError = TEXT("object_property settings entries require property_path.");
					return false;
				}

				const TSharedPtr<FJsonValue>* Value = SettingObject->Values.Find(TEXT("value"));
				if (!Value || !Value->IsValid())
				{
					OutError = TEXT("object_property settings entries require value.");
					return false;
				}

				Setting.Value = *Value;
				OutRequest.Settings.Add(MoveTemp(Setting));
			}
			if (OutRequest.Settings.Num() == 0)
			{
				OutError = TEXT("object_property settings cannot be empty.");
				return false;
			}
			return true;
		}

		FBlueprintHelperObjectPropertySetting Setting;
		Payload->TryGetStringField(TEXT("property_path"), Setting.PropertyPath);
		if (Setting.PropertyPath.IsEmpty())
		{
			OutError = TEXT("object_property adapter payload requires property_path.");
			return false;
		}

		const TSharedPtr<FJsonValue>* Value = Payload->Values.Find(TEXT("value"));
		if (!Value || !Value->IsValid())
		{
			OutError = TEXT("object_property adapter payload requires value.");
			return false;
		}

		Setting.Value = *Value;
		OutRequest.Settings.Add(MoveTemp(Setting));
		return true;
	}

	static FBlueprintHelperToolResultBase ExecuteObjectPropertyTaskPlanStep(
		const FBlueprintHelperPropertyReflectionService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		FBlueprintHelperSetObjectPropertiesRequest Request;
		FString Error;
		if (!TryBuildObjectPropertyRequest(Payload, Request, Error))
		{
			return MakeFailure(
				TEXT("object_property"),
				TEXT("invalid_object_property_adapter_payload"),
				EBlueprintHelperToolStage::ParseInput,
				Error);
		}

		if (AdapterOperation == FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperty)
		{
			return Service.SetObjectProperty(Request);
		}
		if (AdapterOperation == FBlueprintHelperObjectPropertyTaskPlanAdapter::AdapterOperationSetObjectProperties)
		{
			return Service.SetObjectProperties(Request);
		}

		return MakeFailure(
			TEXT("object_property"),
			TEXT("unsupported_object_property_adapter_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Unsupported object_property adapter operation."));
	}

	static FBlueprintHelperToolResultBase ExecuteSignatureTaskPlanStep(
		const FBlueprintHelperBlueprintStructureService& Service,
		const FString& AdapterOperation,
		const TSharedPtr<FJsonObject>& Payload)
	{
		const bool bEnsureFunction = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureFunction;
		const bool bEnsureCustomEvent = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureCustomEvent;
		const bool bRemoveSignature = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationRemoveSignature;
		const bool bEnsureEventDispatcher = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureEventDispatcher;
		const bool bEnsureOverrideEvent = AdapterOperation == FBlueprintHelperSignatureTaskPlanAdapter::AdapterOperationEnsureOverrideEvent;
		if (!bEnsureFunction && !bEnsureCustomEvent && !bRemoveSignature && !bEnsureEventDispatcher && !bEnsureOverrideEvent)
		{
			return MakeFailure(
				TEXT("blueprint_signature"),
				TEXT("unsupported_signature_adapter_operation"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Unsupported signature adapter operation."));
		}

		FString AssetPath;
		FString FunctionName;
		FString EventName;
		FString GraphName;
		FString DispatcherName;
		FString EventKind;
		FString SignatureKind;
		FString SignatureName;
		FString NameCollisionPolicy = TEXT("reuse_if_exists");
		FString InterfaceEntryKind;
		FString SignatureMismatchPolicy;
		FString ExecutePolicy;
		bool bIsPure = false;
		bool bDryRun = false;
		bool bRequireReferenceContext = true;
		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Outputs = nullptr;
		if (Payload.IsValid())
		{
			Payload->TryGetStringField(TEXT("asset_path"), AssetPath);
			Payload->TryGetStringField(TEXT("function_name"), FunctionName);
			Payload->TryGetStringField(TEXT("event_name"), EventName);
			Payload->TryGetStringField(TEXT("graph_name"), GraphName);
			Payload->TryGetStringField(TEXT("dispatcher_name"), DispatcherName);
			Payload->TryGetStringField(TEXT("event_kind"), EventKind);
			Payload->TryGetStringField(TEXT("signature_kind"), SignatureKind);
			Payload->TryGetStringField(TEXT("signature_name"), SignatureName);
			Payload->TryGetStringField(TEXT("name_collision_policy"), NameCollisionPolicy);
			Payload->TryGetStringField(TEXT("interface_entry_kind"), InterfaceEntryKind);
			Payload->TryGetStringField(TEXT("signature_mismatch_policy"), SignatureMismatchPolicy);
			Payload->TryGetStringField(TEXT("execute_policy"), ExecutePolicy);
			Payload->TryGetBoolField(TEXT("is_pure"), bIsPure);
			Payload->TryGetBoolField(TEXT("dry_run"), bDryRun);
			Payload->TryGetBoolField(TEXT("require_reference_context"), bRequireReferenceContext);
			Payload->TryGetArrayField(TEXT("inputs"), Inputs);
			Payload->TryGetArrayField(TEXT("outputs"), Outputs);
		}

		FBlueprintHelperSignatureService SignatureService(Service);

		if (bEnsureFunction)
		{
			FBlueprintHelperEnsureFunctionSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.FunctionName = FunctionName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			Request.bDryRun = bDryRun;
			Request.bIsPure = bIsPure;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			if (Outputs)
			{
				Request.Outputs = *Outputs;
			}
			return SignatureService.EnsureFunction(Request);
		}

		if (bEnsureCustomEvent)
		{
			FBlueprintHelperEnsureCustomEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			Request.bDryRun = bDryRun;
			if (Payload.IsValid())
			{
				Payload->TryGetStringField(TEXT("interface_path"), Request.InterfacePath);
			}
			Request.InterfaceEntryKind = InterfaceEntryKind;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureCustomEvent(Request);
		}

		if (bEnsureEventDispatcher)
		{
			FBlueprintHelperEnsureEventDispatcherSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.DispatcherName = DispatcherName;
			Request.NameCollisionPolicy = NameCollisionPolicy;
			if (!SignatureMismatchPolicy.IsEmpty())
			{
				Request.SignatureMismatchPolicy = SignatureMismatchPolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureEventDispatcher(Request);
		}

		if (bEnsureOverrideEvent)
		{
			FBlueprintHelperEnsureOverrideEventSignatureRequest Request;
			Request.AssetPath = AssetPath;
			Request.GraphName = GraphName;
			Request.EventName = EventName;
			Request.EventKind = EventKind;
			if (!ExecutePolicy.IsEmpty())
			{
				Request.ExecutePolicy = ExecutePolicy;
			}
			Request.bDryRun = bDryRun;
			if (Inputs)
			{
				Request.Inputs = *Inputs;
			}
			return SignatureService.EnsureOverrideEvent(Request);
		}

		FBlueprintHelperRemoveSignatureRequest Request;
		Request.AssetPath = AssetPath;
		Request.GraphName = GraphName;
		Request.SignatureKind = SignatureKind;
		Request.SignatureName = SignatureName;
		if (!ExecutePolicy.IsEmpty())
		{
			Request.ExecutePolicy = ExecutePolicy;
		}
		Request.bDryRun = bDryRun;
		Request.bRequireReferenceContext = bRequireReferenceContext;
		return SignatureService.RemoveSignature(Request);
	}

};

FBlueprintHelperTaskRuntimeService::FBlueprintHelperTaskRuntimeService(
	const FBlueprintHelperAppendBlueprintGraphService& InAppendGraphService,
	const FBlueprintHelperReplaceBlueprintGraphService& InReplaceGraphService,
	const FBlueprintHelperPatchBlueprintGraphService& InPatchGraphService,
	const FBlueprintHelperMergeBlueprintGraphService& InMergeGraphService,
	const FBlueprintHelperBlueprintVariableService& InVariableService,
	const FBlueprintHelperBlueprintStructureService& InStructureService,
	const FBlueprintHelperAssetFactoryService& InAssetFactoryService,
	const FBlueprintHelperComponentService& InComponentService,
	const FBlueprintHelperClassSettingsService& InClassSettingsService,
	const FBlueprintHelperWidgetService& InWidgetService,
	const FBlueprintHelperDataTableService& InDataTableService,
	const FBlueprintHelperPropertyReflectionService& InPropertyReflectionService,
	const FBlueprintHelperCompileAssetService& InCompileAssetService,
	const FBlueprintHelperAssetBrowseService& InAssetBrowseService,
	const FBlueprintHelperDebugEntryService* InDebugEntryService)
	: ClusterHub(MakeUnique<FBlueprintHelperTaskRuntimeClusterHub>(
		InAppendGraphService,
		InReplaceGraphService,
		InPatchGraphService,
		InMergeGraphService,
		InVariableService,
		InStructureService,
		InAssetFactoryService,
		InComponentService,
		InClassSettingsService,
		InWidgetService,
		InDataTableService,
		InPropertyReflectionService))
	, CompileAssetService(InCompileAssetService)
	, AssetBrowseService(InAssetBrowseService)
	, DebugEntryService(InDebugEntryService)
{
}

FBlueprintHelperTaskRuntimeService::~FBlueprintHelperTaskRuntimeService() = default;

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
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
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
		FString OperationField;
		if (StepObject->TryGetStringField(TEXT("operation"), OperationField))
		{
			OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("unsupported_graph_write_operation_field"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("GraphWrite IR TaskPlan steps use capability/write; adapter operation fields are runtime lowering details."),
				TEXT("task_plan.steps[0].operation"));
			return false;
		}

		TSharedPtr<FJsonObject> Payload;
		FString AdapterOperation;
		if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::TryBuildGraphWriteIrPayload(TaskPlan, StepObject, bDryRun, Payload, AdapterOperation, OutError))
		{
			return false;
		}

		OutLoweredStep.Capability = TEXT("graph_write");
		OutLoweredStep.RuntimeOperation = TEXT("graph_write");
		OutLoweredStep.AdapterOperation = AdapterOperation;
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
			OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
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
		OutLoweredStep.bAdapterDryRunSupported = true;
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

	if (Capability == FBlueprintHelperSignatureTaskPlanAdapter::CapabilityName)
	{
		return FBlueprintHelperSignatureTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (Capability == FBlueprintHelperWidgetTaskPlan::Capability::UMGWidget)
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

	if (Capability == FBlueprintHelperObjectPropertyTaskPlanAdapter::CapabilityObjectProperty)
	{
		return FBlueprintHelperObjectPropertyTaskPlanAdapter::TryLowerTaskPlanStep(
			TaskPlan,
			StepObject,
			bDryRun,
			OutLoweredStep,
			OutError);
	}

	if (!Capability.IsEmpty())
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_capability"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime does not support this TaskPlan capability yet."),
			TEXT("task_plan.steps[0].capability"));
		return false;
	}

	FString StepOperation;
	StepObject->TryGetStringField(TEXT("operation"), StepOperation);
	if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::IsGraphWriteTaskPlanOperation(StepOperation))
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
			TEXT("unsupported_taskplan_operation"),
			EBlueprintHelperToolStage::ParseInput,
			TEXT("Task Runtime currently supports graph write TaskPlan steps only."),
			TEXT("task_plan.steps[0].operation"));
		return false;
	}

	FString StepPayloadErrorCode;
	FString StepPayloadErrorMessage;
	FString StepPayloadErrorField;
	TSharedRef<FJsonObject> StepPayload = FBlueprintHelperTaskRuntimeServiceLocalUtils::BuildGraphWritePayload(
		StepOperation,
		StepObject,
		bDryRun,
		StepPayloadErrorCode,
		StepPayloadErrorMessage,
		StepPayloadErrorField);
	if (!StepPayloadErrorCode.IsEmpty())
	{
		OutError = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
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
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeRuntimeData(TaskPlan, TaskRunId, StepRecords, {}, bDryRun);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildRuntimeDataForSteps(
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FString& TaskRunId,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords,
	bool bDryRun)
{
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeRuntimeData(TaskPlan, TaskRunId, StepRecords, PostOperationRecords, bDryRun);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForStep(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep,
	const FBlueprintHelperToolResultBase& StepResult)
{
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(TaskRunId, TaskPlan, LoweredStep, StepResult);
}

TSharedRef<FJsonObject> FBlueprintHelperTaskRuntimeService::BuildTaskRunJournalForSteps(
	const FString& TaskRunId,
	const TSharedPtr<FJsonObject>& TaskPlan,
	const TArray<FBlueprintHelperTaskRuntimeStepRecord>& StepRecords,
	const TArray<FBlueprintHelperTaskRuntimePostOperationRecord>& PostOperationRecords)
{
	return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(TaskRunId, TaskPlan, StepRecords, PostOperationRecords);
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
		return FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
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
	bool bIncludeTiming = false;
	if (Payload.IsValid())
	{
		Payload->TryGetBoolField(TEXT("include_timing"), bIncludeTiming);
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FTimingTrace TimingTrace =
		FBlueprintHelperTaskRuntimeTimingUtils::StartTrace(RuntimeOperation, bIncludeTiming);
	auto AttachTimingToResult = [&TimingTrace](FBlueprintHelperToolResultBase& Result)
	{
		if (!TimingTrace.bEnabled)
		{
			return;
		}

		if (!Result.Data.IsValid())
		{
			Result.Data = MakeShared<FJsonObject>();
		}
		FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Result.Data, TimingTrace);
	};

	const double ParseStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	if (!Payload.IsValid())
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("parse_task_plan"), ParseStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(RuntimeOperation, TEXT("invalid_taskplan_payload"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload is required."), TEXT("payload"));
		AttachTimingToResult(Result);
		return Result;
	}

	const TSharedPtr<FJsonObject>* TaskPlanPtr = nullptr;
	if (!Payload->TryGetObjectField(TEXT("task_plan"), TaskPlanPtr) || !TaskPlanPtr || !TaskPlanPtr->IsValid())
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("parse_task_plan"), ParseStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(RuntimeOperation, TEXT("missing_task_plan"),
			EBlueprintHelperToolStage::ParseInput, TEXT("payload.task_plan is required."), TEXT("payload.task_plan"));
		AttachTimingToResult(Result);
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
	if (!(*TaskPlanPtr)->TryGetArrayField(TEXT("steps"), StepsArray) || !StepsArray || StepsArray->Num() == 0)
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("parse_task_plan"), ParseStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(RuntimeOperation, TEXT("unsupported_taskplan_step_count"),
			EBlueprintHelperToolStage::ParseInput, TEXT("Task Runtime requires at least one TaskPlan step."),
			TEXT("task_plan.steps"));
		AttachTimingToResult(Result);
		return Result;
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("parse_task_plan"), ParseStageStart);

	const FString TaskRunId = bDryRun
		? TEXT("")
		: FString::Printf(TEXT("task_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString ArchiveSessionId = bDryRun
		? TEXT("")
		: FString::Printf(TEXT("archive_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));

	FBlueprintHelperTaskRuntimeServiceLocalUtils::FBlueprintHelperReviewBaselinePolicyEvaluation BaselinePolicy;
	FBlueprintHelperToolError BaselinePolicyError;
	const double BaselinePolicyStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
	if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::EvaluateReviewBaselinePolicy(
		*TaskPlanPtr,
		bDryRun,
		AssetBrowseService,
		BaselinePolicy,
		BaselinePolicyError))
	{
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_policy"), BaselinePolicyStageStart);
		FBlueprintHelperToolResultBase Result = FBlueprintHelperToolResultBuilder::Failure(
			RuntimeOperation,
			FBlueprintHelperToolResultBuilder::GenerateTraceId(),
			BaselinePolicyError);
		AttachTimingToResult(Result);
		return Result;
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_policy"), BaselinePolicyStageStart);

	if (!bDryRun)
	{
		const double BaselineCaptureStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		FBlueprintHelperReviewArchiveSession ArchiveSession;
		ArchiveSession.ArchiveSessionId = ArchiveSessionId;
		ArchiveSession.TaskRunId = TaskRunId;
		ArchiveSession.AllowedTargetAssets = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
		ArchiveSession.BaselineDirtyAssetPolicy = BaselinePolicy.PolicyString;
		ArchiveSession.BaselineSnapshotTrust = BaselinePolicy.SnapshotTrust;
		ArchiveSession.DirtyTargetAssets = BaselinePolicy.DirtyTargetAssets;
		ArchiveSession.BaselineWarnings = BaselinePolicy.Warnings;
		ArchiveSession.BaselineSnapshotRefs = FBlueprintHelperTaskRuntimeServiceLocalUtils::CaptureReviewBaselineSnapshots(
			ArchiveSessionId,
			ArchiveSession.AllowedTargetAssets);
		FBlueprintHelperReviewBaselineSnapshotService BaselineSnapshotService;
		ArchiveSession.BaselineSemanticSnapshotRefs = BaselineSnapshotService.CaptureSemanticBaselineSnapshots(
			ArchiveSessionId,
			ArchiveSession.AllowedTargetAssets,
			&ArchiveSession.BaselineWarnings);
		BaselinePolicy.Warnings = ArchiveSession.BaselineWarnings;
		ArchiveSession.CreatedAt = FDateTime::UtcNow().ToIso8601();
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_baseline_capture"), BaselineCaptureStageStart);

		const double ArchiveSessionWriteStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		FBlueprintHelperReviewStoreService ReviewStore;
		FString ArchiveSessionError;
		if (!ReviewStore.SaveArchiveSession(ArchiveSession, ArchiveSessionError))
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_archive_session_write"), ArchiveSessionWriteStageStart);
			FBlueprintHelperToolResultBase Result = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeFailure(
				RuntimeOperation,
				TEXT("review_archive_session_write_failed"),
				EBlueprintHelperToolStage::Execute,
				ArchiveSessionError,
				TEXT("review.archive_session"));
			AttachTimingToResult(Result);
			return Result;
		}
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("review_archive_session_write"), ArchiveSessionWriteStageStart);
	}
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedBlueprintHelperReviewContext ReviewContext(!bDryRun, ArchiveSessionId, TaskRunId);
	FBlueprintHelperTaskRuntimeServiceLocalUtils::FScopedBlueprintHelperGraphLayoutTask GraphLayoutTask(!bDryRun);

	TArray<FBlueprintHelperTaskRuntimeStepRecord> StepRecords;
	TArray<FBlueprintHelperTaskRuntimePostOperationRecord> PostOperationRecords;
	TArray<FBlueprintHelperTaskRuntimeServiceLocalUtils::FResolvedCallFunctionRuntimeFact> ResolvedCallFunctionFacts;
	FBlueprintHelperValidationSummary BaseValidation;
	bool bSawStepValidation = false;
	TMap<FString, FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus> StepExecutionStatuses;
	TSet<FString> DryRunPlannedComponentKeys;
	TSet<FString> DryRunPlannedWidgetKeys;
	TSet<FString> DryRunPlannedDataTableRowKeys;
	bool bSawExecutionFailure = false;
	bool bHasFirstExecutionError = false;
	FBlueprintHelperToolError FirstExecutionError;

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
		FBlueprintHelperToolResultBase RuntimeResult = (bDryRun && StepRecords.Num() > 0)
			? FBlueprintHelperToolResultBuilder::DryRun(
				RuntimeOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId())
			: FBlueprintHelperToolResultBuilder::Failure(
				RuntimeOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId(),
				Error);

		if (bSawStepValidation || FBlueprintHelperTaskRuntimeServiceLocalUtils::HasExecutionPolicyValidationFields(*TaskPlanPtr))
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

		if (bDryRun && StepRecords.Num() > 0 && RuntimeResult.Data.IsValid())
		{
			const TSharedPtr<FJsonObject>* DryRunObject = nullptr;
			const bool bHasDryRunObject =
				RuntimeResult.Data->TryGetObjectField(TEXT("dry_run"), DryRunObject) &&
				DryRunObject && DryRunObject->IsValid();
			if (!bHasDryRunObject)
			{
				TSharedRef<FJsonObject> DryRun = MakeShared<FJsonObject>();
				DryRun->SetStringField(TEXT("result"), TEXT("blocked"));
				DryRun->SetBoolField(TEXT("can_execute"), false);
				DryRun->SetArrayField(TEXT("conflicts"), {});

				TSharedRef<FJsonObject> Issue = MakeShared<FJsonObject>();
				Issue->SetStringField(TEXT("code"), Error.Code.IsEmpty() ? TEXT("task_preview_blocked") : Error.Code);
				if (!Error.Message.IsEmpty())
				{
					Issue->SetStringField(TEXT("message"), Error.Message);
				}
				if (!Error.Field.IsEmpty())
				{
					Issue->SetStringField(TEXT("target"), Error.Field);
				}
				Issue->SetStringField(TEXT("source"), TEXT("task_runtime"));

				TArray<TSharedPtr<FJsonValue>> Errors;
				Errors.Add(MakeShared<FJsonValueObject>(Issue));
				DryRun->SetArrayField(TEXT("errors"), Errors);
				RuntimeResult.Data->SetObjectField(TEXT("dry_run"), DryRun);
			}
		}
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
			RuntimeResult.Data,
			ResolvedCallFunctionFacts);
		AttachTimingToResult(RuntimeResult);

		if (!bDryRun && !TaskRunId.IsEmpty() && (StepRecords.Num() > 0 || PostOperationRecords.Num() > 0))
		{
			TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(
				TaskRunId,
				*TaskPlanPtr,
				StepRecords,
				PostOperationRecords,
				true,
				FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeReviewBaselinePolicyJson(BaselinePolicy));
			FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
				Journal,
				ResolvedCallFunctionFacts);
			FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Journal, TimingTrace);
			TaskRunJournals.Add(TaskRunId, Journal);
		}

		if (DebugEntryService)
		{
			const FString ErrorCodeLower = Error.Code.ToLower();
			FBlueprintHelperDebugEntryEventInput DebugInput;
			DebugInput.SourceLayer = TEXT("task_runtime");
			DebugInput.Source = bDryRun
				? TEXT("task_preview_blocker")
				: ((StepRecords.Num() > 0 || PostOperationRecords.Num() > 0)
					? TEXT("task_partial_failure")
					: TEXT("task_execute_failure"));
			if (ErrorCodeLower.Contains(TEXT("compile")))
			{
				DebugInput.Source = TEXT("compile_failure");
			}
			else if (ErrorCodeLower.Contains(TEXT("save")))
			{
				DebugInput.Source = TEXT("save_failure");
			}
			else if (ErrorCodeLower.Contains(TEXT("rollback")) || Error.RollbackResult == EBlueprintHelperRollbackResult::Failed)
			{
				DebugInput.Source = TEXT("review_snapshot_restore_failure");
			}
			DebugInput.Operation = RuntimeOperation;
			DebugInput.Stage = ToolStageToString(Error.Stage);
			DebugInput.TraceId = RuntimeResult.TraceId;
			DebugInput.TaskRunId = TaskRunId;
			DebugInput.AssetPaths = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
			DebugInput.Error.Code = Error.Code;
			DebugInput.Error.Message = Error.Message;
			DebugInput.RecommendedNext = TEXT("get_debug_case");
			if (RuntimeResult.bOk)
			{
				DebugEntryService->RecordEventBestEffort(DebugInput);
			}
			else
			{
				DebugEntryService->AttachDebugCaseToFailureBestEffort(RuntimeResult, DebugInput);
			}
		}

		return RuntimeResult;
	};

	auto ExecuteLoweredStep = [&](const FBlueprintHelperTaskRuntimeLoweredStep& LoweredStep) -> FBlueprintHelperToolResultBase
	{
		return ClusterHub->ExecuteStep(LoweredStep, bDryRun);
	};

	for (int32 StepIndex = 0; StepIndex < StepsArray->Num(); ++StepIndex)
	{
		const TSharedPtr<FJsonObject> StepObject =
			(*StepsArray)[StepIndex].IsValid()
				? (*StepsArray)[StepIndex]->AsObject()
				: nullptr;
		if (!StepObject.IsValid())
		{
			return BuildFailureResult(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("invalid_taskplan_step"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("TaskPlan step must be an object."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex)));
		}

		const FString PlannedStepId = FBlueprintHelperTaskRuntimeServiceLocalUtils::GetTaskPlanStepId(StepObject, StepIndex);
		if (!bDryRun)
		{
			const TArray<FString> DependsOn = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadStepDependsOn(StepObject);
			bool bBlockedByDependency = false;
			for (const FString& DependsOnStepId : DependsOn)
			{
				const FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus* DependencyStatus = StepExecutionStatuses.Find(DependsOnStepId);
				if (DependencyStatus &&
					(*DependencyStatus == FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed ||
					 *DependencyStatus == FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Blocked))
				{
					bBlockedByDependency = true;
					break;
				}
			}

			if (bBlockedByDependency)
			{
				StepExecutionStatuses.Add(PlannedStepId, FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Blocked);
				bSawExecutionFailure = true;
				continue;
			}
		}

		FBlueprintHelperTaskRuntimeLoweredStep LoweredStep;
		FBlueprintHelperToolError LoweringError;
		const double LoweringStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		if (!ClusterHub->TryLowerStep(*TaskPlanPtr, StepObject, bDryRun, LoweredStep, LoweringError))
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.lowering"), *PlannedStepId),
				LoweringStageStart);
			NormalizeErrorField(LoweringError, StepIndex);
			return BuildFailureResult(LoweringError);
		}
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			TimingTrace,
			FString::Printf(TEXT("step.%s.lowering"), *LoweredStep.StepId),
			LoweringStageStart);

		if (!LoweredStep.Payload.IsValid())
		{
			return BuildFailureResult(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
				TEXT("invalid_taskplan_lowered_payload"),
				EBlueprintHelperToolStage::ParseInput,
				TEXT("Task Runtime lowering did not produce a payload."),
				FString::Printf(TEXT("task_plan.steps[%d]"), StepIndex)));
		}

		TArray<FBlueprintHelperTaskRuntimeServiceLocalUtils::FResolvedCallFunctionRuntimeFact> StepResolvedCallFunctionFacts;
		FBlueprintHelperToolError CallFunctionResolutionError;
		TSharedPtr<FJsonObject> CallFunctionBlockedData;
		const double CallFunctionStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		if (!FBlueprintHelperTaskRuntimeServiceLocalUtils::TryResolveTaskRuntimeCallFunctions(
			StepObject,
			StepIndex,
			LoweredStep.StepId,
			bDryRun,
			StepResolvedCallFunctionFacts,
			CallFunctionResolutionError,
			CallFunctionBlockedData))
		{
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.call_function_resolution"), *LoweredStep.StepId),
				CallFunctionStageStart);
			const FString StepOperation = LoweredStep.AdapterOperation.IsEmpty()
				? LoweredStep.RuntimeOperation
				: LoweredStep.AdapterOperation;
			FBlueprintHelperToolResultBase StepResult = FBlueprintHelperToolResultBuilder::Failure(
				StepOperation.IsEmpty() ? TEXT("graph_write") : StepOperation,
				FBlueprintHelperToolResultBuilder::GenerateTraceId(),
				CallFunctionResolutionError);
			StepResult.Data = CallFunctionBlockedData;
			StepRecords.Add({LoweredStep, StepResult});
			StepExecutionStatuses.Add(
				LoweredStep.StepId,
				FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed);
			return BuildFailureResult(CallFunctionResolutionError);
		}
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			TimingTrace,
			FString::Printf(TEXT("step.%s.call_function_resolution"), *LoweredStep.StepId),
			CallFunctionStageStart);
		ResolvedCallFunctionFacts.Append(StepResolvedCallFunctionFacts);

		FBlueprintHelperWriteReviewEvidence PreStepReviewEvidence;
		bool bHasPreStepReviewEvidence = false;
		if (!bDryRun)
		{
			const double ReviewBeforeStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			bHasPreStepReviewEvidence = FBlueprintHelperTaskRuntimeClusterExecutionUtils::TryBuildTaskRuntimeReviewEvidence(
				LoweredStep,
				ArchiveSessionId,
				TaskRunId,
				StepIndex,
				PreStepReviewEvidence);
			if (bHasPreStepReviewEvidence)
			{
				FBlueprintHelperTaskRuntimeServiceLocalUtils::PopulateTaskRuntimeReviewTargetSnapshots(
					PreStepReviewEvidence,
					true);
			}
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.review_before_snapshot"), *LoweredStep.StepId),
				ReviewBeforeStageStart);
		}

		const double ExecuteStepStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		FBlueprintHelperToolResultBase StepResult = ExecuteLoweredStep(LoweredStep);
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
			TimingTrace,
			FString::Printf(TEXT("step.%s.cluster_execute"), *LoweredStep.StepId),
			ExecuteStepStageStart);
		if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedComponentPropertyDryRun(LoweredStep, StepResult, DryRunPlannedComponentKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedComponentPropertyDryRunResult(LoweredStep);
		}
		else if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedWidgetPropertyDryRun(LoweredStep, StepResult, DryRunPlannedWidgetKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedWidgetPropertyDryRunResult(LoweredStep);
		}
		else if (bDryRun && FBlueprintHelperTaskRuntimeServiceLocalUtils::IsPlannedDataTableRowUpdateDryRun(LoweredStep, StepResult, DryRunPlannedDataTableRowKeys))
		{
			StepResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedDataTableRowUpdateDryRunResult(LoweredStep);
		}
		StepRecords.Add({LoweredStep, StepResult});
		if (!bDryRun && StepResult.bOk)
		{
			const double ReviewAfterStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
			FBlueprintHelperWriteReviewEvidence RuntimeEvidence = PreStepReviewEvidence;
			const bool bHasReviewEvidence = bHasPreStepReviewEvidence || ClusterHub->BuildReviewEvidence(
				LoweredStep,
				StepResult,
				ArchiveSessionId,
				TaskRunId,
				StepIndex,
				RuntimeEvidence);
			if (bHasReviewEvidence)
			{
				FBlueprintHelperTaskRuntimeServiceLocalUtils::PopulateTaskRuntimeReviewTargetSnapshots(
					RuntimeEvidence,
					false);
				FBlueprintHelperReviewStoreService ReviewStore;
				TArray<FBlueprintHelperWriteReviewEvidence> RuntimeEvidences;
				RuntimeEvidences.Add(RuntimeEvidence);
				const TArray<FBlueprintHelperReviewRecord> RuntimeReviewRecords =
					ReviewStore.BuildReviewRecordsFromEvidence(RuntimeEvidences);
				FString ReviewRecordError;
				if (!ReviewStore.SaveReviewRecords(RuntimeReviewRecords, ReviewRecordError))
				{
					StepResult.bOk = false;
					StepResult.Error = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
						TEXT("review_record_write_failed"),
						EBlueprintHelperToolStage::Execute,
						ReviewRecordError,
						FString::Printf(TEXT("task_plan.steps[%d].review"), StepIndex));
					StepRecords.Last().Result = StepResult;
				}
			}
			FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(
				TimingTrace,
				FString::Printf(TEXT("step.%s.review_after_record_write"), *LoweredStep.StepId),
				ReviewAfterStageStart);
		}
		StepExecutionStatuses.Add(
			LoweredStep.StepId,
			StepResult.bOk
				? FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Completed
				: FBlueprintHelperTaskRuntimeServiceLocalUtils::EBlueprintHelperTaskJournalStepStatus::Failed);
		if (bDryRun &&
			StepResult.bOk &&
			LoweredStep.AdapterOperation == FBlueprintHelperComponentTaskPlanAdapter::AdapterOperationAddComponent)
		{
			FString PlannedAssetPath;
			FString PlannedComponentName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadComponentPayloadIdentity(LoweredStep, PlannedAssetPath, PlannedComponentName))
			{
				DryRunPlannedComponentKeys.Add(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedComponentKey(PlannedAssetPath, PlannedComponentName));
			}
		}
		if (bDryRun &&
			StepResult.bOk &&
			LoweredStep.AdapterOperation == FBlueprintHelperWidgetTaskPlan::AdapterOperation::AddWidget)
		{
			FString PlannedAssetPath;
			FString PlannedWidgetName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadWidgetPayloadIdentity(LoweredStep, PlannedAssetPath, PlannedWidgetName))
			{
				DryRunPlannedWidgetKeys.Add(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedWidgetKey(PlannedAssetPath, PlannedWidgetName));
			}
		}
		if (bDryRun &&
			StepResult.bOk &&
			LoweredStep.AdapterOperation == FBlueprintHelperDataTableTaskPlanAdapter::AdapterOperationAddRow)
		{
			FString PlannedAssetPath;
			FString PlannedRowName;
			if (FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadDataTablePayloadIdentity(LoweredStep, PlannedAssetPath, PlannedRowName))
			{
				DryRunPlannedDataTableRowKeys.Add(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakePlannedDataTableRowKey(PlannedAssetPath, PlannedRowName));
			}
		}

		if (StepResult.Validation.IsSet())
		{
			BaseValidation.bShouldCompile = BaseValidation.bShouldCompile || StepResult.Validation->bShouldCompile;
			BaseValidation.bShouldSave = BaseValidation.bShouldSave || StepResult.Validation->bShouldSave;
			bSawStepValidation = true;
		}

		if (!StepResult.bOk)
		{
			if (!bHasFirstExecutionError)
			{
				FirstExecutionError = StepResult.Error.IsSet()
					? *StepResult.Error
					: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step failed."));
				bHasFirstExecutionError = true;
			}
			bSawExecutionFailure = true;
		}
	}

	if (bSawExecutionFailure)
	{
		return BuildFailureResult(
			bHasFirstExecutionError
				? FirstExecutionError
				: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_step_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan step failed.")));
	}

	if (!bDryRun)
	{
		const double GraphLayoutStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
		GraphLayoutTask.FlushAndComplete();
		FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("graph_layout_flush"), GraphLayoutStageStart);
	}

	if (!bDryRun)
	{
		bool bShouldCompile = false;
		bool bShouldSave = false;
		const bool bHasCompilePolicy = FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadExecutionPolicyBool(*TaskPlanPtr, TEXT("should_compile"), bShouldCompile);
		const bool bHasSavePolicy = FBlueprintHelperTaskRuntimeServiceLocalUtils::TryReadExecutionPolicyBool(*TaskPlanPtr, TEXT("should_save"), bShouldSave);
		if ((bHasCompilePolicy && bShouldCompile) || (bHasSavePolicy && bShouldSave))
		{
			const TArray<FString> TargetAssets = FBlueprintHelperTaskRuntimeServiceLocalUtils::ReadTargetAssets(*TaskPlanPtr);
			if (TargetAssets.Num() == 0)
			{
				return BuildFailureResult(FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(
					TEXT("missing_target_assets_for_post_operation"),
					EBlueprintHelperToolStage::ParseInput,
					TEXT("TaskPlan execution_policy compile/save requires target_assets."),
					TEXT("task_plan.target_assets")));
			}

			if (bHasCompilePolicy && bShouldCompile)
			{
				const double CompileStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
				for (const FString& AssetPath : TargetAssets)
				{
					TSharedRef<FJsonObject> CompilePayload = MakeShared<FJsonObject>();
					CompilePayload->SetStringField(TEXT("asset_path"), AssetPath);
					FBlueprintHelperToolResultBase CompileResult = CompileAssetService.Execute(CompilePayload);
					PostOperationRecords.Add({TEXT("compile_blueprint_asset"), CompileResult});
					if (!CompileResult.bOk)
					{
						FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("post_operation.compile"), CompileStageStart);
						FBlueprintHelperToolResultBase FailureResult = BuildFailureResult(
							CompileResult.Error.IsSet()
								? *CompileResult.Error
								: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_compile_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan compile post operation failed.")));
						if (!FailureResult.Data.IsValid())
						{
							FailureResult.Data = MakeShared<FJsonObject>();
						}
						FailureResult.Data->SetObjectField(TEXT("post_operation_failure"), CompileResult.ToJson());
						return FailureResult;
					}
				}
				FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("post_operation.compile"), CompileStageStart);
			}

			if (bHasSavePolicy && bShouldSave)
			{
				const double SaveStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
				for (const FString& AssetPath : TargetAssets)
				{
					FBlueprintHelperToolResultBase SaveResult = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeSaveAssetToolResult(AssetBrowseService, AssetPath);
					PostOperationRecords.Add({TEXT("save_asset"), SaveResult});
					if (!SaveResult.bOk)
					{
						FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("post_operation.save"), SaveStageStart);
						return BuildFailureResult(
							SaveResult.Error.IsSet()
								? *SaveResult.Error
								: FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRuntimeError(TEXT("task_save_failed"), EBlueprintHelperToolStage::Execute, TEXT("TaskPlan save post operation failed.")));
					}
				}
				FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("post_operation.save"), SaveStageStart);
			}
		}
	}

	const double ResultWrapStageStart = FBlueprintHelperTaskRuntimeTimingUtils::StartStage(TimingTrace);
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

	if (bSawStepValidation || FBlueprintHelperTaskRuntimeServiceLocalUtils::HasExecutionPolicyValidationFields(*TaskPlanPtr))
	{
		RuntimeResult.Validation = BuildRuntimeValidation(*TaskPlanPtr, BaseValidation);
	}

	RuntimeResult.Data = BuildRuntimeDataForSteps(
		*TaskPlanPtr,
		TaskRunId,
		StepRecords,
		PostOperationRecords,
		bDryRun);
	FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
		RuntimeResult.Data,
		ResolvedCallFunctionFacts);
	AttachTimingToResult(RuntimeResult);

	if (!bDryRun && !TaskRunId.IsEmpty())
	{
		TSharedRef<FJsonObject> Journal = FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeTaskRunJournal(
			TaskRunId,
			*TaskPlanPtr,
			StepRecords,
			PostOperationRecords,
			false,
			FBlueprintHelperTaskRuntimeServiceLocalUtils::MakeReviewBaselinePolicyJson(BaselinePolicy));
		FBlueprintHelperTaskRuntimeServiceLocalUtils::AttachRuntimeFacts(
			Journal,
			ResolvedCallFunctionFacts);
		FBlueprintHelperTaskRuntimeTimingUtils::AttachTiming(Journal, TimingTrace);
		TaskRunJournals.Add(TaskRunId, Journal);
	}
	FBlueprintHelperTaskRuntimeTimingUtils::FinishStage(TimingTrace, TEXT("result_wrap"), ResultWrapStageStart);
	AttachTimingToResult(RuntimeResult);

	return RuntimeResult;
}
