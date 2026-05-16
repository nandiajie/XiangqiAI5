// movegen.h - 着法生成
#pragma once
#include "types.h"
#include "position.h"

namespace xq {

enum GenType { GEN_ALL, GEN_CAPTURES, GEN_QUIETS };

struct ExtMove {
    Move move;
    int score;  // 用于着法排序
    operator Move() const { return move; }
    bool operator<(const ExtMove& o) const { return score > o.score; }  // 降序
};

// 生成伪合法着法 (不检查我方将是否被攻击 / 飞将)
// 返回生成的着法数量
template<GenType GT>
int generate(const Position& pos, ExtMove* moves);

// 便捷封装: 生成所有合法着法
int generate_legal(Position& pos, ExtMove* moves);

}  // namespace xq
