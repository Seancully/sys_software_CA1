#include "../inc/daemon.h"
#include "../inc/company.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>


int main(void) {
    int msgid;
    char cwd[PATH_MAX];
    char upload_dir[PATH_MAX];
    char reporting_dir[PATH_MAX];
    char backup_dir[PATH_MAX];
    char log_dir[PATH_MAX];
    
    // Get the current working directory before daemonizing
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Failed to get current directory");
        exit(EXIT_FAILURE);
    }
    
    // Create absolute paths
    snprintf(upload_dir, sizeof(upload_dir), "%s/data/upload", cwd);
    snprintf(reporting_dir, sizeof(reporting_dir), "%s/data/reporting", cwd);
    snprintf(backup_dir, sizeof(backup_dir), "%s/data/backup", cwd);
    snprintf(log_dir, sizeof(log_dir), "%s/logs", cwd);

    // The rest of your main function remains the same
    // Check if daemon is already running
    if (!check_singleton(LOCK_FILE)) {
        fprintf(stderr, "Error: Daemon is already running or could not acquire lock\n");
        exit(EXIT_FAILURE);
    }

    // Initialize as a daemon process
    daemonize();
    
    // Write PID file
    write_pid(PID_FILE);
    
    // Set up signal handlers
    setup_signals();
    
    // Initialize IPC message queue for communication
    msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgid == -1) {
        log_message(LOG_ERR, "Failed to create message queue: %s", strerror(errno));
        cleanup();
        exit(EXIT_FAILURE);
    }
    
    log_message(LOG_INFO, "Company daemon started successfully");
    
    // Create necessary directories if they don't exist
    mkdir(upload_dir, 0755);
    mkdir(reporting_dir, 0755);
    mkdir(backup_dir, 0755);
    mkdir(log_dir, 0755);
    
    // Override the default paths with absolute paths
    // This ensures the daemon can find directories after changing working directory
    char *abs_upload_dir = strdup(upload_dir);
    char *abs_reporting_dir = strdup(reporting_dir);
    char *abs_backup_dir = strdup(backup_dir);
    char *abs_log_dir = strdup(log_dir);

    // Main daemon loop
    while (1) {
        time_t now;
        struct tm *tm_now;
        
        // Monitor uploads directory for changes
monitor_uploads_with_path(abs_upload_dir);        
        // Check if it's time for the scheduled transfer (1 AM)
        time(&now);
        tm_now = localtime(&now);
        
        if (tm_now->tm_hour == TRANSFER_TIME_HOUR && tm_now->tm_min == TRANSFER_TIME_MIN) {
            log_message(LOG_INFO, "Starting scheduled transfer and backup");
            
            // Lock directories before backup and transfer
            lock_directories();
            
            // Check for missing uploads
            check_missing_uploads();
            
            // Backup reporting directory
            backup_reporting_dir();
            
            // Transfer files from upload to reporting
            transfer_uploads();
            
            // Unlock directories after operations
            unlock_directories();
            
            // Sleep for one minute to avoid running the task multiple times
            sleep(60);
        }
        
        // Sleep briefly to avoid high CPU usage
        sleep(10);
    }
    
    // Clean up resources (though this is normally unreachable)
    cleanup_ipc(msgid);
    cleanup();
    
    return EXIT_SUCCESS;
}