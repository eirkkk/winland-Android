# Master Plan — Winland Android Compositor

## v0.1 Status

Phase 1–3 (XWayland اكتمال) ✅ — 6 issues fixed (X8, WM1, X9, X10, W2, Clipboard).
التطبيق "functional" الآن: XWayland يعمل، clipboard يعمل، position صحيح، fullscreen/dialog يدعم.

## الأهداف الحقيقية المتبقية

### 🚨 Critical (crash أو panic في production)

| # | Severity | Title | Fix |
|---|----------|-------|-----|
| R1 | 🔴 | `slice::from_raw_parts(ptr, len)` في SHM read loop — لا bounds check عبر 4 call sites | إضافة `len ≤ remaining` قبل `from_raw_parts` |
| W1 | 🟠 | `lock().unwrap()` على `CursorImageSurfaceData` — يـ panic على mutex مسموم | استبدال بـ `lock().ok().and_then(...)` |

### 🎯 High Value (تؤثر على كل مستخدم كل جلسة)

| # | Severity | Title | Fix |
|---|----------|-------|-----|
| T2 | 🟠 | Titlebar غير مرئي — المستخدم لا يعرف أين يضغط للتحريك/الإغلاق | رسم شريط 24px شفاف + أزرار close/minimize/maximize |

### 📦 V2 Features (مفيدة لكن ليست حرجة)

| # | Severity | Title | Notes |
|---|----------|-------|-------|
| WM3 | 🟡 | Focus candidate على unmapped | MRU بدلاً من stacking order |
| WM4 | 🟡 | Maximize restore خارج الشاشة | Clamp restore position |
| WM6 | 🟡 | Resize/titlebar zone overlap | titlebar (24px) > resize (20px) |
| N1 | 🟡 | Cow surface mismatch على remap | إزالة/إضافة من wl_to_window |
| N2 | 🟡 | X11Surface stale في Window clones | استخدام Window ref بعد new_x11_window |
| N3 | 🟡 | Unmanaged surface position (0, reserved_top) | tooltips/popups تظهر في موقع خاطئ |
| W5 | 🟡 | Popup dismissal يخطئ على overlapping popups | استخدام geometry tree بدلاً من element_under |
| S2 | 🟠 | render_all على كل input event | Coalesce مع flag + deferred render |
| S3 | 🟡 | eglMakeCurrent context thrash | تخزين context بدلاً من إعادة الربط كل frame |
| T3 | 🟡 | Multi-touch centroid merging | تتبع finger IDs فردياً |
| T5 | 🟡 | Hardcoded pixel zones (غير DPI-aware) | Scale بـ output_scale + ui_scale |
| T6 | 🔵 | Two-finger right-click | Down→Up في 300ms → right-click |
| M1 | 🟠 | Dual cursor (Wayland + XWayland) | تم إصلاحه جزئياً، قد يحتاج متابعة |

### 🗑️ Irrelevant on Android (حذف من الخطة)

| # | سبب الحذف |
|---|-----------|
| X6 | جهاز Android واحد = display واحد، multi-XWayland irrelevant |
| P1-P6 | Trackpad غير موجود على هاتف |
| M2-M6 | Cursor لا يظهر في وضع اللمس، irrelevant |
| W6-W8 | DnD, relative pointer, constraints — ميزات سطح مكتب |
| WM5 | Minimize animation — Android له task switcher خاص به |
| WM7-WM9 | Keyboard shortcuts, snap, context menu — ميزات إضافية لسطح مكتب |
| R2-R5,R7 | تحسينات rendering — غير مطلوبة لـ v0.1 functional |
| S1,S4 | Performance — غير مطلوبة لـ v0.1 (S4 مقبول بـ bounded channel لاحقاً) |

## خطة العمل القادمة

### Sprint 1: Safety (R1 + W1) ✅
1. **R1** — إضافة bounds check عبر `shm_slice()` helper قبل `from_raw_parts` في جميع call sites (4 مواقع في seat.rs) ✅
2. **W1** — استبدال `lock().unwrap()` بـ `lock().ok()` في handlers.rs (3 مواقع) ✅

### Sprint 2: UX (T2)
3. **T2** — رسم titlebar شفاف (24px) + أزرار close/minimize/maximize

### بعدها
- V2 features انتقائياً إذا احتاجها أحد
- أو إعلان v0.1 والتفرغ لميزات جديدة
