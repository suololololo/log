#include "logger.h"

const char *LevelString[5] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};
LogBuffer::LogBuffer(int size) : buffersize(size), used(0), state(LogBuffer::BufState::FREE)
{
    buffer = new char[buffersize];
    if (buffer == nullptr)
    {
        std::cerr << "malloc error" << std::endl;
    }
}

LogBuffer::~LogBuffer()
{
    if (buffer != nullptr)
    {
        delete[] buffer;
    }
}

void LogBuffer::append(const char *logline, int len)
{
    memcpy(buffer + used, logline, len);
    used += len;
}
void LogBuffer::flushToFile(FILE *fp)
{
    uint32_t w = fwrite(buffer, 1, used, fp);
    if (w != used)
    {
        std::cerr << "write error" << std::endl;
    }
    used = 0;
    fflush(fp);
}

Logger::Logger() : level(LoggerLevel::INFO), file(nullptr), start(false), bufNums(0)
{
}

Logger::~Logger()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::map<std::thread::id, LogBuffer *>::iterator it;
        for (it = bufferMap.begin(); it != bufferMap.end(); ++it)
        {
            it->second->setState(LogBuffer::BufState::FLUSH);
            {
                std::lock_guard<std::mutex> lock(flushmutex);
                flushQueue.push(it->second);
            }
        }
    }
    flushCondition.notify_one();
    start = false;
    flushCondition.notify_one();
    if (flushThread.joinable())
        flushThread.join();
    if (file != nullptr)
    {
        fclose(file);
    }

    while (!freeQueue.empty())
    {

        LogBuffer *curr = freeQueue.front();
        freeQueue.pop();
        delete curr;
    }

    while (!flushQueue.empty())
    {
        LogBuffer *curr = flushQueue.front();
        flushQueue.pop();
        delete curr;
    }
}
void Logger::init(const char *logdir, LoggerLevel lev)
{
    time_t now = time(nullptr);
    struct tm *ptm = localtime(&now);

    char logPath[256] = {0};
    snprintf(logPath, 255, "%s/log_%d_%d_%d", logdir, ptm->tm_yday + 1900, ptm->tm_mon + 1, ptm->tm_mday);
    level = lev;
    file = fopen(logPath, "w");
    if (file == nullptr)
    {
        std::cerr << "open file error" << std::endl;
    }

    flushThread = std::thread(&Logger::flush, this);
}
void Logger::append(int lev, const char *file, int line, const char *func, const char *format, ...)
{
    // 1. 获取当前时间
    struct timeval tv;
    mingw_gettimeofday(&tv, NULL);
    static time_t lastsc = 0;
    if (tv.tv_sec == lastsc)
    {
        time_t now = time(NULL);
        struct tm *ptm = localtime(&now);
        lastsc = tv.tv_sec;
        int k = snprintf(save_ymdhms, 64, "%04d-%02d-%02d %02d:%02d:%02d", ptm->tm_year, ptm->tm_mon, ptm->tm_mday,
                         ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        save_ymdhms[k] = '\0';
    }
    std::thread::id tid = std::this_thread::get_id();

    char logLine[LOGLINESIZE];
    int m = snprintf(logLine, LOGLINESIZE, "[%s][%s.%03ld][%s:%d %s][pid:%u] ", LevelString[lev], save_ymdhms,
                     tv.tv_usec / 1000, file, line, func, std::hash<std::thread::id>()(tid));

    va_list args;
    va_start(args, format);
    int n = vsnprintf(logLine + m, LOGLINESIZE - m, format, args);
    va_end(args);

    // 总共写入长度
    int len = m + n;
    // 找个buffer 写入
    LogBuffer *buf = nullptr;
    std::map<std::thread::id, LogBuffer *>::iterator it = bufferMap.begin();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        it = bufferMap.find(tid);
        if (it != bufferMap.end())
        {
            buf = it->second;
        }
        else
        {
            bufferMap[tid] = buf = new LogBuffer();
            bufNums++;
            std::cout << "create new buffer " << bufNums << std::endl;
        }
    }
    if (buf->getAvailable() >= len && buf->getState() == LogBuffer::BufState::FREE)
    {
        buf->append(logLine, len);
    }
    else
    {
        if (buf->getState() == LogBuffer::BufState::FREE)
        {
            //加到flushQueue中
            buf->setState(LogBuffer::BufState::FLUSH);
            {
                std::lock_guard<std::mutex> lock(flushmutex);
                flushQueue.push(buf);
            }
            flushCondition.notify_one();
            std::lock_guard<std::mutex> lock(freemutex);
            // 从freeQueue 中 取出一个
            if (!freeQueue.empty())
            {
                buf = freeQueue.front();
                freeQueue.pop();
            }
            // new buf
            else
            {
                if (bufNums * BUFFERSIZE < MEM_LIMIT)
                {
                    buf = new LogBuffer();
                    bufNums++;
                }
                else
                {
                    std::cout << "no memory to create buffer" << std::endl;
                    return;
                }
            }
            buf->append(logLine, len);
            {
                std::lock_guard<std::mutex> mlock(m_mutex);
                it->second = buf;
            }
        }
        // buf 正在flush 在free 中取一个
        else
        {
            std::lock_guard<std::mutex> lock(freemutex);
            if (!freeQueue.empty())
            {
                buf = freeQueue.front();
                freeQueue.pop();
            }
            buf->append(logLine, len);
            it->second = buf;
        }
    }
}
void Logger::flush()
{
    start = true;
    while (true)
    {
        LogBuffer *buffer;
        {
            std::unique_lock<std::mutex> lock(flushmutex);
            while (flushQueue.empty() && start)
            {
                flushCondition.wait(lock);
            }
            if (flushQueue.empty() && !start)
                return;
            buffer = flushQueue.front();
            flushQueue.pop();
        }
        buffer->flushToFile(file);
        buffer->setState(LogBuffer::BufState::FREE);
        {
            std::unique_lock<std::mutex> lock(freemutex);
            freeQueue.push(buffer);
        }
    }
}