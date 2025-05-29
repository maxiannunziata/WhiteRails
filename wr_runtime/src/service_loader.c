#define _DEFAULT_SOURCE // For DT_REG, DT_DIR etc. in <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>   // For malloc, free, ftell, fseek
#include <dirent.h>   // For directory operations
#include <sys/stat.h> // For stat, to check if it's a directory

#include "service_loader.h"
#include "schema.h"       // For SERVICE_SCHEMA (used by validator)
// #include "deps/cJSON/cJSON.h" // Already included via service_loader.h

// Logging - temporary, replace with syslog later
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: SvcLoader: " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("INFO: SvcLoader: " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) printf("DEBUG: SvcLoader: " fmt "\n", ##__VA_ARGS__)

// Static storage for service configurations
static service_config_t loaded_services[MAX_SERVICES];
static int num_loaded_services = 0;

// From previous step (Step 6)
static char last_err[256];

int validate_json_with_hardcoded_schema(const cJSON *json_service_obj, const char *service_name_for_log) {
    last_err[0] = '\0';
    if (!json_service_obj) {
        snprintf(last_err, sizeof(last_err), "Service '%s': JSON object is NULL.", service_name_for_log);
        return -1;
    }
    if (!cJSON_IsObject(json_service_obj)) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Root is not a JSON object.", service_name_for_log);
        return -1;
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json_service_obj, "name");
    if (!name) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Missing required field 'name'.", service_name_for_log);
        return -1;
    }
    if (!cJSON_IsString(name) || (name->valuestring == NULL) || (name->valuestring[0] == '\0')) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Field 'name' must be a non-empty string.", service_name_for_log);
        return -1;
    }
    const cJSON *condition = cJSON_GetObjectItemCaseSensitive(json_service_obj, "condition");
    if (!condition) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Missing required field 'condition'.", service_name_for_log);
        return -1;
    }
    if (!cJSON_IsString(condition) || (condition->valuestring == NULL) || (condition->valuestring[0] == '\0')) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Field 'condition' must be a non-empty string.", service_name_for_log);
        return -1;
    }
    const cJSON *interval = cJSON_GetObjectItemCaseSensitive(json_service_obj, "interval");
    if (interval && !cJSON_IsNumber(interval)) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Optional field 'interval' must be an integer.", service_name_for_log);
        return -1;
    }
    if (interval && interval->valuedouble < 0) {
         snprintf(last_err, sizeof(last_err), "Service '%s': Optional field 'interval' must be a non-negative integer.", service_name_for_log);
        return -1;
    }
    const cJSON *actions = cJSON_GetObjectItemCaseSensitive(json_service_obj, "actions");
    if (!actions) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Missing required field 'actions'.", service_name_for_log);
        return -1;
    }
    if (!cJSON_IsArray(actions)) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Field 'actions' must be an array.", service_name_for_log);
        return -1;
    }
    if (cJSON_GetArraySize(actions) == 0) {
        snprintf(last_err, sizeof(last_err), "Service '%s': Field 'actions' array cannot be empty.", service_name_for_log);
        return -1;
    }
    cJSON *action_item;
    int action_idx = 0;
    cJSON_ArrayForEach(action_item, actions) {
        if (!cJSON_IsObject(action_item)) {
            snprintf(last_err, sizeof(last_err), "Service '%s', Action #%d: Item is not a JSON object.", service_name_for_log, action_idx);
            return -1;
        }
        const cJSON *action_type = cJSON_GetObjectItemCaseSensitive(action_item, "type");
        if (!action_type) {
            snprintf(last_err, sizeof(last_err), "Service '%s', Action #%d: Missing required field 'type'.", service_name_for_log, action_idx);
            return -1;
        }
        if (!cJSON_IsString(action_type) || (action_type->valuestring == NULL) || (action_type->valuestring[0] == '\0')) {
            snprintf(last_err, sizeof(last_err), "Service '%s', Action #%d: Field 'type' must be a non-empty string.", service_name_for_log, action_idx);
            return -1;
        }
        const char* optional_props[] = {"path", "command", "message"};
        for (int i = 0; i < (sizeof(optional_props) / sizeof(optional_props[0])); ++i) {
            const cJSON* prop = cJSON_GetObjectItemCaseSensitive(action_item, optional_props[i]);
            if (prop) {
                if (!cJSON_IsString(prop) || (prop->valuestring == NULL) || (prop->valuestring[0] == '\0')) {
                     snprintf(last_err, sizeof(last_err), "Service '%s', Action #%d: Optional field '%s' must be a non-empty string if present.", service_name_for_log, action_idx, optional_props[i]);
                     return -1;
                }
            }
        }
        action_idx++;
    }
    return 0; // Success
}

const char* get_service_validation_error() {
   return last_err;
}
// End of functions from previous step

void SvcLoader_init(void) {
    SvcLoader_free_all_services(); // Clear any existing services first
    num_loaded_services = 0;
    for (int i = 0; i < MAX_SERVICES; i++) {
        memset(&loaded_services[i], 0, sizeof(service_config_t));
        loaded_services[i].loaded = 0;
        loaded_services[i].interval_seconds = 0; // Default: run once or as per condition only
        loaded_services[i].last_run_timestamp = 0;
    }
    LOG_DEBUG("Service loader initialized. Max services: %d", MAX_SERVICES);
}

