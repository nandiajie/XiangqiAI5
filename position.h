// position.h - 棋盘局面表示
#pragma once
#include "types.h"
#include <string>
#include <array>

namespace xq {

// 单步历史信息 (用于 unmake_move 回滚)
struct StateInfo {
    Piece captured;          // 被吃子 (NO_PIECE 表示无)
    uint64_t key;            // 走子前的 Zobrist hash
    int rule50;              // 自上次吃子/兵卒动以来的回合数
    Square kingSq[2];        // 走子前双方将位
    Move move;               // 这步走的着法
};

class Position {
public:
    Position();
    void set_startpos();
    void set(const std::string& fen);
    std::string fen() const;
    std::string to_string() const;

    // 走子 / 撤销
    void make_move(Move m);
    void unmake_move();
    void make_null_move();
    void unmake_null_move();

    // 查询
    Piece piece_on(Square s) const { return Piece(board_[s]); }
    Color side_to_move() const { return stm_; }
    Square king_square(Color c) const { return kingSq_[c]; }
    uint64_t key() const { return key_; }
    int rule50() const { return rule50_; }
    int game_ply() const { return gamePly_; }
    int piece_count(Piece p) const { return pieceCount_[p]; }
    int piece_count(Color c, PieceType pt) const { return pieceCount_[make_piece(c, pt)]; }

    // 合法性 / 将军判定
    bool is_attacked(Square s, Color by) const;
    bool in_check() const { return is_attacked(kingSq_[stm_], ~stm_); }
    bool kings_face_each_other() const;  // 飞将检测
    bool is_legal(Move m) const;          // 走完是否非法 (将被攻 / 飞将)
    bool gives_check(Move m) const;       // 此着是否将军对方

    // 重复局面检测 (3-fold repetition)
    bool is_repetition(int searchPly) const;

    // 子力总值 (不含将)
    int non_pawn_material(Color c) const;
    bool only_pawns_left(Color c) const;

    // 辅助
    void put_piece(Piece pc, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);

    // 调试
    void mirror();  // 红黑互换 (用于评估对称性测试)

private:
    int board_[SQUARE_NB];     // board_[sq] = Piece (用 int 存储便于 SIMD 未来扩展)
    Square kingSq_[COLOR_NB];
    int pieceCount_[PIECE_NB];
    Color stm_;
    uint64_t key_;
    int rule50_;
    int gamePly_;

    StateInfo history_[MAX_HISTORY];
    int historyIdx_;

    // 历史 key 数组 (用于重复局面检测)
    uint64_t historyKeys_[MAX_HISTORY];

    void compute_key();
};

// FEN 工具
bool fen_to_position(const std::string& fen, Position& pos);
std::string position_to_fen(const Position& pos);

}  // namespace xq
