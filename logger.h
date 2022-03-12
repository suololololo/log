#ifndef LOGGER_H__
#define LOGGER_H__
#include <unistd.h>
#include <cstdint>
// #include <pthread.h>
#include <queue>
#include <string>
#include <exception>
#include <thread>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <time.h>
#include <map>
#include <stdarg.h>



const int BUFFERSIZE = 8 * 1024 * 1024;
const int LOGLINESIZE = 4096;
const int MEM_LIMIT = 512 * 1024 * 1024;
class LogBuffer
{
public:
    enum BufState
    {
        FREE = 0,
        FLUSH = 1
    };
    LogBuffer(int size = BUFFERSIZE);
    ~LogBuffer();

    int getAvailable() const
    {
        return buffersize - used;
    }
    int getUsed() const
    {
        return used;
    }

    int getState() const
    {
        return state;
    }
    void setState(BufState s)
    {
        state = s;
    }
    void append(const char *logLine, int len);
    void flushToFile(FILE *file);

private:
    char *buffer;
    uint32_t used;
    uint32_t buffersize;
    int state;
};
enum LoggerLevel
{
    DEBUG = 0,
    INFO,
    WARN,
    ERROR,
    FATAL
};

class Logger
{
public:
    Logger();
    ~Logger();
    void init(const char *logdir, LoggerLevel lev);
    void append(int lev, const char *file, int line, const char *func, const char *format, ...);
    void flush();

    LoggerLevel getLevel() const
    {
        return level;
    }

public:
    static Logger *getInstance()
    {
        static Logger logger;
        return &logger;
    }

private:
    FILE *file;
    LoggerLevel level;
    int bufNums;
    char save_ymdhms[64];
    // map 的锁
    std::mutex m_mutex;

    std::queue<LogBuffer *> flushQueue;
    std::thread flushThread;
    std::mutex flushmutex;
    std::condition_variable flushCondition;
    bool start; // flush 状态

    std::queue<LogBuffer *> freeQueue;
    std::mutex freemutex;

    std::map<std::thread::id, LogBuffer *> bufferMap;
};


#define LOG(level, format, ...)                                                                     \
    do                                                                                              \
    {                                                                                               \
        if (Logger::getInstance()->getLevel() <= level)                                             \
        {                                                                                           \
            Logger::getInstance()->append(level, __FILE__, __LINE__, __FUNCTION__, format, __VA_ARGS__); \
        }                                                                                           \
    } while (0)
    
#define LOG_INIT(logdir, level)                     \
    do                                              \
    {                                               \
        Logger::getInstance()->init(logdir, level); \
    } while (0)




#endif