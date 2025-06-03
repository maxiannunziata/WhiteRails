#include <stdio.h>    // For printf (temp), snprintf, sscanf
#include <string.h>   // For strncmp, strchr
#include <sys/time.h> // For gettimeofday
#include <stdlib.h>   // For atoi (simple parsing)

#include "condition.h"
// No #include "deps/cJSON/cJSON.h" needed here unless params are used by evaluators

// Static variable to store the timestamp of the last recorded activity
static struct timeval last_activity_timestamp = {0, 0};
static int activity_recorded_at_least_once = 0; // Flag

// Call this function to update the last activity timestamp
void record_activity(void) {
    gettimeofday(&last_activity_timestamp, NULL);
    activity_recorded_at_least_once = 1;
    // printf("Activity recorded at: %ld.%06ld\n", last_activity_timestamp.tv_sec, last_activity_timestamp.tv_usec); // Temporary log
    // Later: syslog(LOG_DEBUG, "Activity recorded");
}

// Condition evaluator: always_true
// Params and service_name_for_log are unused by this specific evaluator but part of signature
int eval_condition_always_true(const cJSON *params, const char *service_name_for_log) {
    (void)params; // Suppress unused warning
    (void)service_name_for_log; // Suppress unused warning for service_name_for_log
    // printf("Evaluating condition 'always_true' for service '%s': TRUE\n", service_name_for_log); // Temporary log
    // Later: syslog(LOG_DEBUG, "Condition 'always_true' for service '%s' evaluated: TRUE", service_name_for_log);
    return 1; // Condition is always met
}

// Condition evaluator: no_activity(threshold_seconds)
// Params currently unused by this specific evaluator as threshold is parsed from condition_str
int eval_condition_no_activity(const cJSON *params, const char *service_name_for_log) {
    (void)params; // Suppress unused warning
    
    // This function is a placeholder if threshold is passed via `params`.
    // The actual logic is in `evaluate_service_condition` which parses the threshold from the string.
    // If we shift to cJSON params for threshold, this function would contain the core logic.
    // For now, it should not be called directly if `evaluate_service_condition` handles parsing.
    fprintf(stderr, "Service '%s': eval_condition_no_activity direct call not implemented, use 'no_activity(SECONDS)' string format.\n", service_name_for_log);
    return -1; // Error or undefined behavior
}

// Top-level function to parse and evaluate a condition string
int evaluate_service_condition(const char *condition_str, const char *service_name_for_log) {
    if (!condition_str || *condition_str == '\0') {
        fprintf(stderr, "Service '%s': Condition string is NULL or empty. Defaulting to TRUE.\n", service_name_for_log);
        // Later: syslog(LOG_WARNING, "Service '%s': Condition string is NULL or empty. Defaulting to TRUE.", service_name_for_log);
        return 1; // Or -1 for error, depending on desired strictness
    }

    if (strcmp(condition_str, "always_true") == 0) {
        return eval_condition_always_true(NULL, service_name_for_log);
    } else if (strncmp(condition_str, "no_activity(", 12) == 0) {
        if (!activity_recorded_at_least_once) {
            // printf("Service '%s': Condition 'no_activity' - no activity ever recorded. Condition MET (assuming startup/idle).\n", service_name_for_log);
            // Later: syslog(LOG_DEBUG, "Service '%s': 'no_activity' - no prior activity. Condition MET.", service_name_for_log);
            return 1; // No activity means the "no activity" condition IS met.
        }

        int threshold_seconds = 0;
        if (sscanf(condition_str + 12, "%d)", &threshold_seconds) == 1) {
            if (threshold_seconds < 0) {
                fprintf(stderr, "Service '%s': Invalid negative threshold %d for 'no_activity'. Condition evaluation FAILED.\n", service_name_for_log, threshold_seconds);
                // Later: syslog(LOG_ERR, "Service '%s': Invalid 'no_activity' threshold %d. Eval FAILED.", service_name_for_log, threshold_seconds);
                return -1; // Error
            }
            
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            long seconds_since_last_activity = current_time.tv_sec - last_activity_timestamp.tv_sec;

            // printf("Service '%s': Condition 'no_activity(%d)' - Last activity: %ld s ago. Current time: %ld. Last activity time: %ld\n",
            //        service_name_for_log, threshold_seconds, seconds_since_last_activity, current_time.tv_sec, last_activity_timestamp.tv_sec); // Temp log

            if (seconds_since_last_activity >= threshold_seconds) {
                // printf("Service '%s': Condition 'no_activity(%d)' MET. (%ld >= %d)\n", service_name_for_log, threshold_seconds, seconds_since_last_activity, threshold_seconds); // Temp
                // Later: syslog(LOG_DEBUG, "Service '%s': 'no_activity(%d)' MET.", service_name_for_log, threshold_seconds);
                return 1; // Condition met: no activity for at least threshold_seconds
            } else {
                // printf("Service '%s': Condition 'no_activity(%d)' NOT MET. (%ld < %d)\n", service_name_for_log, threshold_seconds, seconds_since_last_activity, threshold_seconds); // Temp
                // Later: syslog(LOG_DEBUG, "Service '%s': 'no_activity(%d)' NOT MET.", service_name_for_log, threshold_seconds);
                return 0; // Condition not met: activity within threshold
            }
        } else {
            fprintf(stderr, "Service '%s': Could not parse threshold from 'no_activity' string: '%s'. Evaluation FAILED.\n", service_name_for_log, condition_str);
            // Later: syslog(LOG_ERR, "Service '%s': Failed to parse 'no_activity' string: '%s'. Eval FAILED.", service_name_for_log, condition_str);
            return -1; // Error
        }
    } else {
        fprintf(stderr, "Service '%s': Unknown condition type '%s'. Evaluation FAILED.\n", service_name_for_log, condition_str);
        // Later: syslog(LOG_ERR, "Service '%s': Unknown condition type '%s'. Eval FAILED.", service_name_for_log, condition_str);
        return -1; // Unknown condition type, error
    }
}
