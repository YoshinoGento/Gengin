#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmWeatherSystem.h"

namespace farmui {
inline void BuildWeatherView(View& view, FarmWeatherSystem::View weather) {
    using P = FarmWeatherSystem::Phase;
    if (weather.phase == P::Disabled) return;
    const Label label = weather.phase == P::ForecastRain ? Label::WeatherForecast :
        weather.phase == P::Rain ? Label::WeatherRain : Label::WeatherClear;
    view.Add(label, {354, 86, 630, 38});
}
}
