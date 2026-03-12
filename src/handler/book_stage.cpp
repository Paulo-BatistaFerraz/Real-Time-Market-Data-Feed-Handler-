#include "qf/core/pipeline.hpp"
#include "qf/common/clock.hpp"

namespace qf::core {

void Pipeline::book_stage() {
    ParsedEvent event;

    while (running_.load(std::memory_order_acquire)) {
        if (!q2_.try_pop(event)) {
            std::this_thread::yield();
            continue;
        }

        BookUpdate update = book_manager_.process(event);

        if (!q3_.try_push(std::move(update))) {
            ++stats_.queue_drops;
        } else {
            ++stats_.book_updates;
        }
    }
}

}  // namespace qf::core
