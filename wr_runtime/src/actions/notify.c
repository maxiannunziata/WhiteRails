#include <stdio.h>
#include <string.h>
#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include "../condition.h" // For record_activity()
// #include <syslog.h> // For future syslog integration

#define LOG_NOTIFY_INFO(fmt, ...) printf("INFO: notify: " fmt "\n", ##__VA_ARGS__)
#define LOG_NOTIFY_ERROR(fmt, ...) fprintf(stderr, "ERROR: notify: " fmt "\n", ##__VA_ARGS__)

void app_action_notify(const cJSON *action_params) {
    const cJSON *msg_json = cJSON_GetObjectItemCaseSensitive(action_params, "message");
    if (!cJSON_IsString(msg_json) || (msg_json->valuestring == NULL)) {
        LOG_NOTIFY_ERROR("%s", "Missing or invalid 'message' parameter.");
        return;
    }
    const char *message = msg_json->valuestring;

    // For now, print to stdout. Later, this will go to syslog.
    // syslog(LOG_NOTICE, "User Notification: %s", message);
    printf("NOTIFICATION: %s\n", message); 
    LOG_NOTIFY_INFO("Notification action executed with message: %s", message);
    record_activity();
}
