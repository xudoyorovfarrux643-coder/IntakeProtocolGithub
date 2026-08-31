#include "BlueprintReaderEditorModule.h"

#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Commands/UIAction.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "BPExportModel.h"
#include "BlueprintExtractor.h"
#include "BRJsonExporter.h"
#include "BRMarkdownExporter.h"

#define LOCTEXT_NAMESPACE "BlueprintReader"

void FBlueprintReaderEditorModule::StartupModule()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserMenuExtender_SelectedAssets>& MenuExtenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	MenuExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(this, &FBlueprintReaderEditorModule::OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
}

void FBlueprintReaderEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded(TEXT("ContentBrowser")))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders().RemoveAll(
			[this](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
			{
				return Delegate.GetHandle() == ContentBrowserExtenderDelegateHandle;
			});
	}
}

TSharedRef<FExtender> FBlueprintReaderEditorModule::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	TArray<FAssetData> BlueprintAssets;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.IsInstanceOf(UBlueprint::StaticClass(), EResolveClass::Yes))
		{
			BlueprintAssets.Add(Asset);
		}
	}

	if (BlueprintAssets.Num() == 0)
	{
		return Extender;
	}

	Extender->AddMenuExtension(
		"CommonAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([this, BlueprintAssets](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ExportForAI", "Export for AI Review (BlueprintReader)"),
				LOCTEXT("ExportForAITooltip", "Export selected Blueprint(s) logic as JSON + Markdown for LLM code review, into Saved/BlueprintReaderExport/"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FBlueprintReaderEditorModule::ExportSelectedBlueprints, BlueprintAssets)));
		}));

	return Extender;
}

void FBlueprintReaderEditorModule::ExportSelectedBlueprints(TArray<FAssetData> SelectedAssets)
{
	const FString OutDir = FPaths::ProjectSavedDir() / TEXT("BlueprintReaderExport");
	IFileManager::Get().MakeDirectory(*OutDir, true);

	TArray<FBRExportModel> Models;
	for (const FAssetData& AssetData : SelectedAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
		if (!Blueprint)
		{
			continue;
		}

		FBRExportModel Model = FBlueprintExtractor::Extract(Blueprint);

		FBRJsonExporter::ExportToFile(Model, OutDir / (Blueprint->GetName() + TEXT(".bp.json")));
		FBRMarkdownExporter::ExportToFile(Model, OutDir / (Blueprint->GetName() + TEXT(".bp.md")));

		Models.Add(MoveTemp(Model));
	}

	if (Models.Num() > 1)
	{
		FBRJsonExporter::ExportProjectSummaryToFile(Models, OutDir / TEXT("ProjectBlueprintGraph.json"));
		FBRMarkdownExporter::ExportProjectSummaryToFile(Models, OutDir / TEXT("ProjectBlueprintGraph.md"));
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("ExportComplete", "BlueprintReader: exported {0} Blueprint(s) to {1}"),
		FText::AsNumber(Models.Num()),
		FText::FromString(OutDir)));
	Info.ExpireDuration = 6.0f;
	FSlateNotificationManager::Get().AddNotification(Info);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBlueprintReaderEditorModule, BlueprintReaderEditor)
