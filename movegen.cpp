// movegen.cpp
#include "movegen.h"

namespace xq {

// 复用 position.cpp 的偏移表的扩展版本
static const int HORSE_MOVES[8][4] = {
    { 1,  2,  0,  1}, {-1,  2,  0,  1},
    { 1, -2,  0, -1}, {-1, -2,  0, -1},
    { 2,  1,  1,  0}, { 2, -1,  1,  0},
    {-2,  1, -1,  0}, {-2, -1, -1,  0}
};
static const int ELEPHANT_MOVES[4][4] = {
    { 2,  2,  1,  1}, { 2, -2,  1, -1},
    {-2,  2, -1,  1}, {-2, -2, -1, -1}
};
static const int ADVISOR_MOVES[4][2] = {
    { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
};
static const int ORTHO[4][2] = {
    { 0,  1}, { 0, -1}, { 1,  0}, {-1,  0}
};

template<GenType GT>
static inline void add_move(ExtMove*& cur, const Position& pos, Square from, Square to) {
    if (GT == GEN_CAPTURES) {
        if (pos.piece_on(to) == NO_PIECE) return;
    } else if (GT == GEN_QUIETS) {
        if (pos.piece_on(to) != NO_PIECE) return;
    }
    cur->move = make_move(from, to);
    cur->score = 0;
    ++cur;
}

template<GenType GT>
int generate(const Position& pos, ExtMove* moves) {
    ExtMove* cur = moves;
    Color us = pos.side_to_move();

    for (int s = 0; s < SQUARE_NB; ++s) {
        Piece pc = pos.piece_on(s);
        if (pc == NO_PIECE || color_of(pc) != us) continue;
        int f = file_of(s), r = rank_of(s);
        PieceType pt = type_of(pc);

        switch (pt) {
        case KING: {
            // 九宫内 4 直线方向
            for (const auto& o : ORTHO) {
                int nf = f + o[0], nr = r + o[1];
                if (nf < 0 || nf >= 9 || nr < 0 || nr >= 10) continue;
                Square ns = make_square(nf, nr);
                if (!in_own_palace(us, ns)) continue;
                Piece target = pos.piece_on(ns);
                if (target != NO_PIECE && color_of(target) == us) continue;
                add_move<GT>(cur, pos, s, ns);
            }
            break;
        }
        case ADVISOR: {
            for (const auto& o : ADVISOR_MOVES) {
                int nf = f + o[0], nr = r + o[1];
                if (nf < 0 || nf >= 9 || nr < 0 || nr >= 10) continue;
                Square ns = make_square(nf, nr);
                if (!in_own_palace(us, ns)) continue;
                Piece target = pos.piece_on(ns);
                if (target != NO_PIECE && color_of(target) == us) continue;
                add_move<GT>(cur, pos, s, ns);
            }
            break;
        }
        case ELEPHANT: {
            for (const auto& o : ELEPHANT_MOVES) {
                int nf = f + o[0], nr = r + o[1];
                if (nf < 0 || nf >= 9 || nr < 0 || nr >= 10) continue;
                Square ns = make_square(nf, nr);
                if (crossed_river(us, ns)) continue;  // 象不过河
                // 塞象眼
                int bf = f + o[2], br = r + o[3];
                if (pos.piece_on(make_square(bf, br)) != NO_PIECE) continue;
                Piece target = pos.piece_on(ns);
                if (target != NO_PIECE && color_of(target) == us) continue;
                add_move<GT>(cur, pos, s, ns);
            }
            break;
        }
        case HORSE: {
            for (const auto& o : HORSE_MOVES) {
                int nf = f + o[0], nr = r + o[1];
                if (nf < 0 || nf >= 9 || nr < 0 || nr >= 10) continue;
                // 蹩马腿
                int bf = f + o[2], br = r + o[3];
                if (pos.piece_on(make_square(bf, br)) != NO_PIECE) continue;
                Square ns = make_square(nf, nr);
                Piece target = pos.piece_on(ns);
                if (target != NO_PIECE && color_of(target) == us) continue;
                add_move<GT>(cur, pos, s, ns);
            }
            break;
        }
        case ROOK: {
            for (const auto& o : ORTHO) {
                int df = o[0], dr = o[1];
                int nf = f + df, nr = r + dr;
                while (nf >= 0 && nf < 9 && nr >= 0 && nr < 10) {
                    Square ns = make_square(nf, nr);
                    Piece target = pos.piece_on(ns);
                    if (target == NO_PIECE) {
                        add_move<GT>(cur, pos, s, ns);
                    } else {
                        if (color_of(target) != us) add_move<GT>(cur, pos, s, ns);
                        break;
                    }
                    nf += df; nr += dr;
                }
            }
            break;
        }
        case CANNON: {
            for (const auto& o : ORTHO) {
                int df = o[0], dr = o[1];
                int nf = f + df, nr = r + dr;
                // 阶段 1: 找空格 (平移)
                while (nf >= 0 && nf < 9 && nr >= 0 && nr < 10 &&
                       pos.piece_on(make_square(nf, nr)) == NO_PIECE) {
                    add_move<GT>(cur, pos, s, make_square(nf, nr));
                    nf += df; nr += dr;
                }
                // 阶段 2: 越过炮架找敌子
                if (nf < 0 || nf >= 9 || nr < 0 || nr >= 10) continue;
                nf += df; nr += dr;
                while (nf >= 0 && nf < 9 && nr >= 0 && nr < 10) {
                    Square ns = make_square(nf, nr);
                    Piece target = pos.piece_on(ns);
                    if (target != NO_PIECE) {
                        if (color_of(target) != us) add_move<GT>(cur, pos, s, ns);
                        break;
                    }
                    nf += df; nr += dr;
                }
            }
            break;
        }
        case PAWN: {
            int forward = (us == RED) ? 1 : -1;
            // 前进
            int nf = f, nr = r + forward;
            if (nr >= 0 && nr < 10) {
                Square ns = make_square(nf, nr);
                Piece target = pos.piece_on(ns);
                if (target == NO_PIECE || color_of(target) != us)
                    add_move<GT>(cur, pos, s, ns);
            }
            // 过河后可左右
            if (crossed_river(us, s)) {
                for (int df : {-1, 1}) {
                    int nf2 = f + df, nr2 = r;
                    if (nf2 < 0 || nf2 >= 9) continue;
                    Square ns = make_square(nf2, nr2);
                    Piece target = pos.piece_on(ns);
                    if (target == NO_PIECE || color_of(target) != us)
                        add_move<GT>(cur, pos, s, ns);
                }
            }
            break;
        }
        default: break;
        }
    }
    return int(cur - moves);
}

// 显式实例化
template int generate<GEN_ALL>(const Position&, ExtMove*);
template int generate<GEN_CAPTURES>(const Position&, ExtMove*);
template int generate<GEN_QUIETS>(const Position&, ExtMove*);

int generate_legal(Position& pos, ExtMove* moves) {
    ExtMove pseudo[MAX_MOVES];
    int n = generate<GEN_ALL>(pos, pseudo);
    int out = 0;
    for (int i = 0; i < n; ++i) {
        pos.make_move(pseudo[i].move);
        // 我方将不被攻击 + 不飞将
        Color us = ~pos.side_to_move();  // 走完后 stm 已切换
        if (!pos.is_attacked(pos.king_square(us), ~us) && !pos.kings_face_each_other())
            moves[out++] = pseudo[i];
        pos.unmake_move();
    }
    return out;
}

}  // namespace xq
