#pragma once

#include "engine/Schedule.h"

#include <atomic>
#include <memory>

namespace curvable
{

/** 把消息线程编译好的 Schedule 无锁地交给音频线程。

    - 消息线程：publish() 提交新版本；collectGarbage() 删除音频线程已不再使用的旧版本（定期调用）。
    - 音频线程：acquire() 取得最新版本。不加锁、不分配、不释放内存。

    音频线程换用新版本时，把旧版本放进"待回收"槽；该槽尚未被消息线程清空时，暂不换用（下一块再换）。
*/
class ScheduleExchange
{
public:
    ScheduleExchange() = default;

    ~ScheduleExchange()
    {
        delete pending.exchange (nullptr);
        delete retired.exchange (nullptr);
        delete current;
    }

    /** 消息线程。 */
    void publish (std::unique_ptr<Schedule> schedule)
    {
        collectGarbage();
        // 音频线程还没取走的上一个版本可以直接丢弃
        delete pending.exchange (schedule.release());
    }

    /** 消息线程。 */
    void collectGarbage()
    {
        delete retired.exchange (nullptr);
    }

    /** 音频线程：返回当前版本（可能为 nullptr）。 */
    const Schedule* acquire() noexcept
    {
        if (retired.load (std::memory_order_acquire) == nullptr)
        {
            if (auto* next = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                retired.store (current, std::memory_order_release);
                current = next;
            }
        }

        return current;
    }

private:
    std::atomic<Schedule*> pending { nullptr };
    std::atomic<Schedule*> retired { nullptr };
    Schedule* current = nullptr; ///< 只由音频线程访问（析构时除外）

    ScheduleExchange (const ScheduleExchange&) = delete;
    ScheduleExchange& operator= (const ScheduleExchange&) = delete;
};

} // namespace curvable
