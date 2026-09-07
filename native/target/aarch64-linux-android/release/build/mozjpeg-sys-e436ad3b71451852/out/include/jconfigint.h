
        #define BUILD "1788734321-mozjpeg-sys"
        #ifndef INLINE
            #if defined(__clang__) || defined(__GNUC__)
                #define INLINE inline __attribute__((always_inline))
            #elif defined(_MSC_VER)
                #define INLINE __forceinline
            #else
                #define INLINE inline
            #endif
        #endif
        #ifndef HIDDEN
            #if defined(__clang__) || defined(__GNUC__)
                #define HIDDEN  __attribute__((visibility("hidden")))
            #endif
        #endif
        #ifndef THREAD_LOCAL
            #if defined (_MSC_VER)
                #define HAVE_THREAD_LOCAL
                #define THREAD_LOCAL  __declspec(thread)
            #elif defined(__clang__) || defined(__GNUC__)
                #define HAVE_THREAD_LOCAL
                #define THREAD_LOCAL  __thread
            #else
                #define THREAD_LOCAL
            #endif
        #endif
        #define SIZEOF_SIZE_T 8
        #ifndef HAVE_BUILTIN_CTZL
            #if defined (_MSC_VER)
                #if (SIZEOF_SIZE_T == 8)
                    #define HAVE_BITSCANFORWARD64
                #elif (SIZEOF_SIZE_T == 4)
                    #define HAVE_BITSCANFORWARD
                #endif
            #elif defined(__clang__) || defined(__GNUC__)
                #define HAVE_BUILTIN_CTZL 1
            #endif
        #endif
        #define FALLTHROUGH
        #define PACKAGE_NAME "mozjpeg-sys"
        #define VERSION "2.2.3"
        
