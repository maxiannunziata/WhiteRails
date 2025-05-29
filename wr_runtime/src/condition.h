#ifndef CONDITION_H
#define CONDITION_H

#include "cJSON.h" // Not strictly needed by evaluators yet, but good for future - Found via CFLAGS -I deps/cJSON

// Return 1 if condition met (true), 0 if not (false), -1 on error.
typedef int (condition_eval_fn)(const cJSON *params, const char *service_name_for_log);

// Specific condition evaluators (params currently unused by these two)
int eval_condition_always_true(const cJSON *params, const char *service_name_for_log);
int eval_condition_no_activity(const cJSON *params, const char *service_name_for_log); 
                               // Params might hold threshold if we change parsing

// Top-level function to evaluate a condition string like "always_true" or "no_activity(SECONDS)"
int evaluate_service_condition(const char *condition_str, const char *service_name_for_log);

// Call this function from relevant places (e.g., after an action runs) to signify activity
void record_activity(void);

#endif // CONDITION_H
