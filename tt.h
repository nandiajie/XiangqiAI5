// tt.h - 置换表 (Transposition Table)
#pragma once
#include "types.h"
#include <atomic>
#include <cstdint>

namespace xq {

enum Bound : uint8_t {
    BOUND_NONE = 0,
    BOUND_UPPER = 1,  // 实际分 <= score (alpha 失败)
    BOUND_LOWER = 2,  // 实际分 >= score (beta 截断)
    BOUND_EXACT = 3   // 精确分
};

// 单个置换表条目, 紧凑布局 12 字节
struct TTEntry {
    uint32_t key32;   // hash 高 32 位
    Move     move;    // 最佳着法 (16 位)
    int16_t  value;   // 评估值 (centipawn)
    int16_t  eval;    // 静态评估缓存
    uint8_t  depth;   // 搜索深度
    uint8_t  genBound; // 高 6 位 = generation, 低 2 位 = bound

    Bound bound() const { return Bound(genBound & 3); }
    uint8_t generation() const { return genBound >> 2; }
};

// 三槽桶 (cache 友好)
struct TTBucket {
    TTEntry entries[3];
    uint16_t pad;  // 对齐填充, 总 38 字节,补 2 字节= 40,可不要
};

class TranspositionTable {
public:
    TranspositionTable();
    ~TranspositionTable();

    void resize(size_t mb);   // 设置大小 (MB)
    void clear();
    void new_search();

    bool probe(uint64_t key, TTEntry& out) const;
    void store(uint64_t key, int value, int eval, Bound b, int depth, Move m);

    int hashfull() const;     // 千分比 (0-1000)

private:
    TTBucket* table_ = nullptr;
    size_t  numBuckets_ = 0;
    uint8_t generation_ = 0;
};

extern TranspositionTable TT;

}  // namespace xq
