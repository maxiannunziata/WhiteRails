#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
// Assuming dispatcher.h declares app_action_run_command, or ensure signature matches
// For action_fn: #include "dispatcher.h" 
#include "../condition.h" // For record_activity()

// Temporary logging macros (replace with syslog later)
#define LOG_RC_INFO(fmt, ...) printf("INFO: run_command: " fmt "\n", ##__VA_ARGS__)
#define LOG_RC_ERROR(fmt, ...) fprintf(stderr, "ERROR: run_command: " fmt "\n", ##__VA_ARGS__)

void app_action_run_command(const cJSON *action_params) {
    const cJSON *cmd_json = cJSON_GetObjectItemCaseSensitive(action_params, "command");
    if (!cJSON_IsString(cmd_json) || (cmd_json->valuestring == NULL)) {
        LOG_RC_ERROR("Missing or invalid 'command' parameter.");
        return;
    }
    const char *cmd = cmd_json->valuestring;
    LOG_RC_INFO("Executing command: %s", cmd);

    pid_t pid = fork();
    if (pid == -1) {
        LOG_RC_ERROR("Failed to fork: %m"); // %m prints strerror(errno)
        return;
    } else if (pid == 0) { // Child process
        if (setsid() == -1) { // Create new session and detach from terminal
            LOG_RC_ERROR("Child setsid failed: %m");
            _exit(EXIT_FAILURE);
        }
        // Close standard file descriptors (optional, good for daemons)
        // close(STDIN_FILENO); close(STDOUT_FILENO); close(STDERR_FILENO);
        
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        LOG_RC_ERROR("execl failed for /bin/sh -c '%s': %m", cmd); // Log if execl fails
        _exit(EXIT_FAILURE); // Exit child if execl fails
    } else { // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            LOG_RC_ERROR("waitpid failed: %m");
        } else {
            if (WIFEXITED(status)) {
                LOG_RC_INFO("Command '%s' exited with status %d", cmd, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOG_RC_INFO("Command '%s' killed by signal %d", cmd, WTERMSIG(status));
            } else {
                LOG_RC_INFO("Command '%s' ended with unknown status", cmd);
            }
            record_activity(); // Record activity if command execution attempt was made
        }
    }
}
