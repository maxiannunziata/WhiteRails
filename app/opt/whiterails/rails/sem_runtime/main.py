import os
import json
import time
import importlib
import sys

# 1. Import jsonschema and note dependency
# Note: jsonschema library is now a conceptual dependency.
# It would need to be installed in the Python environment (e.g., pip install jsonschema).
import jsonschema

SERVICES_DIR = "/app/opt/whiterails/services/"
ACTIONS_DIR_PATH = "/app/opt/whiterails/rails/sem_runtime/actions/" # Base path for actions
SCHEMA_FILE_PATH = os.path.join(ACTIONS_DIR_PATH, "semantic_service_schema.json")

# Global variable to hold the loaded schema
loaded_schema = None

def get_battery_level(): # Example context function
    """Placeholder for getting current battery level."""
    return 100 # Simulate 100% battery

# 2. Implement validate_service_json function
def validate_service_json(service_data, schema, service_name_for_log):
    """
    Validates a service data dictionary against the loaded JSON schema.
    Args:
        service_data: The loaded service JSON as a Python dictionary.
        schema: The loaded JSON schema as a Python dictionary.
        service_name_for_log: The name of the service (from file or 'name' field) for logging.
    Returns:
        True if validation passes, False otherwise.
    """
    if not schema: # Check if schema was loaded successfully
        print(f"[MAIN_PY_VALIDATE_ERR] Schema not loaded. Cannot validate service: {service_name_for_log}", file=sys.stderr)
        return False
    try:
        jsonschema.validate(instance=service_data, schema=schema)
        print(f"[MAIN_PY] Service '{service_name_for_log}' passed schema validation.")
        return True
    except jsonschema.exceptions.ValidationError as e:
        # Provide more specific error: which field, what was the value, what was expected.
        error_path = " -> ".join(str(p) for p in e.path) if e.path else "N/A"
        error_validator = e.validator
        error_validator_value = e.validator_value
        
        log_message = (
            f"[MAIN_PY_VALIDATE_ERR] Schema validation failed for service: '{service_name_for_log}'.\n"
            f"  Error: {e.message}\n"
            f"  Path to error: '{error_path}'\n"
            f"  Violated schema rule: '{error_validator}: {error_validator_value}'\n"
            f"  Problematic value: '{e.instance}'"
        )
        print(log_message, file=sys.stderr)
        return False
    except Exception as e: # Catch other potential errors from jsonschema library
        print(f"[MAIN_PY_VALIDATE_ERR] General error during schema validation for service '{service_name_for_log}': {e}", file=sys.stderr)
        return False

