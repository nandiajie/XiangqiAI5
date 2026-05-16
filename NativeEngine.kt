package com.xiangqiai.engine

/**
 * NativeEngine - C++ 引擎的 Kotlin 桥接。
 *
 * 使用约定:
 *   1. 先 init() 一次
 *   2. setStartpos() 或 setFen(...) 设置局面
 *   3. UI 走子用 makeMove(from, to)
 *   4. AI 思考用 search(timeMs, depth) — 阻塞当前线程, 通常在协程里调
 *   5. 实时进度通过 listener 回调 (onSearchInfo)
 *
 * 坐标系: square = file + rank * 9 (file 0-8, rank 0-9; rank 0 红底)
 * 棋子编码: 0=空; 红 1-7; 黑 9-15  (KING=1, ADVISOR=2, ELEPHANT=3, HORSE=4, ROOK=5, CANNON=6, PAWN=7)
 */
class NativeEngine {

    companion object {
        init { System.loadLibrary("xiangqiengine") }

        const val NO_PIECE = 0
        const val KING = 1
        const val ADVISOR = 2
        const val ELEPHANT = 3
        const val HORSE = 4
        const val ROOK = 5
        const val CANNON = 6
        const val PAWN = 7
        const val RED = 0
        const val BLACK = 1

        fun colorOf(piece: Int): Int = piece shr 3
        fun typeOf(piece: Int): Int = piece and 7
    }

    interface SearchListener {
        fun onInfo(depth: Int, score: Int, nodes: Long, timeMs: Long, fromSq: Int, toSq: Int)
    }

    var listener: SearchListener? = null

    init { nativeInit() }

    fun setHashMb(mb: Int) = nativeSetHashMb(mb)
    fun setStartpos() = nativeSetStartpos()
    fun setFen(fen: String): Boolean = nativeSetFen(fen)
    fun getFen(): String = nativeGetFen()

    /** 获取整个棋盘的 90 元素数组 */
    fun getBoard(): IntArray = nativeGetBoard()

    /** 走子, 返回 true 表示合法 */
    fun makeMove(from: Int, to: Int): Boolean = nativeMakeMove(from, to) != 0

    /** 撤销, 返回 true 表示成功 */
    fun undoMove(): Boolean = nativeUndoMove() != 0

    /** 从某格出发的合法目标格列表 */
    fun legalTargets(from: Int): IntArray = nativeLegalTargets(from)

    fun inCheck(): Boolean = nativeInCheck()
    fun sideToMove(): Int = nativeSideToMove()

    /** 游戏状态: 0=进行中, 1=红胜, 2=黑胜, 3=和棋 */
    fun gameStatus(): Int = nativeGameStatus()

    /** 同步搜索: 阻塞调用线程, 返回 (from, to) Pair 或 null */
    fun search(timeMs: Int, depth: Int = 0): Pair<Int, Int>? {
        val code = nativeSearch(timeMs, depth)
        if (code == 0) return null
        return (code ushr 16) to (code and 0xFFFF)
    }

    fun stopSearch() = nativeStopSearch()

    fun evaluate(): Int = nativeEvaluate()

    // 此方法由 native 层在搜索过程中回调
    @Suppress("unused")
    fun onSearchInfo(depth: Int, score: Int, nodes: Long, timeMs: Long, fromSq: Int, toSq: Int) {
        listener?.onInfo(depth, score, nodes, timeMs, fromSq, toSq)
    }

    // ===== JNI declarations =====
    private external fun nativeInit()
    private external fun nativeSetHashMb(mb: Int)
    private external fun nativeSetStartpos()
    private external fun nativeSetFen(fen: String): Boolean
    private external fun nativeGetFen(): String
    private external fun nativeGetBoard(): IntArray
    private external fun nativeMakeMove(from: Int, to: Int): Int
    private external fun nativeUndoMove(): Int
    private external fun nativeLegalTargets(from: Int): IntArray
    private external fun nativeInCheck(): Boolean
    private external fun nativeSideToMove(): Int
    private external fun nativeGameStatus(): Int
    private external fun nativeSearch(timeMs: Int, depth: Int): Int
    private external fun nativeStopSearch()
    private external fun nativeEvaluate(): Int
}
