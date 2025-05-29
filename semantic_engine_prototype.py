import json
import time

# 1. Define a simulated system state
simulated_system_state = {
    "last_activity_time": time.time(),
    "screen_locked": False,
    "files_and_dirs": ["/home/user/existing_file.txt"], # Added
}

# 2. Implement call_lock_screen()
def call_lock_screen():
    """Simulates locking the screen."""
    print("[Semantic Engine] Executing: lock_screen")
    simulated_system_state["screen_locked"] = True
    print(f"[Semantic Engine] System state updated: screen_locked = {simulated_system_state['screen_locked']}")

# New function: call_create_directory
def call_create_directory(path):
    """Simulates creating a directory."""
    print(f"[Semantic Engine] Executing: create_directory with path '{path}'")
    if path not in simulated_system_state["files_and_dirs"]:
        simulated_system_state["files_and_dirs"].append(path)
        print(f"[Semantic Engine] Directory '{path}' created.")
    else:
        print(f"[Semantic Engine] Directory '{path}' already exists.")
    print(f"[Semantic Engine] Current files and directories: {simulated_system_state['files_and_dirs']}")

# 3. Define ACTION_DISPATCHER
ACTION_DISPATCHER = {
    "lock_screen": call_lock_screen,
    "create_directory": call_create_directory, # Added
    "error_unknown_command": lambda: print("[Semantic Engine] Executing: error_unknown_command. This command is not recognized by the system.") # Added
}

# 4. Implement evaluate_condition()
def evaluate_condition(condition_str, current_input_value):
    """
    Evaluates a simplified condition string.
    Example: "no_activity > Xs" or "always_true"
    """
    print(f"Evaluating condition: {condition_str} with input '{current_input_value}'")
    try:
        if condition_str == "always_true":
            return True
        elif "no_activity > " in condition_str:
            parts = condition_str.split(">")
            threshold_str = parts[1].strip().replace("s", "")
            threshold_seconds = int(threshold_str)

            no_activity_duration = time.time() - simulated_system_state["last_activity_time"]
            print(f"Evaluating: no_activity ({no_activity_duration:.2f}s) > {threshold_seconds}s")

            return no_activity_duration > threshold_seconds
        else:
            print(f"Warning: Unknown condition format: {condition_str}")
            return False
    except ValueError:
        print(f"Error: Invalid threshold in condition string: {condition_str}")
        return False
    except Exception as e:
        print(f"Error evaluating condition '{condition_str}': {e}")
        return False

# 5. Implement execute_semantic_json()
def execute_semantic_json(json_str):
    """
    Parses a JSON string and executes the described action if the condition is met.
    """
    print(f"\nExecuting JSON: {json_str.strip()}") # .strip() for cleaner multiline JSON
    try:
        data = json.loads(json_str)
    except json.JSONDecodeError as e:
        print(f"Error: Could not decode JSON string. Details: {e}")
        return

    app = data.get("app")
    current_input = data.get("input")
    # Default condition to "always_true" if not present
    condition = data.get("condition", "always_true")
    action_details_str = data.get("action")

    print(f"Interpreting: App='{app}', Input='{current_input}', Condition='{condition}', Action='{action_details_str}'")

    # Check for required fields, condition is now optional due to default
    if not all([app, current_input, action_details_str]):
        print("Error: JSON is missing one or more required fields (app, input, action).")
        return

    condition_met = evaluate_condition(condition, current_input)

    if condition_met:
        print(f"Condition '{condition}' is TRUE.")
        if action_details_str.startswith("call('") and action_details_str.endswith("')"):
            content = action_details_str[len("call('"):-len("')")]
            
            # Split by "', '" to handle arguments correctly.
            # Ensure that we don't split if there are no arguments.
            if "', '" in content:
                 parts = content.split("', '")
                 action_name = parts[0]
                 args = [arg.strip("'") for arg in parts[1:]] # Remove potential single quotes from args themselves
            else: # No arguments
                 action_name = content # The content is just the action name
                 args = []

            print(f"Parsed Action: Name='{action_name}', Args={args}")

            action_function = ACTION_DISPATCHER.get(action_name)
            if action_function:
                try:
                    action_function(*args)
                except TypeError as e:
                    print(f"Error: Argument mismatch for action '{action_name}'. {e}")
                except Exception as e:
                    print(f"Error executing action '{action_name}': {e}")
            else:
                # If action is not in dispatcher, try error_unknown_command
                error_action_fn = ACTION_DISPATCHER.get("error_unknown_command")
                if error_action_fn:
                    print(f"Warning: Unknown action '{action_name}'. Triggering 'error_unknown_command'.")
                    error_action_fn()
                else: # Should not happen if error_unknown_command is in dispatcher
                    print(f"Critical Error: Unknown action '{action_name}' and 'error_unknown_command' is not available.")
        else:
            print(f"Error: Invalid action format '{action_details_str}'. Expected format: \"call('action_name', 'arg1', ...)\".")
    else:
        print(f"Condition '{condition}' is FALSE. No action taken.")

