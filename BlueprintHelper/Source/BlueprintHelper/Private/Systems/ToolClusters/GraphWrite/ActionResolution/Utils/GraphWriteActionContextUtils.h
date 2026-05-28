#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/Context/BlueprintHelperActionContextTypes.h"
#include "Systems/ToolClusters/GraphWrite/GraphStatement/BlueprintHelperGraphSemanticIR.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionResolutionCore.h"
#include "Shared/BlueprintHelperToolResultTypes.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperProjectedSpawnerEvidence.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperAssetActionProjectionService.h"
#include "Systems/ToolClusters/GraphWrite/ActionResolution/BlueprintHelperActionDatabaseProjectionService.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"

#include "GraphWriteActionContextUtils.generated.h"

UCLASS()
class BLUEPRINTHELPER_API UGraphWriteActionContextUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ========== From BlueprintHelperActionContextDemandCollector (original named namespace) ==========

	static FString EvidenceValue(const TMap<FString, FString>& Evidence, const FString& Key);

	static void AddDefaultIfPresent(
		FBlueprintHelperActionContextDemand& Demand,
		const FString& Key,
		const FString& Value);

	static void RemoveEmptyFacts(TMap<FString, FString>& Facts);

	static void AddFieldDemandFacts(FBlueprintHelperActionContextDemand& Demand);

	static void ApplyStatementCapabilityFacts(
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperActionContextDemand& Demand);

	static void ApplyExpressionCapabilityFacts(
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperActionContextDemand& Demand);

	static bool IsEventDelegateSemantic(EBlueprintHelperActionSemanticKind SemanticKind);

	static FString BuildStatementQuery(
		const FBlueprintHelperGraphStatementIR& Statement,
		EBlueprintHelperActionSemanticKind SemanticKind);

	static FString BuildStatementTargetPath(
		const FBlueprintHelperGraphStatementIR& Statement,
		EBlueprintHelperActionSemanticKind SemanticKind);

	static FString BuildExpressionQuery(
		const FBlueprintHelperGraphExpressionIR& Expression,
		EBlueprintHelperActionSemanticKind SemanticKind);

	static FString DemandIdFromPath(const FString& Prefix, const FString& Path);

	static TArray<FString> SortedArgumentNames(
		const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Args);

	static void CopyExpressionMapContext(
		const TMap<FString, TSharedPtr<FBlueprintHelperGraphExpressionIR>>& Expressions,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void CopyNamedExpressionContext(
		const FString& Name,
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Expression,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static FString ResolveComponentPathFromTarget(const FBlueprintHelperGraphResolvedTarget& Target);

	static EBlueprintHelperActionSemanticFamily ResolveSemanticFamily(
		EBlueprintHelperActionSemanticKind SemanticKind,
		const FString& FieldScope);

	static EBlueprintHelperTypeOperation ResolveTypeOperation(EBlueprintHelperActionSemanticKind SemanticKind);

	static FString GetDefaultValue(
		const FBlueprintHelperActionContextDemand& Demand,
		const TCHAR* Key);

	static FString NormalizeSemanticOperationToken(const FString& Operation);

	static FString NormalizeOpOperationId(const FString& Operation);

	static void AddOpEvidenceIfPresent(
		FBlueprintHelperActionContextDemand& Demand,
		const FString& Key,
		const FString& Value);

	static bool IsFocusedContextEvidenceKey(const FString& Key);

	static void AddFocusedContextEvidenceIfPresent(
		FBlueprintHelperActionContextDemand& Demand,
		const FString& Key,
		const FString& Value);

	static void CopyFocusedContextEvidence(
		const TMap<FString, FString>& Evidence,
		FBlueprintHelperActionContextDemand& Demand);

	static void CopyOpContextEvidence(
		const TMap<FString, FString>& Evidence,
		FBlueprintHelperActionContextDemand& Demand);

	static FString ResolveOpOperationId(
		const FString& ExplicitFunctionOperation,
		const TMap<FString, FString>& Evidence,
		const FString& Operator,
		const FString& Query);

	static void ApplyOpEvidence(
		const FString& ExplicitFunctionOperation,
		const FString& Operator,
		const TMap<FString, FString>& Evidence,
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Left,
		const TSharedPtr<FBlueprintHelperGraphExpressionIR>& Right,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static bool ShouldRouteConvertToGeneric(
		const FString& ExplicitFunctionOperation,
		const FString& ExplicitTransformOperation);

	static bool ShouldRouteScheduleToGeneric(
		const FString& ExplicitFunctionOperation,
		const FString& ExplicitScheduleOperation);

	static void ApplyFunctionSemanticOperations(FBlueprintHelperActionContextDemand& InOutDemand);

	static void RouteConvertScheduleDemandToGeneric(FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyConvertScheduleEvidence(
		const FString& ExplicitFunctionOperation,
		const FString& ExplicitTransformOperation,
		const FString& ExplicitScheduleOperation,
		const FString& ExplicitClassPath,
		const FString& ExplicitTarget,
		const FString& ExplicitGraphLatentAllowed,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyCreateStatementEvidence(
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyCreateExpressionEvidence(
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static FString ResolveExpressionTargetPath(const FBlueprintHelperGraphExpressionIR& Expression);

	static void ApplyContainerTypeEvidence(
		const FString& ExplicitElementType,
		const FString& ExplicitKeyType,
		const FString& ExplicitValueType,
		const FString& ExplicitPinType,
		const FString& ExplicitKeyPinType,
		const FString& ExplicitValuePinType,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyContainerActionStatementEvidence(
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyContainerActionExpressionEvidence(
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyEventDelegateStatementEvidence(
		const FBlueprintHelperGraphStatementIR& Statement,
		FBlueprintHelperActionContextDemand& InOutDemand);

	static void ApplyEventDelegateExpressionEvidence(
		const FBlueprintHelperGraphExpressionIR& Expression,
		FBlueprintHelperActionContextDemand& InOutDemand);

	// ========== From BlueprintHelperActionContextBundleProjector (anonymous namespace) ==========

	static void AppendPinType(FString& Stable, const FBlueprintHelperCallFunctionPinType& PinType);

	static FString BuildSemanticConstraintsHash(
		const FBlueprintHelperActionSemanticConstraints& Semantic,
		const TMap<FString, FString>& Evidence);

	static FString BuildProjectedContextHash(
		const FBlueprintHelperResolvedActionContextBundle& Bundle,
		const FBlueprintHelperResolvedActionContext& Context);

	// ========== From BlueprintHelperActionContextInferenceService (original named namespace) ==========

	static bool MatchesToken(const FString& Value, const FString& Token);

	static void AddEvidenceIfPresent(
		FBlueprintHelperResolvedActionContext& Context,
		const FString& Key,
		const FString& Value);

	static void AddGuidEvidenceIfPresent(
		FBlueprintHelperResolvedActionContext& Context,
		const FString& Key,
		const FGuid& Value);

	static FString SnapshotFactValue(
		const FBlueprintHelperActionContextFieldSnapshot& Field,
		const FString& Key);

	static FString DemandFactValue(
		const FBlueprintHelperActionContextDemand& Demand,
		const FString& Key);

	static FString DescribePinTypeEvidence(const FBlueprintHelperCallFunctionPinType& PinType);

	static FBlueprintHelperCallFunctionPinType MakeFieldPinType(
		const FBlueprintHelperActionContextFieldSnapshot& Field);

	static bool FindFirstLinkedPinType(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const TArray<FString>& SymbolIds,
		FBlueprintHelperCallFunctionPinType& OutPinType);

	static const FBlueprintHelperActionContextFieldSnapshot* FindField(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);

	static const FBlueprintHelperActionContextFieldSnapshot* FindDelegateField(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);

	static const FBlueprintHelperActionContextFieldSnapshot* FindComponentField(
		const FBlueprintHelperActionContextSnapshot& Snapshot,
		const FBlueprintHelperActionContextDemand& Demand);

	static void ProjectFieldSnapshot(
		FBlueprintHelperResolvedActionContext& Context,
		const FBlueprintHelperActionContextDemand& Demand,
		const FBlueprintHelperActionContextFieldSnapshot& Field);

	// ========== From BlueprintHelperActionContextSnapshotBuilder (anonymous namespace) ==========

	static bool SameFieldSnapshotIdentity(
		const FBlueprintHelperActionContextFieldSnapshot& Field,
		const FString& Name,
		const FString& OwnerClassPath,
		const FString& FieldPath);

	static FBlueprintHelperActionContextFieldSnapshot& FindOrAddFieldSnapshot(
		FBlueprintHelperActionContextSnapshot& Snapshot,
		const FString& Name,
		const FString& OwnerClassPath,
		const FString& FieldPath);

	static void AddCapabilityFact(
		FBlueprintHelperActionContextFieldSnapshot& Field,
		const FString& Key,
		const FString& Value);

	static void AddCapabilityGuidFact(
		FBlueprintHelperActionContextFieldSnapshot& Field,
		const FString& Key,
		const FGuid& Value);

	static FString ContainerTypeToString(EPinContainerType ContainerType);

	static void CaptureDelegateFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot);

	static void CaptureClassFields(const UClass* Class, FBlueprintHelperActionContextSnapshot& Snapshot);

	static void CaptureFunctionLocalVariables(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot);

	static void CaptureFunctionInputParameters(UBlueprint* Blueprint, FBlueprintHelperActionContextSnapshot& Snapshot);

	// ========== From BlueprintHelperGraphWriteProjectedEvidenceQueryService (anonymous namespace) ==========

	static FBlueprintHelperToolResultBase MakeFailure(
		const FString& Code,
		EBlueprintHelperToolStage Stage,
		const FString& Message,
		const FString& Field = FString());

	static FString ReadStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName);

	static TArray<FString> ReadQueries(const TSharedPtr<FJsonObject>& Object);

	static TSharedRef<FJsonObject> MakeEvidenceJson(const FBlueprintHelperProjectedAssetActionEvidence& Evidence);

	static TSharedRef<FJsonObject> MakeEvidenceJson(const FBlueprintHelperProjectedScheduleActionEvidence& Evidence);

	static TSharedRef<FJsonObject> MakeItemBase(
		const TSharedPtr<FJsonObject>& Request,
		const FString& Kind);

	static TSharedRef<FJsonObject> MakeItemFailure(
		const TSharedPtr<FJsonObject>& Request,
		const FString& Kind,
		const FString& Message);

	static TSharedRef<FJsonObject> MakeItemSuccess(
		const TSharedPtr<FJsonObject>& Request,
		const FString& Kind,
		const TSharedRef<FJsonObject>& Evidence,
		const FString& Message);

	static bool TryProjectExactAssetAction(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperAssetActionProjectedCandidate& Candidate,
		FBlueprintHelperProjectedAssetActionEvidence& OutEvidence);

	static bool TryProjectAssetActionEvidence(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FString>& Queries,
		FBlueprintHelperProjectedAssetActionEvidence& OutEvidence,
		FString& OutMessage);

	static bool TryProjectExactSchedule(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const FBlueprintHelperActionDatabaseProjectedCandidate& Candidate,
		FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence);

	static bool TryProjectScheduleEvidence(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TArray<FString>& Queries,
		FBlueprintHelperProjectedScheduleActionEvidence& OutEvidence,
		FString& OutMessage);

	static TSharedRef<FJsonObject> ProjectRequestItem(
		UBlueprint* Blueprint,
		UEdGraph* Graph,
		const TSharedPtr<FJsonObject>& Request,
		bool& bAllResolved);
};
