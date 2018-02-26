// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CustomRenderCommands.h"

#define LOCTEXT_NAMESPACE "FCustomRenderModule"

void FCustomRenderCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "CustomRender", "Execute CustomRender action", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
