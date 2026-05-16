// position.cpp
#include "position.h"
#include "zobrist.h"
#include <cstring>
#include <sstream>
#include <iostream>
#include <cstdlib>

namespace xq {

// 棋子的 FEN 字符 (大写红, 小写黑)
// K=将, A=士, B=象, N=马(Knight), R=车, C=炮, P=兵
// 索引必须与 Piece 枚举对齐:
//   0=NO_PIECE, 1-7=红 (K/A/B/N/R/C/P), 8=空位 (NO_COLOR), 9-15=黑 (k/a/b/n/r/c/p)
static const char* PIECE_CHARS = ".KABNRCP.kabnrcp";

static Piece char_to_piece(char c) {
    for (int i = 0; i < 16; ++i)
        if (PIECE_CHARS[i] == c) return Piece(i);
    return NO_PIECE;
}
static char piece_to_char(Piece p) { return PIECE_CHARS[p]; }

Position::Position() { set_startpos(); }

void Position::set_startpos() {
    set("rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - - 0 1");
}

// FEN 格式: 棋盘部分(从 rank 9 黑底到 rank 0 红底,用 "/" 分隔) + 走方 + ...
// 注意 FEN 习惯把黑方放上面 (rank 9 在第一行)
void Position::set(const std::string& fen) {
    std::memset(board_, 0, sizeof(board_));
    std::memset(pieceCount_, 0, sizeof(pieceCount_));
    kingSq_[RED] = kingSq_[BLACK] = SQ_NONE;
    historyIdx_ = 0;
    rule50_ = 0;
    gamePly_ = 0;

    std::istringstream ss(fen);
    std::string boardPart, turnPart;
    ss >> boardPart >> turnPart;

    // 解析棋盘: FEN 第一段从 rank 9 (黑方底线) 到 rank 0 (红方底线)
    int r = 9, f = 0;
    for (char c : boardPart) {
        if (c == '/') { --r; f = 0; }
        else if (c >= '1' && c <= '9') { f += c - '0'; }
        else {
            Piece pc = char_to_piece(c);
            if (pc != NO_PIECE && f < 9 && r >= 0) {
                Square sq = make_square(f, r);
                board_[sq] = pc;
                pieceCount_[pc]++;
                if (type_of(pc) == KING) kingSq_[color_of(pc)] = sq;
                ++f;
            }
        }
    }
    stm_ = (turnPart == "b") ? BLACK : RED;

    // rule50 + gamePly 可选
    std::string dummy;
    int rule50 = 0, gamePly = 1;
    if (ss >> dummy) { /* halfmove field optional */ }
    ss >> rule50 >> gamePly;
    rule50_ = rule50;
    gamePly_ = (gamePly - 1) * 2 + (stm_ == BLACK ? 1 : 0);

    compute_key();
}

void Position::compute_key() {
    key_ = 0;
    for (int s = 0; s < SQUARE_NB; ++s) {
        if (board_[s] != NO_PIECE)
            key_ ^= Zobrist::psq[board_[s]][s];
    }
    if (stm_ == BLACK) key_ ^= Zobrist::side;
}

std::string Position::fen() const {
    std::ostringstream ss;
    for (int r = 9; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 9; ++f) {
            Piece pc = piece_on(make_square(f, r));
            if (pc == NO_PIECE) ++empty;
            else {
                if (empty) { ss << empty; empty = 0; }
                ss << piece_to_char(pc);
            }
        }
        if (empty) ss << empty;
        if (r > 0) ss << '/';
    }
    ss << ' ' << (stm_ == RED ? 'w' : 'b') << " - - " << rule50_ << ' ' << (gamePly_ / 2 + 1);
    return ss.str();
}

std::string Position::to_string() const {
    std::ostringstream ss;
    ss << "  +---------------+\n";
    for (int r = 9; r >= 0; --r) {
        ss << r << " |";
        for (int f = 0; f < 9; ++f) {
            char c = piece_to_char(piece_on(make_square(f, r)));
            ss << c << ' ';
        }
        ss << "|\n";
        if (r == 5) ss << "  | --- 楚河 漢界 --- |\n";
    }
    ss << "  +---------------+\n";
    ss << "    0 1 2 3 4 5 6 7 8\n";
    ss << "Turn: " << (stm_ == RED ? "RED" : "BLACK") << "  Key: " << std::hex << key_ << std::dec << "\n";
    return ss.str();
}

void Position::put_piece(Piece pc, Square s) {
    board_[s] = pc;
    pieceCount_[pc]++;
    if (type_of(pc) == KING) kingSq_[color_of(pc)] = s;
    key_ ^= Zobrist::psq[pc][s];
}

void Position::remove_piece(Square s) {
    Piece pc = piece_on(s);
    key_ ^= Zobrist::psq[pc][s];
    pieceCount_[pc]--;
    board_[s] = NO_PIECE;
}

