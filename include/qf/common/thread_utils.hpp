#pragma once

#include <string>
#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace qf {

enum class ThreadPriority {
    Low,
    Normal,
    High,
    Realtime
};

// Sets the current thread's OS-visible name for debugger/profiler identification.
// Returns true on success, false on failure or unsupported platform.
inline bool set_thread_name(const std::string& name) {
#ifdef _WIN32
    // SetThreadDescription available on Windows 10 1607+
    // Dynamically load to support MinGW and older SDK headers
    using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    static auto fn = reinterpret_cast<SetThreadDescriptionFn>(
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetThreadDescription"));
    if (!fn) return false;

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
    if (wide_len <= 0) return false;
    std::wstring wide_name(static_cast<size_t>(wide_len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wide_name[0], wide_len);
    HRESULT hr = fn(GetCurrentThread(), wide_name.c_str());
    return SUCCEEDED(hr);
#elif defined(__linux__)
    // Linux: pthread_setname_np truncates to 16 chars (including null)
    std::string truncated = name.substr(0, 15);
    return pthread_setname_np(pthread_self(), truncated.c_str()) == 0;
#elif defined(__APPLE__)
    // macOS: only the calling thread can set its own name
    return pthread_setname_np(name.substr(0, 63).c_str()) == 0;
#else
    (void)name;
    return false;  // Unsupported platform — graceful no-op
#endif
}

// Pins the current thread to a specific CPU core.
// core_id is zero-based. Returns true on success.
inline bool pin_to_core(uint32_t core_id) {
#ifdef _WIN32
    DWORD_PTR mask = static_cast<DWORD_PTR>(1) << core_id;
    DWORD_PTR prev = SetThreadAffinityMask(GetCurrentThread(), mask);
    return prev != 0;
#elif defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(static_cast<int>(core_id), &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)core_id;
    return false;  // Unsupported platform — graceful no-op
#endif
}

// Sets the scheduling priority of the current thread.
// Returns true on success.
inline bool set_thread_priority(ThreadPriority priority) {
#ifdef _WIN32
    int win_priority;
    switch (priority) {
        case ThreadPriority::Low:      win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case ThreadPriority::Normal:   win_priority = THREAD_PRIORITY_NORMAL;       break;
        case ThreadPriority::High:     win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case ThreadPriority::Realtime: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        default:                       win_priority = THREAD_PRIORITY_NORMAL;       break;
    }
    return SetThreadPriority(GetCurrentThread(), win_priority) != 0;
#elif defined(__linux__) || defined(__APPLE__)
    // Map to POSIX nice-style scheduling via pthread
    sched_param param{};
    int policy;
    switch (priority) {
        case ThreadPriority::Low:
            policy = SCHED_OTHER;
            param.sched_priority = 0;
            break;
        case ThreadPriority::Normal:
            policy = SCHED_OTHER;
            param.sched_priority = 0;
            break;
        case ThreadPriority::High:
            policy = SCHED_FIFO;
            param.sched_priority = sched_get_priority_min(SCHED_FIFO);
            break;
        case ThreadPriority::Realtime:
            policy = SCHED_FIFO;
            param.sched_priority = sched_get_priority_max(SCHED_FIFO);
            break;
        default:
            policy = SCHED_OTHER;
            param.sched_priority = 0;
            break;
    }
    return pthread_setschedparam(pthread_self(), policy, &param) == 0;
#else
    (void)priority;
    return false;  // Unsupported platform — graceful no-op
#endif
}

}  // namespace qf
