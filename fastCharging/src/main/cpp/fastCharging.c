#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <android/log.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include "minIni.h"
#include <pthread.h>

#define LOG_TAG "FastChargingModule"

int current_max = -1;
int interval = 1000;
int charging_wait_time = 5000;
int reload_config_interval = 60;
int enable_log = 1;
int log_max_size = 1024;

char log_file_path[PATH_MAX];
char ini_file_path[PATH_MAX];

char chargingData[1024][2][1024];
int dataCount = 0;

void log_message(int level, const char *format, ...) {
    if (!enable_log) return;
    char type;
    switch (level) {
        case ANDROID_LOG_DEBUG:
            type = 'D';
            break;
        case ANDROID_LOG_INFO:
            type = 'I';
            break;
        case ANDROID_LOG_WARN:
            type = 'W';
            break;
        case ANDROID_LOG_ERROR:
            type = 'E';
    }
    va_list args;
    va_start(args, format);

    struct stat st;
    if (stat(log_file_path, &st) == 0 && st.st_size >= (log_max_size * 1024)) {
        char backup_file_path[PATH_MAX];
        snprintf(backup_file_path, sizeof(backup_file_path), "%s.bak", log_file_path);
        rename(log_file_path, backup_file_path);
    }

    __android_log_vprint(level, LOG_TAG, format, args);

    FILE *log_file = fopen(log_file_path, "a");
    if (log_file != NULL) {
        time_t current_time;
        struct tm *time_info;
        char time_string[20];

        time(&current_time);
        time_info = localtime(&current_time);
        strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);

        fprintf(log_file, "[%s] [%c] ", time_string, type);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");

        fclose(log_file);
    }

    va_end(args);
}

#define LOGD(...) log_message(ANDROID_LOG_DEBUG, __VA_ARGS__)
#define LOGI(...) log_message(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGW(...) log_message(ANDROID_LOG_WARN, __VA_ARGS__)
#define LOGE(...) log_message(ANDROID_LOG_ERROR, __VA_ARGS__)

void write_file(const char *_Nonnull path, const char *_Nonnull text) {

    struct stat buffer;
    if (stat(path, &buffer) != 0) {
        return;
    }

    if (access(path, W_OK) != 0) {
        LOGW("文件%s没有写入权限，尝试添加写入权限", path);
        if (chmod(path, S_IWUSR) != 0) {
            LOGE("添加写入权限错误：%s", strerror(errno));
            return;
        }
    }

    FILE *fp = fopen(path, "w");
    if (fp == NULL)
        return;
    fputs(text, fp);
    fclose(fp);
}

void write_fast_charging_data() {
    char str[1024];

    for (int i = 0; i < dataCount; i++) {
        if (strcmp(chargingData[i][1], "{current_max}") == 0) {
            sprintf(str, "%d\n", current_max);
            write_file(chargingData[i][0], str);
        } else {
            sprintf(str, "%s\n", chargingData[i][1]);
            write_file(chargingData[i][0], str);
        }
    }
}

int isCharging() {
    FILE *fp;
    char status[20];

    fp = fopen("/sys/class/power_supply/battery/status", "r");
    if (fp == NULL) {
        LOGE("打开文件错误：%s", strerror(errno));
        return -1;
    }

    if (fgets(status, sizeof(status), fp) != NULL) {
        status[strcspn(status, "\n")] = 0;

        if (strcmp(status, "Charging") == 0) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

void load_config() {
    // 读取配置文件
    current_max = ini_getl("setting", "current_max", -1, ini_file_path);
    interval = ini_getl("setting", "interval", 1000, ini_file_path);
    reload_config_interval = ini_getl("setting", "reload_config_interval", 1000, ini_file_path);
    charging_wait_time = ini_getl("setting", "charging_wait_time", 5000, ini_file_path);
    enable_log = ini_getl("setting", "enable_log", 1, ini_file_path);
    log_max_size = ini_getl("setting", "log_max_size", 1024, ini_file_path);

    if (current_max == -1) {
        LOGE("配置文件必须填写current_max值");
        exit(1);
    }

    interval *= 1000;
    charging_wait_time *= 1000;

    char section[] = "chargingData";
    char key[PATH_MAX];
    char value[1024];
    dataCount = 0;
    while (ini_getkey(section, dataCount, key, sizeof(key), ini_file_path) > 0) {
        ini_gets(section, key, "", value, sizeof(value), ini_file_path);
        strcpy(chargingData[dataCount][0], key);
        strcpy(chargingData[dataCount][1], value);
        dataCount++;
    }
}

void *reload_config_thread(void *arg) {
    while (1) {
        usleep(reload_config_interval * 1000 * 1000);
        load_config();
    }
    return NULL;
}

int main() {
    char *program_dir = "/data/adb/fastCharging";

    // 日志文件和配置文件路径
    snprintf(log_file_path, sizeof(log_file_path), "%s/log.txt", program_dir);
    snprintf(ini_file_path, sizeof(ini_file_path), "%s/config.ini", program_dir);

    load_config();

    pthread_t tid;
    if (pthread_create(&tid, NULL, reload_config_thread, NULL) != 0) {
        LOGE("创建线程失败：%s", strerror(errno));
        return 1;
    }

    // 开始监听
    int charging;
    LOGI("开始监听充电状态");
    while (1) {
        charging = isCharging();
        if (charging == -1) exit(1);
        if (charging) {
            LOGI("开始充电");
            // 充电开始后等待一定时间再循环写入电流数据
            usleep(charging_wait_time);
            while (1) {
                charging = isCharging();
                if (charging == -1) return 1;
                // 充电结束，退出此循环，在父循环继续监听
                if (!charging) {
                    LOGI("充电结束");
                    break;
                }
                // 写入电流数据
                write_fast_charging_data();
                usleep(interval);
            }
        }
        usleep(interval);
    }
}