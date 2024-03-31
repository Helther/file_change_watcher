#pragma once
#include <string>
#include <fstream>
#include <cassert>
#include <csignal>
#include <array>

#ifndef inFile
#define inFile "/tmp/indata.txt"
#endif
#ifndef outFile
#define outFile "/tmp/outdata.txt"
#endif

// return true if successful
inline bool setProcessAffinity(pid_t pid, unsigned int core) noexcept
{
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(core, &cpuSet);
    int affinityRes = sched_setaffinity(pid, sizeof(cpuSet), &cpuSet);
    return affinityRes == 0;
}

inline void removeOutputFile() noexcept
{
    std::fstream file;
    file.open(outFile, std::ios::out);
    if (file.is_open())
        std::remove(outFile);
}

//======= Get Cpu info to correctly set core affinity for processes ======//
struct CpuInfo
{
    unsigned coreCount;
    unsigned threadsPerCore;
};

inline void cpuID(unsigned i, unsigned regs[4])
{
  asm volatile
    ("cpuid" : "=a" (regs[0]), "=b" (regs[1]), "=c" (regs[2]), "=d" (regs[3])
     : "a" (i), "c" (0));
  // ECX is set to zero for CPUID function 4
}

inline CpuInfo getCpuInfo()
{
    unsigned regs[4];

    // Get vendor
    char vendor[12];
    cpuID(0, regs);
    reinterpret_cast<unsigned*>(vendor)[0] = regs[1]; // EBX
    reinterpret_cast<unsigned*>(vendor)[1] = regs[3]; // EDX
    reinterpret_cast<unsigned*>(vendor)[2] = regs[2]; // ECX
    std::string cpuVendor = std::string(vendor, 12);

    // Logical core count per CPU
    cpuID(1, regs);
    unsigned int logical = (regs[1] >> 16) & 0xff; // EBX[23:16]
    unsigned int cores = logical;

    if (cpuVendor == "GenuineIntel") {
      // Get DCP cache info
      cpuID(4, regs);
      cores = ((regs[0] >> 26) & 0x3f) + 1; // EAX[31:26] + 1

    } else if (cpuVendor == "AuthenticAMD") {
      // Get NC: Number of CPU cores - 1
      cpuID(0x80000008, regs);
      cores = (reinterpret_cast<unsigned int>(regs[2] & 0xff)) + 1; // ECX[7:0] + 1
    }

    assert(cores > 0);

    return {cores, logical / cores};
}

//==================== Signal handling utils =======================//
static constexpr std::array<int, 12> SIGNALS_TO_INTERRUPT = {
    SIGABRT,
    SIGFPE,
    SIGILL,
    SIGINT,
    SIGSEGV,
    SIGTERM,
    SIGHUP,
    SIGQUIT,
    SIGTRAP,
    SIGKILL,
    SIGPIPE,
    SIGALRM
};

inline void setupSigHandlers(void(*handler)(int))
{
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    for (const auto& sig : SIGNALS_TO_INTERRUPT)
        sigaction(sig, &sigIntHandler, NULL);
}
