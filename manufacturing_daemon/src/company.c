#include <syslog.h>
#include <stdarg.h>
#include "../inc/company.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pwd.h>
#include <limits.h>

/**
 * Lock the upload and reporting directories to prevent modifications
 * during backup and transfer operations
 * 
 * @return 1 on success, 0 on failure
 */
int lock_directories(void) {
    int success = 1;
    
    log_message(LOG_INFO, "Locking directories for backup/transfer operations");
    
    // Change permissions to read-only for upload directory
    if (chmod(UPLOAD_DIR, 0555) < 0) { // r-xr-xr-x
        log_message(LOG_ERR, "Failed to lock upload directory: %s", strerror(errno));
        success = 0;
    }
    
    // Change permissions to read-only for reporting directory
    if (chmod(REPORTING_DIR, 0555) < 0) { // r-xr-xr-x
        log_message(LOG_ERR, "Failed to lock reporting directory: %s", strerror(errno));
        success = 0;
    }
    
    return success;
}

/**
 * Unlock the upload and reporting directories after backup and transfer
 * 
 * @return 1 on success, 0 on failure
 */
int unlock_directories(void) {
    int success = 1;
    
    log_message(LOG_INFO, "Unlocking directories after backup/transfer operations");
    
    // Restore normal permissions for upload directory
    if (chmod(UPLOAD_DIR, 0755) < 0) { // rwxr-xr-x
        log_message(LOG_ERR, "Failed to unlock upload directory: %s", strerror(errno));
        success = 0;
    }
    
    // Restore normal permissions for reporting directory
    if (chmod(REPORTING_DIR, 0755) < 0) { // rwxr-xr-x
        log_message(LOG_ERR, "Failed to unlock reporting directory: %s", strerror(errno));
        success = 0;
    }
    
    return success;
}

/**
 * Backup the reporting directory to the backup location
 * 
 * @return 1 on success, 0 on failure
 */
int backup_reporting_dir(void) {
    DIR *dir;
    struct dirent *entry;
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    FILE *src_file, *dst_file;
    char buffer[4096];
    size_t bytes;
    int success = 1;
    time_t now;
    struct tm *time_info;
    char timestamp[20];
    
    log_message(LOG_INFO, "Starting backup of reporting directory");
    
    // Get current time for backup folder naming
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", time_info);
    
    // Create a timestamped backup directory
    char backup_dir_path[PATH_MAX];
    snprintf(backup_dir_path, sizeof(backup_dir_path), "%s/backup_%s", BACKUP_DIR, timestamp);
    
    if (mkdir(backup_dir_path, 0755) < 0) {
        log_message(LOG_ERR, "Failed to create backup directory %s: %s", 
                    backup_dir_path, strerror(errno));
        return 0;
    }
    
    // Open reporting directory
    dir = opendir(REPORTING_DIR);
    if (dir == NULL) {
        log_message(LOG_ERR, "Failed to open reporting directory: %s", strerror(errno));
        return 0;
    }
    
    // Copy each file in reporting directory to backup
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Only backup XML files
        if (strstr(entry->d_name, ".xml") == NULL) {
            continue;
        }
        
        // Create full path for source and destination
        snprintf(src_path, sizeof(src_path), "%s/%s", REPORTING_DIR, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", backup_dir_path, entry->d_name);
        
        // Open source file for reading
        src_file = fopen(src_path, "rb");
        if (src_file == NULL) {
            log_message(LOG_ERR, "Failed to open source file %s: %s",
                       src_path, strerror(errno));
            success = 0;
            continue;
        }
        
        // Open destination file for writing
        dst_file = fopen(dst_path, "wb");
        if (dst_file == NULL) {
            log_message(LOG_ERR, "Failed to create destination file %s: %s",
                       dst_path, strerror(errno));
            fclose(src_file);
            success = 0;
            continue;
        }
        
        // Copy file contents
        while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
            if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
                log_message(LOG_ERR, "Error writing to destination file %s: %s",
                           dst_path, strerror(errno));
                success = 0;
                break;
            }
        }
        
        // Check for read error
        if (ferror(src_file)) {
            log_message(LOG_ERR, "Error reading from source file %s: %s",
                       src_path, strerror(errno));
            success = 0;
        }
        
        // Close files
        fclose(src_file);
        fclose(dst_file);
        
        log_message(LOG_INFO, "Backed up file: %s", entry->d_name);
    }
    
    closedir(dir);
    
    if (success) {
        log_message(LOG_INFO, "Backup completed successfully to %s", backup_dir_path);
    } else {
        log_message(LOG_WARNING, "Backup completed with errors");
    }
    
    return success;
}

