#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace gpu::dlss {

// One ledger per device session, serialized on the command worker. Recording
// retains a feature even if NGX failed and its isolated list will be excluded.
// Only a successful prefix-batch submit can assign that use a completion fence.
class SubmissionLifetime {
public:
    uint64_t Record() {
        if (!nextId_) return 0; // Never wrap and alias an earlier use ID.
        const uint64_t id = nextId_++;
        uses_.push_back({id, 0});
        return id;
    }

    bool Submit(uint64_t id, uint64_t serial) {
        if (!id || !serial) return false;
        const auto it = std::find_if(uses_.begin(), uses_.end(),
            [id](const Use& use) { return use.id == id; });
        if (it == uses_.end()) return false;
        // Duplicate notifications cannot move a live resource to an earlier
        // fence. Nor can a recorded use claim an already completed submission.
        if (it->serial) return it->serial == serial;
        if (serial <= completed_) return false;
        it->serial = serial;
        return true;
    }

    void Discard(uint64_t id) {
        // A discard belongs to a batch that was never submitted. Once assigned
        // a fence, only GPU completion can release the use.
        std::erase_if(uses_, [id](const Use& use) { return use.id == id && !use.serial; });
    }

    void CompleteThrough(uint64_t serial) {
        completed_ = std::max(completed_, serial);
        std::erase_if(uses_, [this](const Use& use) { return use.serial && use.serial <= completed_; });
    }

    bool Empty() const { return uses_.empty(); }

private:
    struct Use { uint64_t id, serial; };
    std::vector<Use> uses_;
    uint64_t nextId_ = 1;
    uint64_t completed_ = 0;
};

} // namespace gpu::dlss
