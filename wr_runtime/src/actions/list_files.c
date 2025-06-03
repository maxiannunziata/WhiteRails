#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h> // For WIFEXITED, WEXITSTATUS etc.
#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include "../condition.h" // For record_activity()

#define LOG_LF_INFO(fmt, ...) printf("INFO: list_files: " fmt "\n", ##__VA_ARGS__)
#define LOG_LF_ERROR(fmt, ...) fprintf(stderr, "ERROR: list_files: " fmt "\n", ##__VA_ARGS__)
#define MAX_LS_OUTPUT_LEN 4096


void app_action_list_files(const cJSON *action_params) {
    const cJSON *path_json = cJSON_GetObjectItemCaseSensitive(action_params, "path");
    if (!cJSON_IsString(path_json) || (path_json->valuestring == NULL)) {
        LOG_LF_ERROR("Missing or invalid 'path' parameter.");
        return;
    }
    const char *path = path_json->valuestring;

    char cmd_buffer[512];
    // Basic sanitization: prevent command injection if path were to contain ` ;` etc.
    // For robust solution, validate path characters or use non-shell based listing.
    // This simple snprintf is okay if path is trusted or simple.
    snprintf(cmd_buffer, sizeof(cmd_buffer), "ls -la \"%s\" 2>&1", path); 

    LOG_LF_INFO("Executing: %s", cmd_buffer);

    FILE *fp = popen(cmd_buffer, "r");
    if (fp == NULL) {
        LOG_LF_ERROR("popen() failed for command: %s", cmd_buffer);
        return;
    }

    char output_buffer[MAX_LS_OUTPUT_LEN + 1]; // +1 for null terminator
    size_t total_read = 0;
    char line_buffer[256];
    output_buffer[0] = '\0'; // Initialize buffer for strcat

    LOG_LF_INFO("Output of '%s':", cmd_buffer);
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        // For now, just print to our stdout. Later, this might go to syslog.
        printf("%s", line_buffer); 
        if (total_read + strlen(line_buffer) < MAX_LS_OUTPUT_LEN) {
            strcat(output_buffer + total_read, line_buffer); // Concatenate to buffer
            total_read += strlen(line_buffer);
        } else {
            // Buffer full, stop copying to prevent overflow if we were storing it all
        }
    }
    // output_buffer[total_read] = '\0'; // Null-terminate the combined output // Already handled by initial output_buffer[0] = '\0' and strcat

    int status = pclose(fp);
    if (status == -1) {
        LOG_LF_ERROR("pclose() failed.");
    } else {
        if (WIFEXITED(status)) {
            LOG_LF_INFO("Command '%s' (popen) exited with status %d.", cmd_buffer, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            LOG_LF_INFO("Command '%s' (popen) killed by signal %d.", cmd_buffer, WTERMSIG(status));
        }
        // If output_buffer were to be sent to syslog, truncate if total_read >= MAX_LS_OUTPUT_LEN
        // For now, we just printed line by line.
        // syslog(LOG_INFO, "list_files output for %s: %s", path, output_buffer_truncated);
    }
    record_activity();
}
