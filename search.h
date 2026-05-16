// search.h - 搜索引擎接口
#pragma once
#include "types.h"
#include "position.h"
#include <atomic>
#include <chrono>
#include <functional>

namespace xq {

struct SearchLimits {
    int wtime = 0, btime = 0;   // 剩余时间 ms
    int winc = 0, binc = 0;     // 增秒 ms
    int movetime = 0;           // 单步固定时间 ms
    int depth = 0;              // 固定深度 (0 = 不限)
    int nodes = 0;              // 节点数上限 (0 = 不限)
    bool infinite = false;
};

struct SearchInfo {
    int depth;
    int seldepth;
    int score;
    int64_t nodes;
    int64_t timeMs;
    Move bestMove;
    Move pv[MAX_PLY];
    int pvLen;
};

// 搜索回调: 每完成一层迭代时调用
using SearchCallback = std::function<void(const SearchInfo&)>;

class Search {
public:
    Search();
    void start(Position& pos, const SearchLimits& limits, SearchCallback cb = nullptr);
    void stop();
    bool stopped() const { return stop_.load(); }

    Move best_move() const { return bestMove_; }
    int  best_score() const { return bestScore_; }
    int64_t nodes_searched() const { return nodes_; }

private:
    std::atomic<bool> stop_;
    Move bestMove_ = MOVE_NONE;
    int  bestScore_ = 0;
    int64_t nodes_ = 0;
    int64_t timeMs_ = 0;

    // 时间管理
    std::chrono::steady_clock::time_point startTime_;
    int64_t softLimitMs_ = 0;
    int64_t hardLimitMs_ = 0;
    int     maxDepth_ = MAX_PLY;
    int64_t maxNodes_ = 0;
    int64_t elapsed_ms() const;
    bool out_of_time() const;
    void set_limits(const SearchLimits& l, Color stm);

    // 启发式表 (实例字段以便 reset)
    Move killers_[MAX_PLY][2];
    int  history_[16][SQUARE_NB];   // history_[piece][to]
    Move counterMove_[16][SQUARE_NB];

    int  pvLen_[MAX_PLY];
    Move pvTable_[MAX_PLY][MAX_PLY];

    void reset_heuristics();
    int  negamax(Position& pos, int alpha, int beta, int depth, int ply, bool isPv, bool cutNode);
    int  qsearch(Position& pos, int alpha, int beta, int ply);
    int  see(const Position& pos, Move m) const;

    void score_moves(const Position& pos, struct ExtMove* moves, int count, Move ttMove, int ply);
    void pick_next_move(struct ExtMove* moves, int count, int idx);
};

}  // namespace xq
