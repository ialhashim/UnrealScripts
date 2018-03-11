// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CustomRender.h"
#include "CustomRenderStyle.h"
#include "CustomRenderCommands.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "LevelEditor.h"

SWindow * pwindow = nullptr;
TArray<AActor*> selectedActors;
float lastTime = 0.0f;

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

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <EngineUtils.h>
#include <Editor.h>
#include <ObjectTools.h>
#include <AssetDeleteModel.h>

#include <Runtime/AssetRegistry/Public/AssetRegistryModule.h>
#include <Runtime/Slate/Public/Framework/Application/SlateApplication.h>
#include <Runtime/Slate/Public/Widgets/Layout/SScrollBox.h>
#include <Runtime/Engine/Classes/Engine/Selection.h>
#include <Runtime/Engine/Classes/Engine/StaticMeshActor.h>
#include <Runtime/CinematicCamera/Public/CineCameraActor.h>
#include <Runtime/CinematicCamera/Public/CineCameraComponent.h>
#include <Runtime/LevelSequence/Public/LevelSequence.h>
#include <Widgets/Input/SSpinBox.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/Input/SCheckBox.h>

#include <Developer/AssetTools/Public/IAssetTools.h>
#include <Developer/AssetTools/Public/AssetToolsModule.h>
#include <Private/LevelSequenceEditorToolkit.h>
#include <Editor/Sequencer/Public/ISequencer.h>
#include <Editor/UnrealEd/Public/LevelEditorViewport.h>
#include <Tracks/MovieSceneCameraCutTrack.h>
#include <Runtime/MovieScene/Public/MovieScene.h>
#include <Runtime/MovieScene/Public/MovieSceneSection.h>
#include <Runtime/MovieSceneTracks/Public/Sections/MovieSceneCameraCutSection.h>
#include <Runtime/MovieSceneTracks/Public/Tracks/MovieScene3DTransformTrack.h>

void allChildWidgets(std::vector<TSharedRef<SWidget>> & result, TSharedRef<SWidget> parent) {
	for (int i = 0; i < parent->GetChildren()->Num(); i++) {
		auto child = parent->GetChildren()->GetChildAt(i);
		result.push_back(child);
		allChildWidgets(result, child);
	}
}

TArray<AActor*> getSelectedActors() {
	TArray<AActor*> actorsArray;
	USelection * selection = GEditor->GetSelectedActors();
	selection->GetSelectedObjects<AActor>(actorsArray);
	return actorsArray;
}

