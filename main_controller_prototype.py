import json
import time

# --- Copied from semantic_engine_prototype.py ---
simulated_system_state = {
    "last_activity_time": time.time(), # Will be manipulated for tests
    "screen_locked": False,
    "files_and_dirs": ["/home/user/", "/home/user/existing_file.txt", "/etc/", "/home/user/another_dir/"], # Updated
    "file_contents": {}, # Added
}

def call_lock_screen():
    """Simulates locking the screen."""
    print("[Semantic Engine] Executing: lock_screen")
    simulated_system_state["screen_locked"] = True
    print(f"[Semantic Engine] System state updated: screen_locked = {simulated_system_state['screen_locked']}")

def call_create_directory(path):
    """Simulates creating a directory."""
    print(f"[Semantic Engine] Executing: create_directory with path '{path}'")
    # Ensure path ends with a slash for consistency if it's a directory
    if not path.endswith('/'):
        path += '/'
    if path not in simulated_system_state["files_and_dirs"]:
        simulated_system_state["files_and_dirs"].append(path)
        print(f"[Semantic Engine] Directory '{path}' created.")
    else:
        print(f"[Semantic Engine] Directory '{path}' already exists.")

# New action functions
def call_list_files(path):
    """Simulates listing files in a given path."""
    print(f"[Semantic Engine] Executing: list_files for path '{path}'")
    if not path.endswith('/'): # Basic check if it's intended as a directory path
        print(f"[Semantic Engine] Error: Path '{path}' is not a directory path (must end with '/').")
        return
    if path not in simulated_system_state["files_and_dirs"] and path + '/' not in simulated_system_state["files_and_dirs"] :
        # Check if the directory itself is listed (e.g. /home/user/ is in files_and_dirs)
        is_known_dir = any(d.startswith(path) and d.endswith('/') for d in simulated_system_state["files_and_dirs"])
        if not is_known_dir and path not in simulated_system_state["files_and_dirs"]:
             print(f"[Semantic Engine] Error: Directory '{path}' not found or not listed as a directory.")
             return

    found_items = []
    for item in simulated_system_state["files_and_dirs"]:
        if item.startswith(path) and item != path: # item is under path and not the path itself
            found_items.append(item)
    # Also list files directly from file_contents that might be in this path
    for item_path in simulated_system_state["file_contents"].keys():
        if item_path.startswith(path) and item_path not in found_items:
            found_items.append(item_path)
    
    # Remove duplicates that might occur if a file path is in both lists
    found_items = sorted(list(set(found_items)))

    if found_items:
        print(f"[Semantic Engine] Files/directories in '{path}':")
        for item in found_items:
            print(f"- {item}")
    else:
        print(f"[Semantic Engine] No files or subdirectories found in '{path}'.")

def call_read_file(path):
    """Simulates reading a file."""
    print(f"[Semantic Engine] Executing: read_file for path '{path}'")
    if path in simulated_system_state["file_contents"]:
        print(f"[Semantic Engine] Content of '{path}':\n{simulated_system_state['file_contents'][path]}")
    else:
        print(f"[Semantic Engine] File not found or no content: '{path}'")

def call_write_file(path, content):
    """Simulates writing content to a file."""
    print(f"[Semantic Engine] Executing: write_file to path '{path}' with content '{content}'")
    simulated_system_state["file_contents"][path] = content
    if path not in simulated_system_state["files_and_dirs"]:
        simulated_system_state["files_and_dirs"].append(path) # Add path if it's a new file
    print(f"[Semantic Engine] Content written to '{path}'.")


ACTION_DISPATCHER = {
    "lock_screen": call_lock_screen,
    "create_directory": call_create_directory,
    "list_files": call_list_files,     # Added
    "read_file": call_read_file,       # Added
    "write_file": call_write_file,     # Added
    "error_unknown_command": lambda: print("[Semantic Engine] Executing: error_unknown_command. This command is not recognized by the system.")
}

def evaluate_condition(condition_str, current_input_value=None):
    """
    Evaluates a simplified condition string.
    Example: "no_activity > Xs" or "always_true"
    """
    # Print statement modified to show it's from Semantic Engine part
    print(f"[Semantic Engine] Evaluating condition: {condition_str} with input '{current_input_value}'")
    try:
        if condition_str == "always_true":
            return True
        elif "no_activity > " in condition_str:
            parts = condition_str.split(">")
            threshold_str = parts[1].strip().replace("s", "")
            threshold_seconds = int(threshold_str)

            # This uses the global simulated_system_state['last_activity_time']
            # which is set by the main controller for testing conditions
            no_activity_duration = time.time() - simulated_system_state["last_activity_time"]
            print(f"[Semantic Engine] Evaluating: no_activity ({no_activity_duration:.2f}s) > {threshold_seconds}s")
            return no_activity_duration > threshold_seconds
        else:
            print(f"[Semantic Engine] Warning: Unknown condition format: {condition_str}")
            return False
    except ValueError:
        print(f"[Semantic Engine] Error: Invalid threshold in condition string: {condition_str}")
        return False
    except Exception as e:
        print(f"[Semantic Engine] Error evaluating condition '{condition_str}': {e}")
        return False

