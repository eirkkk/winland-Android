package com.winland.server;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

/**
 * خدمة Winland Server الرئيسية
 * تقوم بتشغيل Wayland Compositor في الخلفية
 */
public class WinlandService extends Service implements SurfaceHolder.Callback {
    
    private static final String TAG = "WinlandService";
    private static final String CHANNEL_ID = "winland_server_channel";
    private static final int NOTIFICATION_ID = 1;
    
    // حالات الخدمة
    public enum ServiceState {
        STOPPED,
        INITIALIZING,
        RUNNING,
        ERROR
    }
    
    private ServiceState mState = ServiceState.STOPPED;
    private final IBinder mBinder = new LocalBinder();
    private Handler mMainHandler;
    
    // نافذة العرض
    private WindowManager mWindowManager;
    private SurfaceView mSurfaceView;
    private WindowManager.LayoutParams mLayoutParams;
    
    // المستمعون
    private OnServiceStateChangeListener mStateListener;
    private OnTouchEventListener mTouchListener;
    
    // الأبعاد
    private int mWidth = 1920;
    private int mHeight = 1080;
    private String mSocketName = "wayland-0";
    
    // تحميل المكتبة الأصلية
    static {
        System.loadLibrary("winland-native");
    }
    
    // واجهة المستمع لتغييرات الحالة
    public interface OnServiceStateChangeListener {
        void onStateChanged(ServiceState newState);
        void onError(String error);
    }
    
    // واجهة المستمع لأحداث اللمس
    public interface OnTouchEventListener {
        void onTouchEvent(MotionEvent event);
    }
    
