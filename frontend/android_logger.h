#pragma once
#ifdef __ANDROID__

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <android/log.h>
#include <string>

namespace AndroidLogger
{

inline FILE* g_logFp = nullptr;

inline void Print(const char* fmt, ...)
{
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // 1. Android Logcat
    __android_log_print(ANDROID_LOG_INFO, "PokemonDS", "%s", buf);

    // 2. Direct to log file if open
    if (g_logFp)
    {
        time_t now = time(nullptr);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char timeBuf[32];
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_info);

        fprintf(g_logFp, "[%s] %s\n", timeBuf, buf);
        fflush(g_logFp);
    }
}

inline void CrashSignalHandler(int sig, siginfo_t* info, void* ucontext)
{
    const char* sigName = "UNKNOWN";
    if (sig == SIGSEGV) sigName = "SIGSEGV (Segmentation Fault / Invalid Memory Access)";
    else if (sig == SIGBUS) sigName = "SIGBUS (Bus Error / Misaligned Access)";
    else if (sig == SIGABRT) sigName = "SIGABRT (Abort called)";
    else if (sig == SIGFPE) sigName = "SIGFPE (Arithmetic Exception)";
    else if (sig == SIGILL) sigName = "SIGILL (Illegal Instruction)";

    Print("\n========================================================");
    Print("FATAL CRASH CAUGHT BY ANDROID LOGGER!");
    Print("Signal: %d (%s)", sig, sigName);
    Print("Faulting Address: %p", info ? info->si_addr : nullptr);
    Print("========================================================\n");

    if (g_logFp) fflush(g_logFp);

    // Restore default handler and re-raise to produce standard system crash dump
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, nullptr);
    raise(sig);
}

inline void Init(const std::string& stateDir)
{
    // Try multiple possible paths in order of accessibility
    const char* paths[] = {
        "/storage/emulated/0/pokemon_ds_log.txt",
        "/storage/emulated/0/Download/pokemon_ds_log.txt"
    };

    for (const char* p : paths)
    {
        g_logFp = fopen(p, "w");
        if (g_logFp) break;
    }

    if (!g_logFp && !stateDir.empty())
    {
        std::string fallback = stateDir + "/pokemon_ds_log.txt";
        g_logFp = fopen(fallback.c_str(), "w");
    }

    if (g_logFp)
    {
        // Unbuffered so crash lines are not lost in buffers
        setvbuf(g_logFp, nullptr, _IONBF, 0);

        // Redirect stderr and stdout so all core / melonDS / SDL logs are recorded
        int fd = fileno(g_logFp);
        dup2(fd, STDERR_FILENO);
        dup2(fd, STDOUT_FILENO);
        setvbuf(stderr, nullptr, _IONBF, 0);
        setvbuf(stdout, nullptr, _IONBF, 0);
    }

    Print("=== Pokemon DS Widescreen Logger Initialized ===");
    Print("Build: mobile rev");
    Print("State Directory: %s", stateDir.c_str());

    // Register fatal signal crash handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = CrashSignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);

    Print("Signal crash handlers registered for SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL");
}

} // namespace AndroidLogger

#endif // __ANDROID__