void Position::move_piece(Square from, Square to) {
    Piece pc = piece_on(from);
    key_ ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    board_[from] = NO_PIECE;
    board_[to] = pc;
    if (type_of(pc) == KING) kingSq_[color_of(pc)] = to;
}

void Position::make_move(Move m) {
    StateInfo& st = history_[historyIdx_];
    st.captured = NO_PIECE;
    st.key = key_;
    st.rule50 = rule50_;
    st.kingSq[0] = kingSq_[0];
    st.kingSq[1] = kingSq_[1];
    st.move = m;
    historyKeys_[historyIdx_] = key_;
    ++historyIdx_;

    Square from = from_sq(m), to = to_sq(m);
    Piece moved = piece_on(from);
    Piece captured = piece_on(to);

    if (captured != NO_PIECE) {
        st.captured = captured;
        remove_piece(to);
        rule50_ = 0;
    } else {
        rule50_++;
    }
    if (type_of(moved) == PAWN) rule50_ = 0;

    move_piece(from, to);

    stm_ = ~stm_;
    key_ ^= Zobrist::side;
    gamePly_++;
}

void Position::unmake_move() {
    --historyIdx_;
    const StateInfo& st = history_[historyIdx_];
    Move m = st.move;
    Square from = from_sq(m), to = to_sq(m);

    stm_ = ~stm_;
    gamePly_--;

    // 撤销移动
    Piece moved = piece_on(to);
    board_[from] = moved;
    board_[to] = NO_PIECE;
    if (type_of(moved) == KING) kingSq_[color_of(moved)] = from;

    if (st.captured != NO_PIECE) {
        board_[to] = st.captured;
        pieceCount_[st.captured]++;
    }

    key_ = st.key;
    rule50_ = st.rule50;
    kingSq_[0] = st.kingSq[0];
    kingSq_[1] = st.kingSq[1];
}

void Position::make_null_move() {
    StateInfo& st = history_[historyIdx_];
    st.captured = NO_PIECE;
    st.key = key_;
    st.rule50 = rule50_;
    st.kingSq[0] = kingSq_[0];
    st.kingSq[1] = kingSq_[1];
    st.move = MOVE_NONE;
    historyKeys_[historyIdx_] = key_;
    ++historyIdx_;

    stm_ = ~stm_;
    key_ ^= Zobrist::side;
    gamePly_++;
}

void Position::unmake_null_move() {
    --historyIdx_;
    const StateInfo& st = history_[historyIdx_];
    stm_ = ~stm_;
    gamePly_--;
    key_ = st.key;
    rule50_ = st.rule50;
}

// ============== 攻击检测 ==============
// is_attacked(s, by): 检查 by 方的任何棋子是否能攻击到 s

// 马的 8 个走法 (df, dr) 及对应的蹩马腿位 (dfBlock, drBlock)
static const int HORSE_OFFSETS[8][4] = {
    // df, dr, blockDf, blockDr
    { 1,  2,  0,  1}, {-1,  2,  0,  1},
    { 1, -2,  0, -1}, {-1, -2,  0, -1},
    { 2,  1,  1,  0}, { 2, -1,  1,  0},
    {-2,  1, -1,  0}, {-2, -1, -1,  0}
};

// 象走田字 4 方向 + 塞象眼
static const int ELEPHANT_OFFSETS[4][4] = {
    { 2,  2,  1,  1}, { 2, -2,  1, -1},
    {-2,  2, -1,  1}, {-2, -2, -1, -1}
};

// 士斜走 4 方向
static const int ADVISOR_OFFSETS[4][2] = {
    { 1,  1}, { 1, -1}, {-1,  1}, {-1, -1}
};

// 将/兵直走 4 方向
static const int ORTHO_OFFSETS[4][2] = {
    { 0,  1}, { 0, -1}, { 1,  0}, {-1,  0}
};

