#include "logger.h"

int64_t get_current_millis(void)
{
    struct timeval tv;
    mingw_gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

//单线程异步写入测试
static int logs = 100 * 10000;
void single_thread_test()
{
    printf("single_thread_test...\n");
    uint64_t start_ts = get_current_millis();
    for (int i = 0; i < logs; ++i)
    {
        LOG(LoggerLevel::ERROR, "log test %d\n", i);
    }
    uint64_t end_ts = get_current_millis();
    printf("1 million times logtest, time use %lums, %ldw logs/second\n", end_ts - start_ts, logs / (end_ts - start_ts) / 10);
}

static int threadnum = 4;
void func() {
    for (int i = 0;i < logs; ++i)
    {
        LOG(LoggerLevel::ERROR, "log test %d\n", i);
    }    
}

void multi_thread_test() {
    printf("multi_thread_test, threadnum: %d ...\n", threadnum);
    uint64_t start_ts = get_current_millis();
    std::thread threads[threadnum];
    for(int i = 0; i < threadnum; ++i) {
        threads[i] = std::thread(&func);
    }
    for(int i = 0; i < threadnum; ++i) {
        threads[i].join();
    }
    uint64_t end_ts = get_current_millis();
    printf("%d million times logtest, time use %lums, %ldw logs/second\n", threadnum, end_ts - start_ts, threadnum*logs/(end_ts - start_ts)/10);

}


int main()
{
    LOG_INIT(".", LoggerLevel::INFO);
    single_thread_test();
    multi_thread_test();
}