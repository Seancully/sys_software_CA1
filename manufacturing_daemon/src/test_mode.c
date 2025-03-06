#include "../inc/daemon.h"
#include "../inc/company.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>

int main(int argc, char *argv[]) {
    int msgid;
    char cwd[PATH_MAX];
    char upload_dir[PATH_MAX];
    char reporting_dir[PATH_MAX];
    char backup_dir[PATH_MAX];
    char log_dir[PATH_MAX];
    
    printf("Starting Company Daemon in test mode (foreground)\n");
    
    // Get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Failed to get current directory");
        exit(EXIT_FAILURE);
    }
    
    printf("Working directory: %s\n", cwd);
    
    // Create absolute paths
    snprintf(upload_dir, sizeof(upload_dir), "%s/data/upload", cwd);
    snprintf(reporting_dir, sizeof(reporting_dir), "%s/data/reporting", cwd);
    snprintf(backup_dir, sizeof(backup_dir), "%s/data/backup", cwd);
    snprintf(log_dir, sizeof(log_dir), "%s/logs", cwd);
    
    printf("Paths:\n");
    printf("  Upload: %s\n", upload_dir);
    printf("  Reporting: %s\n", reporting_dir);
    printf("  Backup: %s\n", backup_dir);
    printf("  Logs: %s\n", log_dir);
    
    // Create necessary directories if they don't exist
    mkdir(upload_dir, 0755);
    mkdir(reporting_dir, 0755);
    mkdir(backup_dir, 0755);
    mkdir(log_dir, 0755);
    
    // Initialize IPC message queue
    msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("Failed to create message queue");
        exit(EXIT_FAILURE);
    }
    
    printf("Message queue initialized: %d\n", msgid);
    
    // Process files immediately (instead of waiting for 1AM)
    printf("Starting transfer of uploads...\n");
    transfer_uploads();
    
    printf("Starting backup of reporting directory...\n");
    backup_reporting_dir();
    
    printf("Checking for missing uploads...\n");
    check_missing_uploads();
    
    printf("Test completed successfully!\n");
    
    // Clean up
    cleanup_ipc(msgid);
    
    printf("\nTest mode continuing to run. Press Ctrl+C to stop.\n");
    printf("You can send signals using: kill -USR1 %d\n", getpid());
    
    // Keep running and waiting for signals
    while(1) {
        sleep(10);
    }

    return EXIT_SUCCESS;
}