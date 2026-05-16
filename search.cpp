// search.cpp - 现代 Negamax + Alpha-Beta + 大量剪枝
#include "search.h"
#include "movegen.h"
#include "evaluate.h"
#include "tt.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace xq {

// LMR 归约表 [depth][moveCount]
static int LMR_TABLE[64][64];

static void init_lmr() {
    for (int d = 1; d < 64; ++d)
        for (int m = 1; m < 64; ++m)
            LMR_TABLE[d][m] = int(0.7 + std::log(d) * std::log(m) / 2.25);
    LMR_TABLE[0][0] = 0;
}

// 静态初始化器
struct LMRInit { LMRInit() { init_lmr(); } } _lmr_init;

Search::Search() { stop_ = false; reset_heuristics(); }

void Search::reset_heuristics() {
    std::memset(killers_, 0, sizeof(killers_));
    std::memset(history_, 0, sizeof(history_));
    std::memset(counterMove_, 0, sizeof(counterMove_));
    std::memset(pvLen_, 0, sizeof(pvLen_));
    std::memset(pvTable_, 0, sizeof(pvTable_));
}

int64_t Search::elapsed_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - startTime_).count();
}

bool Search::out_of_time() const {
    if (stop_.load()) return true;
    if (hardLimitMs_ > 0 && elapsed_ms() >= hardLimitMs_) return true;
    if (maxNodes_ > 0 && nodes_ >= maxNodes_) return true;
    return false;
}

void Search::set_limits(const SearchLimits& l, Color stm) {
    if (l.infinite) {
        softLimitMs_ = hardLimitMs_ = 1LL << 30;
    } else if (l.movetime > 0) {
        softLimitMs_ = hardLimitMs_ = l.movetime;
    } else if (l.wtime > 0 || l.btime > 0) {
        int rem = (stm == RED) ? l.wtime : l.btime;
        int inc = (stm == RED) ? l.winc  : l.binc;
        // 简单时间分配: 假设还有 25 步, 用剩余时间的 1/25 + 80% 增秒
        int alloc = rem / 25 + inc * 4 / 5;
        // 限制不超过剩余时间的 30%
        alloc = std::min(alloc, rem * 3 / 10);
        if (alloc < 50) alloc = 50;
        softLimitMs_ = alloc;
        hardLimitMs_ = std::min(alloc * 5, rem * 4 / 10);
    } else {
        softLimitMs_ = hardLimitMs_ = (l.depth > 0 || l.nodes > 0) ? (1LL << 30) : 5000;
    }
    maxDepth_ = l.depth > 0 ? l.depth : MAX_PLY;
    maxNodes_ = l.nodes > 0 ? l.nodes : 0;
}

// ============== SEE: Static Exchange Evaluation ==============
// 评估"吃子链"的最终损益。简化版: 单次吃子的物质净值
int Search::see(const Position& pos, Move m) const {
    Square to = to_sq(m);
    Piece captured = pos.piece_on(to);
    if (captured == NO_PIECE) return 0;
    Piece moved = pos.piece_on(from_sq(m));
    return PieceValue[type_of(captured)] - PieceValue[type_of(moved)] / 8;
    // 这里使用简化 SEE: 完整版需要模拟反复吃子, 但对应象棋已足够区分好/坏吃子
}

// ============== 着法评分 ==============
void Search::score_moves(const Position& pos, ExtMove* moves, int count, Move ttMove, int ply) {
    for (int i = 0; i < count; ++i) {
        Move m = moves[i].move;
        if (m == ttMove) { moves[i].score = 1 << 30; continue; }

        Piece captured = pos.piece_on(to_sq(m));
        Piece moved = pos.piece_on(from_sq(m));
        if (captured != NO_PIECE) {
            // MVV-LVA: 高价值受害者 + 低价值攻击者
            int s = PieceValue[type_of(captured)] * 10 - PieceValue[type_of(moved)];
            moves[i].score = (1 << 28) + s;
        } else if (m == killers_[ply][0]) {
            moves[i].score = (1 << 27) + 100;
        } else if (m == killers_[ply][1]) {
            moves[i].score = (1 << 27);
        } else {
            // History
            moves[i].score = history_[moved][to_sq(m)];
        }
    }
}