/**
 * Transfer files from upload directory to reporting directory
 * 
 * @return 1 on success, 0 on failure
 */
int transfer_uploads(void) {
    DIR *dir;
    struct dirent *entry;
    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];
    int success = 1;
    
    log_message(LOG_INFO, "Starting transfer of uploads to reporting directory");
    
    // Open upload directory
    dir = opendir(UPLOAD_DIR);
    if (dir == NULL) {
        log_message(LOG_ERR, "Failed to open upload directory: %s", strerror(errno));
        return 0;
    }
    
    // Process each file in the upload directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Only process XML files
        if (strstr(entry->d_name, ".xml") == NULL) {
            continue;
        }
        
        // Create full path for source and destination
        snprintf(src_path, sizeof(src_path), "%s/%s", UPLOAD_DIR, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", REPORTING_DIR, entry->d_name);
        
        // Handle case where file already exists in reporting directory
        struct stat st;
        if (stat(dst_path, &st) == 0) {
            // File exists, append timestamp to avoid overwrite
            time_t now;
            struct tm *time_info;
            char timestamp[20];
            
            time(&now);
            time_info = localtime(&now);
            strftime(timestamp, sizeof(timestamp), "_%Y%m%d_%H%M%S", time_info);
            
            // Update destination path with timestamp
            char filename[PATH_MAX];
            char *dot_pos = strrchr(entry->d_name, '.');
            if (dot_pos) {
                // Insert timestamp before file extension
                size_t basename_len = dot_pos - entry->d_name;
                strncpy(filename, entry->d_name, basename_len);
                filename[basename_len] = '\0';
                strcat(filename, timestamp);
                strcat(filename, dot_pos);
            } else {
                // No extension, just append timestamp
                strcpy(filename, entry->d_name);
                strcat(filename, timestamp);
            }
            
            snprintf(dst_path, sizeof(dst_path), "%s/%s", REPORTING_DIR, filename);
        }
        
        // Use rename to move the file (atomic operation if on same filesystem)
        if (rename(src_path, dst_path) != 0) {
            // If rename fails (possibly due to different filesystems), fall back to copy and delete
            FILE *src_file, *dst_file;
            char buffer[4096];
            size_t bytes;
            int copy_success = 1;
            
            // Open source file for reading
            src_file = fopen(src_path, "rb");
            if (src_file == NULL) {
                log_message(LOG_ERR, "Failed to open source file %s: %s",
                           src_path, strerror(errno));
                success = 0;
                continue;
            }
            
            // Open destination file for writing
            dst_file = fopen(dst_path, "wb");
            if (dst_file == NULL) {
                log_message(LOG_ERR, "Failed to create destination file %s: %s",
                           dst_path, strerror(errno));
                fclose(src_file);
                success = 0;
                continue;
            }
            
            // Copy file contents
            while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
                    log_message(LOG_ERR, "Error writing to destination file %s: %s",
                               dst_path, strerror(errno));
                    copy_success = 0;
                    break;
                }
            }
            
            // Check for read error
            if (ferror(src_file)) {
                log_message(LOG_ERR, "Error reading from source file %s: %s",
                           src_path, strerror(errno));
                copy_success = 0;
            }
            
            // Close files
            fclose(src_file);
            fclose(dst_file);
            
            // If copy was successful, delete the source file
            if (copy_success) {
                if (unlink(src_path) != 0) {
                    log_message(LOG_WARNING, "Failed to delete source file after copy %s: %s",
                               src_path, strerror(errno));
                    // Still consider the transfer successful
                }
            } else {
                success = 0;
                continue;
            }
        }
        
        log_message(LOG_INFO, "Transferred file: %s to reporting directory", entry->d_name);
    }
    
    closedir(dir);
    
    if (success) {
        log_message(LOG_INFO, "File transfer completed successfully");
    } else {
        log_message(LOG_WARNING, "File transfer completed with errors");
    }
    
    return success;
}

