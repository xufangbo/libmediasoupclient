#include "log.hpp"

#include <dirent.h>
#include <malloc.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <errno.h>
#include <error.h>
#include <sys/stat.h>  //mkdir
#include <sys/time.h>
#endif

#define ZONE __FUNCTION__, __FILE__, __LINE__
#define MAX_DEBUG 1024
#define LEVEL_COUNT 6
#define FILE_LENGTH 1024

#define true 1
#define false 0

#if defined(_WIN32) || defined(_WIN64)
#define green FOREGROUND_GREEN
#define red FOREGROUND_RED
#define blue FOREGROUND_BLUE
#define white FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE
#define yellow FOREGROUND_RED | FOREGROUND_GREEN
#define gray FOREGROUND_BLUE | FOREGROUND_GREEN
#define purple FOREGROUND_BLUE | FOREGROUND_RED
#else

/**
 * \033[显示方式;前景色;背景色m
 * 显示方式: 0（默认值）、1（高亮）、22（非粗体）、4（下划线）、24（非下划线）、5（闪烁）、25（非闪烁）、7（反显）、27（非反显）
 * 前景色:  30（黑色）、31（红色）、32（绿色）、 33（黄色）、34（蓝色）、35（洋红）、36（青色）、37（白色）
 * 背景色:  40（黑色）、41（红色）、42（绿色）、 43（黄色）、44（蓝色）、45（洋红）、46（青色）、47（白色）
 */

static const char* NONE = "\033[0m";         // 清除颜色，即之后的打印为正常输出，之前的不受影响
// static const char* BLACK = "\033[0;30m";     // 深黑
// static const char* L_BLACK = "\033[1;30m";   // 亮黑，偏灰褐
// static const char* RED = "\033[0;31m";       // 深红，暗红
static const char* L_RED = "\033[1;31m";     // 鲜红
// static const char* GREEN = "\033[0;32m";     // 深绿，暗绿
static const char* L_GREEN = "\033[1;32m";   // 鲜绿
// static const char* BROWN = "\033[0;33m";     // 深黄，暗黄
static const char* YELLOW = "\033[1;33m";    // 鲜黄
// static const char* BLUE = "\033[0;34m";      // 深蓝，暗蓝
// static const char* L_BLUE = "\033[1;34m";    // 亮蓝，偏白灰
// static const char* PURPLE = "\033[0;35m";    // 深粉，暗粉，偏暗紫
static const char* L_PURPLE = "\033[1;35m";  // 亮粉，偏白灰
static const char* CYAN = "\033[0;36m";      // 暗青色
// static const char* L_CYAN = "\033[1;36m";    // 鲜亮青色
static const char* GRAY = "\033[0;37m";      // 灰色
// static const char* WHITE = "\033[1;37m";     // 白色，字体粗一点，比正常大，比bold小
// static const char* BOLD = "\033[1m";         // 白色，粗体
// static const char* UNDERLINE = "\033[4m";    // 下划线，白色，正常大小
// static const char* BLINK = "\033[5m";        // 闪烁，白色，正常大小
// static const char* REVERSE = "\033[7m";      // 反转，即字体背景为白色，字体为黑色
// static const char* HIDE = "\033[8m";         // 隐藏
// static const char* CLEAR = "\033[2J";        // 清除
// static const char* CLRLINE = "\r\033[K";     // 清除行
#endif

#if defined(WIN32) || defined(WIN64)
static const char* seperator = "\\";
#else
static const char* seperator = "/";
#endif

static const char levels[LEVEL_COUNT][6] = {"trace", "debug", "info ", "warn ", "error", "fatal"};
static log_options logopts;
static FILE* fp = NULL;
static time_t this_day = 0;  // 天数据，每天一个日志文件
static char fileName[FILE_LENGTH];

// pthread_t tid;
pthread_mutex_t mut;

static void wirte_console(LogLevel level, char* message);
static void LogFile_write(LogLevel level, char* message);
static void LogFile_generateFileName();
static int string_sub(char* dest, char* src, int startIndex, int endIndex);
static int string_indexOf(char* str, char c);
static time_t DateTime_now();
static int DateTime_now_int();
static int mkdirs(char* path);
static void DateTime_day(char* date, int len, int days);
static void DateTime_millsecond(char* date, int len);

