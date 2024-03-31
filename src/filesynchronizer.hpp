#pragma once

#include <sys/inotify.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <fstream>

// handles file descriptors and watch descriptor
// and processes inotify events
class FileSynchronizer
{
public:
    FileSynchronizer();
    ~FileSynchronizer();

    void run();

private:
    void onFileModified();

    std::fstream iFile;
    std::fstream oFile;
    int watchFileD;
    int watchD;

    // position of the last-known end of the file
    std::streampos oFileEnd;
};
