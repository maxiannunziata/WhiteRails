#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "cJSON.h" // For cJSON type - found via CFLAGS -I deps/cJSON

// Forward declaration if cJSON is used as pointer, otherwise full include
// struct cJSON; // Or use #include "deps/cJSON/cJSON.h" if cJSON objects are passed by value or members accessed

typedef void (action_fn)(const cJSON *action_params);

// Declare action functions
action_fn app_action_list_files;
action_fn app_action_mkdir;
action_fn app_action_run_command;
action_fn app_action_notify;
action_fn app_action_shell;

// Declare the dispatcher function itself
void dispatch_action(const char *type, const cJSON *params);

#endif // DISPATCHER_H
