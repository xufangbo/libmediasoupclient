#pragma once

#define log_trace(fromat, ...) logger(log_trace, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)
#define log_debug(fromat, ...) logger(log_debug, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)
#define log_info(fromat, ...) logger(log_info, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)
#define log_warn(fromat, ...) logger(log_warn, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)
#define log_error(fromat, ...) logger(log_error, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)
#define log_fatal(fromat, ...) logger(log_fatal, __FUNCTION__, __FILE__, __LINE__, fromat, ##__VA_ARGS__)

typedef enum {
  log_trace = 0,
  log_debug = 1,
  log_info = 2,
  log_warn = 3,
  log_error = 4,
  log_fatal = 5
} LogLevel;

typedef short boolean; // 兼容C/C++

typedef struct {
  /**
   * 选项是否已经初始化
   * bool类型
   * **/
  boolean initalized;
  /**
   * 日志级别
   * **/
  LogLevel level;
  /**
   * 是否显示时间
   */
  boolean is_time;

  /**
   * 是否显示level
   */
  boolean is_level;

  /**
   * 日志目录
   */
  char path[512];

  /**
   * 名称
   */
  char name[30];

  /**
   * 启用linux控制台颜色
   * 在cmd控制台中linux的颜色不能正常显示
   */
  boolean is_color;

  /**
   * @brief 日志是否写入文件
   * bool类型
   *
   */
  boolean writeFile;

  /**
   * a+ / w+
   */
  char fileMode[10];

  /**
   * 日志保留天数
   */
  int days;
  boolean hideWorkingPath;
  int workingPath;
} log_options;

/* 执行日志 */
void logger(LogLevel level, const char* function, const char* file, int line, const char* fromat, ...);

/* 通过配置文件初始化log设置 */
void log_read_config(const char* name);

/* 设置日志配置 */
int log_init(log_options option);

int log_init_default();

