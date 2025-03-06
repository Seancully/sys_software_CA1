#define _POSIX_C_SOURCE 200809L
#include "../inc/daemon.h"
#include "../inc/company.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>


// Function declarations
void log_message(int level, const char *format, ...);
void cleanup(void);
void signal_handler(int sig);

/**
 * Daemonize the process - convert the process into a proper daemon
 * that runs in the background, detached from the terminal
 */
void daemonize(void) {
    pid_t pid;
    
    // Fork off the parent process
    pid = fork();
    
    // An error occurred
    if (pid < 0) {
        log_message(LOG_ERR, "Failed to fork daemon process: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Success: Let the parent terminate
    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }
    
    // On success: The child process becomes the session leader
    if (setsid() < 0) {
        log_message(LOG_ERR, "Failed to become session leader: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Catch, ignore and handle signals
    signal(SIGCHLD, SIG_IGN);  // Ignore child death
    signal(SIGHUP, SIG_IGN);   // Ignore terminal hangup
    
    // Fork off for the second time
    pid = fork();
    
    // An error occurred
    if (pid < 0) {
        log_message(LOG_ERR, "Failed to fork daemon process (2nd fork): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Success: Let the parent terminate
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    // Set new file permissions
    umask(0);
    
    // Commented out changing working directory to root to avoid path issues
    // if (chdir("/") < 0) {
    //     log_message(LOG_ERR, "Failed to change working directory: %s", strerror(errno));
    //     exit(EXIT_FAILURE);
    // }
    
    // Close all file descriptors
    for (int i = sysconf(_SC_OPEN_MAX); i >= 0; i--) {
        close(i);
    }
    
    // Redirect standard file descriptors to /dev/null
    int stdin_fd = open("/dev/null", O_RDWR);
    dup2(stdin_fd, STDIN_FILENO);
    dup2(stdin_fd, STDOUT_FILENO);
    dup2(stdin_fd, STDERR_FILENO);
    
    // Open syslog for logging
    openlog("company_daemon", LOG_PID, LOG_DAEMON);
    log_message(LOG_INFO, "Daemon initialized successfully");
}

/**
 * Handle signals received by the daemon
 */
void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            log_message(LOG_INFO, "Received termination signal, cleaning up and exiting");
            cleanup();
            exit(EXIT_SUCCESS);
            break;
        case SIGUSR1:
            log_message(LOG_INFO, "Received user-defined signal, performing backup/transfer");
            // Perform backup or transfer
            break;
        default:
            log_message(LOG_WARNING, "Unhandled signal (%d) received", sig);
            break;
    }
}

/**
 * Set up signal handlers for the daemon
 */
/**
 * Set up signal handlers for the daemon
 */
void setup_signals(void) {
    // Use signal() function as an alternative to sigaction
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        log_message(LOG_ERR, "Failed to set up SIGTERM handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        log_message(LOG_ERR, "Failed to set up SIGINT handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
        log_message(LOG_ERR, "Failed to set up SIGUSR1 handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    log_message(LOG_INFO, "Signal handlers established");
}

/**
 * Check if another instance of this daemon is already running
 * Implements the singleton pattern for the daemon
 */
int check_singleton(const char *lock_file) {
    int fd;
    struct flock fl;
    
    // Open or create the lock file
    fd = open(lock_file, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        log_message(LOG_ERR, "Failed to open/create lock file %s: %s", lock_file, strerror(errno));
        return 0;
    }
    
    // Initialize the flock structure for a write lock
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    
    // Try to lock the file
    if (fcntl(fd, F_SETLK, &fl) < 0) {
        // Could not acquire lock, another instance is running
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            return 0;
        }
        
        // Some other error
        log_message(LOG_ERR, "Failed to lock file %s: %s", lock_file, strerror(errno));
        close(fd);
        return 0;
    }
    
    // Successfully acquired the lock, keep the file open
    // Note: We intentionally don't close fd since it would release the lock
    
    return 1;
}

/**
 * Write the process ID to a file for future reference
 */
void write_pid(const char *pid_file) {
    FILE *fp;
    
    fp = fopen(pid_file, "w");
    if (fp == NULL) {
        log_message(LOG_ERR, "Failed to open PID file %s: %s", pid_file, strerror(errno));
        return;
    }
    
    fprintf(fp, "%d\n", getpid());
    fclose(fp);
    
    log_message(LOG_INFO, "PID %d written to %s", getpid(), pid_file);
}

/**
 * Clean up resources before exiting
 */
void cleanup(void) {
    log_message(LOG_INFO, "Cleaning up daemon resources");
    
    // Ensure directories are unlocked when exiting
    unlock_directories();
    
    // Remove PID file
    if (unlink(PID_FILE) < 0 && errno != ENOENT) {
        log_message(LOG_WARNING, "Failed to remove PID file: %s", strerror(errno));
    }
    
    // Remove lock file
    if (unlink(LOCK_FILE) < 0 && errno != ENOENT) {
        log_message(LOG_WARNING, "Failed to remove lock file: %s", strerror(errno));
    }
    
    // Close syslog
    closelog();
}