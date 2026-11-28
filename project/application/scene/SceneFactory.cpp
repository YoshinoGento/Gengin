#include "SceneFactory.h"
#include "TitleScene.h"
#include "CanalLabScene.h"
#include "WaterTrialScene.h"
#include <Windows.h>
#include <filesystem>
#include "GamePlayScene.h"

/**
 * シーン生成の実装
 */
std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName) {
	if (sceneName == "TITLE") {
        wchar_t trial[2]{};
        if(GetEnvironmentVariableW(L"SUIRO_WATER_TRIAL",trial,2)==1 && trial[0]==L'1')
            return std::make_unique<WaterTrialScene>();
		std::error_code error;
		if (std::filesystem::is_regular_file("Settings/runtime/first_person.flag",error))
			return std::make_unique<GamePlayScene>();
		if (std::filesystem::is_regular_file("Settings/runtime/canal_workshop.flag",error))
			return std::make_unique<CanalLabScene>();
		// make_unique を使うことで、生成と同時にスマートポインタの箱に入れます
		return std::make_unique<TitleScene>();
	} else if (sceneName == "GAMEPLAY") {
		return std::make_unique<GamePlayScene>();
	}

	return nullptr;
}
