#pragma once

#include "CoreMinimal.h"
#include "BlueprintNodeBinder.h"

class UEdGraph;
class UBlueprintNodeSpawner;
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
	using FNodeConfigurationHook = TFunction<bool(UK2Node& SpawnedNode, const FBlueprintHelperActionNodeSpawnContext& Context, FString& OutError)>;

	FString NodeId;
	IBlueprintNodeBinder::FBindingSet Bindings;
	TMap<FString, FString> DefaultValues;
	bool bApplyDefaultValues = true;
	bool bReconstructAfterSpawn = true;
	FNodeConfigurationHook NodeConfigurationHook;
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

	static UK2Node* InvokeNodeSpawner(
		UEdGraph* TargetGraph,
		UBlueprintNodeSpawner* NodeSpawner,
		const FString& StableId,
		const FVector2D& Location,
		const FBlueprintHelperActionNodeSpawnOptions& Options,
		FString& OutError,
		TArray<FBlueprintGeneratorDiagnostic>* OutDefaultValueDiagnostics = nullptr);
};
