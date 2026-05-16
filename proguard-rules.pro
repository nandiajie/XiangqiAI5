# 保留 JNI 类不被混淆 (native 库通过 JNI 名查找 Java 方法)
-keep class com.xiangqiai.engine.NativeEngine { *; }
-keepclassmembers class com.xiangqiai.engine.NativeEngine {
    native <methods>;
    public void onSearchInfo(int, int, long, long, int, int);
}

# 保留 Activity (启动器和 manifest 引用)
-keep public class com.xiangqiai.engine.MainActivity
-keep public class com.xiangqiai.engine.GameActivity

# 自定义 View 在 XML 中通过类全名实例化
-keep public class com.xiangqiai.engine.BoardView { *; }