# 6. Include a test section
if __name__ == "__main__":
    print("--- Semantic Execution Engine Prototype ---")
    print(f"Initial system state: {simulated_system_state}")


    test_json_lock_screen = """
    {
      "app": "sensorMonitor",
      "input": "motion",
      "condition": "no_activity > 5s",
      "action": "call('lock_screen')"
    }
    """

    # Test Case 1: Condition should be true (Lock Screen)
    print("\n--- Test Case 1: Condition True (Lock Screen) ---")
    simulated_system_state["last_activity_time"] = time.time() - 6  # 6 seconds ago
    simulated_system_state["screen_locked"] = False # Ensure it's unlocked
    print(f"Initial state for test: last_activity_time set to ~6s ago, screen_locked = {simulated_system_state['screen_locked']}")
    execute_semantic_json(test_json_lock_screen)
    print(f"Final state for Test Case 1: screen_locked = {simulated_system_state['screen_locked']}")

    # Test Case 2: Condition should be false (No Action)
    print("\n--- Test Case 2: Condition False (No Action) ---")
    simulated_system_state["last_activity_time"] = time.time() - 1  # 1 second ago
    simulated_system_state["screen_locked"] = False # Reset for this test
    print(f"Initial state for test: last_activity_time set to ~1s ago, screen_locked = {simulated_system_state['screen_locked']}")
    execute_semantic_json(test_json_lock_screen)
    print(f"Final state for Test Case 2: screen_locked = {simulated_system_state['screen_locked']}")

    # Test Case 3: Create New Directory
    print("\n--- Test Case 3: Create New Directory ---")
    test_json_create_dir = """
    {
      "app": "fileManager",
      "input": "userCommand",
      "condition": "always_true",
      "action": "call('create_directory', '/home/user/new_folder')"
    }
    """
    execute_semantic_json(test_json_create_dir)

    # Test Case 4: Attempt to Create Existing Directory
    print("\n--- Test Case 4: Attempt to Create Existing Directory ---")
    execute_semantic_json(test_json_create_dir) # Use the same JSON as above

    # Test Case 5: Unknown action (should trigger error_unknown_command)
    print("\n--- Test Case 5: Unknown Action ---")
    test_json_unknown_action_dispatch = """
    {
      "app": "someApp",
      "input": "userInput",
      "condition": "always_true",
      "action": "call('non_existent_action')"
    }
    """
    execute_semantic_json(test_json_unknown_action_dispatch)

    # Test Case 6: Invalid action format
    print("\n--- Test Case 6: Invalid Action Format ---")
    test_json_invalid_action_format = """
    {
      "app": "sensorMonitor",
      "input": "motion",
      "condition": "no_activity > 1s",
      "action": "do_something_else"
    }
    """
    simulated_system_state["last_activity_time"] = time.time() - 2
    execute_semantic_json(test_json_invalid_action_format)

    # Test Case 7: Invalid JSON
    print("\n--- Test Case 7: Invalid JSON ---")
    test_invalid_json = """
    {
      "app": "sensorMonitor",
      "input": "motion",
      "condition": "no_activity > 1s",
      "action": "call('lock_screen')"
    """ # Missing closing brace
    execute_semantic_json(test_invalid_json)

    # Test Case 8: Missing fields in JSON (app, input, or action)
    print("\n--- Test Case 8: Missing fields in JSON ---")
    test_missing_fields_json = """
    {
      "app": "sensorMonitor",
      "input": "motion"
    }
    """ # Missing action
    execute_semantic_json(test_missing_fields_json)
    
    # Test Case 9: JSON with no condition field (should default to always_true)
    print("\n--- Test Case 9: No Condition Field (Defaults to always_true) ---")
    test_json_no_condition = """
    {
      "app": "fileManager",
      "input": "userCommand",
      "action": "call('create_directory', '/home/user/another_folder')"
    }
    """
    # Ensure the folder does not exist from a previous partial run if applicable
    if '/home/user/another_folder' in simulated_system_state["files_and_dirs"]:
        simulated_system_state["files_and_dirs"].remove('/home/user/another_folder')
    execute_semantic_json(test_json_no_condition)


    print("\n--- End of Tests ---")
