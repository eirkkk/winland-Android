package com.winland.server;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

/**
 * مستقبل بدء التشغيل - يبدأ الخدمة تلقائياً عند إعادة تشغيل الجهاز
 */
public class BootReceiver extends BroadcastReceiver {
    
    private static final String TAG = "BootReceiver";
    
    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            Log.i(TAG, "Boot completed, starting Winland Service");
            
            // يمكن تفعيل هذا لبدء الخدمة تلقائياً عند الإقلاع
            // Intent serviceIntent = new Intent(context, WinlandService.class);
            // context.startForegroundService(serviceIntent);
        }
    }
}