void FCustomRenderModule::PluginButtonClicked()
{
	selectedActors = getSelectedActors();

	if (selectedActors.Num() == 0) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("No objects selected."));
		return;
	}

	TSharedRef<SScrollBox> ParentBox = SNew(SScrollBox);
	ParentBox->AddSlot()
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot().Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[SNew(STextBlock).Text(FText::FromString("Global Camera Settings"))]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Aperture:"))]
		+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("Aperture").MinValue(0.7f).MaxValue(32.0f).Value(1.8f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Focal length:"))]
		+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("FocalLength").MinValue(0.1f).MaxValue(300.0f).Value(4.0f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Camera Height:"))]
		+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("CameraHeight").MinValue(-1000.0f).MaxValue(1000.0f).Value(150.0f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Radius Multiplier:"))]
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("RadiusMultiplier").MinValue(0).MaxValue(20.0f).Value(3.0f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Start Angle:"))]
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("StartAngle").MinValue(-360.0f).MaxValue(720.0f).Value(0.0f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("End Angle:"))]
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("EndAngle").MinValue(-360.0f).MaxValue(720.0f).Value(180.0f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("Lookat Height Adjust:"))]
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("LookatHeightAdjust").MinValue(-20.0f).MaxValue(20.0f).Value(0.5f)]
		]
		+ SScrollBox::Slot().Padding(5)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(STextBlock).Text(FText::FromString("FPS:"))]
			+ SHorizontalBox::Slot().FillWidth(0.5f)[SNew(SSpinBox<float>).Tag("FPS").MinValue(1.0f).MaxValue(512.0f).Value(30.0f)]
		]
		+ SScrollBox::Slot().Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.5f).HAlign(HAlign_Right).VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text(FText::FromString("Create Sequence..."))
					.OnClicked_Lambda([this]()
				{
					this->CreateSequence();
					return FReply::Handled();
				})
			]
		]
		];

	ParentBox->AddSlot()
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot().Padding(10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(FText::FromString("Per Object Settings"))]
		]
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString(" "))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.4f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("Object"))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("CH"))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("R"))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("LH"))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("SA"))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f).HAlign(HAlign_Center)[SNew(STextBlock).Text(FText::FromString("EA"))]
		]
	];

	for (auto actor : selectedActors)
	{
		auto actorLabelString = actor->GetActorLabel();
		auto actorLabelText = FText::FromString(actorLabelString);
		
		auto actorCheck = FString(actorLabelString + "-isEnabled");
		auto actorCH = FString(actorLabelString + "-CH");
		auto actorR = FString(actorLabelString + "-R");
		auto actorLH = FString(actorLabelString + "-LH");
		auto actorSA = FString(actorLabelString + "-SA");
		auto actorEA = FString(actorLabelString + "-EA");
		
		ParentBox->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SCheckBox).Tag(FName(*actorCheck))]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.4f)[SNew(STextBlock).Text(actorLabelText)]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SSpinBox<float>).Tag(FName(*actorCH)).Value(0).MinValue(-1000).MaxValue(1000)]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SSpinBox<float>).Tag(FName(*actorR)).Value(1.0f).MinValue(0.0f).MaxValue(20.0f)]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SSpinBox<float>).Tag(FName(*actorLH)).Value(1.0f).MinValue(-20).MaxValue(20)]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SSpinBox<float>).Tag(FName(*actorSA)).Value(0.0f).MinValue(-360.0f).MaxValue(720)]
			+ SHorizontalBox::Slot().Padding(2).FillWidth(0.1f)[SNew(SSpinBox<float>).Tag(FName(*actorEA)).Value(180.0f).MinValue(-360.0f).MaxValue(720)]
		];
	}

	// Settings window:
	auto Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("Custom Render")))
		.ClientSize(FVector2D(450, 620))
		.SupportsMaximize(true)
		.SupportsMinimize(false)
		.Content()[ParentBox];

	auto window = FSlateApplication::Get().AddWindowAsNativeChild(Window, FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(), true);
	pwindow = &window.Get();
}

