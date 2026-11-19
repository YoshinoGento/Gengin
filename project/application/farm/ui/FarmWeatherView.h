#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmWeatherSystem.h"
#include "farm/system/FarmGrowthSystem.h"

namespace farmui {
inline void BuildWeatherView(View& view, FarmWeatherSystem::View weather,
    FarmRainIntakeAdvice advice = FarmRainIntakeAdvice::None) {
    using P = FarmWeatherSystem::Phase;
    if (weather.phase == P::Disabled) return;
    const Label label = weather.phase == P::ForecastRain ? Label::WeatherForecast :
        weather.phase != P::Rain ? Label::WeatherClear :
        advice == FarmRainIntakeAdvice::CanClose ? Label::RainCanClose :
        advice == FarmRainIntakeAdvice::RainOnly ? Label::RainOnly : Label::WeatherRain;
    view.Add(label, {354, 86, 630, 38});
}
}
