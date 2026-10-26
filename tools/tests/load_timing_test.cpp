#include "base/LoadTiming.h"
#include <iostream>
#include <stdexcept>
#include <thread>

void Require(bool value) { if (!value) throw std::runtime_error("load timing assertion"); }
int main() try {
    using namespace loading;
    { Scope scope(Stage::TextureDecode); }
    Require(counters[static_cast<std::size_t>(Stage::TextureDecode)].calls == 0);
    enabled = true;
    { Scope scope(Stage::TextureDecode); scope.Stop(); scope.Stop(); }
    const auto& measured = counters[static_cast<std::size_t>(Stage::TextureDecode)];
    Require(measured.calls == 1 && measured.totalMs >= 0 && measured.maximumMs == measured.totalMs);
    { Scope scope(Stage::Count); }
    { Scope scope(static_cast<Stage>(999)); }
    { Scope outer(Stage::ModelLookup); { Scope inner(Stage::ModelImport); } }
    Require(counters[static_cast<std::size_t>(Stage::ModelLookup)].calls == 1);
    Require(counters[static_cast<std::size_t>(Stage::ModelImport)].calls == 1);
    std::thread other([] { Require(!enabled); enabled = true; Scope scope(Stage::TextureDecode); });
    other.join();
    Require(measured.calls == 1);
    Reset();
    for (const auto& counter : counters) Require(counter.calls == 0 && counter.totalMs == 0 && counter.maximumMs == 0);
    enabled = false;
    std::cout << "PASS load timing: disabled, exactly-once, bounds, nested scopes, thread isolation, reset\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
