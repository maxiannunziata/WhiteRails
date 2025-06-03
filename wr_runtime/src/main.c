#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // For sleep, setsid, fork, STDIN_FILENO etc. (fork/setsid if daemonizing here)
#include <syslog.h>   // For openlog, syslog, closelog
#include <time.h>     // For time()
#include <string.h>   // For strcmp
#include <errno.h>    // For errno
#include <fcntl.h>    // For open, O_RDWR
#include <sys/stat.h> // For umask (if daemonizing)

#include "cJSON.h" // Found via CFLAGS -I deps/cJSON
#include "service_loader.h"
#include "dispatcher.h"
#include "condition.h"

#define MAIN_LOOP_SLEEP_SECONDS 1
#define SERVICE_RELOAD_INTERVAL_SECONDS 60

// Simple daemonize function (optional, can be handled by init system too)
// For a more robust daemon, consider double fork, proper signal handling, pid file etc.
static void __attribute__((unused)) daemonize_basic() {
    pid_t pid;

    // Fork off the parent process
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork failed during daemonize: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { // Parent exits
        exit(EXIT_SUCCESS);
    }

    // Create a new session
    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid failed during daemonize: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Fork again to prevent process from acquiring a controlling terminal (optional)
    /*
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "second fork failed during daemonize: %m");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { // First child exits
        exit(EXIT_SUCCESS);
    }
    */
    
    // Change the file mode mask
    // umask(0); // Or a more restrictive mask like 027

    // Change the current working directory to a safe place (optional)
    // if ((chdir("/")) < 0) {
    //    syslog(LOG_ERR, "chdir failed during daemonize: %m");
    //    exit(EXIT_FAILURE);
    // }

    // Close standard file descriptors
    syslog(LOG_INFO, "Daemonizing: Closing stdin, stdout, stderr.");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // Redirect stdin, stdout, stderr to /dev/null (optional, but good practice)
    // Ensure /dev/null can be opened. If not, this part might fail.
    // Some init systems handle this.
    int fd0 = open("/dev/null", O_RDWR);
    if (fd0 != -1) {
        dup2(fd0, STDIN_FILENO);
        dup2(fd0, STDOUT_FILENO);
        dup2(fd0, STDERR_FILENO);
        if (fd0 > 2) { // If fd0 was not 0, 1, or 2
            close(fd0);
        }
    } else {
        syslog(LOG_ERR, "Failed to open /dev/null for redirection");
    }
}


int main(int argc, char *argv[]) {
    (void)argc; // Suppress unused warning
    (void)argv; // Suppress unused warning

    // Initialize syslog
    // LOG_DAEMON is typical for daemons. LOG_PID includes PID in each message.
    openlog("wr_runtime", LOG_PID | LOG_PERROR, LOG_DAEMON); // LOG_PERROR also logs to stderr until daemonized/redirected
    syslog(LOG_INFO, "WhiteRAILS Runtime starting up...");

    // Basic daemonization (can be commented out if init system handles it, e.g. OpenRC's start-stop-daemon)
    // The OpenRC script provided earlier uses start-stop-daemon, which handles daemonizing.
    // So, explicit daemonize_basic() here might be redundant or conflict if OpenRC manages it.
    // For testing without OpenRC, it can be useful. For now, let's assume OpenRC handles it.
    // If daemonize_basic() is called, LOG_PERROR in openlog will stop having an effect after redirection.
    // daemonize_basic(); 
    // syslog(LOG_INFO, "Daemonized. Continuing startup."); // Log after potential daemonization

    SvcLoader_init();
    SvcLoader_load_services(DEFAULT_SERVICE_DIR); // Load initial services

    record_activity(); // Record initial system activity after setup

    time_t last_service_reload_time = time(NULL);

    syslog(LOG_INFO, "Entering main loop...");
    while (1) {
        time_t current_time = time(NULL);

        // Periodically reload services
        if (current_time - last_service_reload_time >= SERVICE_RELOAD_INTERVAL_SECONDS) {
            syslog(LOG_INFO, "Reloading services list.");
            SvcLoader_reload_services(DEFAULT_SERVICE_DIR); // This calls init then load
            last_service_reload_time = current_time;
            record_activity(); // Reloading services is an activity
        }

        int services_count = SvcLoader_get_count();
        // syslog(LOG_DEBUG, "Main loop tick. Processing %d services.", services_count); // Too verbose for INFO

        for (int i = 0; i < services_count; i++) {
            service_config_t *svc = SvcLoader_get_service_by_index(i);
            if (!svc || !svc->loaded) {
                continue; // Should not happen if count is correct
            }

            // Check if it's time to run based on interval
            // If interval is 0, it means it's always "time to run" if condition is also met.
            // Or, could mean run-once (not handled here yet) or only condition-driven.
            // For interval=0, last_run_timestamp check essentially makes it run once per condition met.
            // To make interval=0 truly condition-driven without being run-once,
            // last_run_timestamp should not be updated, or a different logic is needed.
            // Current logic: interval 0 means it can run every loop cycle if condition met.
            if (svc->interval_seconds == 0 || (current_time >= (svc->last_run_timestamp + svc->interval_seconds))) {
                syslog(LOG_DEBUG, "Service '%s': Interval met. Checking condition '%s'.", svc->name, svc->condition_str);
                
                int condition_result = evaluate_service_condition(svc->condition_str, svc->name);
                
                if (condition_result == 1) { // Condition met
                    syslog(LOG_INFO, "Service '%s': Condition '%s' MET. Executing actions.", svc->name, svc->condition_str);
                    
                    cJSON *actions_array = cJSON_GetObjectItemCaseSensitive(svc->config_json, "actions");
                    cJSON *action_item_json;
                    int action_idx = 0;
                    cJSON_ArrayForEach(action_item_json, actions_array) {
                        cJSON *action_type_json = cJSON_GetObjectItemCaseSensitive(action_item_json, "type");
                        if (cJSON_IsString(action_type_json) && (action_type_json->valuestring != NULL)) {
                            const char *action_type_str = action_type_json->valuestring;
                            syslog(LOG_DEBUG, "Service '%s', Action #%d: Dispatching type '%s'.", svc->name, action_idx, action_type_str);
                            dispatch_action(action_type_str, action_item_json); // Pass the whole action object as params
                        } else {
                            syslog(LOG_ERR, "Service '%s', Action #%d: 'type' is missing or not a string.", svc->name, action_idx);
                        }
                        action_idx++;
                    }
                    svc->last_run_timestamp = current_time; // Update last run time for this service
                    record_activity(); // Record activity after a service's actions are run
                } else if (condition_result == 0) { // Condition not met
                    syslog(LOG_DEBUG, "Service '%s': Condition '%s' NOT MET.", svc->name, svc->condition_str);
                } else { // Error evaluating condition
                    syslog(LOG_ERR, "Service '%s': Error evaluating condition '%s'.", svc->name, svc->condition_str);
                }
            }
        }
        sleep(MAIN_LOOP_SLEEP_SECONDS);
    }

    syslog(LOG_INFO, "WhiteRAILS Runtime shutting down (main loop exited - unexpected).");
    SvcLoader_free_all_services(); // Clean up
    closelog(); // Close syslog
    return 0; // Should not be reached in normal daemon operation
}
