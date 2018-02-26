// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "CustomRenderStyle.h"

class FCustomRenderCommands : public TCommands<FCustomRenderCommands>
{
public:

	FCustomRenderCommands()
		: TCommands<FCustomRenderCommands>(TEXT("CustomRender"), NSLOCTEXT("Contexts", "CustomRender", "CustomRender Plugin"), NAME_None, FCustomRenderStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};
