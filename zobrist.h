// zobrist.h - Zobrist 哈希: 棋盘局面到 64 位整数的映射
// 用于置换表索引、重复局面检测
#pragma once
#include "types.h"

namespace xq {

struct Zobrist {
    static uint64_t psq[PIECE_NB][SQUARE_NB];  // [棋子][位置]
    static uint64_t side;                       // 轮到走方
    static void init();
};

}  // namespace xq
