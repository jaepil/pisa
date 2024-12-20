#pragma once

#include "concepts/posting_cursor.hpp"
#include "topk_queue.hpp"

namespace pisa {

template <typename QueryAlg>
struct range_query {
    explicit range_query(topk_queue& topk) : m_topk(topk) {}

    template <typename CursorRange>
        requires(concepts::MaxScorePostingCursor<typename std::decay_t<CursorRange>::value_type>)
    void operator()(CursorRange&& cursors, uint64_t max_docid, size_t range_size) {
        m_topk.clear();
        if (cursors.empty()) {
            return;
        }

        for (size_t end = range_size; end + range_size < max_docid; end += range_size) {
            process_range(cursors, end);
        }
        process_range(cursors, max_docid);
    }

    std::vector<typename topk_queue::entry_type> const& topk() const { return m_topk.topk(); }

    template <typename CursorRange>
        requires(concepts::MaxScorePostingCursor<typename std::decay_t<CursorRange>::value_type>)
    void process_range(CursorRange&& cursors, size_t end) {
        QueryAlg query_alg(m_topk);
        query_alg(cursors, end);
    }

  private:
    topk_queue& m_topk;
};

}  // namespace pisa
