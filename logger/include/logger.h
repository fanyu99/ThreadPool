#pragma once
#include <chrono>
#include <iostream>
#include <string>
#include <fstream>
#include <mutex>//多线程下保证日志线程安全
#define default_color "\033[0m"
#define blue "\033[34m"
#define green "\033[32m"
#define yellow "\033[33m"
#define red "\033[31m"
#define purple "\033[35m"
enum class Loglevel
{
    DEBUG = 1,   // 调试信息
    INFO = 2,    // 普通信息
    WARNING = 3, // 警告
    ERROR = 4,   // 错误
    FATAL = 5    // 致命错误
};
class Logger
{
private:
    mutable std::mutex logmtx;
    std::ofstream logfile;
    Loglevel currentLevel;
    bool writeToConsole;
    bool writeToFile;
    bool autoFlush;
    std::string logfilePath;
    // 辅助函数
    std::string getCurrenttime() const;              // 获取当前的时间
    std::string LevelToString(Loglevel level) const; // 将日志级别转变为字符串
    void setConsoleColor(Loglevel level) const;//设置终端颜色

public:
    // 实现单例模式
    Logger() ;
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;
    ~Logger();
    static Logger &instance();     // 返回单例
    void setLevel(Loglevel level); // 设置日志级别
    Loglevel getlevel() const;     // 返回当前的日志级别
    void enableConsole(bool act);
    void enableFile(bool act);
    void setAutoFlush(bool act);
    void setLogfilePath(const std::string &path);     // 设置日志文件路径
    void log(Loglevel level, const std::string &msg); // 日志记录
    // 日志级别的记录
    void debug(const std::string &msg);   // DEBUG
    void info(const std::string &msg);    // INFO
    void warning(const std::string &msg); // WARNING
    void error(const std::string &msg);   // ERROR
    void fatal(const std::string &msg);   // FATAL
};