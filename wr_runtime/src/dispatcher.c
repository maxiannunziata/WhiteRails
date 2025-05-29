#include <stdio.h>  // For printf/fprintf (temporary, will be syslog)
#include <string.h> // For strcmp
#include "dispatcher.h" // Includes cJSON.h and action function declarations

// Action table mapping action names to function pointers
static const struct {
    const char *name;
    action_fn *fn;
} action_table[] = {
    {"list_files",    app_action_list_files},
    {"mkdir",         app_action_mkdir},
    {"run_command",   app_action_run_command},
    {"notify",        app_action_notify},
    {"shell",         app_action_shell},
    // Future actions can be added here
    {NULL, NULL} // Sentinel to mark the end of the table
};

// Dispatch an action based on its type
void dispatch_action(const char *type, const cJSON *params) {
    if (type == NULL) {
        fprintf(stderr, "dispatch_action: Error - action type is NULL.\n");
        // Later: syslog(LOG_ERR, "dispatch_action: Error - action type is NULL.");
        return;
    }

    for (int i = 0; action_table[i].name != NULL; i++) {
        if (strcmp(type, action_table[i].name) == 0) {
            if (action_table[i].fn != NULL) {
                // Log intent to run action (temporarily to stdout)
                printf("Dispatching action '%s'\n", type); 
                // Later: syslog(LOG_INFO, "Dispatching action '%s'", type);
                action_table[i].fn(params); // Call the action function
                return;
            } else {
                fprintf(stderr, "dispatch_action: Error - action '%s' has a NULL function pointer.\n", type);
                // Later: syslog(LOG_ERR, "dispatch_action: Error - action '%s' has a NULL function pointer.", type);
                return;
            }
        }
    }

    fprintf(stderr, "dispatch_action: Error - Unknown action type '%s'.\n", type);
    // Later: syslog(LOG_ERR, "dispatch_action: Error - Unknown action type '%s'.", type);
}
