#include <stdlib.h>
#include <stdint.h>

/**
 * Winland Fix: ASensorManager Shim
 * 
 * These symbols override the Android NDK sensor manager functions.
 * On Android 13+, libraries like android-activity (used by winit) 
 * trigger early Binder initialization via ASensorManager_getInstance.
 * By returning NULL/zero, we prevent the Binder service search and 
 * avoid the libbinder crash seen on OnePlus/Oppo devices.
 */

void* ASensorManager_getInstance() {
    return NULL;
}

void* ASensorManager_getInstanceForPackage(const char* packageName) {
    (void)packageName;
    return NULL;
}

int ASensorManager_getSensorList(void* manager, void** list) {
    (void)manager;
    (void)list;
    return 0;
}

void* ASensorManager_getDefaultSensor(void* manager, int type) {
    (void)manager;
    (void)type;
    return NULL;
}
