// GraphWrite AutoSearch ActionDatabase pure-data snapshot service.

#pragma once

#include "CoreMinimal.h"
#include "Systems/ToolClusters/GraphWrite/AutoSearch/BlueprintHelperGraphWriteAutoSearchTypes.h"

class FBlueprintHelperActionDatabaseSnapshotService
{
public:
	FBlueprintHelperActionDatabaseSnapshot BuildSnapshotOnGameThread();
	void MarkDirty();
	bool IsDirty() const;

private:
	bool bDirty = true;
};