void Search::pick_next_move(ExtMove* moves, int count, int idx) {
    int best = idx;
    for (int i = idx + 1; i < count; ++i)
        if (moves[i].score > moves[best].score) best = i;
    if (best != idx) std::swap(moves[idx], moves[best]);
}

// ============== Quiescence Search ==============
int Search::qsearch(Position& pos, int alpha, int beta, int ply) {
    nodes_++;
    if ((nodes_ & 4095) == 0 && out_of_time()) { stop_ = true; return 0; }

    if (ply >= MAX_PLY - 1) return evaluate(pos);

    // 重复局面
    if (pos.is_repetition(ply) || pos.rule50() >= 60) return VALUE_DRAW;

    bool inCheck = pos.in_check();
    int standPat;

    if (inCheck) {
        standPat = -VALUE_INFINITE;
    } else {
        standPat = evaluate(pos);
        if (standPat >= beta) return standPat;
        if (standPat > alpha) alpha = standPat;
    }

    ExtMove moves[MAX_MOVES];
    int count = inCheck
                  ? generate<GEN_ALL>(pos, moves)
                  : generate<GEN_CAPTURES>(pos, moves);

    score_moves(pos, moves, count, MOVE_NONE, ply);

    int bestScore = standPat;
    int legalMoves = 0;
    for (int i = 0; i < count; ++i) {
        pick_next_move(moves, count, i);
        Move m = moves[i].move;

        // SEE 剪枝: 非将军时跳过明显坏吃子
        if (!inCheck && see(pos, m) < -50) continue;

        pos.make_move(m);
        // 合法性
        if (pos.is_attacked(pos.king_square(~pos.side_to_move()), pos.side_to_move()) ||
            pos.kings_face_each_other()) {
            pos.unmake_move();
            continue;
        }
        legalMoves++;
        int score = -qsearch(pos, -beta, -alpha, ply + 1);
        pos.unmake_move();

        if (stop_.load()) return 0;

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                alpha = score;
                if (alpha >= beta) break;
            }
        }
    }

    if (inCheck && legalMoves == 0) return mated_in(ply);
    return bestScore;
}

