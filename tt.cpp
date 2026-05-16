// tt.cpp
#include "tt.h"
#include <cstdlib>
#include <cstring>

namespace xq {

TranspositionTable TT;

TranspositionTable::TranspositionTable() {}
TranspositionTable::~TranspositionTable() { free(table_); }

void TranspositionTable::resize(size_t mb) {
    size_t bytes = mb * 1024 * 1024;
    size_t n = bytes / sizeof(TTBucket);
    // 取 2 的幂便于 & 掩码
    size_t p = 1;
    while (p * 2 <= n) p *= 2;
    if (p < 1024) p = 1024;

    free(table_);
    table_ = (TTBucket*)aligned_alloc(64, p * sizeof(TTBucket));
    if (!table_) {
        // 退回到 malloc
        table_ = (TTBucket*)malloc(p * sizeof(TTBucket));
    }
    numBuckets_ = p;
    clear();
}

void TranspositionTable::clear() {
    if (table_) std::memset(table_, 0, numBuckets_ * sizeof(TTBucket));
    generation_ = 0;
}

void TranspositionTable::new_search() {
    generation_ = (generation_ + 1) & 0x3F;
}

bool TranspositionTable::probe(uint64_t key, TTEntry& out) const {
    if (!table_) return false;
    size_t idx = (size_t)key & (numBuckets_ - 1);
    uint32_t k32 = uint32_t(key >> 32);
    const TTBucket& b = table_[idx];
    for (int i = 0; i < 3; ++i) {
        if (b.entries[i].key32 == k32 && b.entries[i].bound() != BOUND_NONE) {
            out = b.entries[i];
            return true;
        }
    }
    return false;
}

void TranspositionTable::store(uint64_t key, int value, int eval, Bound b, int depth, Move m) {
    if (!table_) return;
    size_t idx = (size_t)key & (numBuckets_ - 1);
    uint32_t k32 = uint32_t(key >> 32);
    TTBucket& bucket = table_[idx];

    // 找替换: 优先空槽 / 同 key / 最浅 + 旧 generation
    TTEntry* replace = &bucket.entries[0];
    for (int i = 0; i < 3; ++i) {
        TTEntry& e = bucket.entries[i];
        if (e.bound() == BOUND_NONE || e.key32 == k32) {
            replace = &e;
            break;
        }
        // 替换优先级: 老一代 + 浅深度的优先被替换
        int curScore = e.depth - ((generation_ - e.generation()) & 0x3F) * 8;
        int repScore = replace->depth - ((generation_ - replace->generation()) & 0x3F) * 8;
        if (curScore < repScore) replace = &e;
    }

    // 写入: 同 key 时保留更深的 move (避免覆盖好着法)
    if (m != MOVE_NONE || replace->key32 != k32) replace->move = m;
    replace->key32 = k32;
    replace->value = int16_t(value);
    replace->eval = int16_t(eval);
    replace->depth = uint8_t(depth);
    replace->genBound = uint8_t((generation_ << 2) | b);
}

int TranspositionTable::hashfull() const {
    if (!table_) return 0;
    int cnt = 0;
    int samples = std::min<size_t>(1000, numBuckets_);
    for (int i = 0; i < samples; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (table_[i].entries[j].bound() != BOUND_NONE &&
                table_[i].entries[j].generation() == generation_) cnt++;
        }
    }
    return cnt * 1000 / (samples * 3);
}

}  // namespace xq
