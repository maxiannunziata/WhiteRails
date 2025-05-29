# WhiteRails

WhiteRails is a semantic runtime environment designed to understand and execute natural language commands on a Linux-based system. It bridges the gap between human language and system actions by leveraging a Large Language Model (LLM) to interpret commands and a native runtime to execute them securely and efficiently.

## Core Concepts

WhiteRails operates on a modular architecture designed to process natural language commands and translate them into system-level actions. The typical workflow is as follows:

1.  **Natural Language Input**: A user provides a command in natural language (e.g., "create a folder named 'documents'").
2.  **API Bridge (`api_bridge.py`)**: This Python-based HTTP server receives the natural language query.
3.  **LLM Processing**: The API Bridge forwards the query to a local Large Language Model (LLM). The LLM interprets the command and translates it into a structured **Semantic JSON** format. This JSON precisely defines the action to be performed, any conditions for its execution, and the necessary parameters.
4.  **`wr_runtime` (WhiteRails Runtime)**: The core C-based daemon, `wr_runtime`, continuously monitors for valid Semantic JSON definitions or receives them (future capability). These definitions can be pre-defined "services" or dynamically generated commands from the LLM.
5.  **Action Execution**: `wr_runtime` parses the Semantic JSON. If all conditions are met, it executes the specified actions using its built-in action dispatchers (e.g., creating a directory, running a shell command).

### Key Components:

*   **`wr_runtime`**: A lightweight, efficient C daemon that runs in the background. It is responsible for loading service definitions (JSON files), evaluating conditions, and executing the corresponding actions. It's designed to be robust and have minimal system overhead.
*   **`api_bridge.py`**: A Python HTTP server that acts as the primary interface for natural language input. It handles communication with the LLM (e.g., Gemma) to convert user queries into the Semantic JSON format that `wr_runtime` understands. It includes fallback mechanisms for simulated responses if the LLM is unavailable or fails.
*   **Service JSONs**: These are `.json` files (typically stored in `/var/lib/whiterails/services/`) that define pre-configured tasks or responses for `wr_runtime`. Each service JSON specifies a name, conditions for execution (e.g., time intervals, system states), and a sequence of actions to perform.
*   **LLM (Large Language Model)**: A local instance of a language model (e.g., Gemma). WhiteRails uses it via `llama_local` to understand the intent behind natural language commands and convert them into the structured Semantic JSON required by `wr_runtime`.

## Features (Version 1.0)

*   **Natural Language Command Processing**: Leverages a local Large Language Model (e.g., Gemma accessed via `llama_local`) to interpret user commands.
*   **Semantic JSON Representation**: Converts natural language into a structured JSON format that precisely defines actions and conditions.
*   **Core Runtime Actions**: The `wr_runtime` daemon supports a set of built-in actions:
    *   `notify`: Send a system notification (e.g., via `notify-send`).
    *   `shell`: Execute arbitrary shell commands.
    *   `list_files`: List contents of a specified directory.
    *   `mkdir`: Create a new directory.
    *   `run_command`: A more structured way to run specific, predefined commands (distinct from `shell` for broader execution).
*   **Service Definitions**: Users can define custom, persistent tasks or automated responses as JSON files located in `/var/lib/whiterails/services/`. These services are loaded by `wr_runtime` and can be triggered by time intervals or system conditions.
*   **Extensible Architecture**: While version 1.0 focuses on core functionality, the system is designed for future expansion with more actions and sophisticated condition evaluations.
*   **OpenRC Integration**: Provides an init script for managing `wr_runtime` as a service on systems using OpenRC (like Alpine Linux).

## Installation and Setup (Linux)

This section guides you through installing and configuring WhiteRails. While designed with Alpine Linux in mind (due to the provided `APKBUILD`), the components can be manually set up on other Linux distributions.

**Target Operating System:**

*   **Recommended:** Alpine Linux (for `APKBUILD` and OpenRC script).
*   **Other Linux Distributions:** Manual installation is possible. You may need to adapt init script management if not using OpenRC.

**Prerequisites:**

1.  **General Build Tools:**
    *   `git` (to clone the repository)
    *   `make`
    *   `gcc` (or a compatible C compiler)
    *   Python 3 (for `api_bridge.py`)
