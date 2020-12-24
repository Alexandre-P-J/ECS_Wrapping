#include <iostream>
#include <chrono>

#include <entt/entt.hpp>
#include <functional>
#include <limits>

#include "testing.h"

struct Test0
{
    int i = 9;
    int j = 111;
};

struct Test1
{
    int i = 7;
    int k = 211;
};

struct Test2
{
    int m = 332;
    int l = 5;
};

int main() {
    auto ecsm = std::make_shared<JuicyEngine::Registry>();
    ecsm->expose_internal_component<Test1>("Test1");
    ecsm->expose_internal_component<Test2>("Test2");
    ecsm->expose_internal_component<Test0>("Test0");
    JuicyEngine::RegistryProxy<Test0> ecs_proxy{ecsm};

    for (int i = 0; i < 10000; ++i) {
        auto entt = ecsm->create();
        ecsm->emplace<Test1>(entt);
        ecsm->emplace<Test2>(entt);
        ecsm->emplace<Test0>(entt);
        ecs_proxy.set(entt, "Test3", Test0{});
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    //auto view = ecsm->view<Test1, Test2>();
    auto rview = ecs_proxy.view({"Test3", "Test1", "Test2", "Test0"});
    int i = 0;
    for (auto e : rview) {
        ++i;
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
    std::cout << i << " in : " << time << std::endl;
    
}
