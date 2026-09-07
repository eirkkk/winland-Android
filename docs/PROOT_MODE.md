# وضع proot (التشغيل بدون روت) — التوثيق

وثّق هذا الملف كل الإصلاحات والإضافات الخاصة بوضع التشغيل بدون روت (proot)
في مشروع Winland-Android.

## نظرة عامة

وضع `PROOT` يشغّل سطح مكتب Linux (ubuntu/kali) داخل proot (محاكاة chroot
بمساحة المستخدم عبر `ptrace`) بدون الحاجة لصلاحيات الروت وبدون mount حقيقي.
يبقى وضع `ROOT` موجوداً كخيار. اختيار الوضع محفوظ في `winland_prefs`
ومدار عبر `ExecutionModeManager`.

## ملفات جديدة

| الملف | الوظيفة |
|---|---|
| `engine/ProotManager.kt` | مسارات ثنائي proot (الأولوية لـ `nativeLibraryDir`)، `baseArgs()`، `prepareGuestDirs()`، `deployProot()` |
| `engine/ProotRootfsExtractor.kt` | فك ضغط الروتfs بلغة Java الخالصة (commons-compress + xz) + `finalizeStagedRootfs()` (فك التغليف/التحقق/التبديل) — لأن busybox غير قابل للتنفيذ على أجهزة targetSdk 29+ (W^X) |
| `ExecutionMode.kt` | `enum ExecutionMode { ROOT, PROOT }` + `ExecutionModeManager` (حفظ/قراءة الوضع) |
| `ui/ExecutionModeDialog.kt` | حوار اختيار الوضع عند الإقلاع |
| `jniLibs/arm64-v8a/libproot.so` | ثنائي proot (aarch64، مبني من مصدر Termux) كمكتبة native ليُثبَّت في `nativeLibraryDir` (مسموح التنفيذ فيه) |
| `jniLibs/arm64-v8a/libproot-loader.so` | محمّل proot |
| `assets/bin/proot`, `assets/bin/proot-loader` | نسخة احتياطية للأجهزة القديمة (filesDir) |

## تعديلات على ملفات موجودة

- `AndroidManifest.xml`: إضافة `android:extractNativeLibs="true"` (إلزامي ليُستخرج proot إلى `nativeLibraryDir`).
- `app/build.gradle.kts`: `ndkVersion 29.0.14206865` + اعتماديتا `commons-compress:1.26.2` و `xz:1.9` لفك الضغط بلغة Java.
- `engine/ChrootInstaller.kt`:
  - `extractRootfs()`: فرع PROOT يستخدم `ProotRootfsExtractor` بدل سكربت الشل (يحذف الأرشيف التالف كما يفعل السكربت).
  - `setupRootfs()`: فرع PROOT ينسخ `setup_<distro>.sh` من assets إلى `rootfs/tmp/` بلغة Kotlin (وضع ROOT كان يعتمد على ثنائي `winland-setup` الخاص بالروت).
- `engine/ChrootScriptBuilder.kt`:
  - `prootPrelude()` + `proot_enter()`: وسائط proot (`-0 -r -w /root --link2symlink` + ربط `/proc /sys /dev ...`).
  - ربط المقابس: `-b "$tmpDir:/tmp"` يربط `files/tmp` (مقبس wayland-0 للمركّب) مع `/tmp` الضيف، و `WINLAND_SOCKET_DIR=/tmp`.
  - ربط `/dev/shm` الصريح: `-b "$tmpDir/dev/shm:/dev/shm"` (لأن `-b /dev` يُخفي `shm` الضيف وأندرويد لا يملكه → كان wlroots يفشل `Failed to allocate shm file`).
  - `buildProotPostSetupScript()` / `buildProotRunScript()` / `buildProotStopScript()`: نسخ proot من الإعداد والتشغيل والإيقاف.
