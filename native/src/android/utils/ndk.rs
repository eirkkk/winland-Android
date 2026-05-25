use jni::{JNIEnv, JavaVM};

pub fn run_in_jvm<F, T>(jni_function: F, vm: &JavaVM) -> T
where
    F: FnOnce(&mut JNIEnv) -> T,
{
    let mut env = vm.attach_current_thread().unwrap();
    let res = jni_function(&mut env);
    unsafe { vm.detach_current_thread() };
    res
}
