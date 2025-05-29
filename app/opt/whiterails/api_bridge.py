import http.server
import socketserver
import subprocess
import json # Added for potential future use, though current spec returns raw output
import sys # Added for stderr printing

PORT = 8080
LLAMA_LOCAL_PATH = "/usr/local/bin/llama_local" # Assumed path from Phase 2
MODEL_PATH = "/models/gemma2b.gguf" # Assumed path from Phase 2

# The new prompt string
LLM_SYSTEM_PROMPT = "You are the WhiteRails LLM. Respond ONLY with valid JSON following the Semantic Service Schema. Valid action types: notify, shell, list_files, mkdir, run_command."

class PromptHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/prompt':
            content_length = int(self.headers['Content-Length'])
            post_data = self.rfile.read(content_length)
            user_query = post_data.decode('utf-8')

            # Construct the full prompt
            full_prompt = f"{LLM_SYSTEM_PROMPT}\nUser query: {user_query}" # Example construction

            try:
                # Call llama_local with timeout
                process = subprocess.run(
                    [LLAMA_LOCAL_PATH, "-m", MODEL_PATH, "-p", full_prompt], # Simplified call
                    capture_output=True,
                    text=True,
                    check=True, # This will raise CalledProcessError if llama_local returns non-zero
                    timeout=30  # Added timeout
                )
                raw_llm_output = process.stdout.strip()
                response_code = 200
                response_content_type = 'application/json'
                response_body = raw_llm_output
                print(f"[API_BRIDGE] Responded to /prompt with actual LLM output.")

            except subprocess.TimeoutExpired as e_timeout:
                print(f"[ERR][API_BRIDGE] Timeout calling llama_local ('{LLAMA_LOCAL_PATH}'): {e_timeout}", file=sys.stderr)
                response_code = 504 # Gateway Timeout
                response_content_type = 'text/plain'
                response_body = f"Error: Timeout calling LLM service: {e_timeout}"
            
            except (subprocess.CalledProcessError, FileNotFoundError) as e_process:
                # This block handles errors from llama_local (e.g., non-zero exit, not found)
                # and simulates a response, as per previous logic.
                print(f"[ERR][API_BRIDGE] Error with llama_local ('{LLAMA_LOCAL_PATH}'): {e_process}. Simulating LLM response.", file=sys.stderr)
                if "muéstrame el contenido de /etc" in user_query:
                    simulated_json_output = {
                        "name": "ShowEtcContent",
                        "condition": "always_true", 
                        "actions": [{"type": "list_files", "path": "/etc"}]
                    }
                elif "crea una carpeta /tmp/demo_llm" in user_query:
                    simulated_json_output = {
                        "name": "MakeDemoDirLLM",
                        "condition": "always_true",
                        "actions": [{"type": "mkdir", "path": "/tmp/demo_llm"}]
                    }
                else:
                    simulated_json_output = {
                        "name": "UnknownQueryResponse",
                        "condition": "always_true",
                        "actions": [{"type": "notify", "message": "Query not recognized by fallback simulator."}]
                    }
                response_code = 200 # Still send 200, but with simulated valid JSON
                response_content_type = 'application/json'
                response_body = json.dumps(simulated_json_output, indent=2)
                print(f"[API_BRIDGE] Responded to /prompt with SIMULATED LLM output: {response_body}")

            except Exception as e_generic: # Generic exception handler for other unexpected errors
                print(f"[ERR][API_BRIDGE] Unexpected error: {e_generic}", file=sys.stderr)
                response_code = 500
                response_content_type = 'text/plain'
                response_body = f"An unexpected error occurred: {e_generic}"

            # Send the determined response
            self.send_response(response_code)
            self.send_header('Content-type', response_content_type)
            self.end_headers()
            self.wfile.write(response_body.encode('utf-8'))
            # Logging of actual/simulated response is handled within the try/except blocks now
        else:
            self.send_response(404)
            self.end_headers()

if __name__ == "__main__":
    with socketserver.TCPServer(("", PORT), PromptHandler) as httpd:
        print(f"[API_BRIDGE] Serving on port {PORT}")
        httpd.serve_forever()
