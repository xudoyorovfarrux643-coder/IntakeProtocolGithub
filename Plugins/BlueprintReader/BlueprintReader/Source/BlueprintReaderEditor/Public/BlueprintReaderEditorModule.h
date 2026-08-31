#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Delegates/IDelegateInstance.h"

struct FAssetData;
class FExtender;

class FBlueprintReaderEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	void ExportSelectedBlueprints(TArray<FAssetData> SelectedAssets);

	FDelegateHandle ContentBrowserExtenderDelegateHandle;
};