2.  **For Alpine Linux (if building the package or using OpenRC):**
    *   `alpine-sdk` (provides `abuild` and other development tools)
    *   `openrc` (for service management)
3.  **Large Language Model (LLM):**
    *   **`llama_local` Executable:** The `api_bridge.py` script assumes `llama_local` is available at `/usr/local/bin/llama_local`. If your installation path differs, you'll need to adjust the `LLAMA_LOCAL_PATH` variable in `api_bridge.py`.
    *   **LLM Model File:** A GGUF-formatted model compatible with `llama_local` is required. The `api_bridge.py` script defaults to `/models/gemma2b.gguf` (for Gemma 2B). You'll need to download a suitable model and place it at this path or update the `MODEL_PATH` variable in `api_bridge.py`.

---

**Part 1: Build and Install `wr_runtime` (Core C Daemon)**

**1. Clone the Repository:**
   ```bash
   git clone <repository_url> # Replace <repository_url> with the actual URL
   cd whiterails-project # Replace whiterails-project with the actual repo folder name
   ```

**2. Build `wr_runtime`:**
   ```bash
   cd wr_runtime
   make
   ```
   This will compile the C components and produce the `wr_runtime` executable.

**3. Install `wr_runtime` (Manual/Development Setup):**

   *   **Copy Binary:**
      ```bash
      sudo cp wr_runtime /usr/bin/wr_runtime
      ```
   *   **Copy Init Script (for OpenRC systems like Alpine):**
      ```bash
      sudo cp init.d/whiterails /etc/init.d/whiterails
      sudo chmod +x /etc/init.d/whiterails
      ```
   *   **Create Service Directory and Copy Example Services:**
      ```bash
      sudo mkdir -p /var/lib/whiterails/services
      sudo cp services/*.json /var/lib/whiterails/services/
      # Ensure correct permissions (readable by the user/group wr_runtime runs as)
      sudo chown -R root:root /var/lib/whiterails # Or a dedicated user/group
      sudo chmod 0644 /var/lib/whiterails/services/*.json
      ```
   *   **Enable and Start the Service (OpenRC):**
      ```bash
      sudo rc-update add whiterails default
      sudo rc-service whiterails start
      ```
   *   **Verify Service Status (OpenRC):**
      ```bash
      sudo rc-service whiterails status
      # Check logs (often in /var/log/daemon.log or via journalctl if systemd is present on a non-Alpine system)
      # On Alpine with default syslog-ng, check /var/log/messages or specific daemon logs.
      # wr_runtime logs via syslog using the 'wr_runtime' identifier.
      ```

---

**Part 2: Setup and Run `api_bridge.py` (LLM Interface)**

1.  **Navigate to the API Bridge Directory:**
    ```bash
    cd ../app/opt/whiterails # Adjust path if you are not in the wr_runtime directory
    ```
2.  **Ensure Python 3 is installed.**
3.  **(Optional but Recommended) Create and Activate a Virtual Environment:**
    ```bash
    python3 -m venv .venv
    source .venv/bin/activate
    # No external Python dependencies are listed in the current api_bridge.py,
    # but a venv is good practice.
    ```
4.  **Run the API Bridge Server:**
    ```bash
    python api_bridge.py
    ```
    The server will start and listen on port 8080 by default.
    ```
    [API_BRIDGE] Serving on port 8080
    ```
5.  **Keep the API Bridge Running:**
    For persistent operation, you should run `api_bridge.py` using a process manager like `supervisord`, as a systemd service (on non-Alpine systems), or within a `screen` or `tmux` session.

---

**Alternative: Installation using `APKBUILD` (Alpine Linux Only)**

If you are on Alpine Linux, you can build and install `wr_runtime` as a native Alpine package using the provided `APKBUILD` file.

1.  **Ensure `alpine-sdk` is installed:**
    ```bash
    sudo apk add alpine-sdk
    ```
