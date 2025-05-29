#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>   // For sleep, dup2, STDOUT_FILENO, STDERR_FILENO, close
#include <sys/stat.h> // For mkdir, stat
#include <errno.h>    // For strerror(errno)
#include <syslog.h>   // For syslog functions
#include <fcntl.h>    // For open, O_WRONLY
#include <time.h>     // For time_t, time()
#include <sys/time.h> // For gettimeofday, struct timeval

#include "cJSON.h"    // For JSON parsing

// Define constants
#define SERVICES_DIR "/var/lib/whiterails/services/"
#define DEFAULT_SERVICE_INTERVAL 60 // Default interval for a service if not specified
#define MASTER_LOOP_SLEEP_INTERVAL 5 // Sleep interval for the main checking loop
#define SERVICE_RESCAN_INTERVAL 60   // How often to rescan SERVICES_DIR for changes

// --- Service Management ---
typedef struct {
    char *filepath;         // Full path to the service JSON file
    char *service_name;     // From "name" field or filename
    cJSON *config_json;     // Parsed cJSON object for the service
    int interval_seconds;
    time_t last_executed_timestamp;
    time_t last_modified_timestamp; // To detect file changes
} loaded_service_t;

// Max number of services we can load
#define MAX_LOADED_SERVICES 100
loaded_service_t loaded_services[MAX_LOADED_SERVICES];
int num_loaded_services = 0;

// --- Hardcoded Schema ---
static const char* semantic_service_schema_str = 
"{\n"
"  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n"
"  \"title\": \"WhiteRails Semantic Service\",\n"
"  \"type\": \"object\",\n"
"  \"required\": [\"name\", \"condition\", \"actions\"],\n"
"  \"properties\": {\n"
"    \"name\":        { \"type\": \"string\" },\n"
"    \"input\":       { \"type\": \"string\", \"enum\": [\"system\",\"sensor\",\"event\"], \"default\": \"system\" },\n"
"    \"condition\":   { \"type\": \"string\" },\n"
"    \"actions\": {\n"
"      \"type\": \"array\",\n"
"      \"items\": {\n"
"        \"type\": \"object\",\n"
"        \"required\": [\"type\"],\n"
"        \"properties\": {\n"
"          \"type\":    { \"type\": \"string\", \"enum\": [\"notify\",\"shell\",\"list_files\",\"mkdir\",\"run_command\"] },\n"
"          \"cmd\":     { \"type\": \"string\" },\n"
"          \"message\": { \"type\": \"string\" },\n"
"          \"path\":    { \"type\": \"string\" },\n"
"          \"command\": { \"type\": \"string\" }\n"
"        }\n"
"      }\n"
"    },\n"
"    \"interval_seconds\": { \"type\": \"integer\", \"minimum\": 1, \"default\": 60 }\n"
"  }\n"
"}";

cJSON *global_schema_json = NULL; // Parsed hardcoded schema

// --- Action function declarations ---
void action_list_files_c(const cJSON *action_obj, const void *context);
void action_mkdir_c(const cJSON *action_obj, const void *context);
void action_run_command_c(const cJSON *action_obj, const void *context);
void action_notify_c(const cJSON *action_obj, const void *context);
void action_shell_c(const cJSON *action_obj, const void *context);

// --- Context and Condition Evaluation ---
typedef struct {
    int battery_level;
    struct timeval last_activity_timestamp; // For "no_activity" condition
} app_context_t;

app_context_t global_app_context; // Global context instance

void initialize_app_context() {
    global_app_context.battery_level = 100; // Default
    gettimeofday(&global_app_context.last_activity_timestamp, NULL); // Initialize to program start time
    syslog(LOG_INFO, "Application context initialized. Battery: %d%%, Last activity timestamp set to current time.", 
           global_app_context.battery_level);
}

app_context_t get_current_app_context() {
    // In a real system, this would fetch dynamic values.
    // For now, just return the global_app_context, which might be updated by other parts.
    // For battery, can simulate change if needed for testing:
    // global_app_context.battery_level = (rand() % 100) + 1; 
    syslog(LOG_DEBUG, "Fetched current app context. Battery: %d%%", global_app_context.battery_level);
    return global_app_context;
}

