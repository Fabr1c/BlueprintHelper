#pragma once

#include "CoreMinimal.h"

struct FBlueprintHelperGraphFragmentDag;
struct FBlueprintHelperGraphSemanticIR;

class BLUEPRINTHELPER_API FBlueprintHelperGraphFragmentDagBuilder
{
public:
	static bool BuildFromSemanticIR(
		const FBlueprintHelperGraphSemanticIR& SemanticIR,
		FBlueprintHelperGraphFragmentDag& OutDag);
};