2.  **Navigate to the directory containing the `APKBUILD` file** (root of the project).
3.  **Run `abuild`:**
    As a regular user (not root):
    ```bash
    # First time setup for abuild if you haven't used it before
    # abuild-keygen -a -i 
    abuild -r
    ```
    This will build the package (e.g., `wr-runtime-0.1-r0.apk`) in the packages directory (e.g., `/home/<user>/packages/`).
4.  **Install the Package:**
    ```bash
    sudo apk add --allow-untrusted /path/to/your/package/wr-runtime-*.apk
    ```
    This will install `wr_runtime`, the init script, and example services to their standard locations. You can then manage the service using OpenRC commands as shown above.

This completes the basic installation and setup for WhiteRails.

## How to Use / Test with Natural Language

Once `wr_runtime` is running as a service and `api_bridge.py` is active, you can interact with WhiteRails by sending natural language commands to the API Bridge.

**Interaction Method:**

*   Send a **POST** request to the `api_bridge.py` server.
*   **Endpoint:** `http://localhost:8080/prompt` (assuming `api_bridge.py` is running on the same machine and default port).
*   **Request Body:** Raw text containing your natural language command.
*   **`Content-Type` Header:** `text/plain`.

**Example using `curl`:**

```bash
curl -X POST -H "Content-Type: text/plain" --data "your natural language command here" http://localhost:8080/prompt
```

**Example Natural Language Commands and Expected LLM Output:**

The `api_bridge.py` script includes an `LLM_SYSTEM_PROMPT` that instructs the LLM on how to format its JSON output. The LLM should respond ONLY with valid JSON following the Semantic Service Schema. The key fields are: `name` (a descriptive name for the task), `condition` (when the task should run), and `actions` (an array of actions to perform).

Here are some examples:

1.  **List files in a directory:**
    *   Command: `"muéstrame el contenido de /etc"`
    *   `curl -X POST -H "Content-Type: text/plain" --data "muéstrame el contenido de /etc" http://localhost:8080/prompt`
    *   Expected-like LLM JSON output (which `wr_runtime` would then process):
        ```json
        {
          "name": "ShowEtcContent",
          "condition": "always_true",
          "actions": [{"type": "list_files", "path": "/etc"}]
        }
        ```

2.  **Create a directory:**
    *   Command: `"crea una carpeta /tmp/demo_llm"`
    *   `curl -X POST -H "Content-Type: text/plain" --data "crea una carpeta /tmp/demo_llm" http://localhost:8080/prompt`
    *   Expected-like LLM JSON output:
        ```json
        {
          "name": "MakeDemoDirLLM",
          "condition": "always_true",
          "actions": [{"type": "mkdir", "path": "/tmp/demo_llm"}]
        }
        ```

3.  **Execute a shell command:**
    *   Command: `"ejecuta el comando uptime"`
    *   `curl -X POST -H "Content-Type: text/plain" --data "ejecuta el comando uptime" http://localhost:8080/prompt`
    *   Expected-like LLM JSON output:
        ```json
        {
          "name": "ExecuteUptime",
          "condition": "always_true",
          "actions": [{"type": "shell", "command": "uptime"}]
        }
        ```
    *   *(Note: The actual `shell` action in `wr_runtime` might take parameters like `command` directly from the action object).*

4.  **Send a notification:**
    *   Command: `"notifícame que esto es una prueba"`
    *   `curl -X POST -H "Content-Type: text/plain" --data "notifícame que esto es una prueba" http://localhost:8080/prompt`
    *   Expected-like LLM JSON output:
        ```json
        {
          "name": "SendTestNotification",
          "condition": "always_true",
          "actions": [{"type": "notify", "message": "Esto es una prueba"}]
        }
        ```
    *   *(Note: The `notify` action in `wr_runtime` would take a `message` parameter).*

**Important Considerations for LLM Interaction:**

*   **LLM System Prompt:** The `api_bridge.py` uses the following system prompt for the LLM:
    `"You are the WhiteRails LLM. Respond ONLY with valid JSON following the Semantic Service Schema. Valid action types: notify, shell, list_files, mkdir, run_command."`
    The LLM's ability to generate correct and secure JSON based on this prompt is crucial.
