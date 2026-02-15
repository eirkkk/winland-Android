package com.winland.server;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.provider.Settings;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

/**
 * النشاط الرئيسي لتطبيق Winland Server
 */
public class MainActivity extends Activity {
    
    private static final String TAG = "MainActivity";
    private static final int REQUEST_OVERLAY_PERMISSION = 1;
    private static final int REQUEST_PERMISSIONS = 2;
    
    private WinlandService mService;
    private boolean mBound = false;
    
    // واجهات المستخدم
    private TextView mStatusText;
    private TextView mLogText;
    private Button mStartButton;
    private Button mStopButton;
    private Button mShowWindowButton;
    private Button mHideWindowButton;
    
    // اتصال الخدمة
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            WinlandService.LocalBinder binder = (WinlandService.LocalBinder) service;
            mService = binder.getService();
            mBound = true;
            
            // تعيين مستمع الحالة
            mService.setStateListener(new WinlandService.OnServiceStateChangeListener() {
                @Override
                public void onStateChanged(WinlandService.ServiceState newState) {
                    updateUI(newState);
                }
                
                @Override
                public void onError(String error) {
                    appendLog("Error: " + error);
                }
            });
            
            // تعيين مستمع اللمس
            mService.setTouchListener(new WinlandService.OnTouchEventListener() {
                @Override
                public void onTouchEvent(MotionEvent event) {
                    // يمكن استخدامه لتتبع أحداث اللمس
                }
            });
            