// Placeholder: Mechanism to update last_activity_timestamp
// This function would be called by parts of the system that detect user/system activity.
void record_activity() {
    gettimeofday(&global_app_context.last_activity_timestamp, NULL);
    syslog(LOG_INFO, "Activity recorded, last_activity_timestamp updated.");
}


int check_condition(const char* condition_str, const app_context_t *ctx) {
    if (condition_str == NULL || strlen(condition_str) == 0) {
        syslog(LOG_WARNING, "Condition string is NULL or empty. Assuming true (or handle as error).");
        return 1; // Or 0 if default should be false
    }
    syslog(LOG_DEBUG, "Evaluating condition: \"%s\" with battery_level: %d", condition_str, ctx->battery_level);

    if (strcmp(condition_str, "ALWAYS_TRUE") == 0) { // Renamed for clarity
        return 1;
    }
    if (strcmp(condition_str, "battery_level >= 0") == 0) { // Example, could be more complex
        return (ctx->battery_level >= 0);
    }
    if (strncmp(condition_str, "no_activity > ", strlen("no_activity > ")) == 0) {
        int N_seconds;
        if (sscanf(condition_str, "no_activity > %ds", &N_seconds) == 1) {
            struct timeval current_timeval;
            gettimeofday(&current_timeval, NULL);
            long elapsed_seconds = current_timeval.tv_sec - ctx->last_activity_timestamp.tv_sec;
            syslog(LOG_DEBUG, "Condition 'no_activity > %ds': elapsed_seconds = %ld", N_seconds, elapsed_seconds);
            return elapsed_seconds > N_seconds;
        } else {
            syslog(LOG_WARNING, "Could not parse N from condition_str: %s", condition_str);
            return 0;
        }
    }
    syslog(LOG_WARNING, "Unknown condition string: %s", condition_str);
    return 0; // Default to false for unknown conditions
}

// --- Schema Validation (Manual) ---
int validate_service_with_schema(const cJSON* service_json, const char* service_name_for_log) {
    if (!service_json) return 0;

    // 1. Check required top-level fields
    const char* required_fields[] = {"name", "condition", "actions"};
    for (int i = 0; i < sizeof(required_fields)/sizeof(required_fields[0]); ++i) {
        const cJSON* field = cJSON_GetObjectItemCaseSensitive(service_json, required_fields[i]);
        if (!field) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Missing required field '%s'.", service_name_for_log, required_fields[i]);
            return 0;
        }
        // Type checks
        if (strcmp(required_fields[i], "name") == 0 && !cJSON_IsString(field)) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Field 'name' must be a string.", service_name_for_log); return 0;
        }
        if (strcmp(required_fields[i], "condition") == 0 && !cJSON_IsString(field)) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Field 'condition' must be a string.", service_name_for_log); return 0;
        }
        if (strcmp(required_fields[i], "actions") == 0 && !cJSON_IsArray(field)) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Field 'actions' must be an array.", service_name_for_log); return 0;
        }
    }

    // 2. Check "actions" array items
    const cJSON* actions_array = cJSON_GetObjectItemCaseSensitive(service_json, "actions");
    const cJSON* action_item = NULL;
    cJSON_ArrayForEach(action_item, actions_array) {
        if (!cJSON_IsObject(action_item)) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Item in 'actions' array is not an object.", service_name_for_log); return 0;
        }
        const cJSON* type_item = cJSON_GetObjectItemCaseSensitive(action_item, "type");
        if (!type_item || !cJSON_IsString(type_item)) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Action item missing 'type' or 'type' is not a string.", service_name_for_log); return 0;
        }
        const char* type_str = type_item->valuestring;
        const char* valid_types[] = {"notify", "shell", "list_files", "mkdir", "run_command"};
        int is_valid_type = 0;
        for (int i = 0; i < sizeof(valid_types)/sizeof(valid_types[0]); ++i) {
            if (strcmp(type_str, valid_types[i]) == 0) {
                is_valid_type = 1;
                break;
            }
        }
        if (!is_valid_type) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Invalid action type '%s'.", service_name_for_log, type_str); return 0;
        }

        // Parameter checks based on type
        if (strcmp(type_str, "list_files") == 0 || strcmp(type_str, "mkdir") == 0) {
            if (!cJSON_GetObjectItemCaseSensitive(action_item, "path") || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(action_item, "path"))) {
                 syslog(LOG_ERR, "Schema validation FAILED for '%s': Action '%s' missing 'path' (string).", service_name_for_log, type_str); return 0;
            }
        } else if (strcmp(type_str, "run_command") == 0) {
            if (!cJSON_GetObjectItemCaseSensitive(action_item, "command") || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(action_item, "command"))) {
                 syslog(LOG_ERR, "Schema validation FAILED for '%s': Action '%s' missing 'command' (string).", service_name_for_log, type_str); return 0;
            }
        } else if (strcmp(type_str, "notify") == 0) {
             if (!cJSON_GetObjectItemCaseSensitive(action_item, "message") || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(action_item, "message"))) {
                 syslog(LOG_ERR, "Schema validation FAILED for '%s': Action '%s' missing 'message' (string).", service_name_for_log, type_str); return 0;
            }
        } else if (strcmp(type_str, "shell") == 0) {
            if (!cJSON_GetObjectItemCaseSensitive(action_item, "cmd") || !cJSON_IsString(cJSON_GetObjectItemCaseSensitive(action_item, "cmd"))) {
                 syslog(LOG_ERR, "Schema validation FAILED for '%s': Action '%s' missing 'cmd' (string).", service_name_for_log, type_str); return 0;
            }
        }
    }
    
    // 3. Check optional fields and their types if present
    const cJSON* input_item = cJSON_GetObjectItemCaseSensitive(service_json, "input");
    if (input_item && !cJSON_IsString(input_item)) {
        syslog(LOG_ERR, "Schema validation FAILED for '%s': Optional field 'input' must be a string.", service_name_for_log); return 0;
    }
    // Could add enum check for "input" if desired.

    const cJSON* interval_item = cJSON_GetObjectItemCaseSensitive(service_json, "interval_seconds");
    if (interval_item) {
        if (!cJSON_IsNumber(interval_item) || interval_item->valueint < 1) {
            syslog(LOG_ERR, "Schema validation FAILED for '%s': Optional field 'interval_seconds' must be an integer >= 1.", service_name_for_log); return 0;
        }
    }

    syslog(LOG_INFO, "Schema validation PASSED for service: %s", service_name_for_log);
    return 1;
}

