// Stub for __cxa_pure_virtual — required by Oboe (C++ library) but
// not resolved when libc++_shared.so is loaded with RTLD_LOCAL on Android.
// This should never actually be called in normal operation.
void __cxa_pure_virtual(void) {
    __builtin_trap();
}
