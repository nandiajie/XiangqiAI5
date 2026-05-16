// engine_jni.cpp - JNI 桥接层
// Kotlin 调用约定: com.xiangqiai.engine.NativeEngine
#include <jni.h>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include "position.h"
#include "movegen.h"
#include "search.h"
#include "evaluate.h"
#include "tt.h"
#include "zobrist.h"

using namespace xq;

// 全局引擎状态 (单实例)
namespace {
    std::unique_ptr<Position> gPos;
    std::unique_ptr<Search> gSearch;
    std::mutex gMutex;
    bool gInited = false;
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeInit(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gInited) {
        Zobrist::init();
        init_eval_tables();
        TT.resize(64);  // 默认 64MB 置换表 (移动端友好)
        gInited = true;
    }
    gPos = std::make_unique<Position>();
    gSearch = std::make_unique<Search>();
}

JNIEXPORT void JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeSetHashMb(JNIEnv*, jobject, jint mb) {
    std::lock_guard<std::mutex> lock(gMutex);
    TT.resize(std::max(4, std::min(512, (int)mb)));
}

JNIEXPORT jboolean JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeSetFen(JNIEnv* env, jobject, jstring fen) {
    if (!gPos) return JNI_FALSE;
    const char* s = env->GetStringUTFChars(fen, nullptr);
    std::string str(s);
    env->ReleaseStringUTFChars(fen, s);
    std::lock_guard<std::mutex> lock(gMutex);
    gPos->set(str);
    return JNI_TRUE;
}

JNIEXPORT jstring JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeGetFen(JNIEnv* env, jobject) {
    if (!gPos) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(gMutex);
    return env->NewStringUTF(gPos->fen().c_str());
}

JNIEXPORT void JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeSetStartpos(JNIEnv*, jobject) {
    if (!gPos) return;
    std::lock_guard<std::mutex> lock(gMutex);
    gPos->set_startpos();
}

// 返回 90 元素的 IntArray, 每个格子的棋子 (0..15)
JNIEXPORT jintArray JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeGetBoard(JNIEnv* env, jobject) {
    jintArray result = env->NewIntArray(90);
    if (!gPos) return result;
    std::lock_guard<std::mutex> lock(gMutex);
    jint buf[90];
    for (int i = 0; i < 90; ++i) buf[i] = (jint)gPos->piece_on(i);
    env->SetIntArrayRegion(result, 0, 90, buf);
    return result;
}

// 走子: from, to 是 0..89 的 square 编号
// 返回 1 = 成功, 0 = 非法
JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeMakeMove(JNIEnv*, jobject, jint from, jint to) {
    if (!gPos) return 0;
    std::lock_guard<std::mutex> lock(gMutex);
    Move m = make_move(from, to);
    // 验证合法
    ExtMove moves[MAX_MOVES];
    int n = generate_legal(*gPos, moves);
    for (int i = 0; i < n; ++i) {
        if (moves[i].move == m) {
            gPos->make_move(m);
            return 1;
        }
    }
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeUndoMove(JNIEnv*, jobject) {
    if (!gPos) return 0;
    std::lock_guard<std::mutex> lock(gMutex);
    if (gPos->game_ply() <= 0) return 0;
    gPos->unmake_move();
    return 1;
}

// 获取某格的合法目标格 (返回 IntArray of squares)
JNIEXPORT jintArray JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeLegalTargets(JNIEnv* env, jobject, jint from) {
    if (!gPos) return env->NewIntArray(0);
    std::lock_guard<std::mutex> lock(gMutex);
    ExtMove moves[MAX_MOVES];
    int n = generate_legal(*gPos, moves);
    std::vector<jint> targets;
    for (int i = 0; i < n; ++i) {
        if ((int)from_sq(moves[i].move) == from)
            targets.push_back((jint)to_sq(moves[i].move));
    }
    jintArray result = env->NewIntArray((jsize)targets.size());
    if (!targets.empty())
        env->SetIntArrayRegion(result, 0, (jsize)targets.size(), targets.data());
    return result;
}

// 当前是否将军
JNIEXPORT jboolean JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeInCheck(JNIEnv*, jobject) {
    if (!gPos) return JNI_FALSE;
    std::lock_guard<std::mutex> lock(gMutex);
    return gPos->in_check() ? JNI_TRUE : JNI_FALSE;
}

// 当前走子方: 0=红, 1=黑
JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeSideToMove(JNIEnv*, jobject) {
    if (!gPos) return 0;
    std::lock_guard<std::mutex> lock(gMutex);
    return (jint)gPos->side_to_move();
}

// 游戏状态: 0=进行中, 1=红胜, 2=黑胜, 3=和棋
JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeGameStatus(JNIEnv*, jobject) {
    if (!gPos) return 0;
    std::lock_guard<std::mutex> lock(gMutex);
    ExtMove moves[MAX_MOVES];
    int n = generate_legal(*gPos, moves);
    if (n == 0) {
        // 无子可动 (将死或困毙), 中国象棋规则下都判负
        return gPos->side_to_move() == RED ? 2 : 1;
    }
    if (gPos->rule50() >= 60) return 3;  // 60 回合无吃子 = 和
    return 0;
}

// 启动搜索, timeMs = 时间限制, depth = 深度上限 (0 = 不限)
// 返回 编码后的着法: high 16 = from, low 16 = to, 0 = 无着法
JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeSearch(JNIEnv* env, jobject obj,
                                                     jint timeMs, jint depth) {
    if (!gPos || !gSearch) return 0;

    // 准备 search-info 回调到 Kotlin
    jclass cls = env->GetObjectClass(obj);
    jmethodID midInfo = env->GetMethodID(cls, "onSearchInfo", "(IIJJII)V");
    // 签名: (int depth, int score, long nodes, long timeMs, int from, int to)

    JavaVM* vm = nullptr;
    env->GetJavaVM(&vm);

    auto cb = [vm, obj, midInfo](const SearchInfo& info) {
        if (!vm || !midInfo) return;
        JNIEnv* e = nullptr;
        bool attached = false;
        if (vm->GetEnv((void**)&e, JNI_VERSION_1_6) == JNI_EDETACHED) {
            vm->AttachCurrentThread(&e, nullptr);
            attached = true;
        }
        if (e) {
            int from = info.pvLen > 0 ? from_sq(info.pv[0]) : 0;
            int to = info.pvLen > 0 ? to_sq(info.pv[0]) : 0;
            e->CallVoidMethod(obj, midInfo,
                              (jint)info.depth, (jint)info.score,
                              (jlong)info.nodes, (jlong)info.timeMs,
                              (jint)from, (jint)to);
        }
        if (attached) vm->DetachCurrentThread();
    };

    SearchLimits limits;
    limits.movetime = timeMs;
    limits.depth = depth > 0 ? depth : 0;

    Position posCopy;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        posCopy = *gPos;
    }
    gSearch->start(posCopy, limits, cb);

    Move m = gSearch->best_move();
    if (m == MOVE_NONE) return 0;
    return ((jint)from_sq(m) << 16) | (jint)to_sq(m);
}

JNIEXPORT void JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeStopSearch(JNIEnv*, jobject) {
    if (gSearch) gSearch->stop();
}

// 评估当前局面 (调试用)
JNIEXPORT jint JNICALL
Java_com_xiangqiai_engine_NativeEngine_nativeEvaluate(JNIEnv*, jobject) {
    if (!gPos) return 0;
    std::lock_guard<std::mutex> lock(gMutex);
    return (jint)evaluate(*gPos);
}

}  // extern "C"