bool Position::is_attacked(Square s, Color by) const {
    int tf = file_of(s), tr = rank_of(s);

    // --- 兵攻击 ---
    // 红兵向上攻击 (rank +1), 过河后还可左右 (file ±1)
    // 黑兵向下攻击 (rank -1), 过河后还可左右
    {
        Piece enemyPawn = make_piece(by, PAWN);
        int forwardDr = (by == RED) ? 1 : -1;
        // 1) 前方攻击: 兵在 s 的"后方" (-forwardDr)
        int pf = tf, pr = tr - forwardDr;
        if (pf >= 0 && pf < 9 && pr >= 0 && pr < 10) {
            Square ps = make_square(pf, pr);
            if (piece_on(ps) == enemyPawn) return true;
        }
        // 2) 左右攻击: 兵在 s 同行的左/右,且兵已过河
        // 兵已过河: 红兵在 rank >=5; 黑兵在 rank <=4
        for (int df : {-1, 1}) {
            int pf2 = tf + df, pr2 = tr;
            if (pf2 >= 0 && pf2 < 9 && pr2 >= 0 && pr2 < 10) {
                Square ps = make_square(pf2, pr2);
                if (piece_on(ps) == enemyPawn && crossed_river(by, ps)) return true;
            }
        }
    }

    // --- 马攻击 (反向: 检查能否从 s 出发用马的走法走到一个敌方马) ---
    // 这里要小心: 我们要找能攻击 s 的敌马。
    // 等价于: 从敌马位置 ms 出发, 经马的走法能到 s。
    // 但反向逻辑也对: 从 s 出发用"反向马走法"找出 ms 候选, 同时检查蹩马腿(在 ms 的腿位)。
    // 蹩马腿位置 = 在 ms 旁(沿主方向上一格), 即从 s 反推时, 腿位 = ms + (dfBlock-df_total/2, drBlock-dr_total/2)... 太复杂
    // 直接: 遍历 8 个候选敌马位置 ms = s + (df, dr), 然后检查 ms 是否真的能跳到 s (即从 ms 角度的腿位是否阻挡)
    {
        Piece enemyHorse = make_piece(by, HORSE);
        for (const auto& off : HORSE_OFFSETS) {
            int df = off[0], dr = off[1];
            int mf = tf + df, mr = tr + dr;
            if (mf < 0 || mf >= 9 || mr < 0 || mr >= 10) continue;
            Square ms = make_square(mf, mr);
            if (piece_on(ms) != enemyHorse) continue;
            // 蹩马腿: 从 ms 出发要到 s, 走 (-df, -dr), 腿位在 ms + (-blockDf, -blockDr)
            // 因为 off 表的 block 是相对 from 的方向, 这里 from=ms, to=s, so block 位置 = ms + (-off[2], -off[3])
            int bf = mf - off[2], br = mr - off[3];
            Square bs = make_square(bf, br);
            if (piece_on(bs) == NO_PIECE) return true;
        }
    }

    // --- 象攻击 ---
    {
        Piece enemyElephant = make_piece(by, ELEPHANT);
        for (const auto& off : ELEPHANT_OFFSETS) {
            int df = off[0], dr = off[1];
            int ef = tf + df, er = tr + dr;
            if (ef < 0 || ef >= 9 || er < 0 || er >= 10) continue;
            Square es = make_square(ef, er);
            if (piece_on(es) != enemyElephant) continue;
            // 象不能过河
            if (crossed_river(by, es)) continue;
            // 塞象眼: 从 es 到 s, 眼位在 es + (-blockDf, -blockDr)
            int bf = ef - off[2], br = er - off[3];
            Square bs = make_square(bf, br);
            if (piece_on(bs) == NO_PIECE) return true;
        }
    }

    // --- 士攻击 ---
    {
        Piece enemyAdvisor = make_piece(by, ADVISOR);
        for (const auto& off : ADVISOR_OFFSETS) {
            int af = tf + off[0], ar = tr + off[1];
            if (af < 0 || af >= 9 || ar < 0 || ar >= 10) continue;
            Square as = make_square(af, ar);
            if (piece_on(as) != enemyAdvisor) continue;
            if (!in_own_palace(by, as)) continue;
            return true;
        }
    }

    // --- 将攻击 (含飞将) ---
    {
        Piece enemyKing = make_piece(by, KING);
        for (const auto& off : ORTHO_OFFSETS) {
            int kf = tf + off[0], kr = tr + off[1];
            if (kf < 0 || kf >= 9 || kr < 0 || kr >= 10) continue;
            Square ks = make_square(kf, kr);
            if (piece_on(ks) == enemyKing && in_own_palace(by, ks)) return true;
        }
    }

    // --- 车攻击 (4 方向直线, 第一个阻挡物若为敌车则攻击成立) ---
    {
        Piece enemyRook = make_piece(by, ROOK);
        for (const auto& off : ORTHO_OFFSETS) {
            int df = off[0], dr = off[1];
            int cf = tf + df, cr = tr + dr;
            while (cf >= 0 && cf < 9 && cr >= 0 && cr < 10) {
                Square cs = make_square(cf, cr);
                Piece pc = piece_on(cs);
                if (pc != NO_PIECE) {
                    if (pc == enemyRook) return true;
                    break;
                }
                cf += df; cr += dr;
            }
        }
    }

    // --- 炮攻击 (需要恰好一个炮架) ---
    {
        Piece enemyCannon = make_piece(by, CANNON);
        for (const auto& off : ORTHO_OFFSETS) {
            int df = off[0], dr = off[1];
            int cf = tf + df, cr = tr + dr;
            // 找第一个阻挡物 (炮架)
            while (cf >= 0 && cf < 9 && cr >= 0 && cr < 10 && piece_on(make_square(cf, cr)) == NO_PIECE) {
                cf += df; cr += dr;
            }
            if (cf < 0 || cf >= 9 || cr < 0 || cr >= 10) continue;  // 没有炮架
            // 越过炮架继续找
            cf += df; cr += dr;
            while (cf >= 0 && cf < 9 && cr >= 0 && cr < 10) {
                Square cs = make_square(cf, cr);
                Piece pc = piece_on(cs);
                if (pc != NO_PIECE) {
                    if (pc == enemyCannon) return true;
                    break;
                }
                cf += df; cr += dr;
            }
        }
    }

    return false;
}

