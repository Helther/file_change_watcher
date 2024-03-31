#include "filesynchronizer.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <vector>
#include <fcntl.h>
#include <limits.h>


constexpr uint32_t EVENT_SIZE = sizeof(struct inotify_event);
constexpr int EVENTS_COUNT = 1024;
// will be sufficient to read at least EVENTS_COUNT events.
constexpr size_t BUF_LEN = EVENTS_COUNT * (EVENT_SIZE + NAME_MAX + 1);

void throwMsgErrno(const std::string& prefix)
{
    throw std::runtime_error(prefix + ' ' + strerror(errno));
}

void throwErrorMsg(const std::string& msg)
{
    throw std::runtime_error(msg);
}

FileSynchronizer::FileSynchronizer()
{
    iFile.open(inFile, std::ios::in);
    if (!iFile.is_open())
    {
        throwErrorMsg("Failed to open input file");
    }
    oFile.open(outFile, std::ios::out | std::ios::trunc);
    if (!oFile.is_open())
        throwErrorMsg("Failed to open output file");

    watchFileD = inotify_init();

    if (watchFileD < 0) {
        throwMsgErrno("inotify_init");
    }
    // Set the fd flags with O_NONBLOCK masked out
    // https://stackoverflow.com/questions/24575705/inotify-api-stops-working-after-reporting-once-or-twice
    int saved_flags = fcntl(watchFileD, F_GETFL);
    fcntl(watchFileD, F_SETFL, saved_flags & ~O_NONBLOCK);

    watchD = inotify_add_watch(watchFileD, inFile, IN_MODIFY | IN_DELETE | IN_MOVE);
    if (watchD < 0)
        throwMsgErrno("wd init");

    iFile.seekg(0, std::ios::end);
    oFileEnd = iFile.tellg();
}

FileSynchronizer::~FileSynchronizer()
{
    inotify_rm_watch(watchFileD, watchD);
    close(watchFileD);
    iFile.close();
    oFile.close();
}

void FileSynchronizer::run()
{
    while (true)
    {
        char buffer[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
        // blocking read, unblocks when events were pushed
        ssize_t length = read(watchFileD, buffer, BUF_LEN);

        if (length < 0)
            throwMsgErrno("read");

        ssize_t i = 0;
        while (i < length)  // read available events
        {
            struct inotify_event *event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
            if (event->mask & IN_MODIFY)
                onFileModified();
            else if ((event->mask & IN_MOVE) || (event->mask & IN_DELETE))
                throwErrorMsg("Input file is no longer available");

            i += EVENT_SIZE + event->len;
        }
    }
}

void FileSynchronizer::onFileModified()
{
    // locate the current end of the file
    iFile.seekg(0, std::ios::end);
    std::streampos new_end = iFile.tellg();

    // if the file has been added to
    if (new_end > oFileEnd)
    {
        // read the additions to a character buffer
        size_t addedBytes = static_cast<size_t>(new_end - oFileEnd);
        // move to the beginning of the additions
        iFile.seekg(static_cast<long int>(-addedBytes), std::ios::end);
        std::string buffer(addedBytes, ' ');
        iFile.read(&buffer[0], static_cast<long int>(addedBytes));
        // write to outFile and update
        oFile << buffer;
        oFile.flush();

        // set the new end of the file
        oFileEnd = new_end;
    }
}
