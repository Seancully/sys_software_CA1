#ifndef COMPANY_H
#define COMPANY_H

#include "daemon.h"
#include "sys/msg.h"
#include "dirent.h"
#include "pwd.h"

// The Directory Paths
#define UPLOAD_DIR "/var/company/upload"
#define REPORTING_DIR "/var/company/reporting"
#define BACKUP_DIR "/var/company/backup"
#define LOG_DIR "/var/company/logs"
#define CHANGE_LOG "/var/company/logs/change.log"
#define ERROR_LOG "/var/company/logs/error.log"
#define LOCK_FILE "/var/run/company_daemon.lock"
#define PID_FILE "/var/run/company_daemon.pid"

// Transfer time at 1AM
#define TRANSFER_TIME_HOUR 1
#define TRANSFER_TIME_MIN 0

// Message Structure
struct msg_buffer {
    long msg_type;
    char msg_text[100];
};

// Function declartions for the company operations
int lock_directories(void);
int unlock_directories(void);
int backup_reporting_dir(void);
int transfer_uploads(void);
int check_missing_uploads(void);
void monitor_uploads(void);
void log_message(int priority, const char *format, ...);
int setup_ipc(int msgid, long type, const char *msg);
void cleanup_ipc(int msgid);

#endif 