- `engine/RootCommandRunner.kt`: تنفيذ مباشر `executeDirect()` لوضع proot (بدون `su`).
- `TerminalActivity.kt` + `ui/EmbeddedTerminal.kt`: أوامر proot للطرفية (نفس الربط + `XDG_RUNTIME_DIR`).
- `MainActivity.kt` + `MainViewModel.kt`: تدفق اختيار الوضع + `executionMode` StateFlow.
- `WinlandService.kt` + `CleanupReceiver.kt`: تخطي الـ unmount في وضع proot (لا mounts حقيقية).
- `ui/WinlandDashboardScreen.kt`: قسم "Execution Mode" في الإعدادات + قسم "Help".
- `utils/RootUtils.kt` + `native/*`: دعم كشف الروت/الجسر الأصلي.

## مشاكل حُلّت أثناء التطوير (مرجع)

1. **تنفيذ proot محظور (W^X):** الحل = شحنه كـ `.so` في jniLibs (`nativeLibraryDir` → `apk_data_file` مسموح التنفيذ).
2. **فك الضغط يفشل (busybox غير قابل للتنفيذ):** الحل = مستخرج Java خالص.
3. **`setup_ubuntu.sh` مفقود داخل الضيف:** الحل = نسخه بلغة Kotlin في فرع PROOT.
4. **المقبس `wayland-0` لا يظهر (`FATAL`):** الحل = الربط `-b "$tmpDir:/tmp"` + `WINLAND_SOCKET_DIR=/tmp` (بدل الربط المتماثل الخاطئ).
5. **labwc يفشل `Failed to allocate shm file`:** الحل = ربط `/dev/shm` الصريح من مجلد قابل للكتابة.
6. **تحذيرات `groups: cannot find name`:** الحل = إضافة GIDs أندرويد الموروثة إلى `/etc/group` الضيف تلقائياً (`android_gid_<N>`).
7. **Firefox بلا إنترنت تحت proot:** الحل = متغيرات `MOZ_DISABLE_*` (تعطيل الساندبوكس وsocket process اللذين لا يعملان تحت proot).
8. **Restart/Stop لا يعملان في proot:** الحل = إيقاف متسامح (نجاح دائم + `pkill -9` احتياطي) و `restartChroot` لا يفشل بفشل الإيقاف.
9. **فشل الإقلاع عند التبديل proot←→chroot:** الحل = تنظيف مشترك قبل كل إقلاع (قتل بقايا الوضعين + مسح `.X11-unix` + مسح المخفية/`*.log` بالروت عند توفره) + تحذير عدم تعطيل الروت قبل إتمام التبديل.
10. **الطرفية تسقط في شل مضيف:** الحل = تمرير `argv` كاملة (`sh <script>`) وإظهار الأخطاء بدل السقوط الصامت.
11. **Firefox بلا فيديو (YouTube):** الحل = تثبيت `ffmpeg` + `libavcodec-extra` في سكربتي الإعداد (H.264/AAC).
12. **صمت التشغيل الدائم:** الحل = خيط `playback_thread` خالد (كان يخرج إن بدأ التسجيل أولاً ولا يُعاد إنشاؤه أبداً).
13. **الربط الكامل (من Haven):** تمويه selinux + ثلاثية `/dev/fd` + `urandom→random` + إعادة كتابة `resolv.conf` كل إقلاع.
14. **إعدادات جديدة:** مفتاح الألوان الديناميكية + مفتاح تسريع proot seccomp (تجريبي) في قسمي المظهر والتنفيذ.

## البناء والنشر

```sh
./build-arm64.sh            # بناء كامل (proot من المصدر عند غيابه + Rust + native + APK)
./proot/build-proot-android.sh /opt/android/ndk   # بناء proot فقط → proot/out/
./gradlew assembleDebug     # APK فقط بعد تعديل Kotlin
# app/build/outputs/apk/debug/app-debug.apk
```

مصادر proot مُضمّنة في `proot/` (‏`proot-termux` fork + ‏`talloc-2.4.2`
مع `config.h` المكتوب يدوياً) — قسم Smart Skip في `build-arm64.sh`
يبنيها فقط عند غياب `jniLibs/.../libproot.so`.

> ملاحظة بيئة البناء: NDK r29 ثنائياته aarch64 داخل مجلد `linux-x86_64`
> (رابط رمزي `linux-aarch64` موجود)، و `libxml2.so.16` مثبّت على النظام
> ليعمل `ld.lld`، وأدوات `aapt2/aapt` مستبدلة بنسخ aarch64.