// --- Service Loading and Management ---
time_t get_file_mod_time(const char *filepath) {
    struct stat attr;
    if (stat(filepath, &attr) == 0) {
        return attr.st_mtime;
    }
    return 0; // Error or file not found
}

void free_service(loaded_service_t *service) {
    if (service) {
        free(service->filepath);
        free(service->service_name);
        if (service->config_json) {
            cJSON_Delete(service->config_json);
        }
        service->filepath = NULL;
        service->service_name = NULL;
        service->config_json = NULL;
    }
}

void load_or_update_services() {
    syslog(LOG_INFO, "Scanning services directory: %s", SERVICES_DIR);
    DIR *d;
    struct dirent *dir;
    int current_service_idx = 0; // To track services being kept

    d = opendir(SERVICES_DIR);
    if (!d) {
        syslog(LOG_ERR, "Could not open services directory %s: %s", SERVICES_DIR, strerror(errno));
        // Potentially clear all loaded_services if dir is inaccessible
        for(int i=0; i<num_loaded_services; ++i) free_service(&loaded_services[i]);
        num_loaded_services = 0;
        return;
    }

    // Mark all existing services as "to be removed" unless found again
    int services_to_remove[MAX_LOADED_SERVICES] = {0}; 
    for(int i=0; i<num_loaded_services; ++i) services_to_remove[i] = 1;

    while ((dir = readdir(d)) != NULL) {
        if (strstr(dir->d_name, ".json") == NULL) {
            continue; // Skip non-JSON files
        }

        char filepath[1024]; // Increased buffer size
        snprintf(filepath, sizeof(filepath), "%s%s", SERVICES_DIR, dir->d_name);
        
        time_t mod_time = get_file_mod_time(filepath);
        int found_idx = -1;

        // Check if this service is already loaded and if it has changed
        for (int i = 0; i < num_loaded_services; i++) {
            if (loaded_services[i].filepath && strcmp(loaded_services[i].filepath, filepath) == 0) {
                services_to_remove[i] = 0; // Mark as found
                if (loaded_services[i].last_modified_timestamp == mod_time) {
                    syslog(LOG_DEBUG, "Service '%s' unchanged, skipping reload.", loaded_services[i].service_name);
                    // Move to its new position if we are compacting the array
                    if (i != current_service_idx) loaded_services[current_service_idx] = loaded_services[i];
                    current_service_idx++;
                    found_idx = i; // Mark as processed
                } else {
                    syslog(LOG_INFO, "Service '%s' changed, will reload.", loaded_services[i].service_name);
                    free_service(&loaded_services[i]); // Free old version before reload
                    // It will be re-added as a new service below
                }
                break; // Found, no need to check further in loaded_services
            }
        }
        
        if (found_idx != -1 && loaded_services[found_idx].last_modified_timestamp == mod_time) {
             continue; // Already processed and unchanged
        }

        // If new or changed, load it
        FILE *file = fopen(filepath, "rb");
        if (!file) {
            syslog(LOG_ERR, "Could not open service file %s: %s", filepath, strerror(errno));
            continue;
        }
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *buffer = (char *)malloc(length + 1);
        if (!buffer) {
            syslog(LOG_ERR, "Failed to allocate memory for %s", filepath);
            fclose(file);
            continue;
        }
        if (fread(buffer, 1, length, file) != (size_t)length) {
            syslog(LOG_ERR, "Failed to read file %s", filepath);
            fclose(file); free(buffer); continue;
        }
        buffer[length] = '\0';
        fclose(file);

        cJSON *json_root = cJSON_Parse(buffer);
        free(buffer);

        if (!json_root) {
            syslog(LOG_ERR, "Error parsing JSON for %s: %s", filepath, cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "unknown error");
            continue;
        }
        
        const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(json_root, "name");
        const char *service_name_str = (name_item && cJSON_IsString(name_item)) ? name_item->valuestring : dir->d_name;

        if (!validate_service_with_schema(json_root, service_name_str)) {
            // Validation errors logged by validate_service_with_schema
            cJSON_Delete(json_root);
            continue;
        }

        // Add or update in the services array
        if (current_service_idx < MAX_LOADED_SERVICES) {
            // If it was a changed service, its old slot is already free or will be compacted.
            // We add it as if it's new at current_service_idx.
            loaded_services[current_service_idx].filepath = strdup(filepath);
            loaded_services[current_service_idx].service_name = strdup(service_name_str);
            loaded_services[current_service_idx].config_json = json_root; // Ownership transferred
            
            const cJSON *interval_item = cJSON_GetObjectItemCaseSensitive(json_root, "interval_seconds");
            if (interval_item && cJSON_IsNumber(interval_item)) {
                loaded_services[current_service_idx].interval_seconds = interval_item->valueint;
            } else {
                loaded_services[current_service_idx].interval_seconds = DEFAULT_SERVICE_INTERVAL;
            }
            loaded_services[current_service_idx].last_executed_timestamp = 0; // Execute on first valid check
            loaded_services[current_service_idx].last_modified_timestamp = mod_time;
            
            syslog(LOG_INFO, "Successfully loaded and validated service: %s (interval: %ds)", 
                   loaded_services[current_service_idx].service_name, 
                   loaded_services[current_service_idx].interval_seconds);
            current_service_idx++;
        } else {
            syslog(LOG_WARNING, "Max services limit (%d) reached. Cannot load %s.", MAX_LOADED_SERVICES, service_name_str);
            cJSON_Delete(json_root); // Clean up if not stored
        }
    }
    closedir(d);

    // Remove services that were not found in this scan (compacting the array)
    // This is implicitly handled by only copying over found/updated services to current_service_idx positions
    // and then adjusting num_loaded_services. Any services previously loaded but not "found"
    // (i.e., services_to_remove[i] is still 1) need to be freed.
    for(int i = 0; i < num_loaded_services; ++i) {
        if (services_to_remove[i]) { // This was an old service not found in current scan
             syslog(LOG_INFO, "Service '%s' removed (file deleted or moved).", loaded_services[i].service_name);
             free_service(&loaded_services[i]);
             // The array will be compacted naturally by the current_service_idx logic,
             // but this slot should not be considered valid anymore.
        }
    }
    num_loaded_services = current_service_idx; // Update count to reflect active services
    syslog(LOG_INFO, "Service scan complete. %d services active.", num_loaded_services);
}


