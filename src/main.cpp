#include "filesynchronizer.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <sys/wait.h>
#include <sched.h>
#include <fstream>

#define cmdLess "less"

// global Pid's of worker processes, to allow signal handler to clean up
pid_t FILE_WATCH_PID = 0;
pid_t FILE_VIEW_PID = 0;

// runs in child process and calls "less"
void fileViewWorker() noexcept
{
    // check if outFile exists, if not create one
    std::fstream file;
    file.open(outFile, std::ios::trunc);
    if (file.is_open())
        file.close();
    auto cmdPath = const_cast<char*>(cmdLess);
    auto cmdArg = const_cast<char*>((outFile));
    char *args[] = {cmdPath, cmdArg, NULL};
    int exitCode = execvp(cmdLess, args);
    if (exitCode == -1)
        SimpleSysLogger::instance()->logError();

    exit(exitCode);
}

// runs in child process that handles file updates
void fileWatcherWorker() noexcept
{
    int exitStatus = EXIT_SUCCESS;
    try
    {
        FileSynchronizer watcher;
        watcher.run();
    } catch (const std::exception& e)
    {
        SimpleSysLogger::instance()->logMsg(LOG_ERR, std::string("File Watcher Error: ") + e.what());
        exitStatus = EXIT_FAILURE;
    }

    exit(exitStatus);
}

void handleSignalInterrupt(int sig)
{
    SimpleSysLogger::instance()->logMsg(LOG_ERR, "Main process stopped by signal");

    if (FILE_WATCH_PID != 0)
        kill(FILE_VIEW_PID, SIGTERM);
    if (FILE_VIEW_PID != 0)
        kill(FILE_WATCH_PID, SIGTERM);
    removeOutputFile();
    exit(sig);
}

// runs in main process and handles child processes
void parentProcessWorker(CpuInfo cpu, pid_t fileWatcherPid, pid_t fileViewPid) noexcept
{
    unsigned int currentCPU = 0;
    unsigned int fileWatcherCPU = currentCPU + cpu.threadsPerCore;
    unsigned int fileViewCPU = fileWatcherCPU + cpu.threadsPerCore;
    // setup affinity cores values, if failed set error
    if (!setProcessAffinity(getpid(), currentCPU) ||
         !setProcessAffinity(fileWatcherPid, fileWatcherCPU) ||
            !setProcessAffinity(fileViewPid, fileViewCPU))
    {
        SimpleSysLogger::instance()->logError();
        kill(fileViewPid, SIGTERM);
        kill(fileWatcherPid, SIGTERM);
        removeOutputFile();
        exit(EXIT_FAILURE);
    }

    // setup signal handle in main thread
    FILE_WATCH_PID = fileWatcherPid;
    FILE_VIEW_PID = fileViewPid;
    setupSigHandlers(handleSignalInterrupt);

    int exitStatus = EXIT_SUCCESS;
    // wait for signals from childs
    int childStatus;
    pid_t childPid = wait(&childStatus);
    if (childPid > 0)
    {
        if (!WIFEXITED(childStatus) || WEXITSTATUS(childStatus))
        {
            const std::string childName = childPid == fileWatcherPid ? "File Watcher" : "File Viewer";
            SimpleSysLogger::instance()->logMsg(LOG_ERR, childName + " process stopped incorrectly");
            exitStatus = EXIT_FAILURE;
        }
        // kill other process and exit
        kill(childPid == fileWatcherPid ? fileViewPid : fileWatcherPid , SIGTERM);
    }
    else
    {
        SimpleSysLogger::instance()->logError();
        kill(fileViewPid, SIGTERM);
        kill(fileWatcherPid, SIGTERM);
        exitStatus = EXIT_FAILURE;
    }
    removeOutputFile();
    if (exitStatus == EXIT_SUCCESS)
        SimpleSysLogger::instance()->logMsg(LOG_INFO, "Finished Normally");
    exit(exitStatus);
}

int main()
{
    SimpleSysLogger::instance()->logMsg(LOG_INFO, "Starting");
    // get cpu info
    CpuInfo cpu = getCpuInfo();
    if (cpu.coreCount < 3)
    {
        SimpleSysLogger::instance()->logMsg(LOG_ERR, "Not enougn CPU cores to launch all required teasks");
        return EXIT_FAILURE;
    }
    // start child processes
    pid_t fileWatcherPid = fork();
    if (fileWatcherPid == -1)
    {
        SimpleSysLogger::instance()->logError();
        return EXIT_FAILURE;
    }
    else if (fileWatcherPid > 0)  // main process
    {
        pid_t fileViewPid = fork();
        if (fileViewPid == -1)
        {
            SimpleSysLogger::instance()->logError();
            kill(fileWatcherPid, SIGTERM);
            return EXIT_FAILURE;
        }
        else if (fileViewPid > 0)  // still main process
            parentProcessWorker(cpu, fileWatcherPid, fileViewPid);
        else if (fileViewPid == 0)  // start "less" command from child process
            fileViewWorker();
    }
    else if (fileWatcherPid == 0)  // start file watcher process
        fileWatcherWorker();
}
