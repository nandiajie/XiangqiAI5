// evaluate.h
#pragma once
#include "position.h"

namespace xq {

// 返回当前走子方视角的评分 (centipawn)
int evaluate(const Position& pos);

void init_eval_tables();

}  // namespace xq