            updateUI(mService.getState());
            appendLog("Connected to service");
        }
        
        @Override
        public void onServiceDisconnected(ComponentName name) {
            mBound = false;
            mService = null;
            appendLog("Disconnected from service");
        }
    };
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // إنشاء واجهة المستخدم برمجياً
        createUI();
        
        // التحقق من الأذونات
        checkPermissions();
        
        // ربط الخدمة
        bindService();
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        unbindService();
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        if (mBound && mService != null) {
            updateUI(mService.getState());
        }
    }
    
    // إنشاء واجهة المستخدم
    private void createUI() {
        ScrollView scrollView = new ScrollView(this);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(20, 20, 20, 20);
        
        // عنوان
        TextView titleText = new TextView(this);
        titleText.setText("Winland Server");
        titleText.setTextSize(24);
        titleText.setPadding(0, 0, 0, 20);
        layout.addView(titleText);
        
        // نص الحالة
        mStatusText = new TextView(this);
        mStatusText.setText("Status: Stopped");
        mStatusText.setTextSize(16);
        mStatusText.setPadding(0, 0, 0, 20);
        layout.addView(mStatusText);
        
        // أزرار التحكم
        LinearLayout buttonLayout = new LinearLayout(this);
        buttonLayout.setOrientation(LinearLayout.HORIZONTAL);
        
        mStartButton = new Button(this);
        mStartButton.setText("Start Server");
        mStartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                startServer();
            }
        });
        buttonLayout.addView(mStartButton);
        
        mStopButton = new Button(this);
        mStopButton.setText("Stop Server");
        mStopButton.setEnabled(false);
        mStopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                stopServer();
            }
        });
        buttonLayout.addView(mStopButton);
        
        layout.addView(buttonLayout);
        
        // أزرار النافذة
        LinearLayout windowButtonLayout = new LinearLayout(this);
        windowButtonLayout.setOrientation(LinearLayout.HORIZONTAL);
        
        mShowWindowButton = new Button(this);
        mShowWindowButton.setText("Show Window");
        mShowWindowButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showFloatingWindow();
            }
        });
        windowButtonLayout.addView(mShowWindowButton);
        
        mHideWindowButton = new Button(this);
        mHideWindowButton.setText("Hide Window");
        mHideWindowButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                hideFloatingWindow();
            }
        });
        windowButtonLayout.addView(mHideWindowButton);
        
        layout.addView(windowButtonLayout);
        
        // عنوان السجل
        TextView logTitle = new TextView(this);
        logTitle.setText("Log:");
        logTitle.setTextSize(18);
        logTitle.setPadding(0, 20, 0, 10);
        layout.addView(logTitle);
        
        // نص السجل
        mLogText = new TextView(this);
        mLogText.setText("");
        mLogText.setTextSize(12);
        mLogText.setPadding(10, 10, 10, 10);
        mLogText.setBackgroundColor(0xFF333333);
        mLogText.setTextColor(0xFFFFFFFF);
        layout.addView(mLogText);
        
        scrollView.addView(layout);
        setContentView(scrollView);
    }
    
    // التحقق من الأذونات
    private void checkPermissions() {
        // إذن الظهور فوق التطبيقات
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (!Settings.canDrawOverlays(this)) {
                Intent intent = new Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName())
                );
                startActivityForResult(intent, REQUEST_OVERLAY_PERMISSION);
            }
        }
        
        // الأذونات الأخرى
        String[] permissions = {
            Manifest.permission.INTERNET,
            Manifest.permission.ACCESS_NETWORK_STATE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.RECORD_AUDIO
        };
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            for (String permission : permissions) {
                if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                    requestPermissions(permissions, REQUEST_PERMISSIONS);
                    break;
                }
            }
        }
    }
    
    // ربط الخدمة
    private void bindService() {
        Intent intent = new Intent(this, WinlandService.class);
        bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }
    
    // فك ربط الخدمة
    private void unbindService() {
        if (mBound) {
            unbindService(mConnection);
            mBound = false;
        }
    }
    
    // بدء الخادم
    private void startServer() {
        if (!mBound || mService == null) {
            Toast.makeText(this, "Service not bound", Toast.LENGTH_SHORT).show();
            return;
        }
        
        // بدء الخدمة
        Intent intent = new Intent(this, WinlandService.class);
        startService(intent);
        
        mService.startServer();
        appendLog("Starting server...");
    }
    
    // إيقاف الخادم
    private void stopServer() {
        if (!mBound || mService == null) {
            return;
        }
        
        mService.stopServer();
        appendLog("Stopping server...");
    }
    
    // إظهار النافذة العائمة
    private void showFloatingWindow() {
        if (!mBound || mService == null) {
            Toast.makeText(this, "Service not bound", Toast.LENGTH_SHORT).show();
            return;
        }
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (!Settings.canDrawOverlays(this)) {
                Toast.makeText(this, "Overlay permission required", Toast.LENGTH_LONG).show();
                return;
            }
        }
        
        mService.showFloatingWindow();
        appendLog("Showing floating window");
    }
    
    // إخفاء النافذة العائمة
    private void hideFloatingWindow() {
        if (!mBound || mService == null) {
            return;
        }
        
        mService.hideFloatingWindow();
        appendLog("Hiding floating window");
    }
    
    // تحديث واجهة المستخدم
    private void updateUI(WinlandService.ServiceState state) {
        mStatusText.setText("Status: " + state.toString());
        
        switch (state) {
            case STOPPED:
                mStartButton.setEnabled(true);
                mStopButton.setEnabled(false);
                break;
            case INITIALIZING:
                mStartButton.setEnabled(false);
                mStopButton.setEnabled(false);
                break;
            case RUNNING:
                mStartButton.setEnabled(false);
                mStopButton.setEnabled(true);
                break;
            case ERROR:
                mStartButton.setEnabled(true);
                mStopButton.setEnabled(false);
                break;
        }
    }
    
    // إضافة إلى السجل
    private void appendLog(final String message) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                String currentLog = mLogText.getText().toString();
                mLogText.setText(currentLog + "\n" + message);
            }
        });
    }
    
    // إنشاء قائمة الخيارات
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 1, 0, "Settings");
        menu.add(0, 2, 0, "About");
        menu.add(0, 3, 0, "Clear Log");
        return true;
    }
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case 1:
                showSettings();
                return true;
            case 2:
                showAbout();
                return true;
            case 3:
                mLogText.setText("");
                return true;
        }
        return super.onOptionsItemSelected(item);
    }
    
    // عرض الإعدادات
    private void showSettings() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Settings");
        builder.setMessage("Settings will be implemented in a future version.");
        builder.setPositiveButton("OK", null);
        builder.show();
    }
    
    // عرض نافذة About
    private void showAbout() {
        String version = mBound && mService != null ? mService.getVersion() : "Unknown";
        
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("About Winland Server");
        builder.setMessage(
            "Winland Server v1.0.0\n\n" +
            "Android Wayland Compositor\n\n" +
            "Native Version: " + version + "\n\n" +
            "This application provides a Wayland compositor " +
            "for running Linux GUI applications on Android."
        );
        builder.setPositiveButton("OK", null);
        builder.show();
    }
    
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == REQUEST_OVERLAY_PERMISSION) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                if (Settings.canDrawOverlays(this)) {
                    Toast.makeText(this, "Overlay permission granted", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "Overlay permission denied", Toast.LENGTH_LONG).show();
                }
            }
        }
    }
    
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        
        if (requestCode == REQUEST_PERMISSIONS) {
            for (int i = 0; i < permissions.length; i++) {
                if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                    Log.w(TAG, "Permission denied: " + permissions[i]);
                }
            }
        }
    }
}
