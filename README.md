# sys_software_CA1
##  Manufacturing Company Report Daemon

This daemon system manages reports from different departments in a manufacturing company. It monitors uploads, transfers files at scheduled times, performs backups, and maintains logs for transparency and accountability.

### Features

- **Daemon Process**: Runs in the background, detached from terminal
- **File Monitoring**: Tracks changes to uploaded files and logs who made them
- **Scheduled Transfers**: Automatically moves files from upload to reporting directory at 1 AM
- **Backup System**: Creates timestamped backups of all reports
- **Directory Lockdown**: Prevents modifications during critical operations
- **Missing Report Detection**: Logs when departments haven't submitted their reports
- **IPC Mechanism**: Enables inter-process communication for status reporting
- **Signal Handling**: Supports manual operations through signals

