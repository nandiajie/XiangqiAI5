// zobrist.cpp
#include "zobrist.h"

namespace xq {

uint64_t Zobrist::psq[PIECE_NB][SQUARE_NB];
uint64_t Zobrist::side;

// xorshift64* - 高质量 PRNG, 固定种子保证跨平台可复现
static uint64_t prng_state = 0x9E3779B97F4A7C15ULL;
static uint64_t rand64() {
    uint64_t x = prng_state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    prng_state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

void Zobrist::init() {
    prng_state = 0x9E3779B97F4A7C15ULL;
    for (int p = 0; p < PIECE_NB; ++p)
        for (int s = 0; s < SQUARE_NB; ++s)
            psq[p][s] = rand64();
    side = rand64();
}

}  // namespace xq