// --- Main function ---
int main(int argc, char *argv[]) {
    openlog("wr_runtime", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Starting WhiteRails Runtime (C version)...");

    // Redirect stdout/stderr to /dev/null
    int fd_null = open("/dev/null", O_WRONLY);
    if (fd_null != -1) {
        if (dup2(fd_null, STDOUT_FILENO) < 0) {
            syslog(LOG_ERR, "Failed to dup2 stdout: %s", strerror(errno));
        }
        if (dup2(fd_null, STDERR_FILENO) < 0) {
            syslog(LOG_ERR, "Failed to dup2 stderr: %s", strerror(errno));
        }
        close(fd_null); // Can close after dup2
    } else {
        syslog(LOG_WARNING, "Failed to open /dev/null for redirecting stdout/stderr: %s", strerror(errno));
    }
    
    // Initialize context (especially last_activity_timestamp)
    initialize_app_context();

    // Parse hardcoded schema (should not fail if string is correct)
    global_schema_json = cJSON_Parse(semantic_service_schema_str);
    if (global_schema_json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        syslog(LOG_CRIT, "Failed to parse hardcoded schema: %s. Exiting.", error_ptr ? error_ptr : "unknown error");
        closelog();
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "Hardcoded schema parsed successfully.");

    // Create SERVICES_DIR if it doesn't exist
    struct stat st_services_dir = {0};
    if (stat(SERVICES_DIR, &st_services_dir) == -1) {
        syslog(LOG_INFO, "Services directory %s not found, attempting to create.", SERVICES_DIR);
        char temp_path[256]; // For parent dir creation
        strncpy(temp_path, SERVICES_DIR, sizeof(temp_path) -1);
        temp_path[sizeof(temp_path)-1] = '\0'; // Ensure null termination
        char *last_slash = strrchr(temp_path, '/');
        if (last_slash && last_slash != temp_path) { // Ensure not root and found slash
            *last_slash = '\0'; 
            if (stat(temp_path, &st_services_dir) == -1) {
                 if (mkdir(temp_path, 0755) == 0) {
                     syslog(LOG_INFO, "Created parent directory: %s", temp_path);
                 } else {
                     syslog(LOG_ERR, "Failed to create parent directory %s: %s", temp_path, strerror(errno));
                 }
            }
        }
        if (mkdir(SERVICES_DIR, 0755) == 0) {
            syslog(LOG_INFO, "Created services directory: %s", SERVICES_DIR);
        } else {
            syslog(LOG_ERR, "Failed to create services directory %s: %s. Service loading might fail.", SERVICES_DIR, strerror(errno));
        }
    } else {
        syslog(LOG_INFO, "Services directory: %s already exists.", SERVICES_DIR);
    }

    time_t last_service_scan_time = 0;

    while (1) {
        time_t current_time = time(NULL);

        // Rescan services directory periodically
        if (current_time - last_service_scan_time >= SERVICE_RESCAN_INTERVAL) {
            load_or_update_services();
            last_service_scan_time = current_time;
        }

        app_context_t current_ctx = get_current_app_context(); 

        for (int i = 0; i < num_loaded_services; ++i) {
            loaded_service_t *service = &loaded_services[i];
            if (!service->config_json) continue; // Should not happen if loaded correctly

            if (current_time >= (service->last_executed_timestamp + service->interval_seconds)) {
                syslog(LOG_DEBUG, "Service '%s' due for execution.", service->service_name);
                
                const cJSON *condition_item = cJSON_GetObjectItemCaseSensitive(service->config_json, "condition");
                const char *condition_str = condition_item ? cJSON_GetStringValue(condition_item) : "ALWAYS_TRUE"; // Default to always_true

                if (check_condition(condition_str, &current_ctx)) {
                    syslog(LOG_INFO, "Condition MET for service: %s. Executing actions.", service->service_name);
                    const cJSON *actions_array = cJSON_GetObjectItemCaseSensitive(service->config_json, "actions");
                    const cJSON *action_item = NULL;
                    cJSON_ArrayForEach(action_item, actions_array) {
                        const cJSON *type_item = cJSON_GetObjectItemCaseSensitive(action_item, "type");
                        const char *type_str = type_item ? cJSON_GetStringValue(type_item) : NULL;
                        if (type_str) {
                            syslog(LOG_INFO, "Executing action type: %s for service %s", type_str, service->service_name);
                            if (strcmp(type_str, "list_files") == 0) action_list_files_c(action_item, &current_ctx);
                            else if (strcmp(type_str, "mkdir") == 0) action_mkdir_c(action_item, &current_ctx);
                            else if (strcmp(type_str, "run_command") == 0) action_run_command_c(action_item, &current_ctx);
                            else if (strcmp(type_str, "notify") == 0) action_notify_c(action_item, &current_ctx);
                            else if (strcmp(type_str, "shell") == 0) action_shell_c(action_item, &current_ctx);
                            else syslog(LOG_WARNING, "Unknown action type: %s in service %s", type_str, service->service_name);
                        } else {
                            syslog(LOG_ERR, "Action item missing 'type' or type is not a string in service %s", service->service_name);
                        }
                    }
                } else {
                    syslog(LOG_INFO, "Condition NOT MET for service: %s", service->service_name);
                }
                service->last_executed_timestamp = current_time;
            }
        }
        sleep(MASTER_LOOP_SLEEP_INTERVAL);
    }

    syslog(LOG_INFO, "WhiteRails Runtime (C version) is shutting down (unexpected).");
    closelog(); // Close syslog
    if (global_schema_json) {
        cJSON_Delete(global_schema_json); // Clean up global schema
    }
    // Free all loaded services
    for(int i=0; i<num_loaded_services; ++i) free_service(&loaded_services[i]);
    return 0;
}

// --- Implementation of Action Function Stubs (modified to use syslog) ---
void action_list_files_c(const cJSON *action_obj, const void *context) {
    const cJSON *path_item = cJSON_GetObjectItemCaseSensitive(action_obj, "path");
    const char *path_str = path_item ? cJSON_GetStringValue(path_item) : "/"; // Default to /

    char command[512];
    snprintf(command, sizeof(command), "ls -la %s", path_str);

    FILE *pipe = popen(command, "r");
    if (!pipe) {
        syslog(LOG_ERR, "[LIST_FILES_C] popen failed for command '%s': %s", command, strerror(errno));
        return;
    }
    syslog(LOG_INFO, "[LIST_FILES_C] Executed: %s", command);

    char buffer[256];
    // Log each line of output as a separate syslog entry for better readability in logs
    // This could be verbose for large listings.
    syslog(LOG_INFO, "[LIST_FILES_C] Output for path: %s ---START---", path_str);
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Remove trailing newline if present, as syslog might add its own or format differently
        buffer[strcspn(buffer, "\n")] = 0; 
        syslog(LOG_INFO, "[LIST_FILES_C] %s", buffer);
    }
    syslog(LOG_INFO, "[LIST_FILES_C] Output for path: %s ---END---", path_str);

    int status = pclose(pipe);
    if (status == -1) {
        syslog(LOG_ERR, "[LIST_FILES_C] pclose failed for '%s': %s", command, strerror(errno));
    } else if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            syslog(LOG_WARNING, "[LIST_FILES_C] Command '%s' exited with status %d.", command, exit_code);
        } else {
            syslog(LOG_INFO, "[LIST_FILES_C] Command '%s' completed successfully.", command);
        }
    } else if (WIFSIGNALED(status)) {
        syslog(LOG_WARNING, "[LIST_FILES_C] Command '%s' terminated by signal %d.", command, WTERMSIG(status));
    }
}