void SvcLoader_free_all_services(void) {
    LOG_DEBUG("Freeing all loaded services...");
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (loaded_services[i].loaded && loaded_services[i].config_json != NULL) {
            cJSON_Delete(loaded_services[i].config_json);
            loaded_services[i].config_json = NULL;
        }
        loaded_services[i].loaded = 0; // Mark as free
    }
    num_loaded_services = 0;
    LOG_INFO("All services freed and unloaded.");
}

static char* read_file_to_string(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        LOG_ERROR("Could not open file: %s", filepath);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char*)malloc(length + 1);
    if (!buffer) {
        LOG_ERROR("Could not allocate memory to read file: %s", filepath);
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

void SvcLoader_load_services(const char *services_dir_path) {
    DIR *dir;
    struct dirent *entry;
    char filepath[1024]; // For constructing full path to service files

    if (!services_dir_path) {
        LOG_ERROR("Services directory path is NULL.");
        return;
    }
    
    LOG_INFO("Loading services from directory: %s", services_dir_path);

    if (num_loaded_services >= MAX_SERVICES) {
        LOG_ERROR("Max services limit (%d) already reached. Cannot load more.", MAX_SERVICES);
        return;
    }

    dir = opendir(services_dir_path);
    if (!dir) {
        LOG_ERROR("Could not open services directory: %s. Ensure it exists.", services_dir_path);
        return;
    }

    while ((entry = readdir(dir)) != NULL && num_loaded_services < MAX_SERVICES) {
        if (entry->d_type == DT_REG) { // Regular file
            const char *filename = entry->d_name;
            const char *ext = strrchr(filename, '.');
            if (ext && strcmp(ext, ".json") == 0) {
                snprintf(filepath, sizeof(filepath), "%s/%s", services_dir_path, filename);
                LOG_DEBUG("Processing potential service file: %s", filepath);

                char *file_content = read_file_to_string(filepath);
                if (!file_content) {
                    LOG_ERROR("Failed to read content of service file: %s", filepath);
                    continue; 
                }

                cJSON *json_obj = cJSON_Parse(file_content);
                free(file_content); 

                if (!json_obj) {
                    const char *parse_error = cJSON_GetErrorPtr();
                    LOG_ERROR("Failed to parse JSON from file %s. Error (near): %s", filepath, parse_error ? parse_error : "unknown");
                    continue;
                }

                char temp_service_name_for_log[MAX_SERVICE_NAME_LEN];
                strncpy(temp_service_name_for_log, filename, sizeof(temp_service_name_for_log) -1);
                temp_service_name_for_log[sizeof(temp_service_name_for_log) -1] = '\0'; // Ensure null termination
                char *dot = strrchr(temp_service_name_for_log, '.');
                if (dot) *dot = '\0';


                if (validate_json_with_hardcoded_schema(json_obj, temp_service_name_for_log) == 0) {
                    service_config_t *svc = &loaded_services[num_loaded_services]; 
                    svc->loaded = 0; 

                    cJSON *name_json = cJSON_GetObjectItemCaseSensitive(json_obj, "name");
                    strncpy(svc->name, name_json->valuestring, MAX_SERVICE_NAME_LEN -1);
                    svc->name[MAX_SERVICE_NAME_LEN -1] = '\0';

                    cJSON *condition_json = cJSON_GetObjectItemCaseSensitive(json_obj, "condition");
                    strncpy(svc->condition_str, condition_json->valuestring, MAX_CONDITION_STR_LEN -1);
                    svc->condition_str[MAX_CONDITION_STR_LEN -1] = '\0';
                    
                    cJSON *interval_json = cJSON_GetObjectItemCaseSensitive(json_obj, "interval");
                    if (interval_json && cJSON_IsNumber(interval_json)) {
                        svc->interval_seconds = interval_json->valueint;
                        if (svc->interval_seconds < 0) svc->interval_seconds = 0; 
                    } else {
                        svc->interval_seconds = 0; 
                    }
                    
                    svc->config_json = json_obj; 
                    svc->last_run_timestamp = 0; 
                    svc->loaded = 1;
                    num_loaded_services++;
                    LOG_INFO("Successfully loaded and validated service: %s (Interval: %ds, Condition: '%s')",
                                svc->name, svc->interval_seconds, svc->condition_str);
                } else {
                    LOG_ERROR("Service file %s failed validation: %s", filepath, get_service_validation_error());
                    cJSON_Delete(json_obj); 
                }
            }
        }
    }
    closedir(dir);
    LOG_INFO("Service loading complete. Total services loaded: %d", num_loaded_services);
}

void SvcLoader_reload_services(const char *services_dir_path) {
    LOG_INFO("Reloading services from: %s", services_dir_path ? services_dir_path : "default path");
    SvcLoader_init(); 
    SvcLoader_load_services(services_dir_path ? services_dir_path : DEFAULT_SERVICE_DIR);
}

int SvcLoader_get_count(void) {
    return num_loaded_services;
}

service_config_t* SvcLoader_get_service_by_index(int index) {
    if (index >= 0 && index < num_loaded_services && loaded_services[index].loaded) {
        return &loaded_services[index];
    }
    return NULL;
}