*   **Simulation/Fallback:** `api_bridge.py` has some hardcoded simulation logic for specific queries if the LLM call fails or returns unexpected results. This is mainly for development and testing.
*   **Security:** Be mindful of the commands processed, especially those using the `shell` action type, as they can execute arbitrary commands on your system. The design assumes the LLM and the overall system setup provide a layer of safety, but caution is advised.

The output you receive from `curl` will be the JSON generated by the LLM (or the simulated JSON). `wr_runtime` does not directly respond to the HTTP request; it processes actions based on its service definitions and (in a more advanced setup) potentially from commands pushed to it by the API bridge. For now, the API bridge directly interprets the LLM output for some actions or would (in a full integration) translate this into a service file or command for `wr_runtime`.

## How to Define Custom Services for `wr_runtime`

The `wr_runtime` daemon can execute predefined tasks, called "services," which are defined in JSON files. These files are typically stored in the `/var/lib/whiterails/services/` directory. `wr_runtime` periodically scans this directory, loading new services and reloading updated ones.

**Service JSON File Structure:**

Each `.json` file in the service directory should define a single service with the following structure:

```json
{
  "name": "MyCustomService",
  "condition": "always_true",
  "interval_seconds": 3600,
  "actions": [
    {
      "type": "shell",
      "command": "echo 'Service executed at $(date)' >> /tmp/my_custom_service.log"
    },
    {
      "type": "notify",
      "message": "MyCustomService has just run!"
    }
  ]
}
```

**Fields Explained:**

*   **`name`** (string, required): A unique and descriptive name for the service. Used in logs.
*   **`condition`** (string, required): A condition string that `wr_runtime` evaluates to determine if the actions should be executed. Supported conditions include:
    *   `"always_true"`: The actions will run when the interval is met.
    *   `"no_activity > Xs"`: True if no system activity (as tracked by `wr_runtime`) has been detected for more than `X` seconds (e.g., `"no_activity > 300s"` for 5 minutes).
    *   `"file_exists /path/to/some/file"`: True if the specified file exists.
    *   `"file_not_exists /path/to/some/file"`: True if the specified file does not exist.
    *   *(More conditions may be added in the future.)*
*   **`interval_seconds`** (integer, required): The minimum time in seconds between potential executions of the service.
    *   If `0`, the service's condition is checked on every main loop cycle of `wr_runtime` (typically every second). The actions will run every time the condition is met.
    *   If greater than `0`, the service's condition is checked, and actions are run only if the condition is met AND at least `interval_seconds` have passed since the last execution.
*   **`actions`** (array of objects, required): An array of action objects to be executed in sequence if the condition is met and the interval has passed. Each action object must have a `type` field. Other fields depend on the action type:
    *   **`notify`**:
        *   `type`: `"notify"`
        *   `message`: (string) The message to display.
        *   Example: `{"type": "notify", "message": "Hello from WhiteRails!"}`
    *   **`shell`**:
        *   `type`: `"shell"`
        *   `command`: (string) The shell command to execute.
        *   Example: `{"type": "shell", "command": "touch /tmp/whiterails_was_here"}`
    *   **`list_files`**:
        *   `type`: `"list_files"`
        *   `path`: (string) The directory path whose contents should be listed (output currently goes to syslog).
        *   Example: `{"type": "list_files", "path": "/home/user/documents"}`
    *   **`mkdir`**:
        *   `type`: `"mkdir"`
        *   `path`: (string) The full path of the directory to create.
        *   Example: `{"type": "mkdir", "path": "/tmp/new_whiterails_dir"}`
    *   **`run_command`**:
        *   `type`: `"run_command"`
        *   `executable`: (string) The command or executable to run.
        *   `args`: (array of strings, optional) Arguments for the command.
        *   Example: `{"type": "run_command", "executable": "uptime"}`
        *   Example with args: `{"type": "run_command", "executable": "ls", "args": ["-l", "/var/log"]}`

**Creating a New Service:**

1.  Create a new `.json` file (e.g., `my_service.json`) in `/var/lib/whiterails/services/`.
2.  Define your service using the structure described above.
3.  Ensure the JSON is valid.
4.  `wr_runtime` should automatically pick up the new service during its next reload cycle (default is 60 seconds). You can also restart `wr_runtime` to force an immediate reload (`sudo rc-service whiterails restart`).