def execute_semantic_json(json_str):
    """
    Parses a JSON string and executes the described action if the condition is met.
    """
    print(f"\n[Semantic Engine] Executing JSON: {json_str.strip()}")
    try:
        data = json.loads(json_str)
    except json.JSONDecodeError as e:
        print(f"[Semantic Engine] Error: Could not decode JSON string. Details: {e}")
        return

    app = data.get("app")
    current_input = data.get("input")
    condition = data.get("condition", "always_true")
    action_details_str = data.get("action")

    print(f"[Semantic Engine] Interpreting: App='{app}', Input='{current_input}', Condition='{condition}', Action='{action_details_str}'")

    if not all([app, current_input, action_details_str]):
        print("[Semantic Engine] Error: JSON is missing one or more required fields (app, input, action).")
        return

    condition_met = evaluate_condition(condition, current_input)

    if condition_met:
        print(f"[Semantic Engine] Condition '{condition}' is TRUE.")
        if action_details_str.startswith("call('") and action_details_str.endswith("')"):
            content = action_details_str[len("call('"):-len("')")]
            if "', '" in content:
                 parts = content.split("', '")
                 action_name = parts[0]
                 args = [arg.strip("'") for arg in parts[1:]] # Remove potential single quotes from args themselves
            else:
                 action_name = content
                 args = []
            print(f"[Semantic Engine] Parsed Action: Name='{action_name}', Args={args}")
            action_function = ACTION_DISPATCHER.get(action_name)
            if action_function:
                try:
                    action_function(*args)
                except TypeError as e:
                    print(f"[Semantic Engine] Error: Argument mismatch for action '{action_name}'. {e}")
                except Exception as e:
                    print(f"[Semantic Engine] Error executing action '{action_name}': {e}")
            else:
                error_action_fn = ACTION_DISPATCHER.get("error_unknown_command")
                if error_action_fn:
                    print(f"[Semantic Engine] Warning: Unknown action '{action_name}'. Triggering 'error_unknown_command'.")
                    error_action_fn()
                else:
                    print(f"[Semantic Engine] Critical Error: Unknown action '{action_name}' and 'error_unknown_command' is not available.")
        else:
            print(f"[Semantic Engine] Error: Invalid action format '{action_details_str}'. Expected format: \"call('action_name', 'arg1', ...)\".")
    else:
        print(f"[Semantic Engine] Condition '{condition}' is FALSE. No action taken.")

# --- Copied from llm_interface_prototype.py ---
# LLM_CPP_PATH and MODEL_PATH are not used in this simulation, so not copied.

