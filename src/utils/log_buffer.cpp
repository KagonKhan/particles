#include "utils/log_buffer.hpp"

#include <algorithm>


void LogBuffer::drain(ImGuiConsoleSink& sink)
{
    sink.drain(messages_);
    trim();
    classify();
}

void LogBuffer::setCapacity(std::size_t capacity)
{
    if (capacity == capacity_) {
        return;
    }

    capacity_ = capacity;
    trim();
}

void LogBuffer::setLevel(spdlog::level::level_enum level)
{
    if (level == level_) {
        return;
    }

    level_ = level;
    rebuild();
}

void LogBuffer::clear()
{
    oldest_ += messages_.size();
    messages_.clear();
    rebuild();
}

ConsoleMessage const& LogBuffer::row(std::size_t index) const
{
    return messages_[static_cast<std::size_t>(visible_[index] - oldest_)];
}

void LogBuffer::trim()
{
    if (messages_.size() <= capacity_) {
        return;
    }

    std::size_t const dropped = messages_.size() - capacity_;

    messages_.erase(messages_.begin(), messages_.begin() + static_cast<std::ptrdiff_t>(dropped));
    oldest_ += dropped;

    while (!visible_.empty() && (visible_.front() < oldest_)) {
        visible_.pop_front();
        ++firstRow_;
    }
}

void LogBuffer::classify()
{
    // A burst larger than the capacity can age messages out before they were ever looked at.
    classified_ = std::max(classified_, oldest_);

    std::uint64_t const end = oldest_ + messages_.size();

    for (; classified_ < end; ++classified_) {
        if (messages_[static_cast<std::size_t>(classified_ - oldest_)].level >= level_) {
            visible_.push_back(classified_);
        }
    }
}

void LogBuffer::rebuild()
{
    visible_.clear();
    classified_ = oldest_;
    firstRow_   = 0;
    ++generation_;

    classify();
}
