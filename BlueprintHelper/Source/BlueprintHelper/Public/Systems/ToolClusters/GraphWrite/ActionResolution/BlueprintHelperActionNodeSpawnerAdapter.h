#pragma once

#include "CoreMinimal.h"

class UEdGraph;
class UK2Node;
struct FBlueprintHelperActionResolutionResult;
struct FBlueprintGeneratorDiagnostic;

struct BLUEPRINTHELPER_API FBlueprintHelperActionNodeSpawnContext
{
	UEdGraph* TargetGraph = nullptr;
	const FBlueprintHelperActionResolutionResult* ActionResult = nullptr;
	FVector2D Location = FVector2D::ZeroVector;
	FString NodeId;
};

struct BLUEPRINTHELPER_API FBlueprintHelperActionNodeSpawnOptions
{
	using FPinNormalizationHook = TFunction<void(UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext& Context)>;
	using FDefaultValueProvider = TFunction<void(UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext& Context, TMap<FString, FString>& InOutDefaultValues)>;

	FString NodeId;
	TMap<FString, FString> DefaultValues;
	bool bApplyDefaultValues = true;
	bool bReconstructAfterSpawn = true;
	FPinNormalizationHook PinNormalizationHook;
	FDefaultValueProvider DefaultValueProvider;
};

class BLUEPRINTHELPER_API FBlueprintHelperActionNodeSpawnerAdapter
{
public:
	static void NoOpPinNormalization(
		UK2Node& SpawnedNode,
		const FBlueprintHelperActionNodeSpawnContext& Context);

	static UK2Node* InvokeSelectedSpawner(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		const FVector2D& Location,
		FString& OutError);

	static UK2Node* InvokeSelectedSpawner(
		UEdGraph* TargetGraph,
		const FBlueprintHelperActionResolutionResult& ActionResult,
		const FVector2D& Location,
		const FBlueprintHelperActionNodeSpawnOptions& Options,
		FString& OutError,
		TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics = nullptr);
};
