# JSON UI Schema for wr_ui_runtime

This document outlines the structure of the JSON that `wr_ui_runtime` expects as input to render a user interface.

## Root Object

The root of the JSON object should contain the following properties:

- `window`: An object defining the properties of the main application window.
- `components`: An array of objects, each defining a UI element to be rendered.
- `custom_styles`: (Optional) A string containing CSS rules to be injected into the `<head>` of the generated HTML document.

## `window` Object

| Property     | Type    | Required | Default | Description                                  |
|--------------|---------|----------|---------|----------------------------------------------|
| `title`      | string  | Yes      | N/A     | The title of the application window.         |
| `width`      | integer | No       | 800     | The initial width of the window in pixels.   |
| `height`     | integer | No       | 600     | The initial height of the window in pixels.  |
| `fullscreen` | boolean | No       | `true`  | Whether the window should start in fullscreen. |

**Example:**

```json
"window": {
  "title": "My Dynamic UI",
  "width": 1024,
  "height": 768,
  "fullscreen": false
}
```

## `components` Array

This is an array of UI component objects. Each component object must have a `type` property.

### Common Properties for all Components

| Property     | Type   | Required | Description                                       |
|--------------|--------|----------|---------------------------------------------------|
| `type`       | string | Yes      | The type of the component (e.g., "label", "button"). |
| `attributes` | object | No       | An object containing HTML attributes (key-value pairs) like `id`, `class`, `style`. |

### Component: `label`

Displays static text.

| Property | Type   | Required | Description              |
|----------|--------|----------|--------------------------|
| `text`   | string | Yes      | The text to be displayed. |

**Example:**

```json
{
  "type": "label",
  "text": "Welcome to WhiteRails UI",
  "attributes": {
    "style": "font-size: 24px; color: #333;"
  }
}
```

### Component: `button`

A clickable button that can trigger an action.

| Property | Type   | Required | Description                                                                 |
|----------|--------|----------|-----------------------------------------------------------------------------|
| `text`   | string | Yes      | The text displayed on the button.                                           |
| `action` | object | Yes      | An object defining the action to be performed when the button is clicked. See `action` Object section. |

**Example:**

```json
{
  "type": "button",
  "text": "Execute Task",
  "action": {
    "type": "shell",
    "command": "echo 'Button clicked!'"
  },
  "attributes": {
    "class": "action-button"
  }
}
```

### Component: `input`

An input field for text entry.

| Property          | Type   | Required | Default | Description                                                                    |
|-------------------|--------|----------|---------|--------------------------------------------------------------------------------|
| `placeholder`     | string | No       | ""      | Placeholder text for the input field.                                          |
| `initial_value`   | string | No       | ""      | Initial value of the input field.                                              |
| `action_on_enter` | object | No       | N/A     | (Optional) An `action` object to be triggered when the Enter key is pressed. |
| `attributes`      | object | No       |         | HTML attributes. The `type` attribute (e.g., 'text', 'password') defaults to 'text'. |

**Example:**

```json
{
  "type": "input",
  "placeholder": "Enter command...",
  "initial_value": "ls -l /",
  "action_on_enter": {
    "type": "submit_input",
    "target_id": "my_input_field" // To identify which input if multiple exist
  },
  "attributes": {
    "id": "my_input_field",
    "type": "text"
  }
}
```

## `action` Object

This object defines an action to be performed, typically by a button click or input submission. The entire `action` object is converted to a JSON string and printed to `stdout` by `wr_ui_runtime` when triggered.

| Property | Type   | Required | Description                                                                 |
|----------|--------|----------|-----------------------------------------------------------------------------|
| `type`   | string | Yes      | The type of action (e.g., "shell", "custom_event", "submit_input"). This determines how the receiver (e.g., `api_bridge.py` or `wr_runtime`) should interpret the action. |
| `...`    | ...    | Varies   | Additional properties specific to the action `type`. For example, a "shell" action would include a `command` property. An "submit_input" action might include the `value` of the input field and its `id`. |

**Example `action` for a shell command:**

```json
{
  "type": "shell",
  "command": "notify-send 'Action Triggered'"
}
```

**Example `action` for submitting input (hypothetical, `wr_ui_runtime` would add the value):**

```json
// Action defined in input JSON for an input field
{
  "type": "user_input",
  "inputId": "command_input"
}

// Actual JSON sent to stdout by wr_ui_runtime after user types 'hello' and presses Enter
{
  "type": "user_input",
  "inputId": "command_input",
  "value": "hello"
}
```

## `custom_styles` String

A string containing any valid CSS. This CSS will be placed inside a `<style>` tag in the `<head>` of the generated HTML document.

**Example:**

```json
"custom_styles": "body { background-color: #f0f0f0; margin: 0; padding: 20px; } .error { color: red; }"
```

## Full Example JSON

```json
{
  "window": {
    "title": "WhiteRails Control Panel",
    "width": 600,
    "height": 400
  },
  "custom_styles": "button { padding: 10px; margin: 5px; }",
  "components": [
    {
      "type": "label",
      "text": "Main Operations"
    },
    {
      "type": "button",
      "text": "List Files in /home",
      "action": {
        "type": "list_files",
        "path": "/home"
      }
    },
    {
      "type": "input",
      "placeholder": "Enter a shell command",
      "attributes": { "id": "shell_command_input" },
      "action_on_enter": {
        "type": "shell_command_entered",
        "inputId": "shell_command_input"
      }
    },
    {
      "type": "button",
      "text": "Submit Command",
      "action": {
        "type": "shell_command_from_input",
        "source_input_id": "shell_command_input"
      },
      "attributes": {
        "class": "submit-button"
      }
    }
  ]
}

```
This schema provides a good starting point for the `wr_ui_runtime`. It's extensible for new component types and actions in the future.
