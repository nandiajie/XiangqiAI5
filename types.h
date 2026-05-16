// types.h - 象棋引擎核心类型定义
// 棋盘坐标系统:
//   位置 sq = file + rank * 9, 范围 [0, 89]
//   file (列): 0-8 (从红方视角, 左到右)
//   rank (行): 0-9 (从红方视角, 下到上, 0=红方底线, 9=黑方底线)
//   红方在下 (rank 0-4), 黑方在上 (rank 5-9), 河界在 rank 4 和 5 之间
#pragma once
#include <cstdint>
#include <string>
#include <array>

namespace xq {

// 颜色
enum Color : int { RED = 0, BLACK = 1, COLOR_NB = 2, NO_COLOR = 2 };
constexpr Color operator~(Color c) { return Color(c ^ 1); }

// 棋子类型 (无颜色)
enum PieceType : int {
    NO_PIECE_TYPE = 0,
    KING = 1, ADVISOR = 2, ELEPHANT = 3, HORSE = 4,
    ROOK = 5, CANNON = 6, PAWN = 7,
    PIECE_TYPE_NB = 8
};

// 棋子 (含颜色): piece = color * 8 + type
// 红: 1..7, 黑: 9..15
enum Piece : int {
    NO_PIECE = 0,
    R_KING = 1, R_ADVISOR = 2, R_ELEPHANT = 3, R_HORSE = 4,
    R_ROOK = 5, R_CANNON = 6, R_PAWN = 7,
    B_KING = 9, B_ADVISOR = 10, B_ELEPHANT = 11, B_HORSE = 12,
    B_ROOK = 13, B_CANNON = 14, B_PAWN = 15,
    PIECE_NB = 16
};
constexpr Color color_of(Piece p) { return Color(p >> 3); }
constexpr PieceType type_of(Piece p) { return PieceType(p & 7); }
constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) | pt); }

// 棋盘位置: 0..89
using Square = int;
constexpr Square SQ_NONE = -1;
constexpr int SQUARE_NB = 90;
constexpr int FILE_NB = 9;
constexpr int RANK_NB = 10;
constexpr Square make_square(int f, int r) { return Square(f + r * 9); }
constexpr int file_of(Square s) { return s % 9; }
constexpr int rank_of(Square s) { return s / 9; }
inline bool is_ok(Square s) { return s >= 0 && s < 90; }

// 着法编码 (16 位): bits 0-6 from, bits 7-13 to, bits 14-15 flag
// from 0-89, to 0-89 (各需 7 位)
using Move = uint16_t;
constexpr Move MOVE_NONE = 0;
constexpr Move MOVE_NULL = 65;  // from == to == 0 但 flag = 0,与 (0->0) 冲突,用特殊值

inline Move make_move(Square from, Square to) {
    return Move(from | (to << 7));
}
inline Square from_sq(Move m) { return Square(m & 0x7F); }
inline Square to_sq(Move m) { return Square((m >> 7) & 0x7F); }

// 评分常量 (centipawn 单位)
constexpr int VALUE_ZERO = 0;
constexpr int VALUE_DRAW = 0;
constexpr int VALUE_MATE = 30000;
constexpr int VALUE_INFINITE = 32000;
constexpr int VALUE_NONE = 32001;
constexpr int VALUE_MATE_IN_MAX_PLY = VALUE_MATE - 128;

inline int mate_in(int ply) { return VALUE_MATE - ply; }
inline int mated_in(int ply) { return -VALUE_MATE + ply; }

// 子力分值 (中局评估用, 残局值由 evaluate 内部细调)
constexpr int PieceValue[PIECE_TYPE_NB] = {
    0,      // NO_PIECE_TYPE
    10000,  // KING (不参与子力计算,但需要占位)
    200,    // ADVISOR 士
    200,    // ELEPHANT 象
    400,    // HORSE 马
    900,    // ROOK 车
    450,    // CANNON 炮
    100     // PAWN 兵
};

// 搜索深度上限
constexpr int MAX_PLY = 128;
constexpr int MAX_MOVES = 128;  // 单局面最大合法着法 (象棋最多约 100 个)

// 局面状态边界 (用于重复局面/和棋判定)
constexpr int MAX_HISTORY = 1024;

// 工具: 棋盘外判定 (用于着法生成的安全索引)
inline bool on_board(int f, int r) { return f >= 0 && f < 9 && r >= 0 && r >= 0 && r < 10; }

// 红方/黑方阵地判定
inline bool is_red_side(Square s) { return rank_of(s) < 5; }
inline bool is_black_side(Square s) { return rank_of(s) >= 5; }

// 九宫判定: 红九宫 file 3-5, rank 0-2; 黑九宫 file 3-5, rank 7-9
inline bool in_red_palace(Square s) {
    int f = file_of(s), r = rank_of(s);
    return f >= 3 && f <= 5 && r >= 0 && r <= 2;
}
inline bool in_black_palace(Square s) {
    int f = file_of(s), r = rank_of(s);
    return f >= 3 && f <= 5 && r >= 7 && r <= 9;
}
inline bool in_own_palace(Color c, Square s) {
    return c == RED ? in_red_palace(s) : in_black_palace(s);
}

// 对方阵营 (用于过河判定: 兵过河 / 象不过河)
inline bool crossed_river(Color c, Square s) {
    return c == RED ? rank_of(s) >= 5 : rank_of(s) <= 4;
}

}  // namespace xq
