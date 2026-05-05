#include "NodeHandlers/GraphWrite/BlueprintNodeHandler.h"
#include "GraphWrite/TextToBlueprintGenerator.h"

FBlueprintNodeHandlerRegistry& FBlueprintNodeHandlerRegistry::Get()
{
	static FBlueprintNodeHandlerRegistry Instance;
	return Instance;
}

void FBlueprintNodeHandlerRegistry::Register(TSharedRef<IBlueprintNodeHandler> Handler)
{
	Handlers.Add(Handler);
}

IBlueprintNodeHandler* FBlueprintNodeHandlerRegistry::FindHandler(EParsedBlueprintNodeType NodeType) const
{
	for (int32 Index = Handlers.Num() - 1; Index >= 0; --Index)
	{
		if (Handlers[Index]->CanHandle(NodeType))
		{
			return &Handlers[Index].Get();
		}
	}

	return nullptr;
}

void FBlueprintNodeHandlerRegistry::Reset()
{
	Handlers.Empty();
}