**Example Service: Hourly Backup Reminder**

```json
{
  "name": "HourlyBackupReminder",
  "condition": "always_true",
  "interval_seconds": 3600,
  "actions": [
    {
      "type": "notify",
      "message": "Don't forget to backup your important files!"
    }
  ]
}
```
Save this as `/var/lib/whiterails/services/backup_reminder.json`. `wr_runtime` will then trigger this notification approximately every hour.

## Project Structure

Here's a brief overview of the key directories and files in the WhiteRails project:

```
.
├── APKBUILD              # Alpine Linux package build script for wr_runtime
├── app/
│   └── opt/
│       └── whiterails/
│           ├── api_bridge.py        # Python HTTP server for LLM interface
│           ├── rails/               # (Likely Python components for semantic interpretation - Phase 1)
│           ├── semantic_ops_test.sh # Test script for semantic operations
│           └── services/            # (Likely Python service definitions or clients - Phase 1)
├── build/                # Build artifacts (e.g., packaging output for wr_runtime)
├── llm_interface_prototype.py # Python prototype for LLM interaction
├── main_controller_prototype.py # Python prototype simulating overall control flow
├── semantic_engine_prototype.py # Python prototype for executing semantic JSON
├── wr_runtime/           # Source code and build files for the C-based runtime
│   ├── Makefile            # Makefile for building wr_runtime
│   ├── deps/               # Dependencies (e.g., cJSON library)
│   ├── include/            # Header files for wr_runtime
│   ├── init.d/             # OpenRC init script for wr_runtime service
│   │   └── whiterails
│   ├── services/           # Example JSON service definitions for wr_runtime
│   │   ├── ls_home.json
│   │   ├── make_testdir.json
│   │   └── uptime_cmd.json
│   ├── src/                # Source code for wr_runtime (main.c, dispatcher.c, etc.)
│   └── wr_runtime          # Compiled wr_runtime binary (output of make)
└── README.md             # This file
```

*   **`app/opt/whiterails/`**: Contains the Python components, primarily the `api_bridge.py` which interfaces with the LLM.
*   **`wr_runtime/`**: Contains all C source code, build scripts (`Makefile`), dependencies (`deps/cJSON`), and service management scripts (`init.d/whiterails`) for the core runtime daemon.
    *   **`wr_runtime/src/`**: The C source files for the daemon's logic.
    *   **`wr_runtime/services/`**: Sample JSON files that define services for `wr_runtime`. These are typically copied to `/var/lib/whiterails/services/` in a deployed system.
*   **Prototype files (`*.py` in root)**: Python scripts used for early prototyping of the LLM interface, semantic engine, and main controller logic.
*   **`APKBUILD`**: Used to package `wr_runtime` for Alpine Linux.

## Contributing

Contributions to WhiteRails are welcome! If you're interested in improving the project, please consider the following:

*   **Reporting Bugs:** If you find a bug, please open an issue on the project's issue tracker, providing as much detail as possible.
*   **Suggesting Enhancements:** Have an idea for a new feature or an improvement to an existing one? Open an issue to discuss it.
*   **Code Contributions:**
    1.  Fork the repository.
    2.  Create a new branch for your feature or bug fix.
    3.  Make your changes, adhering to the existing code style.
    4.  If adding new functionality, especially to `wr_runtime`, consider adding corresponding tests or examples.
    5.  Ensure your changes do not break existing functionality.
    6.  Submit a pull request with a clear description of your changes.

We are particularly interested in contributions related to:

*   New action types for `wr_runtime`.
*   More sophisticated condition evaluations.
*   Improvements to the LLM prompting and JSON generation.
*   Enhanced security and sandboxing for action execution.
*   Support for other init systems or packaging formats.

## License

This project is licensed under the **MIT License**.

The MIT License is a permissive free software license originating at the Massachusetts Institute of Technology (MIT). As a permissive license, it puts only very limited restriction on reuse and has, therefore, high license compatibility.

A copy of the MIT License text can typically be found in a `LICENSE` file in the repository. If not present, the general terms of the MIT License apply:

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
