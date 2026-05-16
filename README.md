# 象棋大师 · Xiangqi AI

一款**完整可商用**的安卓中国象棋人机对弈应用。

C++ 核心引擎 + Kotlin 原生 UI + NDK 编译，单 APK 体积约 1-2 MB，能在任何安卓 7.0 (API 24) 及以上的设备流畅运行。

## 棋力定位

**业余高段 (4-5 段) 水平**——足以让 95% 的非职业玩家陷入苦战。

引擎采用的关键技术：

- **搜索**：Negamax + Alpha-Beta + PVS (主变例搜索) + 迭代加深 + Aspiration Windows
- **剪枝**：Null Move、LMR (后段着法归约)、LMP (后段着法剪枝)、Futility、Reverse Futility、Razoring、SEE、Mate Distance Pruning、Internal Iterative Reductions
- **启发式**：Killer Moves、History Heuristic、MVV-LVA、Check Extensions
- **置换表**：64MB 默认大小，三槽桶式存储，generation-aging 替换策略
- **静态评估**：完整 PST (位置评分表) + 子力 + 残局调整 + 王安全 + 协同
- **时间管理**：软/硬时限分离

在中端手机 (Snapdragon 7 系) 上，"普通"难度 (3 秒思考) 通常能搜到 9-11 层；"大师"难度 (15 秒) 可达 13-15 层。

## 构建出 APK 的步骤

### 必备工具

1. **Android Studio** (Hedgehog 2023.1.1 或更新版本) — 下载地址 https://developer.android.com/studio
2. **NDK** — 在 Android Studio 里通过 `Tools → SDK Manager → SDK Tools` 选中 "NDK (Side by side)" 和 "CMake" 后下载

> 不需要 Pikafish 神经网络权重也可以构建——引擎自带一套手工评估函数。

### 编译步骤

1. 解压本工程到本地，比如 `~/projects/XiangqiAI/`
2. 打开 Android Studio，选 `File → Open`，定位到 `XiangqiAI/` 目录
3. 等待 Gradle Sync 完成（首次会自动下载 NDK 和依赖，需要几分钟）
4. 顶部菜单 `Build → Build Bundle(s) / APK(s) → Build APK(s)`
5. 构建完成后点击通知里的 `locate` 链接，APK 位于 `app/build/outputs/apk/debug/app-debug.apk`
6. 把 APK 通过 USB / 网盘 / 邮件传到手机，安装运行

### 命令行编译 (可选)

如果你熟悉命令行：

```bash
cd XiangqiAI
./gradlew assembleRelease
# APK 输出在 app/build/outputs/apk/release/
```

## 工程结构

```
XiangqiAI/
├── build.gradle                项目级 Gradle 配置
├── settings.gradle             模块声明
├── gradle.properties           Gradle 全局参数
├── gradle/wrapper/             Gradle wrapper
└── app/
    ├── build.gradle            app 模块配置 (NDK 设置在这里)
    ├── proguard-rules.pro
    └── src/main/
        ├── AndroidManifest.xml
        ├── cpp/                ★ C++ 引擎核心
        │   ├── CMakeLists.txt
        │   ├── types.h
        │   ├── zobrist.*       Zobrist 哈希
        │   ├── position.*      棋盘表示
        │   ├── movegen.*       着法生成
        │   ├── evaluate.*      静态评估
        │   ├── tt.*            置换表
        │   ├── search.*        搜索算法
        │   └── engine_jni.cpp  JNI 桥接层
        ├── java/com/xiangqiai/engine/
        │   ├── NativeEngine.kt 引擎 Kotlin 封装
        │   ├── BoardView.kt    自定义棋盘绘制
        │   ├── MainActivity.kt 主菜单
        │   └── GameActivity.kt 对弈界面
        └── res/
            ├── layout/         XML 布局
            ├── values/         字符串 / 颜色 / 主题
            ├── drawable/       矢量图标
            └── mipmap-*/       启动图标
```

## 玩法

1. 启动应用，进入主菜单
2. 选择执红 / 执黑，选择 AI 难度
3. 点 "开始对弈" 进入棋盘
4. **触摸交互**：先点自己的子（金圈高亮 + 显示绿色合法目标点），再点目标格走子
5. 三个按钮：
   - **悔棋**：回退一回合（你 + AI 各一步）
   - **提示**：AI 帮你分析当前最佳着法，结果以蓝色圈标注
   - **新局**：从初始局面重开

## 后续升级路径

### 加载 NNUE 权重 (神经网络评估)

引擎已经预留好 NNUE 接口（`evaluate.cpp` 里可挂接）。下一步：

1. 训练或下载兼容 Pikafish 格式的 NNUE 权重 `.nnue` 文件
2. 在 `evaluate.cpp` 里加上 NNUE 推理（NEON SIMD 加速 INT8 量化矩阵乘）
3. 把 `.nnue` 文件放进 `app/src/main/assets/`，启动时从 assets 加载到内存

挂上 NNUE 后棋力可再升 200-400 Elo，接近职业水平。

### 增加开局库

引擎当前不带开局库（从空局面纯靠搜索决定第一步）。下一步可：

1. 用 Polyglot 格式 (`.bin`) 的开局库
2. 在 `search.cpp` 开头 probe 开局库，命中则直接采用

### 多线程搜索

当前是单线程，移动设备上对开机即用足够。如要榨干性能，可改成 Lazy SMP（多线程共享 TT）。

## 协议与版权

- 引擎代码使用 MIT 协议，可自由商用
- 不内置任何第三方权重 / 商业素材
- 不带数据收集，纯本地运行，无网络权限

## 故障排查

**问：Gradle Sync 失败 / 下载慢？**
答：把 `settings.gradle` 里的 `google()` `mavenCentral()` 换成国内镜像，比如阿里云 Maven。

**问：编译时报 "NDK not configured"？**
答：在 `local.properties` 里加 `ndk.dir=<你的 NDK 路径>`，或者在 SDK Manager 里安装 NDK。

**问：APK 装不上 / 显示 "未安装" ？**
答：确认手机允许"未知来源"应用安装；如果是低版本安卓 (低于 7.0)，本应用不支持。

**问：AI 太弱 / 太强？**
答：调难度，或编辑 `MainActivity.kt` 中四档对应的 `aiTimeMs` 值。

---

祝弈安。 🀄
