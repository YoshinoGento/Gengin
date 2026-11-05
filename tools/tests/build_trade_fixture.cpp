#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDocumentSystem.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

// UI comparison only: these records are not evidence of played cultivation.
int main() {
    const std::filesystem::path root="generated/ui_trade_20261102/fixture";
    if (std::filesystem::exists(root)) throw std::runtime_error("Never overwrite a recorded fixture");
    const auto check=[](bool ok) { if (!ok) throw std::runtime_error("Invalid trade fixture"); };
    farm::FarmGrid grid; check(grid.Initialize(5,4));
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::FreeFarming);
    FarmDocumentSystem docs; check(docs.Initialize(root.string(),grid,economy,selection,&date,&progression));
    check(date.RestoreSnapshot({9,0,1}));
    for (int i=0;i<farm::kPlayableCropCount;++i) {
        FarmCropQualityResult quality;
        quality.crop=farm::PlayableCrop(i); quality.score=70+i*8;
        quality.basePrice=100; quality.salePrice=130+i*20;
        quality.maturity=1; quality.waterBalance=.8f; quality.terrainFit=.8f;
        quality.nutrientBalance=.8f; quality.nutrientKnown=true; quality.harvestSize={1.25f,true};
        check(economy.AddHarvest(quality,2+i,8));
        check(economy.AddSeed(quality.crop,5));
    }
    // A separate carrot record demonstrates that protected produce remains after sale.
    auto protectedQuality=economy.GetHarvestRecord(0)->quality;
    protectedQuality.score=95; protectedQuality.salePrice=180;
    check(economy.AddHarvest(protectedQuality,1,9));
    check(economy.SetHarvestProtection(economy.GetHarvestRecord(3)->id,true,economy.GetInventoryGeneration()));
    check(docs.SaveAs("Trade UI comparison",grid,economy,selection));
    check(docs.Load(docs.GetActiveDocumentId(),grid,economy,selection));
    check(economy.GetProtectedCropCount()==1 && economy.PreviewSale().soldCount==9);
    std::cout << "Synthetic UI fixture saved and reloaded: " << root << '\n';
}