def get_semantic_json_from_llm(natural_language_command: str) -> str | None:
    """
    Simulates getting a semantic JSON from an LLM based on a natural language command.
    """
    print(f"\n[LLM Interface] Received Natural Language Command: \"{natural_language_command}\"")
    prompt = f"""
You are an AI assistant for WhiteRails OS. Your task is to convert natural language commands into a specific semantic JSON format.
The JSON output must follow this schema:
{{
  "app": "string (application name, e.g., fileManager, sensorMonitor)",
  "input": "string (source of the input, e.g., userCommand, activitySensor)",
  "condition": "string (condition to evaluate, e.g., always_true, no_activity > Xs)",
  "action": "string (action to perform, e.g., call('action_name', 'arg1', ...))"
}}
Convert the following natural language command into this JSON format.
Natural Language Command: "{natural_language_command}"
Semantic JSON:
"""
    # print(f"\n[LLM Interface] Generated Prompt for LLM:\n{prompt}") # A bit verbose for main controller

    simulated_raw_json_output = ""
    # Using .lower() for more robust matching
    nl_command_lower = natural_language_command.lower()

    if "create a directory named reports" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('create_directory', '/home/user/reports/')"
}
"""
    elif "create a folder called 'projects'" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('create_directory', '/home/user/projects/')"
}
"""
    elif "lock the screen if there is no activity for 10 minutes" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "sensorMonitor",
  "input": "activitySensor",
  "condition": "no_activity > 600s",
  "action": "call('lock_screen')"
}
"""
    elif "lock the screen if there is no activity for 2 minutes" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "sensorMonitor",
  "input": "activitySensor",
  "condition": "no_activity > 120s",
  "action": "call('lock_screen')"
}
"""
    elif "list files in /home/user/" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('list_files', '/home/user/')"
}
"""
    elif "list files in /data/" in nl_command_lower: # For testing empty/non-existent dir
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('list_files', '/data/')"
}
"""
    elif "read file /home/user/existing_file.txt" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('read_file', '/home/user/existing_file.txt')"
}
"""
    elif "read file /home/user/non_existent_file.txt" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('read_file', '/home/user/non_existent_file.txt')"
}
"""
    elif "write 'hello from whiterails' to /home/user/new_doc.txt" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('write_file', '/home/user/new_doc.txt', 'hello from whiterails')"
}
"""
    elif "write 'this is updated content' to /home/user/existing_file.txt" in nl_command_lower:
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('write_file', '/home/user/existing_file.txt', 'this is updated content')"
}
"""
    elif "read file /home/user/new_doc.txt" in nl_command_lower: # To verify write
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('read_file', '/home/user/new_doc.txt')"
}
"""
    else: # Default for unknown commands like "what is the weather?"
        simulated_raw_json_output = """
{
  "app": "unknownApp",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('error_unknown_command')"
}
"""
    simulated_raw_json_output = simulated_raw_json_output.strip()
    print(f"[LLM Interface] Simulated Raw LLM Output:\n{simulated_raw_json_output}")

    try:
        parsed_json = json.loads(simulated_raw_json_output)
        formatted_json_string = json.dumps(parsed_json, indent=2) # Re-format for consistency
        print(f"[LLM Interface] Successfully parsed and formatted JSON.")
        return formatted_json_string
    except json.JSONDecodeError as e:
        print(f"[LLM Interface] Error: Could not decode simulated JSON output. Details: {e}")
        return None

# --- Main Controller Logic ---
if __name__ == "__main__":
    print("--- Main Controller Prototype ---")
    # Initialize file_contents for existing file
    simulated_system_state["file_contents"]["/home/user/existing_file.txt"] = "This is the original content of existing_file.txt."
    print(f"Initial System State: {simulated_system_state}")


    commands = [
        # Existing commands for context
        "Hey WhiteRails, please create a directory named reports in my home folder.",
        "WhiteRails, lock the screen if there is no activity for 10 minutes.", # Condition TRUE
        "WhiteRails, lock the screen if there is no activity for 2 minutes.",  # Condition FALSE
        "WhiteRails, what is the weather?",
        "Create a folder called 'projects'",
        # New file operation commands
        "list files in /home/user/",
        "list files in /data/", # Test non-existent/empty directory for list_files
        "read file /home/user/existing_file.txt",
        "read file /home/user/non_existent_file.txt",
        "write 'hello from whiterails' to /home/user/new_doc.txt",
        "read file /home/user/new_doc.txt", # Verify write to new file
        "write 'this is updated content' to /home/user/existing_file.txt", # Overwrite
        "read file /home/user/existing_file.txt"  # Verify overwrite
    ]

    for i, command_text in enumerate(commands):
        print(f"\n\n--- Processing Command {i+1}: \"{command_text}\" ---")
        # print(f"[Main Controller] Command: \"{command_text}\"") # Redundant with above print

        # Special handling for time-based conditions for testing
        # Reset last_activity_time for non-time-sensitive tests to avoid unintended locking
        simulated_system_state["last_activity_time"] = time.time() 

        if "10 minutes" in command_text:
            print("[Main Controller] Adjusting: last_activity_time for '10 minutes' test (to make condition TRUE)")
            simulated_system_state["last_activity_time"] = time.time() - 601 # 10 minutes and 1 second ago
            simulated_system_state["screen_locked"] = False # Ensure screen is unlocked for test
        elif "2 minutes" in command_text:
            print("[Main Controller] Adjusting: last_activity_time for '2 minutes' test (to make condition FALSE)")
            simulated_system_state["last_activity_time"] = time.time() - 30  # 30 seconds ago
            simulated_system_state["screen_locked"] = False # Ensure screen is unlocked for test

        semantic_json = get_semantic_json_from_llm(command_text)

        if semantic_json:
            execute_semantic_json(semantic_json)
        else:
            print("[Main Controller] Failed to get or parse semantic JSON from LLM.")

        print(f"[Main Controller] Current System State: {simulated_system_state}")

    print("\n\n--- End of Main Controller Prototype ---")
    print(f"Final System State: {simulated_system_state}")