    public class LocalBinder extends Binder {
        public WinlandService getService() {
            return WinlandService.this;
        }
    }
    
    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "Creating Winland Service");
        
        mMainHandler = new Handler(Looper.getMainLooper());
        mWindowManager = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        
        createNotificationChannel();
        initializeSurface();
    }
    
    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "Starting Winland Service");
        
        startForeground(NOTIFICATION_ID, createNotification());
        
        if (mState == ServiceState.STOPPED || mState == ServiceState.ERROR) {
            startServer();
        }
        
        return START_STICKY;
    }
    
    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "Destroying Winland Service");
        
        stopServer();
        removeSurface();
    }
    
    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }
    
    // إنشاء قناة الإشعارات
    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Winland Server",
                NotificationManager.IMPORTANCE_LOW
            );
            channel.setDescription("Wayland Compositor Service");
            
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) {
                manager.createNotificationChannel(channel);
            }
        }
    }
    
    // إنشاء الإشعار
    private Notification createNotification() {
        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent, 
            PendingIntent.FLAG_IMMUTABLE
        );
        
        return new Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("Winland Server")
            .setContentText("Wayland Compositor is running")
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build();
    }
    
    // تهيئة سطح العرض
    private void initializeSurface() {
        mSurfaceView = new SurfaceView(this);
        mSurfaceView.getHolder().addCallback(this);
        
        // إعداد معلمات النافذة العائمة
        int type = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O 
            ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            : WindowManager.LayoutParams.TYPE_PHONE;
        
        mLayoutParams = new WindowManager.LayoutParams(
            mWidth, mHeight,
            type,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
            PixelFormat.TRANSLUCENT
        );
        
        mLayoutParams.gravity = Gravity.TOP | Gravity.START;
        mLayoutParams.x = 0;
        mLayoutParams.y = 0;
        
        // معالجة أحداث اللمس
        mSurfaceView.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                handleTouchEvent(event);
                return true;
            }
        });
    }
    
    // إظهار النافذة العائمة
    public void showFloatingWindow() {
        if (mSurfaceView.getParent() == null) {
            try {
                mWindowManager.addView(mSurfaceView, mLayoutParams);
            } catch (Exception e) {
                Log.e(TAG, "Failed to add floating window", e);
            }
        }
    }
    
    // إخفاء النافذة العائمة
    public void hideFloatingWindow() {
        removeSurface();
    }
    
    // إزالة السطح
    private void removeSurface() {
        if (mSurfaceView != null && mSurfaceView.getParent() != null) {
            try {
                mWindowManager.removeView(mSurfaceView);
            } catch (Exception e) {
                Log.e(TAG, "Failed to remove surface", e);
            }
        }
    }
    
    // معالجة حدث اللمس
    private void handleTouchEvent(MotionEvent event) {
        // إرسال إلى المكتبة الأصلية
        int action = event.getActionMasked();
        int pointerId = event.getPointerId(event.getActionIndex());
        float x = event.getX(event.getActionIndex());
        float y = event.getY(event.getActionIndex());
        
        nativeOnTouch(action, x, y, pointerId);
        
        // إرسال إلى المستمع
        if (mTouchListener != null) {
            mTouchListener.onTouchEvent(event);
        }
    }
    
    // بدء الخادم
    public void startServer() {
        if (mState == ServiceState.RUNNING || mState == ServiceState.INITIALIZING) {
            return;
        }
        
        setState(ServiceState.INITIALIZING);
        
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Log.i(TAG, "Initializing native server...");
                    
                    // تهيئة الخادم الأصلي
                    int result = nativeInitServer(mWidth, mHeight, mSocketName);
                    if (result != 0) {
                        throw new RuntimeException("Failed to initialize server: " + result);
                    }
                    
                    // بدء تشغيل الخادم
                    result = nativeStartServer();
                    if (result != 0) {
                        throw new RuntimeException("Failed to start server: " + result);
                    }
                    
                    // تهيئة جسر الصوت
                    result = nativeInitAudioBridge();
                    if (result != 0) {
                        Log.w(TAG, "Failed to initialize audio bridge: " + result);
                    }
                    
                    // تهيئة توجيه USB
                    result = nativeInitUsbRedirect();
                    if (result != 0) {
                        Log.w(TAG, "Failed to initialize USB redirect: " + result);
                    }
                    
                    setState(ServiceState.RUNNING);
                    
                    showToast("Winland Server started");
                    
                } catch (Exception e) {
                    Log.e(TAG, "Error starting server", e);
                    setState(ServiceState.ERROR);
                    if (mStateListener != null) {
                        mStateListener.onError(e.getMessage());
                    }
                }
            }
        }).start();
    }
    
    // إيقاف الخادم
    public void stopServer() {
        if (mState == ServiceState.STOPPED) {
            return;
        }
        
        Log.i(TAG, "Stopping server...");
        
        nativeStopUsbRedirect();
        nativeStopAudioBridge();
        nativeStopServer();
        
        setState(ServiceState.STOPPED);
        
        showToast("Winland Server stopped");
    }
    
    // تعيين الحالة
    private void setState(ServiceState state) {
        mState = state;
        if (mStateListener != null) {
            mMainHandler.post(new Runnable() {
                @Override
                public void run() {
                    mStateListener.onStateChanged(mState);
                }
            });
        }
    }
    
    // عرض رسالة Toast
    private void showToast(final String message) {
        mMainHandler.post(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(WinlandService.this, message, Toast.LENGTH_SHORT).show();
            }
        });
    }
    
    // دوال setter
    public void setStateListener(OnServiceStateChangeListener listener) {
        mStateListener = listener;
    }
    
    public void setTouchListener(OnTouchEventListener listener) {
        mTouchListener = listener;
    }
    
    public void setResolution(int width, int height) {
        mWidth = width;
        mHeight = height;
        
        if (mLayoutParams != null) {
            mLayoutParams.width = width;
            mLayoutParams.height = height;
            
            if (mSurfaceView.getParent() != null) {
                mWindowManager.updateViewLayout(mSurfaceView, mLayoutParams);
            }
        }
    }
    
    public void setSocketName(String socketName) {
        mSocketName = socketName;
    }
    
    // دوال getter
    public ServiceState getState() {
        return mState;
    }
    
    public String getVersion() {
        return nativeGetVersion();
    }
    
    public boolean isRunning() {
        return mState == ServiceState.RUNNING;
    }
    
    // SurfaceHolder.Callback
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created");
        nativeSetSurface(holder.getSurface());
    }
    
    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);
    }
    
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "Surface destroyed");
    }
    
    // الدوال الأصلية
    public native String nativeGetVersion();
    public native int nativeInitServer(int width, int height, String socketName);
    public native int nativeStartServer();
    public native void nativeStopServer();
    public native void nativeSetSurface(Surface surface);
    public native void nativeOnTouch(int action, float x, float y, int pointerId);
    public native void nativeOnKey(int keyCode, int action, int modifiers);
    public native int nativeInitAudioBridge();
    public native void nativeStopAudioBridge();
    public native int nativeInitUsbRedirect();
    public native void nativeStopUsbRedirect();
    
    // ==================== دوال مدير الحزم ====================
    public native void nativeRequestPackageList();
    public native void nativeInstallPackage(String packageName);
    public native void nativeRemovePackage(String packageName);
    public native void nativeUpgradePackage(String packageName);
    public native void nativeUpgradeAllPackages();
    public native void nativeCleanPackageCache();
    public native void nativeAutoremovePackages();
    public native void nativeSetPackageCallbacks(
        PackageProgressCallback progressCb,
        PackageCompleteCallback completeCb,
        PackageErrorCallback errorCb
    );
    
    // ==================== دوال XWayland ====================
    public native int nativeStartXWayland();
    public native void nativeStopXWayland();
    public native boolean nativeIsXWaylandRunning();
    public native void nativeSetXWaylandCallbacks(
        XWaylandReadyCallback readyCb,
        XWaylandSurfaceCallback surfaceCb
    );
    
    // ==================== دوال الإشعارات ====================
    public native void nativeForwardNotification(
        String appName,
        String title,
        String body,
        int priority,
        int timeout
    );
    public native void nativeSetNotificationCallback(NotificationCallback callback);
    
    // ==================== دوال لوحة المفاتيح ====================
    public native void nativeSetKeymap(String layout);
    public native void nativeEnableShortcuts(boolean enable);
    public native void nativeSetPassthroughMode(boolean enable);
    
    // ==================== دوال إدارة الطاقة ====================
    public native void nativeSetPowerMode(int mode);
    public native int nativeGetPowerMode();
    public native void nativeSetIdleTimeout(int timeoutMs);
    
    // ==================== دوال الشاشات المتعددة ====================
    public native void nativeEnableExternalDisplay(boolean enable);
    public native void nativeSetDisplayMode(int mode);
    public native boolean nativeIsExternalDisplayConnected();
    
    // ==================== دوال مشاركة الملفات ====================
    public native void nativeSyncFiles(String androidPath, String linuxPath);
    public native void nativeEnableAutoSync(boolean enable);
    
    // ==================== واجهات الاستدعاء ====================
    public interface PackageProgressCallback {
        void onProgress(String packageName, int percent);
    }
    
    public interface PackageCompleteCallback {
        void onComplete(boolean success, String message);
    }
    
    public interface PackageErrorCallback {
        void onError(String error);
    }
    
    public interface XWaylandReadyCallback {
        void onReady();
    }
    
    public interface XWaylandSurfaceCallback {
        void onSurfaceCreate(int windowId, String title);
        void onSurfaceDestroy(int windowId);
    }
    
    public interface NotificationCallback {
        void onNotification(String appName, String title, String body);
    }
    
    // ==================== دوال Java المساعدة ====================
    private static WinlandService sInstance;
    
    public static WinlandService getInstance() {
        return sInstance;
    }
    
    public static boolean isRunning() {
        return sInstance != null && sInstance.isRunning();
    }
    
    // ==================== دوال مدير الحزم ====================
    public void requestPackageList() {
        nativeRequestPackageList();
    }
    
    public void installPackage(String packageName) {
        nativeInstallPackage(packageName);
    }
    
    public void removePackage(String packageName) {
        nativeRemovePackage(packageName);
    }
    
    public void upgradePackage(String packageName) {
        nativeUpgradePackage(packageName);
    }
    
    public void upgradeAllPackages() {
        nativeUpgradeAllPackages();
    }
    
    public void cleanPackageCache() {
        nativeCleanPackageCache();
    }
    
    public void autoremovePackages() {
        nativeAutoremovePackages();
    }
    
    // ==================== دوال XWayland ====================
    public boolean startXWayland() {
        return nativeStartXWayland() == 0;
    }
    
    public void stopXWayland() {
        nativeStopXWayland();
    }
    
    public boolean isXWaylandRunning() {
        return nativeIsXWaylandRunning();
    }
    
    // ==================== دوال الإشعارات ====================
    public void forwardNotification(android.app.Notification notification, String packageName) {
        String title = notification.extras.getString(android.app.Notification.EXTRA_TITLE, "");
        String text = notification.extras.getString(android.app.Notification.EXTRA_TEXT, "");
        
        nativeForwardNotification(packageName, title, text, 1, 5000);
    }
    
    // ==================== دوال لوحة المفاتيح ====================
    public void setKeymap(String layout) {
        nativeSetKeymap(layout);
    }
    
    public void enableShortcuts(boolean enable) {
        nativeEnableShortcuts(enable);
    }
    
    public void setPassthroughMode(boolean enable) {
        nativeSetPassthroughMode(enable);
    }
    
    // ==================== دوال إدارة الطاقة ====================
    public static final int POWER_MODE_ACTIVE = 0;
    public static final int POWER_MODE_BALANCED = 1;
    public static final int POWER_MODE_POWERSAVE = 2;
    public static final int POWER_MODE_SUSPEND = 3;
    
    public void setPowerMode(int mode) {
        nativeSetPowerMode(mode);
    }
    
    public int getPowerMode() {
        return nativeGetPowerMode();
    }
    
    public void setIdleTimeout(int timeoutMs) {
        nativeSetIdleTimeout(timeoutMs);
    }
    
    // ==================== دوال الشاشات المتعددة ====================
    public static final int DISPLAY_MODE_MIRROR = 0;
    public static final int DISPLAY_MODE_EXTEND = 1;
    public static final int DISPLAY_MODE_SINGLE = 2;
    
    public void enableExternalDisplay(boolean enable) {
        nativeEnableExternalDisplay(enable);
    }
    
    public void setDisplayMode(int mode) {
        nativeSetDisplayMode(mode);
    }
    
    public boolean isExternalDisplayConnected() {
        return nativeIsExternalDisplayConnected();
    }
    
    // ==================== دوال مشاركة الملفات ====================
    public void syncFiles(String androidPath, String linuxPath) {
        nativeSyncFiles(androidPath, linuxPath);
    }
    
    public void enableAutoSync(boolean enable) {
        nativeEnableAutoSync(enable);
    }
}
