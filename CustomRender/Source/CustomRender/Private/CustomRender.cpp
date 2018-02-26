// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CustomRender.h"
#include "CustomRenderStyle.h"
#include "CustomRenderCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"

static const FName CustomRenderTabName("CustomRender");

#define LOCTEXT_NAMESPACE "FCustomRenderModule"

void FCustomRenderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FCustomRenderStyle::Initialize();
	FCustomRenderStyle::ReloadTextures();

	FCustomRenderCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FCustomRenderCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FCustomRenderModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	// Menu button
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", 
			EExtensionHook::After, 
			PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FCustomRenderModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	// Tool bar button
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", 
			EExtensionHook::After, 
			PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FCustomRenderModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
}

void FCustomRenderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FCustomRenderStyle::Shutdown();

	FCustomRenderCommands::Unregister();
}

void FCustomRenderModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FCustomRenderCommands::Get().PluginAction);
}

void FCustomRenderModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FCustomRenderCommands::Get().PluginAction);
}

#include <vector>
#include <algorithm>
#include <EngineUtils.h>
#include <Editor.h>
#include <Runtime/Engine/Classes/Engine/Selection.h>
#include <Runtime/Engine/Classes/Engine/StaticMeshActor.h>
#include <Runtime/CinematicCamera/Public/CineCameraActor.h>
#include <Runtime/CinematicCamera/Public/CineCameraComponent.h>
#include <Runtime/LevelSequence/Public/LevelSequence.h>

#include <Developer/AssetTools/Public/IAssetTools.h>
#include <Developer/AssetTools/Public/AssetToolsModule.h>
#include <Private/LevelSequenceEditorToolkit.h>
#include <Editor/Sequencer/Public/ISequencer.h>
#include <Tracks/MovieSceneCameraCutTrack.h>
#include <Runtime/MovieScene/Public/MovieScene.h>
#include <Runtime/MovieScene/Public/MovieSceneSection.h>
#include <Runtime/MovieSceneTracks/Public/Sections/MovieSceneCameraCutSection.h>
#include <Runtime/MovieSceneTracks/Public/Tracks/MovieScene3DTransformTrack.h>

/*
#include <Runtime/AssetRegistry/Public/AssetRegistryModule.h>
#include <Runtime/LevelSequence/Public/LevelSequence.h>
#include <Editor/Sequencer/Public/ISequencerModule.h>
#include <Editor/UnrealEd/Public/Toolkits/AssetEditorManager.h>
*/

