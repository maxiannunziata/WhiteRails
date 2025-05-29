#define _DEFAULT_SOURCE // For mode_t and other POSIX features
#include <stdio.h>
#include <stdlib.h> // For system()
#include <string.h>
#include <sys/stat.h> // For POSIX mkdir
#include <errno.h>    // For errno
#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include "../condition.h" // For record_activity()

#define LOG_MKDIR_INFO(fmt, ...) printf("INFO: mkdir: " fmt "\n", ##__VA_ARGS__)
#define LOG_MKDIR_ERROR(fmt, ...) fprintf(stderr, "ERROR: mkdir: " fmt "\n", ##__VA_ARGS__)

// More robust mkdir -p implementation (iterative)
static int mkdir_p(const char *path, mode_t mode) {
    char *p, *temp_path;
    int result = 0;

    // Make a copy of the path string, as strtok will modify it
    temp_path = strdup(path);
    if (temp_path == NULL) {
        LOG_MKDIR_ERROR("strdup failed for path: %s", path);
        return -1;
    }

    // Iterate through the path components
    for (p = temp_path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0'; // Temporarily terminate the string
            if (mkdir(temp_path, mode) == -1) {
                if (errno != EEXIST) {
                    LOG_MKDIR_ERROR("mkdir failed for %s: %s", temp_path, strerror(errno));
                    result = -1;
                    break;
                }
            }
            *p = '/'; // Restore the slash
        }
    }

    // Create the final directory component
    if (result == 0) {
        if (mkdir(temp_path, mode) == -1) {
            if (errno != EEXIST) {
                LOG_MKDIR_ERROR("mkdir failed for %s: %s", temp_path, strerror(errno));
                result = -1;
            }
        }
    }
    
    free(temp_path);
    if (result == 0 || errno == EEXIST) { // Consider EEXIST as success for -p logic
      LOG_MKDIR_INFO("Directory path '%s' ensured (or already existed).", path);
      return 0;
    }
    return result;
}


void app_action_mkdir(const cJSON *action_params) {
    const cJSON *path_json = cJSON_GetObjectItemCaseSensitive(action_params, "path");
    if (!cJSON_IsString(path_json) || (path_json->valuestring == NULL)) {
        LOG_MKDIR_ERROR("Missing or invalid 'path' parameter.");
        return;
    }
    const char *path = path_json->valuestring;

    LOG_MKDIR_INFO("Ensuring directory exists (mkdir -p equivalent): %s", path);
    
    // Using the iterative C mkdir approach
    // mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH; // 0775
    mode_t mode = 0755; // More common default
    if (mkdir_p(path, mode) == 0) {
        LOG_MKDIR_INFO("Successfully ensured directory: %s", path);
        record_activity();
    } else {
        // Error already logged by mkdir_p
        // LOG_MKDIR_ERROR("Failed to ensure directory: %s", path);
    }

    /* // Alternative using system("mkdir -p ...") - simpler but less secure / controlled
    char cmd_buffer[512];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "mkdir -p "%s"", path);
    int ret = system(cmd_buffer);
    if (ret == 0) {
        LOG_MKDIR_INFO("Successfully created directory (or it already existed): %s", path);
        record_activity();
    } else {
        LOG_MKDIR_ERROR("system('mkdir -p %s') failed with status %d.", path, ret);
    }
    */
}