void logger(LogLevel level, const char* function, const char* file, int line, const char* msgfmt, ...) {
  if (!logopts.initalized) {
    log_init_default();
  }
  if (level < logopts.level) {
    return;
  }

  pthread_mutex_lock(&mut);  // 加锁，用于对共享变量操作

  char header[64];
  memset(header, 0, sizeof(header));
  if (logopts.is_time) {
    char time[25];
    DateTime_millsecond(time, 25);

    strcat(header, time);
    strcat(header, " ");
  }

  if (logopts.is_level) {
    strcat(header, "[");
    strcat(header, levels[level]);
    strcat(header, "]");
  }

  const char* fileName = file;
  if (logopts.hideWorkingPath) {
    fileName = file + logopts.workingPath;
  }

  char message[MAX_DEBUG];
  memset(message, '\0', MAX_DEBUG);

  int tid = syscall(SYS_gettid);
  snprintf(message, MAX_DEBUG, "%s [%4u] %s:%d %s : ", header, tid, fileName, line, function);
  int sz = strlen(message);
  char* pos = message;

  // 参数处理
  va_list ap;
  va_start(ap, msgfmt);
  vsnprintf(pos + sz, MAX_DEBUG - sz, msgfmt, ap);
  va_end(ap);

  // stdout
  wirte_console(level, message);
  LogFile_write(level, message);

  pthread_mutex_unlock(&mut);  // 解锁
}

#if defined(_WIN32) || defined(_WIN64)
void wirte_console(LogLevel& level, char* message) {
  HANDLE outPutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

  if (level == log_trace) {
    SetConsoleTextAttribute(outPutHandle, gray);
  } else if (level == log_debug) {
    SetConsoleTextAttribute(outPutHandle, white);
  } else if (level == log_info) {
    SetConsoleTextAttribute(outPutHandle, green);
  } else if (level == log_warn) {
    SetConsoleTextAttribute(outPutHandle, yellow);
  } else if (level == log_error) {
    SetConsoleTextAttribute(outPutHandle, red);
  } else if (level == log_fatal) {
    SetConsoleTextAttribute(outPutHandle, purple);
  } else {
    SetConsoleTextAttribute(outPutHandle, white);
  }

  SetConsoleTextAttribute(outPutHandle, white);

  printf("%s\n", message);
  fflush(stdout);
}
#else
void wirte_console(LogLevel level, char* message) {
  char* color = NULL;
  if (level == log_trace) {
    color = (char*)GRAY;
  } else if (level == log_debug) {
    color = (char*)CYAN;
  } else if (level == log_info) {
    color = (char*)L_GREEN;
  } else if (level == log_warn) {
    color = (char*)YELLOW;
  } else if (level == log_error) {
    color = (char*)L_RED;
  } else if (level == log_fatal) {
    color = (char*)L_PURPLE;
  } else {
    color = (char*)NONE;
  }

  if (logopts.is_color) {
    printf("%s%s%s\n", color, message, NONE);
  } else {
    printf("%s\n", message);
  }

  printf("%s",NONE);
  fflush(stdout);
}
#endif

void LogFile_create() {
  if (!logopts.writeFile) {
    return;
  }
  if (!logopts.initalized) {
    printf("log is not initilaized\n");
    return;
  }
  LogFile_generateFileName();

  fp = fopen(fileName, logopts.fileMode);
  if (fp == NULL) {
    char* err_msg = strerror(errno);
    printf("%s(%s:%d) fail to open log file : %s , error code %d , %s \n", __FUNCTION__, __FILE__, __LINE__, fileName, errno, err_msg);
    return;
  }
  // fclose(fp);
  // fp = NULL;
}