// ============== 主搜索 ==============
int Search::negamax(Position& pos, int alpha, int beta, int depth, int ply, bool isPv, bool cutNode) {
    if (depth <= 0) return qsearch(pos, alpha, beta, ply);

    nodes_++;
    if ((nodes_ & 2047) == 0 && out_of_time()) { stop_ = true; return 0; }
    if (ply >= MAX_PLY - 1) return evaluate(pos);

    pvLen_[ply] = ply;  // 起始 PV 长度

    // 重复局面 / 50 回合规则
    if (ply > 0 && (pos.is_repetition(ply) || pos.rule50() >= 60))
        return VALUE_DRAW;

    // Mate Distance Pruning
    if (ply > 0) {
        alpha = std::max(alpha, mated_in(ply));
        beta  = std::min(beta,  mate_in(ply + 1));
        if (alpha >= beta) return alpha;
    }

    // === TT 探查 ===
    uint64_t key = pos.key();
    TTEntry tte;
    bool ttHit = TT.probe(key, tte);
    Move ttMove = ttHit ? tte.move : MOVE_NONE;
    int  ttValue = ttHit ? int(tte.value) : VALUE_NONE;

    if (ttHit && tte.depth >= depth && ply > 0 && !isPv) {
        if (tte.bound() == BOUND_EXACT ||
            (tte.bound() == BOUND_LOWER && ttValue >= beta) ||
            (tte.bound() == BOUND_UPPER && ttValue <= alpha))
            return ttValue;
    }

    bool inCheck = pos.in_check();

    // === 静态评估 ===
    int eval;
    if (inCheck) eval = -VALUE_INFINITE;
    else if (ttHit) eval = tte.eval;
    else eval = evaluate(pos);

    // === Razoring (浅层, 远低于 alpha 时直接转 qsearch) ===
    if (!isPv && !inCheck && depth <= 3 && eval + 250 * depth <= alpha) {
        int q = qsearch(pos, alpha - 1, alpha, ply);
        if (q < alpha) return q;
    }

    // === Reverse Futility / Static Null Move ===
    if (!isPv && !inCheck && depth <= 7 && eval - 100 * depth >= beta && eval < VALUE_MATE_IN_MAX_PLY)
        return eval;

    // === Null Move Pruning ===
    if (!isPv && !inCheck && depth >= 3 && eval >= beta &&
        pos.non_pawn_material(pos.side_to_move()) > 0) {
        int R = 3 + depth / 4 + std::min(3, (eval - beta) / 200);
        pos.make_null_move();
        int nullScore = -negamax(pos, -beta, -beta + 1, depth - R - 1, ply + 1, false, !cutNode);
        pos.unmake_null_move();
        if (stop_.load()) return 0;
        if (nullScore >= beta) {
            if (nullScore >= VALUE_MATE_IN_MAX_PLY) nullScore = beta;
            return nullScore;
        }
    }

    // === Internal Iterative Reductions (IIR): 无 TT 着法时降一层 ===
    if (depth >= 4 && ttMove == MOVE_NONE) depth--;

    // === 生成着法 ===
    ExtMove moves[MAX_MOVES];
    int count = generate<GEN_ALL>(pos, moves);
    score_moves(pos, moves, count, ttMove, ply);

    int bestScore = -VALUE_INFINITE;
    Move bestMove = MOVE_NONE;
    int legalCount = 0;
    int quietCount = 0;
    Move quietMoves[64];

    for (int i = 0; i < count; ++i) {
        pick_next_move(moves, count, i);
        Move m = moves[i].move;

        Piece moved = pos.piece_on(from_sq(m));
        bool isCapture = pos.piece_on(to_sq(m)) != NO_PIECE;
        bool isQuiet = !isCapture;

        // === Late Move Pruning ===
        if (!isPv && !inCheck && bestScore > -VALUE_MATE_IN_MAX_PLY && depth <= 8) {
            int lmpThreshold = 3 + depth * depth;
            if (legalCount >= lmpThreshold && isQuiet) continue;
        }

        // === Futility Pruning ===
        if (!isPv && !inCheck && depth <= 6 && isQuiet &&
            eval + 90 + 80 * depth <= alpha &&
            bestScore > -VALUE_MATE_IN_MAX_PLY) continue;

        // === SEE 剪枝 (深度浅时,坏吃子直接跳过) ===
        if (depth <= 6 && isCapture && see(pos, m) < -100 * depth) continue;

        pos.make_move(m);
        // 合法性检查
        if (pos.is_attacked(pos.king_square(~pos.side_to_move()), pos.side_to_move()) ||
            pos.kings_face_each_other()) {
            pos.unmake_move();
            continue;
        }
        legalCount++;
        bool givesCheck = pos.in_check();
        if (isQuiet && quietCount < 64) quietMoves[quietCount++] = m;

        int newDepth = depth - 1;
        if (givesCheck) newDepth++;  // Check Extension

        int score;
        if (legalCount == 1) {
            // 首着 / PV move: 全窗口
            score = -negamax(pos, -beta, -alpha, newDepth, ply + 1, isPv, false);
        } else {
            // === LMR ===
            int reduction = 0;
            if (depth >= 3 && legalCount >= 4 && isQuiet && !givesCheck) {
                int d = std::min(63, depth);
                int m_idx = std::min(63, legalCount);
                reduction = LMR_TABLE[d][m_idx];
                if (cutNode) reduction++;
                if (!isPv) reduction++;
                reduction = std::max(0, std::min(reduction, newDepth - 1));
            }
            // 零窗口探测
            score = -negamax(pos, -alpha - 1, -alpha, newDepth - reduction, ply + 1, false, true);
            // 如 reduction 后超出, 重搜全深度
            if (score > alpha && reduction > 0)
                score = -negamax(pos, -alpha - 1, -alpha, newDepth, ply + 1, false, !cutNode);
            // PV node 重搜全窗口
            if (score > alpha && score < beta && isPv)
                score = -negamax(pos, -beta, -alpha, newDepth, ply + 1, true, false);
        }
        pos.unmake_move();

        if (stop_.load()) return 0;

        if (score > bestScore) {
            bestScore = score;
            if (score > alpha) {
                bestMove = m;
                alpha = score;
                // 更新 PV
                if (isPv) {
                    pvTable_[ply][ply] = m;
                    for (int j = ply + 1; j < pvLen_[ply + 1]; ++j)
                        pvTable_[ply][j] = pvTable_[ply + 1][j];
                    pvLen_[ply] = pvLen_[ply + 1];
                }
                if (alpha >= beta) {
                    // Beta cutoff
                    if (isQuiet) {
                        // Killer
                        if (killers_[ply][0] != m) {
                            killers_[ply][1] = killers_[ply][0];
                            killers_[ply][0] = m;
                        }
                        // History bonus
                        int bonus = depth * depth;
                        history_[moved][to_sq(m)] += bonus - history_[moved][to_sq(m)] * std::abs(bonus) / 16384;
                        // 惩罚其他试过的安静着法
                        for (int qi = 0; qi < quietCount - 1; ++qi) {
                            Move qm = quietMoves[qi];
                            Piece qp = pos.piece_on(from_sq(qm));  // 注意: 此时已 unmake, from 又有这个子
                            history_[qp][to_sq(qm)] -= bonus + history_[qp][to_sq(qm)] * std::abs(bonus) / 16384;
                        }
                    }
                    break;
                }
            }
        }
    }

    // 中国象棋规则: 无论是否被将军, 无合法着法都判负 (困毙 = 负, 不像国际象棋是和棋)
    if (legalCount == 0) {
        return mated_in(ply);
    }

    // === 存 TT ===
    Bound bound = bestScore >= beta ? BOUND_LOWER
                 : (isPv && bestMove != MOVE_NONE) ? BOUND_EXACT
                 : BOUND_UPPER;
    TT.store(key, bestScore, eval, bound, depth, bestMove);

    return bestScore;
}