void action_mkdir_c(const cJSON *action_obj, const void *context) {
    const cJSON *path_item_json = cJSON_GetObjectItemCaseSensitive(action_obj, "path");
    const char *path_str = path_item_json ? cJSON_GetStringValue(path_item_json) : NULL;

    if (path_str == NULL || strlen(path_str) == 0) {
        syslog(LOG_ERR, "[MKDIR_C] Missing or empty path parameter.");
        return;
    }
    char command[1024]; 
    snprintf(command, sizeof(command), "mkdir -p '%s'", path_str);
    syslog(LOG_INFO, "[MKDIR_C] Attempting to execute: %s", command);
    int ret = system(command);
    if (ret == 0) {
        syslog(LOG_INFO, "[MKDIR_C] Path '%s' created or already exists.", path_str);
    } else {
        syslog(LOG_ERR, "[MKDIR_C] Failed to create path '%s'. System command returned %d.", path_str, ret);
    }
}

void action_run_command_c(const cJSON *action_obj, const void *context) {
    const cJSON *command_item_json = cJSON_GetObjectItemCaseSensitive(action_obj, "command");
    const char *command_str = command_item_json ? cJSON_GetStringValue(command_item_json) : NULL;

    if (command_str == NULL || strlen(command_str) == 0) {
        syslog(LOG_ERR, "[RUN_COMMAND_C] Missing or empty command parameter.");
        return;
    }
    char bg_command[2048]; 
    snprintf(bg_command, sizeof(bg_command), "(%s) &", command_str);
    syslog(LOG_INFO, "[RUN_COMMAND_C] Attempting to dispatch in background: %s (original: %s)", bg_command, command_str);
    int ret = system(bg_command);
    if (ret == 0) {
        syslog(LOG_INFO, "[RUN_COMMAND_C] Command '%s' dispatched.", command_str);
    } else {
        syslog(LOG_ERR, "[RUN_COMMAND_C] Failed to dispatch command '%s'. System command returned %d.", command_str, ret);
    }
}

