#include <iostream>
#include <optional>
#include <thread>

#include "block_inverted_index.hpp"
#include "boost/algorithm/string/classification.hpp"
#include "boost/algorithm/string/split.hpp"
#include "codec/block_codec_registry.hpp"
#include "query/query_parser.hpp"
#include "spdlog/spdlog.h"

#include "cursor/cursor.hpp"
#include "cursor/max_scored_cursor.hpp"
#include "cursor/scored_cursor.hpp"
#include "memory_source.hpp"
#include "query/algorithm/and_query.hpp"
#include "query/algorithm/maxscore_query.hpp"
#include "query/algorithm/ranked_and_query.hpp"
#include "query/algorithm/wand_query.hpp"
#include "scorer/scorer.hpp"
#include "tokenizer.hpp"
#include "wand_data.hpp"
#include "wand_data_raw.hpp"

using namespace pisa;

template <typename QueryOperator>
void op_profile(QueryOperator const& query_op, std::vector<Query> const& queries) {
    using namespace pisa;

    size_t n_threads = std::thread::hardware_concurrency();
    std::vector<std::thread> threads(n_threads);
    std::mutex io_mutex;

    for (size_t tid = 0; tid < n_threads; ++tid) {
        threads[tid] = std::thread([&, tid]() {
            auto query_op_copy = query_op;  // copy one query_op per thread
            for (size_t i = tid; i < queries.size(); i += n_threads) {
                if (i % 10000 == 0) {
                    std::lock_guard<std::mutex> lock(io_mutex);
                    spdlog::info("{} queries processed", i);
                }

                query_op_copy(queries[i]);
            }
        });
    }

    for (auto& thread: threads) {
        thread.join();
    }
}

void profile(
    BlockInvertedIndex const* index_ptr,
    const std::optional<std::string>& wand_data_filename,
    std::vector<Query> const& queries,
    std::string const& type,
    std::string const& query_type
) {
    auto const& index = *index_ptr;
    using namespace pisa;

    using WandType = wand_data<wand_data_raw>;

    WandType const wdata = [&] {
        if (wand_data_filename) {
            return WandType(MemorySource::mapped_file(*wand_data_filename));
        }
        return WandType{};
    }();

    spdlog::info("Performing {} queries", type);

    std::vector<std::string> query_types;
    boost::algorithm::split(query_types, query_type, boost::is_any_of(":"));

    auto scorer = scorer::from_params(ScorerParams("bm25"), wdata);

    for (auto const& t: query_types) {
        spdlog::info("Query type: {}", t);
        std::function<uint64_t(Query)> query_fun;
        if (t == "and") {
            query_fun = [&](Query query) {
                and_query and_q;
                return and_q(make_cursors(index, query), index.num_docs()).size();
            };
        } else if (t == "ranked_and" && wand_data_filename) {
            query_fun = [&](Query query) {
                topk_queue topk(10);
                ranked_and_query ranked_and_q(topk);
                ranked_and_q(make_scored_cursors(index, *scorer, query), index.num_docs());
                topk.finalize();
                return topk.topk().size();
            };
        } else if (t == "wand" && wand_data_filename) {
            query_fun = [&](Query query) {
                topk_queue topk(10);
                wand_query wand_q(topk);
                wand_q(make_max_scored_cursors(index, wdata, *scorer, query), index.num_docs());
                topk.finalize();
                return topk.topk().size();
            };
        } else if (t == "maxscore" && wand_data_filename) {
            query_fun = [&](Query query) {
                topk_queue topk(10);
                maxscore_query maxscore_q(topk);
                maxscore_q(make_max_scored_cursors(index, wdata, *scorer, query), index.num_docs());
                topk.finalize();
                return topk.topk().size();
            };
        } else {
            spdlog::error("Unsupported query type: {}", t);
        }
        op_profile(query_fun, queries);
    }

    block_profiler::dump(std::cout);
}

int main(int argc, const char** argv) {
    using namespace pisa;

    std::string type = argv[1];
    const char* query_type = argv[2];
    const char* index_filename = argv[3];
    std::optional<std::string> wand_data_filename;
    size_t args = 4;
    if (argc > 4) {
        wand_data_filename = argv[4];
        args++;
    }

    std::vector<Query> queries;
    std::string line;
    QueryParser parser(TextAnalyzer(std::make_unique<WhitespaceTokenizer>()));
    if (std::string(argv[args]) == "--file") {
        args++;
        args++;
        std::filebuf fb;
        if (fb.open(argv[args], std::ios::in) != nullptr) {
            std::istream is(&fb);
            while (std::getline(is, line)) {
                queries.push_back(parser.parse(line));
            }
        }
    } else {
        while (std::getline(std::cin, line)) {
            queries.push_back(parser.parse(line));
        }
    }

    ProfilingBlockInvertedIndex index(
        MemorySource::mapped_file(std::filesystem::path(index_filename)), get_block_codec(type)
    );

    profile(&index, wand_data_filename, queries, type, query_type);
}
