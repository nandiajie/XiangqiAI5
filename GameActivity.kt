package com.xiangqiai.engine

import android.app.AlertDialog
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import kotlinx.coroutines.*

class GameActivity : AppCompatActivity() {

    private lateinit var engine: NativeEngine
    private lateinit var boardView: BoardView
    private lateinit var statusText: TextView
    private lateinit var infoText: TextView

    private val mainScope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private var aiJob: Job? = null

    // 配置
    private var humanColor = NativeEngine.RED
    private var aiThinkTimeMs = 3000      // 默认 3 秒
    private var aiMaxDepth = 0            // 0 = 不限

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_game)

        humanColor = intent.getIntExtra(EXTRA_HUMAN_COLOR, NativeEngine.RED)
        aiThinkTimeMs = intent.getIntExtra(EXTRA_AI_TIME_MS, 3000)

        engine = NativeEngine().apply {
            setHashMb(64)
            setStartpos()
            listener = object : NativeEngine.SearchListener {
                override fun onInfo(depth: Int, score: Int, nodes: Long, timeMs: Long,
                                    fromSq: Int, toSq: Int) {
                    runOnUiThread {
                        infoText.text = formatSearchInfo(depth, score, nodes, timeMs)
                    }
                }
            }
        }

        boardView = findViewById(R.id.boardView)
        statusText = findViewById(R.id.statusText)
        infoText = findViewById(R.id.infoText)

        boardView.setHumanColor(humanColor)
        boardView.setBoard(engine.getBoard())

        boardView.onPieceSelected = { sq ->
            boardView.setLegalTargets(engine.legalTargets(sq))
        }
        boardView.moveListener = { from, to ->
            doHumanMove(from, to)
        }

        findViewById<Button>(R.id.btnUndo).setOnClickListener { undoMove() }
        findViewById<Button>(R.id.btnNew).setOnClickListener { newGame() }
        findViewById<Button>(R.id.btnHint).setOnClickListener { showHint() }

        updateStatus()

        // 若 AI 先手, 让 AI 走
        if (humanColor != engine.sideToMove()) {
            aiMove()
        }
    }

    private fun doHumanMove(from: Int, to: Int): Boolean {
        if (engine.sideToMove() != humanColor) return false
        val piece = engine.getBoard()[from]
        if (!engine.makeMove(from, to)) return false
        // 动画化
        boardView.animateMove(from, to, piece) {
            boardView.setBoard(engine.getBoard())
            boardView.setLastMove(from, to)
            updateStatus()
            if (checkGameOver()) return@animateMove
            // 轮到 AI
            aiMove()
        }
        return true
    }

    private fun aiMove() {
        statusText.text = getString(R.string.thinking)
        aiJob?.cancel()
        aiJob = mainScope.launch {
            val result = withContext(Dispatchers.Default) {
                engine.search(aiThinkTimeMs, aiMaxDepth)
            }
            if (result == null) {
                updateStatus()
                checkGameOver()
                return@launch
            }
            val (from, to) = result
            val piece = engine.getBoard()[from]
            if (!engine.makeMove(from, to)) {
                // 不应该发生
                updateStatus()
                return@launch
            }
            boardView.animateMove(from, to, piece) {
                boardView.setBoard(engine.getBoard())
                boardView.setLastMove(from, to)
                updateStatus()
                checkGameOver()
            }
        }
    }

    private fun showHint() {
        if (engine.sideToMove() != humanColor) return
        statusText.text = getString(R.string.hinting)
        mainScope.launch {
            val result = withContext(Dispatchers.Default) {
                engine.search(1500, 0)
            }
            result?.let { (from, to) ->
                boardView.setLastMove(from, to)
                statusText.text = getString(R.string.hint_suggested)
            } ?: updateStatus()
        }
    }

    private fun undoMove() {
        aiJob?.cancel()
        engine.stopSearch()
        // 撤销两步 (AI + human)
        if (engine.undoMove() && engine.sideToMove() != humanColor) {
            engine.undoMove()
        }
        boardView.setBoard(engine.getBoard())
        boardView.clearSelection()
        boardView.setLastMove(-1, -1)
        updateStatus()
    }

    private fun newGame() {
        aiJob?.cancel()
        engine.stopSearch()
        engine.setStartpos()
        boardView.setBoard(engine.getBoard())
        boardView.clearSelection()
        boardView.setLastMove(-1, -1)
        updateStatus()
        if (humanColor != engine.sideToMove()) aiMove()
    }

    private fun checkGameOver(): Boolean {
        val st = engine.gameStatus()
        if (st == 0) return false
        val msg = when (st) {
            1 -> getString(R.string.red_wins)
            2 -> getString(R.string.black_wins)
            3 -> getString(R.string.draw)
            else -> ""
        }
        AlertDialog.Builder(this)
            .setTitle(R.string.game_over)
            .setMessage(msg)
            .setPositiveButton(R.string.new_game) { _, _ -> newGame() }
            .setNegativeButton(R.string.review, null)
            .show()
        return true
    }

    private fun updateStatus() {
        val turn = if (engine.sideToMove() == NativeEngine.RED)
            getString(R.string.red_turn) else getString(R.string.black_turn)
        val check = if (engine.inCheck()) " ⚠ " + getString(R.string.check) else ""
        statusText.text = turn + check
    }

    private fun formatSearchInfo(depth: Int, score: Int, nodes: Long, timeMs: Long): String {
        val nps = if (timeMs > 0) nodes * 1000 / timeMs else 0
        return getString(R.string.search_info_fmt, depth, score, nodes, timeMs, nps)
    }

    override fun onDestroy() {
        super.onDestroy()
        aiJob?.cancel()
        engine.stopSearch()
        mainScope.cancel()
    }

    companion object {
        const val EXTRA_HUMAN_COLOR = "human_color"
        const val EXTRA_AI_TIME_MS = "ai_time_ms"
    }
}
