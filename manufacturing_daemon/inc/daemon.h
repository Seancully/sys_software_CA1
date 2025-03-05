#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

// Function declarations for daemon management
void daemonize(void);
void setup_signals(void);
void signal_handler(int sig);
int check_singleton(const char *lock_file);
void write_pid(const char *pid_file);
void cleanup(void);

#endif