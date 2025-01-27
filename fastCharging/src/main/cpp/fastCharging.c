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

char log_file_path[PATH_MAX];
char ini_file_path[PATH_MAX];

struct settings_data {
    int interval;
    int charging_wait_time;
    int reload_config_interval;
    int enable_log;
    int log_max_size;
    int retry_times;
} settingsData;

struct charging_data {
    int retry[1024];
    char chargingData[1024][2][1024];
    int dataCount;
} chargingData;

struct battery_info {
    int capacity;
    float temperature;
    int isCharging;
} batteryInfo;

void log_message(int level, const char *format, ...) {
    if (!settingsData.enable_log) return;
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
    if (stat(log_file_path, &st) == 0 && st.st_size >= (settingsData.log_max_size * 1024)) {
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

void write_fast_charging_data() {
    for (int i = 0; i < chargingData.dataCount; i++) {
        if (chargingData.retry[i] < 1) continue;
        if (strcmp("{{SHELL}}", chargingData.chargingData[i][0]) == 0) {

            char command[2048];
            snprintf(command, sizeof(command), "%s 2>&1", chargingData.chargingData[i][1]);

            FILE *fp = popen(command, "r");
            if (fp == NULL) {
                LOGE("执行命令失败: %s", strerror(errno));
                chargingData.retry[i]--;
                continue;
            }

            char buffer[1024];
            while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                LOGI("命令输出: %s", buffer);
            }

            int status = pclose(fp);
            if (status == -1) {
                LOGE("关闭命令失败: %s", strerror(errno));
                chargingData.retry[i]--;
            } else if (WEXITSTATUS(status) != 0) {
                LOGE("命令%s执行失败，返回值: %d", chargingData.chargingData[i][1], WEXITSTATUS(status));
                chargingData.retry[i]--;
            }
        } else {
            struct stat buffer;
            if (stat(chargingData.chargingData[i][0], &buffer) != 0) {
                LOGW("文件%s不存在", chargingData.chargingData[i][0]);
                chargingData.retry[i]--;
                return;
            }

            FILE *fp = fopen(chargingData.chargingData[i][0], "w");
            if (fp == NULL) {
                LOGE("打开文件%s错误：%s", chargingData.chargingData[i][0], strerror(errno));
                chargingData.retry[i]--;
                return;
            }
            fputs(chargingData.chargingData[i][1], fp);
            fputs("\n", fp);
            fclose(fp);
        }
    }
}

char *readFile(const char *path) {
    static char buf[1024];
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        LOGE("打开文件 %s 错误：%s", path, strerror(errno));
        return NULL;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        if (ferror(fp)) {
            LOGE("读取文件 %s 错误", path);
        }
        fclose(fp);
        return NULL;
    }

    buf[strcspn(buf, "\n")] = 0;
    fclose(fp);
    return buf;
}

void refreshBatteryInfo() {
    // capacity
    batteryInfo.capacity = atoi(readFile("/sys/class/power_supply/battery/capacity"));
    // temperature
    batteryInfo.temperature = (float) (atoi(readFile("/sys/class/power_supply/battery/temp")) /
                                       10.0f);
    // isCharging
    batteryInfo.isCharging =
            strcmp(readFile("/sys/class/power_supply/battery/status"), "Charging") == 0;

    char buf[1024];
    snprintf(buf, sizeof(buf), "%d", batteryInfo.capacity);
    setenv("BATTERY_CAPACITY", buf, 1);
    snprintf(buf, sizeof(buf), "%.1f", batteryInfo.temperature);
    setenv("BATTERY_TEMP", buf, 1);
}

void load_config() {
    // 读取配置文件
    settingsData.interval = ini_getl("setting", "interval", 1000, ini_file_path);
    settingsData.reload_config_interval = ini_getl("setting", "reload_config_interval", 60, ini_file_path);
    settingsData.charging_wait_time = ini_getl("setting", "charging_wait_time", 5000, ini_file_path);
    settingsData.enable_log = ini_getl("setting", "enable_log", 1, ini_file_path);
    settingsData.log_max_size = ini_getl("setting", "log_max_size", 8, ini_file_path);
    settingsData.retry_times = ini_getl("setting", "retry_times", 5, ini_file_path);

    settingsData.interval *= 1000;
    settingsData.charging_wait_time *= 1000;

    char section[] = "chargingData";
    char key[PATH_MAX];
    char value[1024];
    chargingData.dataCount = 0;
    while (ini_getkey(section, chargingData.dataCount, key, sizeof(key), ini_file_path) > 0) {
        ini_gets(section, key, "", value, sizeof(value), ini_file_path);
        if (strcmp(key, "{{SHELL}}") != 0 && chmod(key, S_IWUSR | S_IWGRP | S_IWOTH |
                       S_IRUSR | S_IRGRP | S_IROTH |
                       S_IXUSR | S_IXGRP | S_IXOTH) != 0) {
            LOGW("文件%s设置777权限错误：%s", key, strerror(errno));
        }
        strcpy(chargingData.chargingData[chargingData.dataCount][0], key);
        strcpy(chargingData.chargingData[chargingData.dataCount][1], value);
        chargingData.retry[chargingData.dataCount] = settingsData.retry_times;
        chargingData.dataCount++;
    }
}

void *reload_config_thread(void *arg) {
    while (1) {
        sleep(settingsData.reload_config_interval);
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
    LOGI("开始监听充电状态");
    while (1) {
        refreshBatteryInfo();
        if (batteryInfo.isCharging == -1) exit(1);
        if (batteryInfo.isCharging) {
            LOGI("开始充电");
            // 充电开始后等待一定时间再循环写入电流数据
            usleep(settingsData.charging_wait_time);
            while (1) {
                refreshBatteryInfo();
                if (batteryInfo.isCharging == -1) exit(1);
                // 充电结束，退出此循环，在父循环继续监听
                if (!batteryInfo.isCharging) {
                    LOGI("充电结束");
                    break;
                }
                // 写入电流数据
                write_fast_charging_data();
                usleep(settingsData.interval);
            }
        }
        usleep(settingsData.interval);
    }
}