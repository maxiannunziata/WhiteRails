#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include "../condition.h" // For record_activity()

#define LOG_SHELL_INFO(fmt, ...) printf("INFO: shell: " fmt "\n", ##__VA_ARGS__)
#define LOG_SHELL_ERROR(fmt, ...) fprintf(stderr, "ERROR: shell: " fmt "\n", ##__VA_ARGS__)

void app_action_shell(const cJSON *action_params) {
    const cJSON *cmd_json = cJSON_GetObjectItemCaseSensitive(action_params, "command");
    if (!cJSON_IsString(cmd_json) || (cmd_json->valuestring == NULL)) {
        LOG_SHELL_ERROR("Missing or invalid 'command' parameter for shell action.");
        return;
    }
    const char *cmd = cmd_json->valuestring;
    LOG_SHELL_INFO("Executing shell command: %s", cmd);

    pid_t pid = fork();
    if (pid == -1) {
        LOG_SHELL_ERROR("Failed to fork for shell command: %m");
        return;
    } else if (pid == 0) { // Child process
        if (setsid() == -1) {
            LOG_SHELL_ERROR("Child setsid failed for shell command: %m");
            _exit(EXIT_FAILURE);
        }
        execl("/bin/sh", "sh", "-c", cmd, (char *)0);
        LOG_SHELL_ERROR("execl failed for /bin/sh -c '%s' (shell): %m", cmd);
        _exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            LOG_SHELL_ERROR("waitpid failed for shell command: %m");
        } else {
            if (WIFEXITED(status)) {
                LOG_SHELL_INFO("Shell command '%s' exited with status %d", cmd, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                LOG_SHELL_INFO("Shell command '%s' killed by signal %d", cmd, WTERMSIG(status));
            } else {
                LOG_SHELL_INFO("Shell command '%s' ended with unknown status", cmd);
            }
            record_activity();
        }
    }
}