void action_notify_c(const cJSON *action_obj, const void *context) {
    const cJSON *message_item = cJSON_GetObjectItemCaseSensitive(action_obj, "message");
    const char *message_str = message_item ? cJSON_GetStringValue(message_item) : NULL;
    if (message_str) {
        syslog(LOG_INFO, "[NOTIFY_C] Message: %s", message_str);
    } else {
        syslog(LOG_WARNING, "[NOTIFY_C] 'message' parameter missing or not a string.");
    }
}

void action_shell_c(const cJSON *action_obj, const void *context) {
    const cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(action_obj, "cmd");
    const char *cmd_str = cmd_item ? cJSON_GetStringValue(cmd_item) : NULL;
    
    // Fallback to "command" if "cmd" is not present
    if (!cmd_str) {
        const cJSON *command_item = cJSON_GetObjectItemCaseSensitive(action_obj, "command");
        cmd_str = command_item ? cJSON_GetStringValue(command_item) : NULL;
        if (cmd_str) syslog(LOG_DEBUG, "[SHELL_C] Used fallback 'command' key.");
    }

    if (cmd_str) {
        syslog(LOG_INFO, "[SHELL_C] Executing shell command: %s", cmd_str);
        // For shell, typically we want to see its output directly or handle it.
        // Using popen to capture output and log it.
        FILE *pipe = popen(cmd_str, "r");
        if (!pipe) {
            syslog(LOG_ERR, "[SHELL_C] popen failed for command '%s': %s", cmd_str, strerror(errno));
            return;
        }
        char buffer[256];
        syslog(LOG_INFO, "[SHELL_C] Output for command: %s ---START---", cmd_str);
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            buffer[strcspn(buffer, "\n")] = 0;
            syslog(LOG_INFO, "[SHELL_C_OUT] %s", buffer);
        }
        syslog(LOG_INFO, "[SHELL_C] Output for command: %s ---END---", cmd_str);
        
        int status = pclose(pipe);
        if (status == -1) {
            syslog(LOG_ERR, "[SHELL_C] pclose failed for '%s': %s", cmd_str, strerror(errno));
        } else if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) syslog(LOG_WARNING, "[SHELL_C] Command '%s' exited with status %d.", cmd_str, exit_code);
            else syslog(LOG_INFO, "[SHELL_C] Command '%s' completed successfully.", cmd_str);
        } else if (WIFSIGNALED(status)) {
            syslog(LOG_WARNING, "[SHELL_C] Command '%s' terminated by signal %d.", cmd_str, WTERMSIG(status));
        }

    } else {
        syslog(LOG_ERR, "[SHELL_C] 'cmd' or 'command' parameter missing or not a string.");
    }
}
```