bool Position::kings_face_each_other() const {
    Square rk = kingSq_[RED], bk = kingSq_[BLACK];
    if (rk == SQ_NONE || bk == SQ_NONE) return false;
    if (file_of(rk) != file_of(bk)) return false;
    int f = file_of(rk);
    int r1 = rank_of(rk), r2 = rank_of(bk);
    int lo = std::min(r1, r2), hi = std::max(r1, r2);
    for (int r = lo + 1; r < hi; ++r) {
        if (piece_on(make_square(f, r)) != NO_PIECE) return false;
    }
    return true;
}

bool Position::is_legal(Move m) const {
    // 走完后: 1) 我方将不被攻击 2) 两将不面对面
    // 这是在 make_move 之后调用的检查 -- 这里我们要求 caller 已经 make 过
    // 简便起见: 提供一个 "走完后是否合法" 的版本
    // 实际接口: 在 search 中, 先 make, 再 in_check() && !kings_face, 否则 unmake 撤销
    (void)m;
    return !is_attacked(kingSq_[~stm_], stm_) && !kings_face_each_other();
}

bool Position::gives_check(Move m) const {
    // 简化实现: 临时 make 后看是否将军
    // (高效版本可静态分析,但移动端这里不是热点)
    Position* self = const_cast<Position*>(this);
    self->make_move(m);
    bool check = self->in_check();
    self->unmake_move();
    return check;
}

bool Position::is_repetition(int searchPly) const {
    // 检查当前 key 是否在历史中重复出现
    // searchPly: 距离根节点的层数, 用于区分搜索内 vs 全局历史
    if (historyIdx_ < 4) return false;
    uint64_t cur = key_;
    int count = 0;
    // 只检查 rule50 范围内的最近历史 (相同 key 必出现在这范围内)
    int limit = std::min(rule50_, historyIdx_);
    for (int i = historyIdx_ - 2; i >= historyIdx_ - limit; i -= 2) {
        if (i < 0) break;
        if (historyKeys_[i] == cur) {
            count++;
            if (count >= 1) return true;  // 一次重复就当作准和棋, 搜索时会被处理
        }
    }
    return false;
}

int Position::non_pawn_material(Color c) const {
    int v = 0;
    v += pieceCount_[make_piece(c, ADVISOR)]  * PieceValue[ADVISOR];
    v += pieceCount_[make_piece(c, ELEPHANT)] * PieceValue[ELEPHANT];
    v += pieceCount_[make_piece(c, HORSE)]    * PieceValue[HORSE];
    v += pieceCount_[make_piece(c, ROOK)]     * PieceValue[ROOK];
    v += pieceCount_[make_piece(c, CANNON)]   * PieceValue[CANNON];
    return v;
}

bool Position::only_pawns_left(Color c) const {
    return pieceCount_[make_piece(c, HORSE)] == 0 &&
           pieceCount_[make_piece(c, ROOK)] == 0 &&
           pieceCount_[make_piece(c, CANNON)] == 0;
}

void Position::mirror() {
    // 仅用于测试: 红黑互换并垂直翻转
    int newBoard[SQUARE_NB] = {0};
    for (int s = 0; s < SQUARE_NB; ++s) {
        int f = file_of(s), r = rank_of(s);
        int newR = 9 - r;
        Square ns = make_square(f, newR);
        Piece p = piece_on(s);
        if (p != NO_PIECE) {
            Color nc = ~color_of(p);
            newBoard[ns] = make_piece(nc, type_of(p));
        }
    }
    std::memset(pieceCount_, 0, sizeof(pieceCount_));
    for (int s = 0; s < SQUARE_NB; ++s) {
        board_[s] = newBoard[s];
        if (newBoard[s] != NO_PIECE) {
            pieceCount_[newBoard[s]]++;
            if (type_of(Piece(newBoard[s])) == KING)
                kingSq_[color_of(Piece(newBoard[s]))] = s;
        }
    }
    stm_ = ~stm_;
    compute_key();
}

}  // namespace xq