/**
 * Check for missing uploads from departments
 * Uses a naming convention: department_date.xml
 * 
 * @return 1 if all expected reports are present, 0 otherwise
 */
int check_missing_uploads(void) {
    DIR *dir;
    struct dirent *entry;
    int found_warehouse = 0;
    int found_manufacturing = 0;
    int found_sales = 0;
    int found_distribution = 0;
    int all_found = 1;
    time_t now;
    struct tm *time_info;
    char today_date[11];  // Format: YYYY-MM-DD
    
    // Get today's date for checking report names
    time(&now);
    time_info = localtime(&now);
    strftime(today_date, sizeof(today_date), "%Y-%m-%d", time_info);
    
    log_message(LOG_INFO, "Checking for missing uploads for date: %s", today_date);
    
    // Open upload directory
    dir = opendir(UPLOAD_DIR);
    if (dir == NULL) {
        log_message(LOG_ERR, "Failed to open upload directory: %s", strerror(errno));
        return 0;
    }
    
    // Check each file in the upload directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Only check XML files
        if (strstr(entry->d_name, ".xml") == NULL) {
            continue;
        }
        
        // Check if file matches expected naming pattern and today's date
        if (strstr(entry->d_name, "warehouse") && strstr(entry->d_name, today_date)) {
            found_warehouse = 1;
        } else if (strstr(entry->d_name, "manufacturing") && strstr(entry->d_name, today_date)) {
            found_manufacturing = 1;
        } else if (strstr(entry->d_name, "sales") && strstr(entry->d_name, today_date)) {
            found_sales = 1;
        } else if (strstr(entry->d_name, "distribution") && strstr(entry->d_name, today_date)) {
            found_distribution = 1;
        }
    }
    
    closedir(dir);
    
    // Log any missing uploads
    if (!found_warehouse) {
        log_message(LOG_WARNING, "Missing upload: warehouse report for %s", today_date);
        all_found = 0;
    }
    
    if (!found_manufacturing) {
        log_message(LOG_WARNING, "Missing upload: manufacturing report for %s", today_date);
        all_found = 0;
    }
    
    if (!found_sales) {
        log_message(LOG_WARNING, "Missing upload: sales report for %s", today_date);
        all_found = 0;
    }
    
    if (!found_distribution) {
        log_message(LOG_WARNING, "Missing upload: distribution report for %s", today_date);
        all_found = 0;
    }
    
    if (all_found) {
        log_message(LOG_INFO, "All department reports have been received");
    } else {
        // Write to specific missing report log
        FILE *log_file = fopen(LOG_DIR"/missing_reports.log", "a");
        if (log_file) {
            time_t log_time;
            struct tm *log_tm;
            char timestamp[26];
            
            time(&log_time);
            log_tm = localtime(&log_time);
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", log_tm);
            
            fprintf(log_file, "[%s] Missing reports: ", timestamp);
            if (!found_warehouse) fprintf(log_file, "warehouse ");
            if (!found_manufacturing) fprintf(log_file, "manufacturing ");
            if (!found_sales) fprintf(log_file, "sales ");
            if (!found_distribution) fprintf(log_file, "distribution ");
            fprintf(log_file, "\n");
            
            fclose(log_file);
        }
    }
    
    return all_found;
}

/**
 * Monitor uploads directory for changes and log them
 */