void FCustomRenderModule::PluginButtonClicked()
{
	std::vector<ACineCameraActor*> allcams;
	std::vector<FVector> origins, boxes;

	auto world = GEditor->GetEditorWorldContext().World();

	// Get the set of selected objects
	USelection * selection = GEditor->GetSelectedActors();
	TArray<AStaticMeshActor*> selectedActors;
	selection->GetSelectedObjects<AStaticMeshActor>(selectedActors);

	if (selectedActors.Num() == 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("No objects selected."));
		return;
	}

	// Generate a camera for each object in the selection
	for (auto actor : selectedActors)
	{
		auto actorLabel = actor->GetActorLabel();

		// Actor properties
		FVector origin, box;
		actor->GetActorBounds(false, origin, box);

		// Keep record
		origins.push_back(origin);
		boxes.push_back(box);

		// Position camera:
		FActorSpawnParameters CamSpawnInfo;
		FRotator CamRotation(0.0f, 0.0f, 0.0f);
		FVector CamPos = actor->GetActorLocation();

		// Create camera
		auto camera = world->SpawnActor<ACineCameraActor>(CamPos, CamRotation, CamSpawnInfo);
		camera->SetActorLabel(actorLabel + "_cam");
		
		// Camera settings
		auto camSettings = camera->GetCineCameraComponent();
		camSettings->FilmbackSettings.SensorWidth = 4.69469;
		camSettings->FilmbackSettings.SensorHeight = 3.518753;

		camSettings->LensSettings.MinFocalLength = 4.0;
		camSettings->LensSettings.MaxFocalLength = 4.0;
		camSettings->LensSettings.MinFStop = 1.8;
		camSettings->LensSettings.MaxFStop = 1.8;

		camSettings->CurrentFocalLength = 4.0;
		camSettings->CurrentAperture = 1.8;

		// Look at property
		camera->LookatTrackingSettings.ActorToTrack = actor;
		camera->LookatTrackingSettings.RelativeOffset = FVector(0, 0, box.Z * 0.5);
		camera->LookatTrackingSettings.bEnableLookAtTracking = true;
		camera->LookatTrackingSettings.bDrawDebugLookAtTrackingPosition = true;

		allcams.push_back(camera);
	}

	// Create a master sequence
	auto CreateMasterSequence = [](){
		FString MasterSequenceAssetName = TEXT("Sequence");
		FString MasterSequencePackagePath = TEXT("/Game/Cinematics/Sequences");
		MasterSequencePackagePath /= MasterSequenceAssetName;
		MasterSequenceAssetName += TEXT("Master");
		auto CreateLevelSequenceAsset = [](auto AssetName, auto PackagePath) {
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			UObject* NewAsset = nullptr;
			for (TObjectIterator<UClass> It; It; ++It) {
				UClass* CurrentClass = *It;
				if (CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract))) {
					UFactory* Factory = Cast<UFactory>(CurrentClass->GetDefaultObject());
					if (Factory->CanCreateNew() && Factory->ImportPriority >= 0 && Factory->SupportedClass == ULevelSequence::StaticClass()) {
						NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, ULevelSequence::StaticClass(), Factory);
						break;
					}
				}
			}
			return NewAsset;
		};

		return CreateLevelSequenceAsset(MasterSequenceAssetName, MasterSequencePackagePath);
	};

	// Create master sequence
	UObject* MasterSequenceAsset = CreateMasterSequence();
	FAssetEditorManager::Get().OpenEditorForAsset(MasterSequenceAsset);
	IAssetEditorInstance* AssetEditor = FAssetEditorManager::Get().FindEditorForAsset(MasterSequenceAsset, true);
	FLevelSequenceEditorToolkit* LevelSequenceEditor = (FLevelSequenceEditorToolkit*)AssetEditor;
	ISequencer* Sequencer = LevelSequenceEditor->GetSequencer().Get();
	auto seq = Sequencer->GetRootMovieSceneSequence();
	auto scene = seq->GetMovieScene();

	// One second for each camera
	scene->SetPlaybackRange(0, allcams.size());

	// Create camera cut track
	UMovieSceneCameraCutTrack *CameraCutTrack = (UMovieSceneCameraCutTrack*)scene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass());

	// Create camera cut sections
	float startTime = 0;
	float deltaTime = 1.0;

	// Animation key interpolation type
	EMovieSceneKeyInterpolation KeyInterpolation = EMovieSceneKeyInterpolation::Auto;
	bool unwind = false;

	for (size_t i = 0; i < allcams.size(); i++)
	{
		auto & camera = allcams[i];
		auto & box = boxes[i];
		auto & origin = origins[i];

		// Get camera FGuid
		UMovieSceneCameraCutSection *new_section = (UMovieSceneCameraCutSection*)CameraCutTrack->CreateNewSection();		
		FGuid CameraGuid = scene->AddPossessable(camera->GetActorLabel(), camera->GetClass());
		seq->BindPossessableObject(CameraGuid, *camera, camera->GetWorld());

		// Create camera cut track
		CameraCutTrack->Modify();
		auto CamCutNewSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
		CamCutNewSection->SetStartTime(startTime);
		CamCutNewSection->SetEndTime(startTime + deltaTime);
		CamCutNewSection->SetCameraGuid(CameraGuid);
		CameraCutTrack->AddSection(*CamCutNewSection);

		// Create camera motion track
		auto CamMoveTrack = scene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), CameraGuid);
		auto CamMoveSection = Cast<UMovieScene3DTransformSection>(CamMoveTrack->CreateNewSection());
		CamMoveSection->SetStartTime(startTime);
		CamMoveSection->SetEndTime(startTime + deltaTime);
		CamMoveTrack->AddSection(*CamMoveSection);

		// Add camera flying animation
		double timeStepSize = 1.0 / 20.0;
		double camHeight = 150.0;
		double camRadius = std::max(box.X, box.Y) * 3.0;
		FVector Zaxis(0, 0, 1.0);

		for (double time = 0; time <= 1.0; time += timeStepSize) 
		{
			double theta = time * 360.0;

			FVector pos = origin + FVector(0, 0, camHeight) + FVector(camRadius, 0, 0).RotateAngleAxis(theta, Zaxis);

			FTransformKey tx = FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, pos.X, unwind);
			FTransformKey ty = FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, pos.Y, unwind);
			FTransformKey tz = FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, pos.Z, unwind);
			CamMoveSection->AddKey(startTime + time, tx, KeyInterpolation);
			CamMoveSection->AddKey(startTime + time, ty, KeyInterpolation);
			CamMoveSection->AddKey(startTime + time, tz, KeyInterpolation);
		}

		startTime += deltaTime;
	}

	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("All done"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomRenderModule, CustomRender)