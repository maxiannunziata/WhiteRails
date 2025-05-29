#ifndef SERVICE_LOADER_H
#define SERVICE_LOADER_H

#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include <time.h> // For time_t

#define MAX_SERVICES 32 // Max number of services that can be loaded
#define MAX_SERVICE_NAME_LEN 64
#define MAX_CONDITION_STR_LEN 128
#define DEFAULT_SERVICE_DIR "/var/lib/whiterails/services" // Default, can be overridden

typedef struct {
    char name[MAX_SERVICE_NAME_LEN];
    cJSON *config_json;        // Store the original parsed and validated JSON object
    char condition_str[MAX_CONDITION_STR_LEN]; // Store the condition string
    int interval_seconds;        // Service execution interval in seconds
    time_t last_run_timestamp;    // Timestamp of the last execution
    int loaded;                  // 0 if slot is free, 1 if service loaded
} service_config_t;

// Schema validation function (already implemented, ensure declaration)
int validate_json_with_hardcoded_schema(const cJSON *json_service_obj, const char *service_name_for_log);
const char* get_service_validation_error(void);

// Service management functions
void SvcLoader_init(void); // Initializes the service array
void SvcLoader_load_services(const char *services_dir_path);
void SvcLoader_reload_services(const char *services_dir_path); // For now, can just call init then load
void SvcLoader_free_all_services(void); // Frees cJSON objects and resets service array

// Accessors
int SvcLoader_get_count(void);
service_config_t* SvcLoader_get_service_by_index(int index);

#endif // SERVICE_LOADER_H