void monitor_uploads(void) {
    DIR *dir;
    struct dirent *entry;
    static time_t last_check_time = 0;
    struct stat st;
    char filepath[PATH_MAX];
    struct passwd *pwd;
    
    // Only check for changes every 5 seconds to reduce system load
    time_t now = time(NULL);
    if (now - last_check_time < 5) {
        return;
    }
    last_check_time = now;
    
    // Open upload directory
    dir = opendir(UPLOAD_DIR);
    if (dir == NULL) {
        log_message(LOG_ERR, "Failed to open upload directory for monitoring: %s", strerror(errno));
        return;
    }
    
    // Check each file in the upload directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Create full path
        snprintf(filepath, sizeof(filepath), "%s/%s", UPLOAD_DIR, entry->d_name);
        
        // Get file stats
        if (stat(filepath, &st) < 0) {
            log_message(LOG_WARNING, "Failed to stat file %s: %s", filepath, strerror(errno));
            continue;
        }
        
        // Check if file was modified recently (within last check interval)
        if (st.st_mtime >= last_check_time - 5) {
            // Get the username of the file owner
            pwd = getpwuid(st.st_uid);
            if (pwd == NULL) {
                log_message(LOG_WARNING, "Failed to get owner of file %s: %s", 
                           filepath, strerror(errno));
                continue;
            }
            
            // Log the file change
            log_message(LOG_INFO, "File change detected: %s, modified by %s", 
                       entry->d_name, pwd->pw_name);
            
            // Also log to specific change log
            FILE *log_file = fopen(CHANGE_LOG, "a");
            if (log_file) {
                time_t log_time;
                struct tm *log_tm;
                char timestamp[26];
                
                time(&log_time);
                log_tm = localtime(&log_time);
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", log_tm);
                
                fprintf(log_file, "[%s] File: %s, User: %s, Action: modified\n", 
                       timestamp, entry->d_name, pwd->pw_name);
                
                fclose(log_file);
            }
        }
    }
    
    closedir(dir);
}

/**
 * Monitor uploads directory for changes using an absolute path
 */
void monitor_uploads_with_path(const char *upload_dir) {
    DIR *dir;
    struct dirent *entry;
    
    dir = opendir(upload_dir);
    if (dir == NULL) {
        log_message(LOG_ERR, "Failed to open upload directory for monitoring: %s", strerror(errno));
        return;
    }
    
    // Process each entry in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip . and .. entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Process the file here
        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", upload_dir, entry->d_name);
        log_message(LOG_INFO, "Found file in upload directory: %s", full_path);
        
        // You may want to call your existing transfer logic here
        // Or simply identify files to be transferred later
    }
    
    closedir(dir);
}

/**
 * Log a message to syslog and to the error log file
 * 
 * @param priority The syslog priority level
 * @param format Format string for the message
 * @param ... Variable arguments for format string
 */
void log_message(int priority, const char *format, ...) {
    va_list args;
    
    // Log to syslog
    va_start(args, format);
    vsyslog(priority, format, args);
    va_end(args);
    
    // Also log to error log file
    FILE *log_file = fopen(ERROR_LOG, "a");
    if (log_file) {
        time_t log_time;
        struct tm *log_tm;
        char timestamp[26];
        
        time(&log_time);
        log_tm = localtime(&log_time);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", log_tm);
        
        fprintf(log_file, "[%s] ", timestamp);
        
        // Add priority label
        switch(priority) {
            case LOG_ERR:
                fprintf(log_file, "ERROR: ");
                break;
            case LOG_WARNING:
                fprintf(log_file, "WARNING: ");
                break;
            case LOG_INFO:
                fprintf(log_file, "INFO: ");
                break;
            default:
                fprintf(log_file, "[%d]: ", priority);
                break;
        }
        
        // Write the actual message
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fclose(log_file);
    }
}

/**
 * Set up IPC (Inter-Process Communication)
 * 
 * @param msgid Message queue ID
 * @param type Message type
 * @param msg Message text
 * @return 1 on success, 0 on failure
 */
int setup_ipc(int msgid, long type, const char *msg) {
    struct msg_buffer message;
    
    // Copy message parameters
    message.msg_type = type;
    strncpy(message.msg_text, msg, sizeof(message.msg_text) - 1);
    message.msg_text[sizeof(message.msg_text) - 1] = '\0';  // Ensure null-termination
    
    // Send message to queue
    if (msgsnd(msgid, &message, sizeof(message.msg_text), IPC_NOWAIT) == -1) {
        log_message(LOG_ERR, "Failed to send IPC message: %s", strerror(errno));
        return 0;
    }
    
    return 1;
}

/**
 * Clean up IPC resources
 * 
 * @param msgid Message queue ID to clean up
 */
void cleanup_ipc(int msgid) {
    // Remove message queue
    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        log_message(LOG_ERR, "Failed to remove message queue: %s", strerror(errno));
    } else {
        log_message(LOG_INFO, "IPC message queue cleaned up");
    }
}