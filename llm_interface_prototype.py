import json

# 1. Define placeholder constants
LLAMA_CPP_PATH = "/path/to/llama.cpp/main"  # Placeholder
MODEL_PATH = "/path/to/your/model.gguf"   # Placeholder

# 2. Implement get_semantic_json_from_llm()
def get_semantic_json_from_llm(natural_language_command: str) -> str | None:
    """
    Simulates getting a semantic JSON from an LLM based on a natural language command.
    """
    print(f"\nReceived Natural Language Command: \"{natural_language_command}\"")

    # Construct the prompt
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

Natural Language Command:
"{natural_language_command}"

Semantic JSON:
"""
    print(f"\nGenerated Prompt for LLM:\n{prompt}")

    # Simulate LLM Output based on keywords
    simulated_raw_json_output = ""
    if "create a directory named reports" in natural_language_command.lower():
        simulated_raw_json_output = """
{
  "app": "fileManager",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('create_directory', '/home/user/reports')"
}
"""
    elif "lock the screen if there is no activity for 10 minutes" in natural_language_command.lower():
        simulated_raw_json_output = """
{
  "app": "sensorMonitor",
  "input": "activitySensor",
  "condition": "no_activity > 600s",
  "action": "call('lock_screen')"
}
"""
    else:
        simulated_raw_json_output = """
{
  "app": "unknown",
  "input": "userCommand",
  "condition": "always_true",
  "action": "call('error_unknown_command')"
}
"""
    # Ensure the simulated output is stripped of leading/trailing whitespace
    simulated_raw_json_output = simulated_raw_json_output.strip()

    print(f"\nSimulated Raw LLM Output:\n{simulated_raw_json_output}")

    try:
        # Attempt to parse the simulated JSON output
        parsed_json = json.loads(simulated_raw_json_output)
        # Return the re-dumped JSON string with indentation
        formatted_json_string = json.dumps(parsed_json, indent=2)
        print(f"\nSuccessfully parsed and formatted JSON.")
        return formatted_json_string
    except json.JSONDecodeError as e:
        print(f"\nError: Could not decode simulated JSON output. Details: {e}")
        print(f"Problematic JSON string was: >>>{simulated_raw_json_output}<<<")
        return None

# 3. Include a test section
if __name__ == "__main__":
    print("--- LLM Interface Prototype ---")

    # Test Case 1
    command1 = "Hey WhiteRails, please create a directory named reports in my home folder."
    print(f"\n--- Test Case 1 ---")
    result1 = get_semantic_json_from_llm(command1)
    if result1:
        print(f"\nFinal Semantic JSON for Command 1:\n{result1}")
    else:
        print("\nFailed to get Semantic JSON for Command 1.")

    # Test Case 2
    command2 = "WhiteRails, lock the screen if there is no activity for 10 minutes."
    print(f"\n--- Test Case 2 ---")
    result2 = get_semantic_json_from_llm(command2)
    if result2:
        print(f"\nFinal Semantic JSON for Command 2:\n{result2}")
    else:
        print("\nFailed to get Semantic JSON for Command 2.")

    # Test Case 3
    command3 = "WhiteRails, what is the weather?"
    print(f"\n--- Test Case 3 ---")
    result3 = get_semantic_json_from_llm(command3)
    if result3:
        print(f"\nFinal Semantic JSON for Command 3:\n{result3}")
    else:
        print("\nFailed to get Semantic JSON for Command 3.")

    # Test Case 4: Simulate an LLM returning slightly malformed JSON (for testing error handling)
    # This test is more for a real LLM, but we can adapt the simulation
    print(f"\n--- Test Case 4 (Simulated Malformed JSON) ---")
    
    # Temporarily modify the function to simulate bad output for a specific command
    original_sim_logic = get_semantic_json_from_llm.__code__ 
    
    def get_semantic_json_from_llm_malformed_test(natural_language_command: str) -> str | None:
        # Original prompt generation and printing
        print(f"\nReceived Natural Language Command: \"{natural_language_command}\"")
        prompt = f"""
You are an AI assistant for WhiteRails OS. Your task is to convert natural language commands into a specific semantic JSON format.
The JSON output must follow this schema: {{ "app": "string", "input": "string", "condition": "string", "action": "string" }}
Convert the following natural language command into this JSON format.
Natural Language Command: "{natural_language_command}"
Semantic JSON:
"""
        print(f"\nGenerated Prompt for LLM:\n{prompt}")

        # Malformed output
        simulated_raw_json_output = """
        {
          "app": "testApp",
          "input": "testInput",
          "condition": "always_true",
          "action": "call('test_action')"  // Missing comma here
          "extra_field_unexpected": "value" 
        }""" # Trailing comma, or missing comma, or other syntax error
        simulated_raw_json_output = simulated_raw_json_output.strip() + " some extra text after json"


        print(f"\nSimulated Raw LLM Output (Malformed):\n{simulated_raw_json_output}")
        
        try:
            # Attempt to clean and parse
            # For this test, assume cleanup might involve finding the JSON block
            cleaned_json_str = simulated_raw_json_output
            if '{' in cleaned_json_str and '}' in cleaned_json_str:
                 start = cleaned_json_str.find('{')
                 end = cleaned_json_str.rfind('}') + 1
                 cleaned_json_str = cleaned_json_str[start:end]
            
            parsed_json = json.loads(cleaned_json_str)
            formatted_json_string = json.dumps(parsed_json, indent=2)
            print(f"\nSuccessfully parsed and formatted JSON (after cleanup).")
            return formatted_json_string
        except json.JSONDecodeError as e:
            print(f"\nError: Could not decode simulated JSON output. Details: {e}")
            print(f"Problematic JSON string was: >>>{cleaned_json_str}<<<")
            return None

    command4 = "Force a malformed JSON output."
    result4 = get_semantic_json_from_llm_malformed_test(command4)
    if result4:
        print(f"\nFinal Semantic JSON for Command 4:\n{result4}")
    else:
        print("\nFailed to get Semantic JSON for Command 4 (as expected for malformed test).")
    
    # Restore original function logic if needed for further tests, though not strictly necessary here
    # get_semantic_json_from_llm.__code__ = original_sim_logic

    print("\n--- End of LLM Interface Prototype Tests ---")