def main_loop():
    """
    Main loop for the semantic runtime service.
    Loads services, validates them against a schema, and executes actions if conditions are met.
    """
    print("[MAIN_PY] Starting main loop...")
    
    # 3. Load the schema once when the loop starts
    global loaded_schema
    try:
        with open(SCHEMA_FILE_PATH, 'r') as f:
            loaded_schema = json.load(f)
        print(f"[MAIN_PY] Successfully loaded schema from {SCHEMA_FILE_PATH}")
    except FileNotFoundError:
        print(f"[MAIN_PY_ERR] CRITICAL: Schema file not found at {SCHEMA_FILE_PATH}. Validation will be skipped.", file=sys.stderr)
        loaded_schema = None # Ensure it's None if file not found
    except json.JSONDecodeError as e_json:
        print(f"[MAIN_PY_ERR] CRITICAL: Failed to decode schema JSON from {SCHEMA_FILE_PATH}: {e_json}. Validation will be skipped.", file=sys.stderr)
        loaded_schema = None # Ensure it's None if schema is invalid
    except Exception as e_schema: # Catch any other schema loading errors
        print(f"[MAIN_PY_ERR] CRITICAL: Unexpected error loading schema from {SCHEMA_FILE_PATH}: {e_schema}. Validation will be skipped.", file=sys.stderr)
        loaded_schema = None

    # Ensure the parent directory of 'actions' is in sys.path for dynamic imports
    # Assumes main.py is in /app/opt/whiterails/rails/sem_runtime/
    # and actions are in /app/opt/whiterails/rails/sem_runtime/actions/
    # So, the directory containing 'actions' is os.path.dirname(ACTIONS_DIR_PATH)
    # which is /app/opt/whiterails/rails/sem_runtime/
    # This path needs to be in sys.path so that `import actions.module` works.
    runtime_base_dir = os.path.dirname(ACTIONS_DIR_PATH) # This is .../sem_runtime/
    if runtime_base_dir not in sys.path:
        sys.path.insert(0, runtime_base_dir)
        print(f"[MAIN_PY] Added '{runtime_base_dir}' to sys.path for action imports.")


    while True:
        print(f"[MAIN_PY] Scanning services in {SERVICES_DIR}...")
        service_files_found = False
        for service_file_name in os.listdir(SERVICES_DIR):
            if service_file_name.endswith(".json"):
                service_files_found = True
                filepath = os.path.join(SERVICES_DIR, service_file_name)
                service_name_for_log = service_file_name # Default log name to filename
                try:
                    with open(filepath, 'r') as f:
                        service_data = json.load(f)
                    
                    service_name_for_log = service_data.get('name', service_file_name) # Use 'name' field if available
                    print(f"[MAIN_PY] Loaded service data for: '{service_name_for_log}' from {service_file_name}")

                    # 4. Integrate Validation into Service Loading Logic
                    if not validate_service_json(service_data, loaded_schema, service_name_for_log):
                        # Error message already printed by validate_service_json
                        print(f"[MAIN_PY_ERR] Service '{service_name_for_log}' failed schema validation or schema not loaded. Skipping.")
                        continue # Skip this service

                    # If validation passed, proceed with original conceptual logic
                    current_context = {"battery_level": get_battery_level()}
                    # Simplified eval - In a real scenario, use a safer expression evaluation library or method
                    condition_met = eval(service_data.get("condition", "False"), {}, current_context)

                    if condition_met:
                        print(f"[MAIN_PY] Condition met for service: '{service_name_for_log}'")
                        for action_def in service_data.get("actions", []):
                            action_type = action_def.get("type")
                            if not action_type:
                                print(f"[MAIN_PY_ERR] Action missing 'type' in service '{service_name_for_log}'. Skipping action.", file=sys.stderr)
                                continue
                            try:
                                # Dynamic import of action module (e.g., actions.list_files)
                                # Assumes action modules are in a package named 'actions'
                                # located in a directory that is in sys.path (e.g., runtime_base_dir).
                                action_module = importlib.import_module(f"actions.{action_type}")
                                action_module.run(action_def, current_context) # Pass context if needed by actions
                                print(f"[MAIN_PY] Executed action '{action_type}' for service '{service_name_for_log}'")
                            except ImportError:
                                print(f"[MAIN_PY_ERR] Action module 'actions.{action_type}' not found or import error for service '{service_name_for_log}'.", file=sys.stderr)
                            except AttributeError as e_attr: # E.g. if run function is missing
                                print(f"[MAIN_PY_ERR] Action module 'actions.{action_type}' does not have a 'run' function or attribute error: {e_attr} for service '{service_name_for_log}'.", file=sys.stderr)
                            except Exception as e_action:
                                print(f"[MAIN_PY_ERR] Error executing action '{action_type}' for service '{service_name_for_log}': {e_action}", file=sys.stderr)
                    else:
                        print(f"[MAIN_PY] Condition NOT met for service: '{service_name_for_log}'")
                                
                except json.JSONDecodeError:
                    print(f"[MAIN_PY_ERR] Failed to decode JSON from '{filepath}'", file=sys.stderr)
                except Exception as e_service:
                    print(f"[MAIN_PY_ERR] Error processing service file '{filepath}': {e_service}", file=sys.stderr)
        
        if not service_files_found:
            print(f"[MAIN_PY] No service files found in {SERVICES_DIR}.")

        scan_interval = int(os.getenv("WHITERALLS_SCAN_INTERVAL", "60"))
        print(f"[MAIN_PY] Next scan in {scan_interval} seconds...")
        time.sleep(scan_interval)

if __name__ == "__main__":
    print("[MAIN_PY] Semantic Runtime Initializing...")
    # The sys.path modification for actions is now handled inside main_loop's initial setup.
    main_loop()
```