// ============== 顶层迭代加深 ==============
void Search::start(Position& pos, const SearchLimits& limits, SearchCallback cb) {
    stop_ = false;
    nodes_ = 0;
    bestMove_ = MOVE_NONE;
    bestScore_ = 0;
    startTime_ = std::chrono::steady_clock::now();
    reset_heuristics();
    TT.new_search();
    set_limits(limits, pos.side_to_move());

    int prevScore = 0;

    for (int depth = 1; depth <= maxDepth_; ++depth) {
        // Aspiration Windows
        int alpha = -VALUE_INFINITE, beta = VALUE_INFINITE;
        int delta = 25;
        if (depth >= 5) {
            alpha = std::max(prevScore - delta, -VALUE_INFINITE);
            beta  = std::min(prevScore + delta,  VALUE_INFINITE);
        }

        int score;
        while (true) {
            score = negamax(pos, alpha, beta, depth, 0, true, false);
            if (stop_.load()) break;
            if (score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(score - delta, -VALUE_INFINITE);
                delta += delta / 2;
            } else if (score >= beta) {
                beta = std::min(score + delta, VALUE_INFINITE);
                delta += delta / 2;
            } else break;
            if (delta > 1500) { alpha = -VALUE_INFINITE; beta = VALUE_INFINITE; }
        }

        if (stop_.load() && depth > 1) break;  // 保留上一层结果

        prevScore = score;
        bestScore_ = score;
        if (pvLen_[0] > 0) {
            bestMove_ = pvTable_[0][0];
        }
        timeMs_ = elapsed_ms();

        if (cb) {
            SearchInfo info;
            info.depth = depth;
            info.seldepth = depth;
            info.score = score;
            info.nodes = nodes_;
            info.timeMs = timeMs_;
            info.bestMove = bestMove_;
            info.pvLen = pvLen_[0];
            for (int i = 0; i < pvLen_[0]; ++i) info.pv[i] = pvTable_[0][i];
            cb(info);
        }

        // 找到杀棋则停止
        if (std::abs(score) >= VALUE_MATE_IN_MAX_PLY) break;

        // 软时限: 完成本层后超出软时限则停 (除非杀棋路径不稳)
        if (softLimitMs_ > 0 && elapsed_ms() >= softLimitMs_) break;
    }

    if (bestMove_ == MOVE_NONE) {
        // 兜底: 找任意合法着法
        ExtMove moves[MAX_MOVES];
        int n = generate_legal(pos, moves);
        if (n > 0) bestMove_ = moves[0].move;
    }
}

void Search::stop() { stop_ = true; }

}  // namespace xq
