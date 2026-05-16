package com.xiangqiai.engine

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View

/**
 * BoardView - 9x10 中国象棋棋盘的自定义视图。
 *
 * 坐标:
 *   square = file + rank*9  (与引擎一致)
 *   屏幕坐标: file 0-8 从左到右; rank 0-9 从下到上 (红下黑上)
 *
 * 交互:
 *   - 第一次点击: 选中自己的子, 显示合法目标点
 *   - 第二次点击: 若是合法目标格, 走子; 否则取消或重选
 *
 * 走动画: 棋子从 from 平滑滑到 to (200ms ease-out)
 */
class BoardView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : View(context, attrs, defStyle) {

    // ===== 配置 =====
    private var board = IntArray(90)  // 引擎棋盘状态
    private var redOnBottom = true     // 红在下 (默认)
    var humanColor: Int = NativeEngine.RED  // 玩家执子

    // 选择状态
    private var selectedSq: Int = -1
    private var legalTargets: IntArray = IntArray(0)

    // 最后一手 (用于标注)
    private var lastFrom: Int = -1
    private var lastTo: Int = -1

    // 走子动画
    private var animFrom: Int = -1
    private var animTo: Int = -1
    private var animProgress: Float = 0f  // 0..1
    private var animatingPiece: Int = 0
    private var animator: ValueAnimator? = null
    private var animationEndCallback: (() -> Unit)? = null

    // 监听器: UI 层处理走子
    var moveListener: ((from: Int, to: Int) -> Boolean)? = null

