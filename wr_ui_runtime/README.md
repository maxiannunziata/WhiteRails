# WhiteRails UI Runtime (wr_ui_runtime)

`wr_ui_runtime` is a standalone binary that renders a dynamic user interface based on JSON definitions. It is designed to be controlled by an LLM or other processes that generate these UI definitions in real-time. The UI is rendered using WebKitGTK, allowing for HTML, CSS, and JavaScript-based interfaces.

This component is part of the WhiteRails project, aiming to create an AI-controlled generative interface.

## Features

- Renders UI from JSON piped to `stdin`.
- Uses GTK and WebKitGTK for rendering HTML/CSS/JS.
- Supports dynamic components like labels, buttons, and input fields.
- Handles actions from UI elements (e.g., button clicks) by printing action JSON to `stdout`.
- Configurable window properties (title, size, fullscreen) via input JSON.
- Supports custom CSS styling.

## Building `wr_ui_runtime`

A `Makefile` is provided to build the executable. Dependencies include:
- `gcc` (or any C compiler)
- `pkg-config`
- `gtk+-3.0` libraries and development files
- `webkit2gtk-4.0` libraries and development files (version 4.0 is common, adjust if using 4.1 or other)
- `cJSON` (included as a submodule/dependency in `../wr_runtime/deps/cJSON/`)

To build, navigate to the `wr_ui_runtime/` directory and run:
```bash
make
```
This will produce the `wr_ui_runtime` executable in the current directory.

To clean the build files:
```bash
make clean
```

## Running `wr_ui_runtime`

`wr_ui_runtime` expects a JSON string describing the UI to be piped to its standard input.

Example:
```bash
cat example_ui.json | ./wr_ui_runtime
```

The format of the JSON input is crucial. Refer to the [JSON UI Schema documentation](./docs/JSON_UI_SCHEMA.md) for detailed information on the expected structure and available components.

When UI elements like buttons are interacted with, `wr_ui_runtime` will print the corresponding action JSON object as a single line to its standard output (`stdout`). This output can be consumed by another process (e.g., `api_bridge.py`) to trigger further actions.

## Testing

A basic test script and sample JSON are provided:
- `test.json`: A sample UI definition.
- `test.sh`: A script to build and run `wr_ui_runtime` with `test.json`.

To run the test:
```bash
cd wr_ui_runtime/
./test.sh
```
The script will provide instructions for manually verifying button click functionality by observing the terminal output.

## JSON UI Schema

The detailed specification for the JSON format that `wr_ui_runtime` expects can be found in:
[./docs/JSON_UI_SCHEMA.md](./docs/JSON_UI_SCHEMA.md)

This schema defines:
- Window properties (`title`, `width`, `height`, `fullscreen`).
- UI components (`label`, `button`, `input`, etc.) and their properties.
- Action objects for interactive elements.
- Custom styling options.
