#include "engine/ScheduleExchange.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

using namespace curvable;

namespace
{
    std::unique_ptr<Schedule> scheduleWithLength (double length)
    {
        auto s = std::make_unique<Schedule>();
        s->length = length;
        return s;
    }
}

TEST_CASE ("acquire returns the latest published schedule", "[exchange]")
{
    ScheduleExchange ex;
    CHECK (ex.acquire() == nullptr);

    ex.publish (scheduleWithLength (1));
    REQUIRE (ex.acquire() != nullptr);
    CHECK (ex.acquire()->length == 1);

    // 旧版本进入待回收槽，回收后才能再次换用
    ex.publish (scheduleWithLength (2));
    CHECK (ex.acquire()->length == 2);

    ex.publish (scheduleWithLength (3));
    ex.publish (scheduleWithLength (4)); // 3 未被取走就被替换
    ex.collectGarbage();
    CHECK (ex.acquire()->length == 4);
}

TEST_CASE ("concurrent publish and acquire stay consistent", "[exchange]")
{
    ScheduleExchange ex;
    std::atomic<bool> done { false };
    double lastSeen = 0;
    bool wentBackwards = false;

    std::thread audio ([&]
    {
        while (! done.load())
        {
            if (const auto* s = ex.acquire())
            {
                if (s->length < lastSeen)
                    wentBackwards = true;
                lastSeen = s->length;
            }
        }
    });

    for (int i = 1; i <= 20000; ++i)
        ex.publish (scheduleWithLength (i));

    // 等音频线程取到最后一个版本
    for (int i = 0; i < 1000 && lastSeen < 20000; ++i)
    {
        ex.collectGarbage();
        std::this_thread::yield();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    done = true;
    audio.join();

    CHECK_FALSE (wentBackwards);
    CHECK (lastSeen == 20000);
}