void LogFile_write(LogLevel level, char* message) {
  if (!logopts.writeFile) {
    return;
  }
  if (!logopts.initalized) {
    printf("log is not initilaized\n");
    return;
  }

  if (DateTime_now() != this_day) {
    LogFile_generateFileName();

    if (fp != NULL) {
      fclose(fp);
      fp = NULL;
    }

    fp = fopen(fileName, "a+");
    if (fp == NULL) {
      char* err_msg = strerror(errno);
      printf("%s(%s:%d) fail to open log file : %s , error code %d , %s \n",
             __FUNCTION__, __FILE__, __LINE__, fileName, errno, err_msg);
      return;
    } else {
      // printf("write file %s\n", fileName);
    }
  }

  fprintf(fp, "%s\n", message);
  fflush(fp);
}

void LogFile_generateFileName() {
  this_day = DateTime_now();

  memset(fileName, 0, FILE_LENGTH);

  char day_string[12];
  DateTime_day(day_string, 12, 0);

  sprintf(fileName, "%s%s%s-%s.log", logopts.path, seperator, logopts.name, day_string);

  // 删除x天之前文件
  DateTime_day(day_string, 12, logopts.days);
  char del_file[FILE_LENGTH];
  memset(del_file, 0, FILE_LENGTH);
  sprintf(del_file, "%s%s%s-%s.log", logopts.path, seperator, logopts.name, day_string);
  if (access(del_file, F_OK) == 0) {
    int ret = unlink(del_file);
    if (ret != 0) {
      printf("can't delete %s, errno %d,errmsg : %s", del_file, errno, strerror(errno));
    }
  }
}

static pthread_t p;
static void HistoryJob_execute() {
  DIR* dp = opendir(logopts.path);
  if (dp == NULL) {
    return;
  }
  struct dirent* entry;
  while ((entry = readdir(dp)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0) {
      continue;
    }
    if (strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char* fileName = entry->d_name;
    if (strstr(fileName, logopts.name) != 0) {
      continue;
    }
    if (strstr(fileName, ".log") == NULL) {
      continue;
    }

    int index = string_indexOf(fileName, '-');
    if (index < 0) {
      continue;
    }
    fileName += index;
    fileName++;

    char buffer[5];

    memset(buffer, 0, 5);
    string_sub(buffer, fileName, 0, 3);
    int year = atoi(buffer);

    memset(buffer, 0, 5);
    string_sub(buffer, fileName, 5, 6);
    int month = atoi(buffer);

    memset(buffer, 0, 5);
    string_sub(buffer, fileName, 8, 9);
    int day = atoi(buffer);

    int days = DateTime_now_int();
    int fileDay = year * 10000 + month * 100 + day;
    if (fileDay < days) {
      log_info("remove log history file %s", entry->d_name);

      char fullName[512];
      memset(fullName, 0, 512);
      strcat(fullName, logopts.path);
      strcat(fullName, "/");
      strcat(fullName, entry->d_name);
      int rc = unlink(fullName);
      if (rc != 0) {
        log_error("can't delete file %s", fullName);
      }
    }
  }
  closedir(dp);
  dp = NULL;
}

static void* HistoryJob_start(void* arg) {
  while (true) {
    usleep(strlen(logopts.name) * 60 * 1000000);
    HistoryJob_execute();
  }
}

int log_init_default() {
  log_options opts = {};
  opts.is_color = 1;
  opts.writeFile = 0;
  opts.is_time = 1;
  opts.is_level = 0;
  sprintf(opts.fileMode, "%s", "w+");
  return log_init(opts);
}

int log_init(log_options opts) {
  pthread_mutex_init(&mut, NULL);

  // 初始化日志选项
  logopts = opts;
  if (strlen(logopts.name) == 0) {
    sprintf(logopts.name, "%s", "app");
  }

  if (strlen(logopts.fileMode) == 0) {
    sprintf(logopts.fileMode, "%s", "a");
  }

  // 启动日志监控线程
  if (logopts.writeFile) {
    pthread_create(&p, NULL, HistoryJob_start, NULL);

    // 创建日志目录
    if (strlen(logopts.path) == 0) {
      sprintf(logopts.path, "%s", "logs");
    }
    mkdirs(logopts.path);

    // char* buf = get_current_dir_name();

    char cwd_path[512];
    getcwd(cwd_path, 512);
    printf("working directory: %s\n", cwd_path);
    logopts.workingPath = strlen(cwd_path);
  }

  logopts.initalized = true;
  //  printf("option.initalized = true \n");

  LogFile_create();

  return 0;  // fp == NULL ? -1 : 0;
}

// char log_parseLevel(const char* level) {
//   for (int i = 0; i < LEVEL_COUNT; i++) {
//     int count = 0;
//     for (int j = 0; j < 4; j++) {
//       if (levels[i][j] != level[j]) {
//         break;
//       } else {
//         count++;
//       }
//     }

//     if (count == 4) {
//       // printf("parsed level %s\n", level);
//       return i;
//     }
//   }

//   printf("can't parse log level\n");

//   return -1;
// }

//////////////////////////////////////////////////////////////////////

int mkdirs(char* path) {
  printf("log path: %s \n", path);

  char newPath[1024];
  memset(newPath, 0, 1024);

#if defined(_WIN32) || defined(_WIN64)
#else
  if (path[0] == seperator[0]) {
    strcat(newPath, seperator);
  }
#endif

  char* p = strtok(path, seperator);

  while (p) {
    strcat(newPath, p);
    strcat(newPath, seperator);

    if (access(newPath, F_OK) != F_OK) {
      // printf("mkdir %s\n", newPath);

#if defined(_WIN32) || defined(_WIN64)
      int ret = mkdir(newPath);
      if (ret != 0) {
        printf("cant't mkdir %s \n", newPath);
        return 1;
      }
#else
      // int ret = mkdir(newPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      int ret = mkdir(newPath, 0777);
      // https://blog.csdn.src/yinjian1013/article/details/78611009

      if (ret < 0) {
        printf("cant't mkdir %s \n", newPath);
        return 1;
      }
#endif
    } else {
      // printf("%s is exsist\n", newPath);
    }

    p = strtok(NULL, seperator);
  }

  return 0;
}

time_t DateTime_now() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    perror("gettimeofday");
    return 0;
  }

  return tv.tv_sec;
}

