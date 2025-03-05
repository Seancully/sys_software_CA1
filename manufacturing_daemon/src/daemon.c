#include "../inc/daemon.h"
#include "../inc/company.h"

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
    
    // Change the working directory to root (or another appropriate directory)
    if (chdir("/") < 0) {
        log_message(LOG_ERR, "Failed to change working directory: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
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
 * Set up signal handlers for the daemon
 */
void setup_signals(void) {
    struct sigaction sa;
    
    // Set up a signal handler for SIGTERM
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    // Handle SIGTERM (termination signal)
    if (sigaction(SIGTERM, &sa, NULL) < 0) {
        log_message(LOG_ERR, "Failed to set up SIGTERM handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Handle SIGINT (interrupt from keyboard, e.g., Ctrl+C)
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        log_message(LOG_ERR, "Failed to set up SIGINT handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    // Handle SIGUSR1 (user-defined signal, can be used for manual backup/transfer)
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        log_message(LOG_ERR, "Failed to set up SIGUSR1 handler: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    log_message(LOG_INFO, "Signal handlers established");
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
            log_message(LOG_INFO, "Received signal to perform manual backup/transfer");
            // Perform a manual backup and transfer
            lock_directories();
            backup_reporting_dir();
            transfer_uploads();
            unlock_directories();
            break;
        
        default:
            log_message(LOG_WARNING, "Received unhandled signal %d", sig);
            break;
    }
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