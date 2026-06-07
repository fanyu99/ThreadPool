#include "logger.h"
bool operator<(Loglevel l1, Loglevel l2)
{
    return int(l1) < int(l2);
}
Logger::Logger() : currentLevel(Loglevel::INFO), writeToConsole(true), writeToFile(false), autoFlush(false), logfilePath{"App.log"} {}
Logger::~Logger()
{
    if (logfile.is_open())
    {
        logfile.flush();
    }
    logfile.close();
}
Logger &Logger::instance()
{
    static Logger instance;
    return instance;
}
// 通用函数
Loglevel Logger::getlevel() const
{
    return this->currentLevel;
}
void Logger::enableConsole(bool act)
{
    this->writeToConsole = act;
    std::cout << (act ? "已启用终端输出!\n" : "已禁用终端输出!\n");
}
void Logger::enableFile(bool act)
{
    this->writeToFile = act;
    if (act)
    {
        logfile.open(logfilePath, std::ios::app);
        if (!logfile.is_open())
        {
            std::cerr << "日志文件打开失败!\n";
            return;
        }
    }
    else
    {
        if (logfile.is_open())
            logfile.close();
    }
    std::cout << (act ? "已启用写入文件!\n" : "已禁用写入文件!\n");
}
void Logger::setAutoFlush(bool act)
{
    this->autoFlush = act;
    std::cout << (act ? "已启用自动刷新!\n" : "已禁用自动刷新!\n");
}
void Logger::setLevel(Loglevel level)
{
    this->currentLevel = level;
}
void Logger::setLogfilePath(const std::string &path)
{
    if (this->logfile.is_open())
    {
        logfile.close();
    }
    this->logfilePath = path;
    if (this->writeToFile)
    {
        logfile.open(this->logfilePath, std::ios::app);
        if (!logfile.is_open())
        {
            std::cerr << "日志文件未打开/打开失败!请检查路径!\n";
        }
    }
}
// 辅助函数
std::string Logger::getCurrenttime() const
{
    auto now_time_point = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now_time_point);
    std::tm nowtm = *std::localtime(&now_time_t);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y年%m月%d日 %H时:%M分:%S秒", &nowtm);
    return std::string(buf);
}
std::string Logger::LevelToString(Loglevel level) const
{

    switch (level)
    {
    case Loglevel::DEBUG:
        return "DEBUG";
        break;
    case Loglevel::INFO:
        return "INFO";
        break;
    case Loglevel::WARNING:
        return "WARNING";
        break;
    case Loglevel::ERROR:
        return "ERROR";
        break;
    case Loglevel::FATAL:
        return "FATAL";
        break;
    default:
        return "";
        break;
    }
}
void Logger::setConsoleColor(Loglevel level) const
{
    switch (level)
    {
    case Loglevel::DEBUG:
        std::cout << blue;
        break;
    case Loglevel::INFO:
        std::cout << green;
        break;
    case Loglevel::WARNING:
        std::cout << yellow;
        break;
    case Loglevel::ERROR:
        std::cout << red;
        break;
    case Loglevel::FATAL:
        std::cout << purple;
        break;
    default:
        break;
    }
}
// 日志记录:
void Logger::log(Loglevel level, const std::string &msg)
{
    std::lock_guard<std::mutex> lock(logmtx);
    std::string logmsg = "[" + getCurrenttime() + "]\t[" + LevelToString(level) + "]\t[" + msg + "]\n";
    if (writeToConsole&&level>=Loglevel::INFO)
    {
        setConsoleColor(level);
        std::cout << logmsg;
        std::cout << default_color;
    }
    if (writeToFile)
    {
        if (!logfile.is_open())
        {
            std::cerr << "日志文件打开失败!\n";
        }
        else
        {
            logfile << logmsg;
            if (autoFlush)
                logfile.flush();
        }
    }
}
void Logger::debug(const std::string &msg) // DEBUG
{
    log(Loglevel::DEBUG, msg);
}
void Logger::info(const std::string &msg) // INFO
{
    log(Loglevel::INFO, msg);
}
void Logger::warning(const std::string &msg) // WARNING
{
    log(Loglevel::WARNING, msg);
}
void Logger::error(const std::string &msg) // ERROR
{
    log(Loglevel::ERROR, msg);
}
void Logger::fatal(const std::string &msg) // FATAL
{
    log(Loglevel::FATAL, msg);
}