void DateTime_day(char* date, int len, int days) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    perror("gettimeofday");
    return;
  }

  tv.tv_sec -= days * 24 * 60 * 60;

  struct tm* local_time = localtime(&(tv.tv_sec));

  memset(date, 0, len);
  strftime(date, len, "%Y-%m-%d", local_time);
}

int DateTime_now_int() {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    perror("gettimeofday");
    return 0;
  }
  // long int total_microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
  time_t current_time = tv.tv_sec;
  struct tm* local_time = localtime(&current_time);

  return (local_time->tm_year + 1900) * 10000 + (local_time->tm_mon + 1) * 100 + local_time->tm_mday;
}

void DateTime_millsecond(char* date, int len) {
  const int length = 20;
  char str[length];
  memset(str, 0, length);

  struct timeval tv;
  if (gettimeofday(&tv, NULL) == -1) {
    perror("gettimeofday");
    return;
  }
  // long int total_microseconds = tv.tv_sec * 1000000 + tv.tv_usec;
  time_t current_time = tv.tv_sec;
  struct tm* local_time = localtime(&current_time);
  //  asctime(local_time);
  // long int total_microseconds = tv.tv_sec * 1000000 + tv.tv_usec;

  strftime(str, length, "%Y-%m-%d %H:%M:%S", local_time);

  sprintf(date, "%s,%03ld", str, tv.tv_usec / 1000);
}

int string_indexOf(char* str, char c) {
  int len = strlen(str);
  for (int i = 0; i < len; i++) {
    if (str[i] == c) {
      return i;
    }
  }

  return -1;
}

int string_sub(char* dest, char* src, int startIndex, int endIndex) {
  if (startIndex >= endIndex) {
    return -1;
  }

  if (startIndex < 0) {
    return -1;
  }

  int len = strlen(src);
  if (endIndex >= len) {
    return -1;
  }

  for (int i = startIndex; i <= endIndex; i++) {
    dest[i - startIndex] = src[i];
  }

  return 0;
}