// BlueprintHelper Service Layer — 编辑器上下文查询服务实现

#include "Services/BlueprintHelperContextService.h"
#include "GraphSupport/BlueprintHelperGraphResolver.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"

FBlueprintHelperContextService::FBlueprintHelperContextService(const FBlueprintHelperGraphResolver& InResolver)
	: Resolver(InResolver)
{
}

FBlueprintHelperEditorContext FBlueprintHelperContextService::GetContext() const
{
	FBlueprintHelperEditorContext Ctx;

	UBlueprint* Blueprint = Resolver.GetFocusedBlueprint();
	if (!Blueprint)
	{
		return Ctx;
	}

	Ctx.ActiveBlueprintPath = Blueprint->GetPathName();
	Ctx.BlueprintDisplayName = Blueprint->GetName();
	Ctx.BlueprintStatus = static_cast<int32>(Blueprint->Status);
	Ctx.bIsCompiled = (Blueprint->Status == BS_UpToDate);

	UEdGraph* Graph = Resolver.GetFocusedGraph();
	if (Graph)
	{
		Ctx.ActiveGraphName = Graph->GetName();
		Ctx.NodeCount = Graph->Nodes.Num();
	}

	return Ctx;
}
