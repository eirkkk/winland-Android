#include <jni.h>
#include <string>
#include <android/log.h>
#include <pthread.h>

// تضمين ملفات الهيدر الأصلية
extern "C" {
    #include "wayland_compositor.h"
    #include "egl_display.h"
    #include "input_handler.h"
    #include "audio_bridge.h"
    #include "usb_redirect.h"
}

#define LOG_TAG "WinlandNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// المتغيرات العامة
static struct wl_display *g_display = nullptr;
static struct winland_compositor *g_compositor = nullptr;
static pthread_t g_server_thread;
static bool g_running = false;

// هيكل بيانات للحالة
struct ServerState {
    JNIEnv *env;
    jobject thiz;
    jint width;
    jint height;
};

// دالة تشغيل الخادم في thread منفصل
void* server_thread_func(void *arg) {
    ServerState *state = (ServerState*)arg;
    
    LOGI("Starting Wayland server thread...");
    
    // تهيئة العرض
    if (winland_compositor_init(g_compositor) != 0) {
        LOGE("Failed to initialize compositor");
        return nullptr;
    }
    
    // تشغيل الحلقة الرئيسية
    while (g_running) {
        wl_display_flush_clients(g_display);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 1000; // 1ms timeout
        
        wl_display_flush(g_display);
    }
    
    LOGI("Wayland server thread stopped");
    return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_winland_server_WinlandService_nativeGetVersion(JNIEnv* env, jobject /* this */) {
    std::string version = "Winland Server v1.0.0 - Android Wayland Compositor";
    return env->NewStringUTF(version.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeInitServer(
    JNIEnv* env, 
    jobject thiz,
    jint width,
    jint height,
    jstring socketName
) {
    LOGI("Initializing Winland Server - Resolution: %dx%d", width, height);
    
    // إنشاء عرض Wayland
    g_display = wl_display_create();
    if (!g_display) {
        LOGE("Failed to create Wayland display");
        return -1;
    }
    
    // إنشاء المؤلف
    g_compositor = winland_compositor_create(g_display);
    if (!g_compositor) {
        LOGE("Failed to create compositor");
        wl_display_destroy(g_display);
        return -1;
    }
    
    // إضافة socket
    const char *socket_name = env->GetStringUTFChars(socketName, nullptr);
    int socket_fd = wl_display_add_socket(g_display, socket_name);
    env->ReleaseStringUTFChars(socketName, socket_name);
    
    if (socket_fd < 0) {
        LOGE("Failed to add socket");
        winland_compositor_destroy(g_compositor);
        wl_display_destroy(g_display);
        return -1;
    }
    
    // تهيئة EGL
    if (egl_display_init(width, height) != 0) {
        LOGE("Failed to initialize EGL display");
        return -1;
    }
    
    // تهيئة معالج المدخلات
    if (input_handler_init(g_compositor) != 0) {
        LOGE("Failed to initialize input handler");
        return -1;
    }
    
    LOGI("Winland Server initialized successfully");
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeStartServer(JNIEnv* env, jobject thiz) {
    LOGI("Starting Winland Server...");
    
    if (g_running) {
        LOGI("Server already running");
        return 0;
    }
    
    g_running = true;
    
    ServerState *state = new ServerState();
    state->env = env;
    state->thiz = thiz;
    
    if (pthread_create(&g_server_thread, nullptr, server_thread_func, state) != 0) {
        LOGE("Failed to create server thread");
        g_running = false;
        return -1;
    }
    
    LOGI("Server started successfully");
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeStopServer(JNIEnv* env, jobject /* this */) {
    LOGI("Stopping Winland Server...");
    
    g_running = false;
    
    if (g_server_thread) {
        pthread_join(g_server_thread, nullptr);
        g_server_thread = 0;
    }
    
    if (g_compositor) {
        winland_compositor_destroy(g_compositor);
        g_compositor = nullptr;
    }
    
    if (g_display) {
        wl_display_destroy(g_display);
        g_display = nullptr;
    }
    
    egl_display_terminate();
    input_handler_terminate();
    
    LOGI("Server stopped");
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetSurface(
    JNIEnv* env, 
    jobject /* this */, 
    jobject surface
) {
    LOGI("Setting native surface");
    
    // الحصول على ANativeWindow من Surface
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get ANativeWindow from Surface");
        return;
    }
    
    // تعيين النافذة لـ EGL
    egl_display_set_window(window);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeOnTouch(
    JNIEnv* env, 
    jobject /* this */,
    jint action,
    jfloat x,
    jfloat y,
    jint pointerId
) {
    // إرسال حدث اللمس إلى معالج المدخلات
    input_handler_send_touch_event(action, x, y, pointerId);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeOnKey(
    JNIEnv* env, 
    jobject /* this */,
    jint keyCode,
    jint action,
    jint modifiers
) {
    // إرسال حدث المفتاح إلى معالج المدخلات
    input_handler_send_key_event(keyCode, action, modifiers);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeInitAudioBridge(
    JNIEnv* env, 
    jobject thiz
) {
    LOGI("Initializing Audio Bridge...");
    return audio_bridge_init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeStopAudioBridge(
    JNIEnv* env, 
    jobject thiz
) {
    LOGI("Stopping Audio Bridge...");
    audio_bridge_terminate();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeInitUsbRedirect(
    JNIEnv* env, 
    jobject thiz
) {
    LOGI("Initializing USB Redirect...");
    return usb_redirect_init();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeStopUsbRedirect(
    JNIEnv* env, 
    jobject thiz
) {
    LOGI("Stopping USB Redirect...");
    usb_redirect_terminate();
}

// ==================== Package Manager JNI ====================

#ifdef ENABLE_PACKAGE_MANAGER
extern "C" {
    #include "package_manager.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeRequestPackageList(JNIEnv* env, jobject thiz) {
    LOGI("Requesting package list...");
    // TODO: Implement package list retrieval
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeInstallPackage(
    JNIEnv* env, jobject thiz, jstring packageName
) {
    const char* name = env->GetStringUTFChars(packageName, nullptr);
    LOGI("Installing package: %s", name);
    package_manager_install(name);
    env->ReleaseStringUTFChars(packageName, name);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeRemovePackage(
    JNIEnv* env, jobject thiz, jstring packageName
) {
    const char* name = env->GetStringUTFChars(packageName, nullptr);
    LOGI("Removing package: %s", name);
    package_manager_remove(name);
    env->ReleaseStringUTFChars(packageName, name);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeUpgradePackage(
    JNIEnv* env, jobject thiz, jstring packageName
) {
    const char* name = env->GetStringUTFChars(packageName, nullptr);
    LOGI("Upgrading package: %s", name);
    package_manager_upgrade(name);
    env->ReleaseStringUTFChars(packageName, name);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeUpgradeAllPackages(JNIEnv* env, jobject thiz) {
    LOGI("Upgrading all packages...");
    package_manager_upgrade_all();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeCleanPackageCache(JNIEnv* env, jobject thiz) {
    LOGI("Cleaning package cache...");
    package_manager_clean_cache();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeAutoremovePackages(JNIEnv* env, jobject thiz) {
    LOGI("Autoremoving packages...");
    package_manager_autoremove();
}
#endif // ENABLE_PACKAGE_MANAGER

// ==================== XWayland JNI ====================

#ifdef ENABLE_XWAYLAND
extern "C" {
    #include "xwayland.h"
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeStartXWayland(JNIEnv* env, jobject thiz) {
    LOGI("Starting XWayland...");
    return xwayland_start();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeStopXWayland(JNIEnv* env, jobject thiz) {
    LOGI("Stopping XWayland...");
    xwayland_stop();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_winland_server_WinlandService_nativeIsXWaylandRunning(JNIEnv* env, jobject thiz) {
    return xwayland_is_running() ? JNI_TRUE : JNI_FALSE;
}
#endif // ENABLE_XWAYLAND

// ==================== Notification Bridge JNI ====================

#ifdef ENABLE_NOTIFICATIONS
extern "C" {
    #include "notification_bridge.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeForwardNotification(
    JNIEnv* env, jobject thiz,
    jstring appName, jstring title, jstring body,
    jint priority, jint timeout
) {
    const char* app = env->GetStringUTFChars(appName, nullptr);
    const char* t = env->GetStringUTFChars(title, nullptr);
    const char* b = env->GetStringUTFChars(body, nullptr);
    
    notification_bridge_forward(app, t, b, (notification_priority_t)priority, timeout);
    
    env->ReleaseStringUTFChars(appName, app);
    env->ReleaseStringUTFChars(title, t);
    env->ReleaseStringUTFChars(body, b);
}
#endif // ENABLE_NOTIFICATIONS

// ==================== Keyboard Manager JNI ====================

#ifdef ENABLE_KEYBOARD_MANAGER
extern "C" {
    #include "keyboard_manager.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetKeymap(
    JNIEnv* env, jobject thiz, jstring layout
) {
    const char* l = env->GetStringUTFChars(layout, nullptr);
    
    keymap_type_t keymap = KEYMAP_US;
    if (strcmp(l, "ar") == 0) keymap = KEYMAP_ARABIC;
    else if (strcmp(l, "fr") == 0) keymap = KEYMAP_FRENCH;
    else if (strcmp(l, "de") == 0) keymap = KEYMAP_GERMAN;
    
    keyboard_manager_set_keymap(keymap);
    env->ReleaseStringUTFChars(layout, l);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeEnableShortcuts(
    JNIEnv* env, jobject thiz, jboolean enable
) {
    keyboard_manager_enable_shortcuts(enable == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetPassthroughMode(
    JNIEnv* env, jobject thiz, jboolean enable
) {
    keyboard_manager_set_passthrough(enable == JNI_TRUE);
}
#endif // ENABLE_KEYBOARD_MANAGER

// ==================== Power Manager JNI ====================

#ifdef ENABLE_POWER_MANAGER
extern "C" {
    #include "power_manager.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetPowerMode(
    JNIEnv* env, jobject thiz, jint mode
) {
    power_manager_set_state((power_state_t)mode);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_winland_server_WinlandService_nativeGetPowerMode(JNIEnv* env, jobject thiz) {
    return (jint)power_manager_get_state();
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetIdleTimeout(
    JNIEnv* env, jobject thiz, jint timeoutMs
) {
    power_manager_set_idle_timeout(timeoutMs);
}
#endif // ENABLE_POWER_MANAGER

// ==================== Multi-Display JNI ====================

#ifdef ENABLE_MULTI_DISPLAY
extern "C" {
    #include "multi_display.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeEnableExternalDisplay(
    JNIEnv* env, jobject thiz, jboolean enable
) {
    multi_display_enable_external(enable == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSetDisplayMode(
    JNIEnv* env, jobject thiz, jint mode
) {
    multi_display_set_mode((display_mode_t)mode);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_winland_server_WinlandService_nativeIsExternalDisplayConnected(
    JNIEnv* env, jobject thiz
) {
    return multi_display_is_external_connected() ? JNI_TRUE : JNI_FALSE;
}
#endif // ENABLE_MULTI_DISPLAY

// ==================== File Sharing JNI ====================

#ifdef ENABLE_FILE_SHARING
extern "C" {
    #include "file_sharing.h"
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeSyncFiles(
    JNIEnv* env, jobject thiz,
    jstring androidPath, jstring linuxPath
) {
    const char* aPath = env->GetStringUTFChars(androidPath, nullptr);
    const char* lPath = env->GetStringUTFChars(linuxPath, nullptr);
    
    file_sharing_sync(aPath, lPath);
    
    env->ReleaseStringUTFChars(androidPath, aPath);
    env->ReleaseStringUTFChars(linuxPath, lPath);
}

extern "C" JNIEXPORT void JNICALL
Java_com_winland_server_WinlandService_nativeEnableAutoSync(
    JNIEnv* env, jobject thiz, jboolean enable
) {
    struct sharing_config config;
    file_sharing_get_config(&config);
    config.auto_sync = (enable == JNI_TRUE);
    file_sharing_set_config(&config);
}
#endif // ENABLE_FILE_SHARING