    // ===== 画笔 =====
    private val boardPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val linePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(60, 40, 20); style = Paint.Style.STROKE; strokeWidth = 2f
    }
    private val borderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(60, 40, 20); style = Paint.Style.STROKE; strokeWidth = 5f
    }
    private val riverTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(100, 70, 40); textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.SERIF, Typeface.BOLD)
    }
    private val pieceBgPaint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pieceBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeWidth = 3f
    }
    private val pieceTextPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = Typeface.create(Typeface.SERIF, Typeface.BOLD)
    }
    private val highlightPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeWidth = 6f
        color = Color.rgb(255, 200, 0)
    }
    private val legalDotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(150, 30, 180, 60); style = Paint.Style.FILL
    }
    private val lastMovePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeWidth = 4f
        color = Color.argb(180, 80, 130, 255)
    }

    // ===== 尺寸 =====
    private var cellSize: Float = 0f
    private var marginX: Float = 0f
    private var marginY: Float = 0f
    private var pieceRadius: Float = 0f

    init {
        isClickable = true
        isFocusable = true
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        // 9 列宽, 10 行高, 加上外边距
        val cellW = w / 9.5f
        val cellH = h / 10.5f
        cellSize = minOf(cellW, cellH)
        marginX = (w - cellSize * 8) / 2f
        marginY = (h - cellSize * 9) / 2f
        pieceRadius = cellSize * 0.42f
        pieceTextPaint.textSize = cellSize * 0.55f
        riverTextPaint.textSize = cellSize * 0.6f
    }

    override fun onDraw(canvas: Canvas) {
        drawBoardBackground(canvas)
        drawGrid(canvas)
        drawPalaceDiagonals(canvas)
        drawRiverText(canvas)
        drawStarPoints(canvas)
        drawLastMoveMark(canvas)
        drawPieces(canvas)
        drawSelection(canvas)
        drawLegalDots(canvas)
        drawAnimatingPiece(canvas)
    }

    private fun drawBoardBackground(canvas: Canvas) {
        // 木色渐变
        val w = width.toFloat(); val h = height.toFloat()
        boardPaint.shader = LinearGradient(0f, 0f, 0f, h,
            Color.rgb(245, 215, 170), Color.rgb(225, 190, 130),
            Shader.TileMode.CLAMP)
        canvas.drawRect(0f, 0f, w, h, boardPaint)
        boardPaint.shader = null
    }

    private fun drawGrid(canvas: Canvas) {
        // 9 条竖线 (但中间列在河界处断开,符合传统画法)
        for (i in 0..8) {
            val x = marginX + i * cellSize
            // 红方 (rank 0-4): 从底部到河界 (rank 4)
            canvas.drawLine(x, marginY + 5 * cellSize, x, marginY + 9 * cellSize, linePaint)
            // 黑方 (rank 5-9): 从河界到顶部
            canvas.drawLine(x, marginY + 0 * cellSize, x, marginY + 4 * cellSize, linePaint)
        }
        // 两侧外框跨过河界
        canvas.drawLine(marginX, marginY, marginX, marginY + 9 * cellSize, linePaint)
        canvas.drawLine(marginX + 8 * cellSize, marginY, marginX + 8 * cellSize, marginY + 9 * cellSize, linePaint)

        // 10 条横线
        for (j in 0..9) {
            val y = marginY + j * cellSize
            canvas.drawLine(marginX, y, marginX + 8 * cellSize, y, linePaint)
        }

        // 边框 (粗线包围)
        canvas.drawRect(marginX - 6, marginY - 6,
                        marginX + 8 * cellSize + 6, marginY + 9 * cellSize + 6, borderPaint)
    }

    private fun drawPalaceDiagonals(canvas: Canvas) {
        // 红方九宫斜线 (rank 0-2, file 3-5)
        // rank 0 在屏幕底部
        val redRank0Y = marginY + 9 * cellSize  // rank 0 屏幕 Y
        val redRank2Y = marginY + 7 * cellSize  // rank 2 屏幕 Y
        canvas.drawLine(marginX + 3 * cellSize, redRank2Y,
                        marginX + 5 * cellSize, redRank0Y, linePaint)
        canvas.drawLine(marginX + 5 * cellSize, redRank2Y,
                        marginX + 3 * cellSize, redRank0Y, linePaint)
        // 黑方九宫斜线 (rank 7-9)
        val blackRank9Y = marginY + 0 * cellSize  // rank 9 顶部
        val blackRank7Y = marginY + 2 * cellSize  // rank 7
        canvas.drawLine(marginX + 3 * cellSize, blackRank9Y,
                        marginX + 5 * cellSize, blackRank7Y, linePaint)
        canvas.drawLine(marginX + 5 * cellSize, blackRank9Y,
                        marginX + 3 * cellSize, blackRank7Y, linePaint)
    }

    private fun drawRiverText(canvas: Canvas) {
        val cy = marginY + 4.5f * cellSize + riverTextPaint.textSize / 3f
        canvas.drawText("楚 河", marginX + 2 * cellSize, cy, riverTextPaint)
        canvas.drawText("漢 界", marginX + 6 * cellSize, cy, riverTextPaint)
    }

    private fun drawStarPoints(canvas: Canvas) {
        // 兵 / 炮位置的星号标记 (传统棋盘装饰)
        val points = arrayOf(
            // file, rank
            1 to 2, 7 to 2, 0 to 3, 2 to 3, 4 to 3, 6 to 3, 8 to 3,
            1 to 7, 7 to 7, 0 to 6, 2 to 6, 4 to 6, 6 to 6, 8 to 6
        )
        val markSize = cellSize * 0.08f
        val gap = cellSize * 0.12f
        for ((f, r) in points) {
            val pt = squareToScreen(f + r * 9)
            // 画一个 "L" 型十字标 (4 个角的小标记)
            for ((dx, dy) in listOf(-1 to -1, 1 to -1, -1 to 1, 1 to 1)) {
                if (f == 0 && dx == -1) continue
                if (f == 8 && dx == 1) continue
                val x0 = pt.x + dx * gap
                val y0 = pt.y + dy * gap
                canvas.drawLine(x0, y0, x0 + dx * markSize, y0, linePaint)
                canvas.drawLine(x0, y0, x0, y0 + dy * markSize, linePaint)
            }
        }
    }

    private fun drawLastMoveMark(canvas: Canvas) {
        if (lastFrom in 0..89) {
            val p = squareToScreen(lastFrom)
            canvas.drawCircle(p.x, p.y, pieceRadius + 2, lastMovePaint)
        }
        if (lastTo in 0..89) {
            val p = squareToScreen(lastTo)
            canvas.drawCircle(p.x, p.y, pieceRadius + 2, lastMovePaint)
        }
    }

    private fun drawPieces(canvas: Canvas) {
        for (sq in 0 until 90) {
            val pc = board[sq]
            if (pc == 0) continue
            // 跳过正在动画的子
            if (sq == animFrom) continue
            drawPieceAt(canvas, pc, squareToScreen(sq))
        }
    }

    private fun drawPieceAt(canvas: Canvas, piece: Int, pt: PointF) {
        val isRed = NativeEngine.colorOf(piece) == NativeEngine.RED
        // 底色: 米黄
        pieceBgPaint.shader = RadialGradient(
            pt.x - pieceRadius * 0.3f, pt.y - pieceRadius * 0.3f,
            pieceRadius * 1.3f,
            Color.rgb(255, 250, 230), Color.rgb(220, 195, 155),
            Shader.TileMode.CLAMP
        )
        canvas.drawCircle(pt.x, pt.y, pieceRadius, pieceBgPaint)
        // 边框
        pieceBorderPaint.color = if (isRed) Color.rgb(180, 30, 30) else Color.rgb(30, 30, 30)
        canvas.drawCircle(pt.x, pt.y, pieceRadius, pieceBorderPaint)
        // 内圈
        pieceBorderPaint.strokeWidth = 1.5f
        canvas.drawCircle(pt.x, pt.y, pieceRadius * 0.85f, pieceBorderPaint)
        pieceBorderPaint.strokeWidth = 3f
        // 文字
        pieceTextPaint.color = if (isRed) Color.rgb(180, 30, 30) else Color.rgb(30, 30, 30)
        val ch = pieceChar(piece)
        val baseline = pt.y - (pieceTextPaint.descent() + pieceTextPaint.ascent()) / 2
        canvas.drawText(ch, pt.x, baseline, pieceTextPaint)
    }

    private fun drawSelection(canvas: Canvas) {
        if (selectedSq in 0..89) {
            val p = squareToScreen(selectedSq)
            canvas.drawCircle(p.x, p.y, pieceRadius + 4, highlightPaint)
        }
    }

    private fun drawLegalDots(canvas: Canvas) {
        for (sq in legalTargets) {
            val p = squareToScreen(sq)
            if (board[sq] == 0) {
                canvas.drawCircle(p.x, p.y, cellSize * 0.15f, legalDotPaint)
            } else {
                // 吃子目标: 画环
                val ringPaint = Paint(legalDotPaint).apply {
                    style = Paint.Style.STROKE; strokeWidth = 4f
                }
                canvas.drawCircle(p.x, p.y, pieceRadius + 3, ringPaint)
            }
        }
    }

    private fun drawAnimatingPiece(canvas: Canvas) {
        if (animFrom < 0 || animTo < 0) return
        val from = squareToScreen(animFrom)
        val to = squareToScreen(animTo)
        // ease-out cubic
        val t = 1f - (1f - animProgress) * (1f - animProgress) * (1f - animProgress)
        val x = from.x + (to.x - from.x) * t
        val y = from.y + (to.y - from.y) * t
        drawPieceAt(canvas, animatingPiece, PointF(x, y))
    }

    // ============== 坐标转换 ==============
    private fun squareToScreen(sq: Int): PointF {
        val f = sq % 9
        val r = sq / 9
        // 视角: humanColor 决定红是否在下
        val displayR = if (redOnBottom) 9 - r else r
        val displayF = if (redOnBottom) f else 8 - f
        return PointF(marginX + displayF * cellSize, marginY + displayR * cellSize)
    }

    private fun screenToSquare(x: Float, y: Float): Int {
        val df = (x - marginX) / cellSize
        val dr = (y - marginY) / cellSize
        val cf = df.toInt().coerceIn(0, 8)
        val cr = dr.toInt().coerceIn(0, 9)
        // 容差: 距离最近交点的判定
        val nearestF = ((x - marginX) / cellSize).toInt().coerceIn(0, 8)
        val nearestR = ((y - marginY) / cellSize).toInt().coerceIn(0, 9)
        val centerX = marginX + nearestF * cellSize
        val centerY = marginY + nearestR * cellSize
        if (kotlin.math.abs(x - centerX) > cellSize * 0.45f) return -1
        if (kotlin.math.abs(y - centerY) > cellSize * 0.45f) return -1
        val displayR = nearestR
        val displayF = nearestF
        val rankReal = if (redOnBottom) 9 - displayR else displayR
        val fileReal = if (redOnBottom) displayF else 8 - displayF
        return fileReal + rankReal * 9
    }

    // ============== 触摸 ==============
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.action != MotionEvent.ACTION_DOWN) return true
        if (animator?.isRunning == true) return true  // 动画中禁手

        val sq = screenToSquare(event.x, event.y)
        if (sq < 0) {
            // 点空白: 取消选择
            if (selectedSq >= 0) {
                selectedSq = -1
                legalTargets = IntArray(0)
                invalidate()
            }
            return true
        }

        val pc = board[sq]
        if (selectedSq < 0) {
            // 第一次点击: 必须是自己的子
            if (pc != 0 && NativeEngine.colorOf(pc) == humanColor) {
                selectedSq = sq
                // 合法目标点由外部传入 (UI 协调器调 engine.legalTargets)
                onPieceSelected?.invoke(sq)
                invalidate()
            }
        } else {
            // 第二次点击
            if (sq == selectedSq) {
                // 取消
                selectedSq = -1
                legalTargets = IntArray(0)
                invalidate()
            } else if (pc != 0 && NativeEngine.colorOf(pc) == humanColor) {
                // 重选自己的子
                selectedSq = sq
                onPieceSelected?.invoke(sq)
                invalidate()
            } else {
                // 尝试走子
                val from = selectedSq
                if (legalTargets.contains(sq)) {
                    selectedSq = -1
                    legalTargets = IntArray(0)
                    val ok = moveListener?.invoke(from, sq) ?: false
                    if (!ok) invalidate()
                } else {
                    selectedSq = -1
                    legalTargets = IntArray(0)
                    invalidate()
                }
            }
        }
        return true
    }

    var onPieceSelected: ((Int) -> Unit)? = null

    // ============== 公共 API ==============
    fun setBoard(newBoard: IntArray) {
        if (newBoard.size != 90) return
        board = newBoard.copyOf()
        invalidate()
    }

    fun setLegalTargets(targets: IntArray) {
        legalTargets = targets
        invalidate()
    }

    fun clearSelection() {
        selectedSq = -1
        legalTargets = IntArray(0)
        invalidate()
    }

    fun setLastMove(from: Int, to: Int) {
        lastFrom = from
        lastTo = to
        invalidate()
    }

    fun setHumanColor(color: Int) {
        humanColor = color
        redOnBottom = (color == NativeEngine.RED)
        invalidate()
    }

    /** 动画化的走子: 完成后调用 onDone */
    fun animateMove(from: Int, to: Int, piece: Int, onDone: () -> Unit) {
        animFrom = from
        animTo = to
        animatingPiece = piece
        animProgress = 0f
        animationEndCallback = onDone

        animator?.cancel()
        animator = ValueAnimator.ofFloat(0f, 1f).apply {
            duration = 220
            addUpdateListener {
                animProgress = it.animatedValue as Float
                invalidate()
            }
            doOnEnd {
                animFrom = -1
                animTo = -1
                animProgress = 0f
                animationEndCallback?.invoke()
                animationEndCallback = null
                invalidate()
            }
            start()
        }
    }

    private fun ValueAnimator.doOnEnd(block: () -> Unit) {
        addListener(object : android.animation.AnimatorListenerAdapter() {
            override fun onAnimationEnd(animation: android.animation.Animator) { block() }
        })
    }

    // 中文棋子字
    private fun pieceChar(piece: Int): String {
        val isRed = NativeEngine.colorOf(piece) == NativeEngine.RED
        return when (NativeEngine.typeOf(piece)) {
            NativeEngine.KING     -> if (isRed) "帥" else "將"
            NativeEngine.ADVISOR  -> if (isRed) "仕" else "士"
            NativeEngine.ELEPHANT -> if (isRed) "相" else "象"
            NativeEngine.HORSE    -> if (isRed) "傌" else "馬"
            NativeEngine.ROOK     -> if (isRed) "俥" else "車"
            NativeEngine.CANNON   -> if (isRed) "炮" else "砲"
            NativeEngine.PAWN     -> if (isRed) "兵" else "卒"
            else -> ""
        }
    }
}
