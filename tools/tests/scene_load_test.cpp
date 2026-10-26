#include "scene/SceneLoadSystem.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

struct Lifetime {
    int& count;
    explicit Lifetime(int& value) : count(value) { ++count; }
    ~Lifetime() { --count; }
};
InitializationTask Child(int& live, std::vector<int>& order, bool fail) {
    Lifetime lifetime(live);
    for (int i=0;i<200;++i) { order.push_back(i); co_await std::suspend_always{}; }
    if (fail) throw std::runtime_error("test failure");
    co_return true;
}
InitializationTask Parent(int& live, std::vector<int>& order, bool fail) {
    Lifetime lifetime(live);
    if (!co_await Child(live,order,fail)) co_return false;
    order.push_back(200);
    co_await std::suspend_always{};
    co_return true;
}
InitializationTask Reject() { co_return false; }
InitializationTask EmptyChild() { co_return co_await InitializationTask{}; }
int main() {
    static_assert(!std::is_copy_constructible_v<InitializationTask>);
    static_assert(std::is_nothrow_move_constructible_v<InitializationTask>);
    int live=0;
    std::vector<int> order;
    for (int stop=0;stop<208;++stop) {
        auto task=Parent(live,order,false);
        for(int i=0;i<stop;++i) task.ResumeOne();
        auto moved=std::move(task);
        assert(task.Done());
        moved.Reset();
        assert(live==0);
        order.clear();
    }
    assert(Parent(live,order,false).Complete());
    assert(order.size()==201 && live==0);
    for(int i=0;i<=200;++i) assert(order[static_cast<std::size_t>(i)]==i);
    for(bool fail:{false,true}) {
        order.clear();
        scene::SceneLoadSystem loader;
        assert(loader.Begin(Parent(live,order,fail)));
        assert(!loader.Begin(Reject()));
        assert(loader.BlocksScene());
        int frames=0;
        while(loader.GetStatus()==scene::SceneLoadSystem::Status::Loading && frames++<1000) {
            const auto before=loader.GetMetrics().steps;
            loader.Advance();
            assert(loader.GetMetrics().steps-before<=scene::SceneLoadSystem::kMaximumStepsPerFrame);
        }
        assert(frames>=4 && frames<1000 && live==0);
        const auto& metrics=loader.GetMetrics();
        assert(metrics.elapsedMs>=metrics.workMs && metrics.longestFrameWorkMs>=metrics.longestStepMs);
        std::size_t profiledSteps=0;
        double profiledMs=0;
        bool sawChild=false, sawParent=false;
        for (const auto& function : metrics.functions) {
            profiledSteps+=function.steps;
            profiledMs+=function.milliseconds;
            sawChild |= function.name.find("Child(") != std::string_view::npos;
            sawParent |= function.name.find("Parent(") != std::string_view::npos;
        }
        assert(sawChild && sawParent && profiledSteps==metrics.steps);
        assert(std::abs(profiledMs-metrics.workMs)<.0001);
        std::ostringstream report;
        loader.WriteReport(report);
        assert(report.str().find("elapsedMs=")!=std::string::npos);
        assert(report.str().find("Child(")!=std::string::npos);
        assert(metrics.slowest.front().milliseconds==metrics.longestStepMs);
        for(std::size_t i=0;i<metrics.slowest.size();++i) {
            assert(metrics.slowest[i].index<=metrics.steps);
            if(metrics.slowest[i].index) assert(metrics.slowest[i].location.line()!=0);
            if(i) assert(metrics.slowest[i-1].milliseconds>=metrics.slowest[i].milliseconds);
        }
        assert(loader.GetStatus()==(fail ? scene::SceneLoadSystem::Status::Failed : scene::SceneLoadSystem::Status::Ready));
        assert(loader.BlocksScene()==fail);
        loader.Cancel();
        assert(!loader.BlocksScene());
    }
    scene::SceneLoadSystem loader;
    assert(loader.Begin(Parent(live,order,false)));
    loader.Advance(); loader.Cancel(); assert(live==0);
    assert(loader.Begin(Reject())); loader.Advance();
    assert(loader.GetStatus()==scene::SceneLoadSystem::Status::Failed);
    assert(loader.Begin({})); loader.Advance();
    assert(loader.GetStatus()==scene::SceneLoadSystem::Status::Failed);
    assert(loader.Begin(EmptyChild())); loader.Advance();
    assert(loader.GetStatus()==scene::SceneLoadSystem::Status::Failed);
    std::cout << "Scene loading: 208 cancellation points, nested ownership, move, failure, frame cap and ordering PASS\n";
}