void FCustomRenderModule::CreateSequence()
{
	auto world = GEditor->GetEditorWorldContext().World();

	std::vector<TSharedRef<SWidget>> childwidgets;
	allChildWidgets(childwidgets, pwindow->GetChildren()->GetChildAt(0));

	std::map<std::string, float> settings;

	for (auto child : childwidgets) 
	{
		// Spin boxes:
		if (child->GetTypeAsString().Equals("SSpinBox<float>")){
			auto spinbox = (SSpinBox<float>*)(&child.Get());
			settings[std::string(TCHAR_TO_UTF8(*spinbox->GetTag().ToString()))] = spinbox->GetValue();
		}

		// Check boxes:
		if (child->GetTypeAsString().Equals("SCheckBox")){
			auto checkbox = (SCheckBox*)(&child.Get());
			settings[std::string(TCHAR_TO_UTF8(*checkbox->GetTag().ToString()))] = checkbox->IsChecked();
		}
	}

	// DEBUG:
	bool isShowLog = false;
	if (isShowLog) {
		std::string log;
		for (auto kv : settings) { log += kv.first + " = " + std::to_string(kv.second) + "\n"; }
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString(log.c_str())));
	}

	auto CleanupPreviousSequence = [=]() {

		TArray<UObject *> objects;

		// Clean up past sequences
		{
			// Get sequencer
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
			FString AssetPath = "/Game/Cinematics/Sequences";
			FString AssetName = "Master"; AssetPath /= AssetName; AssetPath += "." + AssetName;
			FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

			if (AssetData.IsValid())
			{
				auto MasterSequenceAsset = AssetData.GetAsset();
				IAssetEditorInstance* AssetEditor = FAssetEditorManager::Get().FindEditorForAsset(MasterSequenceAsset, true);
				FLevelSequenceEditorToolkit* LevelSequenceEditor = (FLevelSequenceEditorToolkit*)AssetEditor;
				lastTime = LevelSequenceEditor->GetSequencer().Get()->GetGlobalTime();

				objects.Add(MasterSequenceAsset);
			}
		}

		ObjectTools::ForceDeleteObjects(objects, false);

		// Clean up past cameras
		for (auto actor : selectedActors)
		{
			auto actorLabel = actor->GetActorLabel();
			auto cameraLabel = actorLabel;

			for (TActorIterator<ACineCameraActor> ActorItr(world); ActorItr; ++ActorItr) {
				if (ActorItr->GetActorLabel().Equals(cameraLabel)){
					world->DestroyActor(*ActorItr);
				}
			}
		}

		return objects.Num() > 0;
	};

	auto CreateSequence = [=](std::map<std::string,float> & settings) {
		std::vector<ACineCameraActor*> allcams;
		std::vector<FVector> origins, boxes;
		std::vector<std::string> names;

		// Generate a camera for each object in the selection
		for (auto actor : selectedActors)
		{
			auto actorLabel = actor->GetActorLabel();

			// Actor properties
			FVector origin, box;
			actor->GetActorBounds(false, origin, box);

			// Keep records
			origins.push_back(origin);
			boxes.push_back(box);
			names.push_back(TCHAR_TO_UTF8(*actorLabel));

			// Per object settings
			bool isCustom = settings[std::string(TCHAR_TO_UTF8(*actorLabel)) + "-isEnabled"] != 0.0f;
			float MyLookatHeightAdjust = settings[std::string(TCHAR_TO_UTF8(*actorLabel)) + "-LH"];

			// Position camera:
			FActorSpawnParameters CamSpawnInfo;
			FRotator CamRotation(0, 0, 0);
			FVector CamPos = actor->GetActorLocation();

			// Create camera
			auto camera = world->SpawnActor<ACineCameraActor>(CamPos, CamRotation, CamSpawnInfo);
			camera->SetActorLabel(actorLabel);

			// Camera settings
			auto camSettings = camera->GetCineCameraComponent();

			// Fixed values for: Sony IMX258 sensor
			camSettings->FilmbackSettings.SensorWidth = 4.69469;
			camSettings->FilmbackSettings.SensorHeight = 3.518753;

			camSettings->LensSettings.MinFocalLength = settings["FocalLength"];
			camSettings->LensSettings.MaxFocalLength = settings["FocalLength"];
			camSettings->LensSettings.MinFStop = settings["Aperture"];
			camSettings->LensSettings.MaxFStop = settings["Aperture"];

			camSettings->CurrentFocalLength = settings["FocalLength"];
			camSettings->CurrentAperture = settings["Aperture"];

			// Look at property
			camera->LookatTrackingSettings.ActorToTrack = actor;
			camera->LookatTrackingSettings.RelativeOffset = FVector(0, 0, box.Z * (isCustom ? MyLookatHeightAdjust * settings["LookatHeightAdjust"] : settings["LookatHeightAdjust"]));
			camera->LookatTrackingSettings.bEnableLookAtTracking = true;
			camera->LookatTrackingSettings.bDrawDebugLookAtTrackingPosition = true;

			allcams.push_back(camera);
		}

		// Create a master sequence
		auto CreateMasterSequence = [=]() {
			FString MasterSequenceAssetName = TEXT("Master");
			FString MasterSequencePackagePath = TEXT("/Game/Cinematics/Sequences");

			auto AssetName = MasterSequenceAssetName;
			auto PackagePath = MasterSequencePackagePath;

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
		float deltaTime = 1.0; // seconds

		// Animation key interpolation type
		EMovieSceneKeyInterpolation KeyInterpolation = EMovieSceneKeyInterpolation::Auto;
		bool unwind = false;

		for (size_t i = 0; i < allcams.size(); i++)
		{
			bool isCustom = settings[names[i] + "-isEnabled"] != 0.0f;

			auto & camera = allcams[i];
			auto & box = boxes[i];
			auto & origin = origins[i];

			// Get camera FGuid
			UMovieSceneCameraCutSection *new_section = (UMovieSceneCameraCutSection*)CameraCutTrack->CreateNewSection();
			FGuid CameraGuid = scene->AddPossessable(camera->GetActorLabel(), camera->GetClass());
			seq->BindPossessableObject(CameraGuid, *camera, camera->GetWorld());

			// Bleed into start and end to avoid jarring frames at start or end
			float delta = 1.0, deltaStart = 0, deltaEnd = 0;
			if (i == 0) deltaStart = -delta;
			else if (i == allcams.size() - 1) deltaEnd = delta;

			// Create camera cut section
			CameraCutTrack->Modify();
			auto CamCutNewSection = Cast<UMovieSceneCameraCutSection>(CameraCutTrack->CreateNewSection());
			CamCutNewSection->SetStartTime(startTime + deltaStart);
			CamCutNewSection->SetEndTime(startTime + deltaTime + deltaEnd);
			CamCutNewSection->SetCameraGuid(CameraGuid);
			CameraCutTrack->AddSection(*CamCutNewSection);

			// Create camera motion section
			auto CamMoveTrack = scene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), CameraGuid);
			auto CamMoveSection = Cast<UMovieScene3DTransformSection>(CamMoveTrack->CreateNewSection());
			CamMoveSection->SetStartTime(startTime);
			CamMoveSection->SetEndTime(startTime + deltaTime);
			CamMoveTrack->AddSection(*CamMoveSection);

			// Add camera flying animation
			double timeStepSize = 1.0 / settings["FPS"];
			double camRadius = std::max(box.X, box.Y) * (isCustom ? settings[names[i] + "-R"] * settings["RadiusMultiplier"] : settings["RadiusMultiplier"]);
			FVector Zaxis(0, 0, 1.0);

			// Fly around range
			double startAngle = settings["StartAngle"];
			double endAngle = settings["EndAngle"];

			if (isCustom) {
				startAngle = settings[names[i] + "-SA"];
				endAngle = settings[names[i] + "-EA"];
			}

			double rangeAngle = endAngle - startAngle;

			for (double time = 0; time <= 1.0; time += timeStepSize)
			{
				double theta = startAngle + (time * rangeAngle);

				FVector pos = FVector(origin.X, origin.Y, 0) + FVector(0, 0, (isCustom ? settings[names[i] + "-CH"] + settings["CameraHeight"] : settings["CameraHeight"])) + FVector(camRadius, 0, 0).RotateAngleAxis(theta, Zaxis);

				FTransformKey tx = FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, pos.X, unwind);
				FTransformKey ty = FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, pos.Y, unwind);
				FTransformKey tz = FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, pos.Z, unwind);
				CamMoveSection->AddKey(startTime + time, tx, KeyInterpolation);
				CamMoveSection->AddKey(startTime + time, ty, KeyInterpolation);
				CamMoveSection->AddKey(startTime + time, tz, KeyInterpolation);

				// Set initial camera position for better preview
				if (time == 0) camera->SetActorLocation(pos);
			}

			startTime += deltaTime;
		}

		// Show in viewport
		{
			for (int32 i = 0; i < GEditor->LevelViewportClients.Num(); ++i){
				FLevelEditorViewportClient* LevelVC = GEditor->LevelViewportClients[i];
				if (LevelVC && LevelVC->IsPerspective() && LevelVC->AllowsCinematicPreview() && LevelVC->GetViewMode() != VMI_Unknown){
					LevelVC->SetActorLock(nullptr);
					LevelVC->bLockedCameraView = false;
					LevelVC->UpdateViewForLockedActor();
					LevelVC->Invalidate();
				}
			}
			Sequencer->SetViewRange(TRange<float>(0, startTime + deltaTime), EViewRangeInterpolation::Immediate);
			Sequencer->SetPerspectiveViewportCameraCutEnabled(true);
			Sequencer->ForceEvaluate();
			Sequencer->SetGlobalTime(lastTime);
		}
	};

	CleanupPreviousSequence();
	CreateSequence( settings );

	//FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("All done"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCustomRenderModule, CustomRender)