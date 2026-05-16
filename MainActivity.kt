package com.xiangqiai.engine

import android.content.Intent
import android.os.Bundle
import android.widget.Button
import android.widget.RadioButton
import android.widget.RadioGroup
import androidx.appcompat.app.AppCompatActivity

/**
 * MainActivity - 主菜单
 *
 * 让玩家:
 *  1. 选择执子颜色 (红方先手 / 黑方后手)
 *  2. 选择 AI 难度 (映射到 AI 思考时间)
 *  3. 启动 GameActivity
 */
class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        findViewById<Button>(R.id.btnStart).setOnClickListener {
            val colorGroup = findViewById<RadioGroup>(R.id.colorGroup)
            val humanColor = when (colorGroup.checkedRadioButtonId) {
                R.id.rbBlack -> NativeEngine.BLACK
                else         -> NativeEngine.RED
            }

            val diffGroup = findViewById<RadioGroup>(R.id.difficultyGroup)
            val aiTimeMs = when (diffGroup.checkedRadioButtonId) {
                R.id.rbEasy   -> 800
                R.id.rbHard   -> 6000
                R.id.rbExpert -> 15000
                else          -> 3000  // 普通
            }

            startActivity(Intent(this, GameActivity::class.java).apply {
                putExtra(GameActivity.EXTRA_HUMAN_COLOR, humanColor)
                putExtra(GameActivity.EXTRA_AI_TIME_MS, aiTimeMs)
            })
        }
    }
}
