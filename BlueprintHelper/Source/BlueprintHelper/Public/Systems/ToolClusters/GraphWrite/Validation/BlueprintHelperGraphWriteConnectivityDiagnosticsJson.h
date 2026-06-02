#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/BlueprintGraphWriteResultTypes.h"

class FJsonObject;

class BLUEPRINTHELPER_API FBlueprintHelperGraphWriteConnectivityDiagnosticsJson
{
public:
	static void Attach(
		const TSharedPtr<FJsonObject>& Data,
		const TArray<FBlueprintGeneratorDiagnostic>& Diagnostics);
};
