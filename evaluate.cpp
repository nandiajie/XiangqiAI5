// evaluate.cpp - 手工评估函数
// 设计理念: 在没有 NNUE 权重时, 提供职业业余强手水平的棋感
// 总分构成:
//   - 子力 (Material)
//   - 位置 (Piece-Square Tables, PST)
//   - 机动性 (Mobility)
//   - 王安全 (King Safety)
//   - 协同 (车控线、双炮、车马炮配合)
//   - 残局调整 (子力少时调整子力价值)
#include "evaluate.h"
#include "movegen.h"

namespace xq {

// ============== PST (Piece-Square Tables) ==============
// 数值是红方视角, 索引 = file + rank*9, rank 0 是红方底线
// 黑方使用时通过 mirror(sq) = (9-rank)*9 + file 翻转

// 兵: 过河后值大增, 越靠近敌九宫越值钱
static const int PAWN_PST[90] = {
    //  f=0   1    2    3    4    5    6    7    8
        0,   0,   0,   0,   0,   0,   0,   0,   0,   // r=0
        0,   0,   0,   0,   0,   0,   0,   0,   0,   // r=1
        0,   0,   0,   0,   0,   0,   0,   0,   0,   // r=2
        0,   0,   0,   0,   0,   0,   0,   0,   0,   // r=3
        2,   0,   8,   0,   8,   0,   8,   0,   2,   // r=4 (河界前)
       10,  18,  22,  35,  40,  35,  22,  18,  10,   // r=5 (刚过河)
       20,  27,  30,  40,  42,  40,  30,  27,  20,   // r=6
       20,  30,  45,  55,  55,  55,  45,  30,  20,   // r=7
       20,  30,  50,  65,  70,  65,  50,  30,  20,   // r=8
       0,   3,   6,   9,  12,   9,   6,   3,   0,    // r=9 (黑方底线: 已沉底)
};

// 马: 中心格、过河后值更高, 角落最低
static const int HORSE_PST[90] = {
       90,  90,  90,  96,  90,  96,  90,  90,  90,   // r=0
       90,  96, 103, 97, 94, 97, 103,  96,  90,
       92,  98,  99, 103, 99, 103,  99,  98,  92,
       93, 108, 100, 107,100, 107, 100, 108,  93,
       90, 100, 99, 103, 104, 103, 99, 100,  90,
       90, 98, 101, 102, 103, 102, 101,  98,  90,
       92, 98, 99, 103, 99, 103, 99, 98, 92,
       93, 99, 103,99, 104, 99, 103, 99,  93,
       90, 96, 103, 97, 94, 97, 103, 96, 90,
       88, 90, 92, 93, 90, 93, 92, 90, 88,
};

// 车: 控线最重要, 边线/底线一般
static const int ROOK_PST[90] = {
      206,208,207,213,214,213,207,208,206,
      206,212,209,216,233,216,209,212,206,
      206,208,207,214,216,214,207,208,206,
      206,213,213,216,216,216,213,213,206,
      208,211,211,214,215,214,211,211,208,
      208,212,212,214,215,214,212,212,208,
      204,209,204,212,214,212,204,209,204,
      198,208,204,212,212,212,204,208,198,
      200,208,206,212,200,212,206,208,200,
      194,206,204,212,200,212,204,206,194,
};

// 炮: 中线和肋道好, 控制效率高
static const int CANNON_PST[90] = {
      100,100, 96, 91, 90, 91, 96,100,100,
       98, 98, 96, 92, 89, 92, 96, 98, 98,
       97, 97, 96, 91, 92, 91, 96, 97, 97,
       96, 99, 99, 98,100, 98, 99, 99, 96,
       96, 96, 96, 96,100, 96, 96, 96, 96,
       95, 96, 99, 96,100, 96, 99, 96, 95,
       96, 96, 96, 96, 96, 96, 96, 96, 96,
       97, 96,100, 99,101, 99,100, 96, 97,
       96, 97, 98, 98, 98, 98, 98, 97, 96,
       96, 96, 97, 99, 99, 99, 97, 96, 96,
};

// 士: 仅九宫 5 个位置
static const int ADVISOR_PST[90] = {
        0,0,0,20,0,20,0,0,0,
        0,0,0,0,23,0,0,0,0,
        0,0,0,20,0,20,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,20,0,20,0,0,0,
        0,0,0,0,23,0,0,0,0,
        0,0,0,20,0,20,0,0,0,
};

// 象: 7 个位置
static const int ELEPHANT_PST[90] = {
       0,0,20,0,0,0,20,0,0,
       0,0,0,0,23,0,0,0,0,
      18,0,0,0,23,0,0,0,18,
       0,0,0,0,0,0,0,0,0,
       0,0,20,0,18,0,20,0,0,
       0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,
      18,0,0,0,23,0,0,0,18,
       0,0,0,0,23,0,0,0,0,
       0,0,20,0,0,0,20,0,0,
};

// 将: 九宫 9 个位置, 居中略奖励
static const int KING_PST[90] = {
        0,0,0,11,15,11,0,0,0,
        0,0,0,2,2,2,0,0,0,
        0,0,0,1,1,1,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,
        0,0,0,1,1,1,0,0,0,
        0,0,0,2,2,2,0,0,0,
        0,0,0,11,15,11,0,0,0,
};

static const int* PST[PIECE_TYPE_NB] = {
    nullptr,
    KING_PST, ADVISOR_PST, ELEPHANT_PST, HORSE_PST,
    ROOK_PST, CANNON_PST, PAWN_PST
};

void init_eval_tables() {
    // 预留: 未来可生成 mirror 表加速
}

// 黑方位置镜像访问 PST
static inline int pst_value(Color c, PieceType pt, Square s) {
    if (c == RED) return PST[pt][s];
    int f = file_of(s), r = rank_of(s);
    return PST[pt][make_square(f, 9 - r)];
}

// ============== 主评估 ==============
int evaluate(const Position& pos) {
    // 累加红方分 - 黑方分, 最后按 stm 调整符号
    int scoreRed = 0, scoreBlack = 0;
    int materialRed = 0, materialBlack = 0;

    int rookCount[2] = {0, 0};
    int cannonCount[2] = {0, 0};
    int horseCount[2] = {0, 0};
    int advisorCount[2] = {0, 0};
    int elephantCount[2] = {0, 0};

    for (int s = 0; s < SQUARE_NB; ++s) {
        Piece pc = pos.piece_on(s);
        if (pc == NO_PIECE) continue;
        Color c = color_of(pc);
        PieceType pt = type_of(pc);

        int matVal = PieceValue[pt];
        int posVal = pst_value(c, pt, s);

        if (c == RED) {
            scoreRed += matVal + posVal;
            if (pt != KING) materialRed += matVal;
        } else {
            scoreBlack += matVal + posVal;
            if (pt != KING) materialBlack += matVal;
        }

        if (pt == ROOK) rookCount[c]++;
        else if (pt == CANNON) cannonCount[c]++;
        else if (pt == HORSE) horseCount[c]++;
        else if (pt == ADVISOR) advisorCount[c]++;
        else if (pt == ELEPHANT) elephantCount[c]++;
    }

    // === 残局调整: 当对方士象残缺时, 攻击子价值上调 ===
    // 经典法则: 缺士时怕车, 缺象时怕炮, 缺士象时怕马八路
    for (int c = 0; c < 2; ++c) {
        int enemy = c ^ 1;
        int defenders = advisorCount[enemy] + elephantCount[enemy];
        if (defenders <= 2) {
            int bonus = (4 - defenders) * 8;  // 每缺一个防御子奖励 8 cp
            int rookBonus = bonus * rookCount[c];
            int cannonBonus = (advisorCount[enemy] <= 1 ? bonus / 2 : 0) * cannonCount[c];
            int horseBonus = (defenders <= 1 ? bonus / 2 : 0) * horseCount[c];
            if (c == RED) scoreRed += rookBonus + cannonBonus + horseBonus;
            else          scoreBlack += rookBonus + cannonBonus + horseBonus;
        }
    }

    // === 双车协同 ===
    if (rookCount[RED]  >= 2) scoreRed   += 15;
    if (rookCount[BLACK] >= 2) scoreBlack += 15;

    // === 双炮协同 ===
    if (cannonCount[RED]  >= 2) scoreRed   += 10;
    if (cannonCount[BLACK] >= 2) scoreBlack += 10;

    // === 王安全: 检查九宫附近敌方棋子数 ===
    // 简化: 缺士缺象时直接惩罚
    {
        int redDef = advisorCount[RED] + elephantCount[RED];
        int blackDef = advisorCount[BLACK] + elephantCount[BLACK];
        scoreRed   -= (4 - redDef)   * 6;
        scoreBlack -= (4 - blackDef) * 6;

        // 单缺士单缺象本身有惩罚 (士象不全)
        if (advisorCount[RED]  == 1) scoreRed   -= 8;
        if (elephantCount[RED] == 1) scoreRed   -= 5;
        if (advisorCount[BLACK]  == 1) scoreBlack -= 8;
        if (elephantCount[BLACK] == 1) scoreBlack -= 5;
    }

    // === 飞将危险检测 ===
    // 在 search 中已经过滤了飞将局面,这里给一个倾向: 双方将在同一列时给走方一点奖励/惩罚
    if (file_of(pos.king_square(RED)) == file_of(pos.king_square(BLACK))) {
        // 通常这是一种威胁, 谁能利用谁占优 - 暂不细分, 给小幅奖励给走方
        if (pos.side_to_move() == RED) scoreRed += 5;
        else scoreBlack += 5;
    }

    // === 总分 ===
    int score = scoreRed - scoreBlack;

    // 转换为 stm 视角
    return pos.side_to_move() == RED ? score : -score;
}

}  // namespace xq
