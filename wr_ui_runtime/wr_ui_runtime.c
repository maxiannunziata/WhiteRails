#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "../../wr_runtime/deps/cJSON/cJSON.h"

// Forward declarations
static void apply_window_properties(GtkWindow *window, cJSON *window_json);
static char* generate_html_content(cJSON *root_json);
static void script_message_received_callback(WebKitUserContentManager *manager, WebKitJavascriptResult *result, gpointer user_data);

// read_stdin_all and apply_window_properties remain the same as the previous version.

static char* read_stdin_all() {
    char buffer[1024];
    size_t content_size = 1; 
    char *content_ptr = malloc(content_size);
    if (content_ptr == NULL) {
        perror("Failed to allocate memory for content");
        return NULL;
    }
    content_ptr[0] = '\0'; 

    while (fgets(buffer, sizeof(buffer), stdin)) {
        char *old_content_ptr = content_ptr;
        content_size += strlen(buffer);
        content_ptr = realloc(content_ptr, content_size);
        if (content_ptr == NULL) {
            perror("Failed to reallocate memory for content");
            free(old_content_ptr);
            return NULL;
        }
        strcat(content_ptr, buffer);
    }

    if (ferror(stdin)) {
        perror("Error reading from stdin");
        free(content_ptr);
        return NULL;
    }
    return content_ptr;
}

static void apply_window_properties(GtkWindow *window, cJSON *window_json) {
    if (!GTK_IS_WINDOW(window)) return;

    const char* default_title = "wr_ui_runtime";
    int default_width = 800;
    int default_height = 600;
    gboolean default_fullscreen = TRUE; 

    if (cJSON_IsObject(window_json)) {
        cJSON *title_json = cJSON_GetObjectItemCaseSensitive(window_json, "title");
        if (cJSON_IsString(title_json) && (title_json->valuestring != NULL)) {
            gtk_window_set_title(window, title_json->valuestring);
        } else {
            gtk_window_set_title(window, default_title);
        }

        cJSON *width_json = cJSON_GetObjectItemCaseSensitive(window_json, "width");
        cJSON *height_json = cJSON_GetObjectItemCaseSensitive(window_json, "height");
        int w = cJSON_IsNumber(width_json) ? width_json->valueint : default_width;
        int h = cJSON_IsNumber(height_json) ? height_json->valueint : default_height;
        gtk_window_set_default_size(window, w, h);

        cJSON *fullscreen_json = cJSON_GetObjectItemCaseSensitive(window_json, "fullscreen");
        if (cJSON_IsBool(fullscreen_json)) {
            if (cJSON_IsTrue(fullscreen_json)) {
                gtk_window_fullscreen(window);
            } else {
                gtk_window_unfullscreen(window); 
            }
        } else { 
            if (default_fullscreen) gtk_window_fullscreen(window);
            else gtk_window_unfullscreen(window);
        }
    } else {
        gtk_window_set_title(window, default_title);
        gtk_window_set_default_size(window, default_width, default_height);
        if (default_fullscreen) gtk_window_fullscreen(window);
        else gtk_window_unfullscreen(window);
    }
}

// Modified generate_html_content to include JavaScript for event handling
static char* generate_html_content(cJSON *root_json) {
    if (root_json == NULL) { 
        return g_strdup("<html><head><title>Error</title></head><body><h1>Error: Invalid JSON data (NULL root) provided to HTML generator.</h1></body></html>");
    }

    GString *html = g_string_new("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">");

    cJSON *window_json = cJSON_GetObjectItemCaseSensitive(root_json, "window");
    cJSON *window_title_json = cJSON_IsObject(window_json) ? cJSON_GetObjectItemCaseSensitive(window_json, "title") : NULL;

    if (cJSON_IsString(window_title_json) && (window_title_json->valuestring != NULL)) {
        char* escaped_title = g_markup_escape_text(window_title_json->valuestring, -1);
        g_string_append_printf(html, "<title>%s</title>", escaped_title);
        g_free(escaped_title);
    } else {
        g_string_append(html, "<title>wr_ui_runtime</title>");
    }

    cJSON *custom_styles_json = cJSON_GetObjectItemCaseSensitive(root_json, "custom_styles");
    if (cJSON_IsString(custom_styles_json) && (custom_styles_json->valuestring != NULL)) {
        g_string_append_printf(html, "<style>%s</style>", custom_styles_json->valuestring);
    } else {
        g_string_append(html, "<style>body { font-family: sans-serif; margin: 0; padding:0; box-sizing: border-box; background-color: #2e3440; color: #d8dee9; } #whiterails_container { padding: 20px; } button { background-color: #5e81ac; color: #eceff4; padding: 10px 15px; margin: 5px; cursor: pointer; border-radius: 5px; border: none; } button:hover { background-color: #81a1c1; } input { background-color: #3b4252; color: #eceff4; padding: 10px; margin: 5px; border-radius: 5px; border: 1px solid #4c566a; } ::placeholder {color: #a3abbb;}</style>");
    }
    g_string_append(html, "</head><body><div id=\"whiterails_container\">");

    cJSON *components = cJSON_GetObjectItemCaseSensitive(root_json, "components");
    cJSON *component = NULL;
    if (cJSON_IsArray(components)) {
        cJSON_ArrayForEach(component, components) {
            cJSON *type = cJSON_GetObjectItemCaseSensitive(component, "type");
            cJSON *text_json = cJSON_GetObjectItemCaseSensitive(component, "text");
            cJSON *action = cJSON_GetObjectItemCaseSensitive(component, "action");
            cJSON *attributes = cJSON_GetObjectItemCaseSensitive(component, "attributes");

            GString *attrs_gstr = g_string_new("");
            if (cJSON_IsObject(attributes)) {
                cJSON* attr_node = NULL;
                cJSON_ArrayForEach(attr_node, attributes) { 
                    if (attr_node->string && cJSON_IsString(attr_node) && attr_node->valuestring) {
                        char* escaped_key = g_markup_escape_text(attr_node->string, -1);
                        char* escaped_value = g_markup_escape_text(attr_node->valuestring, -1);
                        g_string_append_printf(attrs_gstr, " %s=\"%s\"", escaped_key, escaped_value);
                        g_free(escaped_key);
                        g_free(escaped_value);
                    }
                }
            }
            
            if (cJSON_IsString(type) && (type->valuestring != NULL)) {
                char* escaped_text = cJSON_IsString(text_json) && text_json->valuestring ? g_markup_escape_text(text_json->valuestring, -1) : g_strdup("");
                
                if (strcmp(type->valuestring, "label") == 0) {
                    g_string_append_printf(html, "<p%s>%s</p>", attrs_gstr->str, escaped_text);
                } else if (strcmp(type->valuestring, "button") == 0) {
                    char* action_json_str = NULL;
                    if (action && cJSON_IsObject(action)) action_json_str = cJSON_PrintUnformatted(action);
                    else action_json_str = strdup("{}");
                    
                    char* escaped_action_json_str = g_markup_escape_text(action_json_str, -1);
                    g_string_append_printf(html, "<button%s data-action='%s'>%s</button>",
                                           attrs_gstr->str, escaped_action_json_str, escaped_text);
                    free(action_json_str); 
                    g_free(escaped_action_json_str);
                } else if (strcmp(type->valuestring, "input") == 0) {
                    cJSON *placeholder = cJSON_GetObjectItemCaseSensitive(component, "placeholder");
                    cJSON *initial_value = cJSON_GetObjectItemCaseSensitive(component, "initial_value");
                    char* escaped_placeholder = cJSON_IsString(placeholder) && placeholder->valuestring ? g_markup_escape_text(placeholder->valuestring, -1) : g_strdup("");
                    char* escaped_initial_value = cJSON_IsString(initial_value) && initial_value->valuestring ? g_markup_escape_text(initial_value->valuestring, -1) : g_strdup("");
                    
                    g_string_append_printf(html, "<input%s placeholder='%s' value='%s' />", 
                                           attrs_gstr->str, escaped_placeholder, escaped_initial_value);
                    g_free(escaped_placeholder);
                    g_free(escaped_initial_value);
                }
                g_free(escaped_text);
            }
            g_string_free(attrs_gstr, TRUE);
        }
    }
    g_string_append(html, "</div>"); // End of whiterails_container

    // Add JavaScript for button click handling
    g_string_append(html, "<script>\n"
                          "document.addEventListener('DOMContentLoaded', function() {\n"
                          "  const buttons = document.querySelectorAll('button[data-action]');\n"
                          "  buttons.forEach(button => {\n"
                          "    button.addEventListener('click', function() {\n"
                          "      const actionJsonString = this.getAttribute('data-action');\n"
                          "      if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.callbackHandler) {\n"
                          "        window.webkit.messageHandlers.callbackHandler.postMessage(actionJsonString);\n"
                          "      } else {\n"
                          "        console.error('WebKit message handler (callbackHandler) not found.');\n"
                          "      }\n"
                          "    });\n"
                          "  });\n"
                          "});\n"
                          "</script>");

    g_string_append(html, "</body></html>");
    return g_string_free(html, FALSE); 
}

// Callback function for script messages
static void script_message_received_callback(WebKitUserContentManager *manager, WebKitJavascriptResult *result, gpointer user_data) {
    JSCValue *value = webkit_javascript_result_get_js_value(result);
    if (jsc_value_is_string(value)) {
        gchar *message_str = jsc_value_to_string(value);
        fprintf(stdout, "%s\n", message_str);
        fflush(stdout); 
        g_free(message_str);
    } else {
        fprintf(stderr, "Received non-string message from JavaScript.\n");
    }
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget *window_widget;
    GtkWindow *window;
    WebKitWebView *web_view;
    WebKitUserContentManager *content_manager;
    
    char *json_input_gs = (char*)user_data; 
    cJSON *root_json = NULL;
    char *html_content = NULL;

    window_widget = gtk_application_window_new(app);
    window = GTK_WINDOW(window_widget);

    if (json_input_gs != NULL && strlen(json_input_gs) > 0) {
        root_json = cJSON_Parse(json_input_gs);
        if (root_json == NULL) {
            const char *error_ptr = cJSON_GetErrorPtr();
            GString* error_html = g_string_new("<html><head><title>JSON Parse Error</title>");
            g_string_append(error_html, "<style>body {font-family: sans-serif; background-color: #2e3440; color: #d8dee9;} pre {white-space: pre-wrap; word-wrap: break-word; background-color: #3b4252; padding: 10px; border-radius: 5px; border: 1px solid #4c566a;}</style></head><body>");
            g_string_append(error_html, "<h1>JSON Parsing Error</h1>");
            if (error_ptr) {
                char* escaped_error = g_markup_escape_text(error_ptr, -1);
                g_string_append_printf(error_html, "<p>Error details: <pre>%s</pre></p>", escaped_error);
                g_free(escaped_error);
            }
            char* escaped_input = g_markup_escape_text(json_input_gs, -1);
            g_string_append_printf(error_html, "<p>Problematic JSON input:</p><pre>%s</pre>", escaped_input);
            g_free(escaped_input);
            g_string_append(error_html, "</body></html>");
            html_content = g_string_free(error_html, FALSE);
            apply_window_properties(window, NULL); 
        } else {
            cJSON *window_json_obj = cJSON_GetObjectItemCaseSensitive(root_json, "window");
            apply_window_properties(window, window_json_obj); 
            html_content = generate_html_content(root_json); 
        }
    } else {
        html_content = g_strdup("<html><head><title>No Input</title></head><body><h1>Error: No JSON input was processed by activate.</h1></body></html>");
        apply_window_properties(window, NULL); 
    }

    content_manager = webkit_user_content_manager_new();
    g_signal_connect(content_manager, "script-message-received::callbackHandler", 
                     G_CALLBACK(script_message_received_callback), NULL);
    
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new_with_user_content_manager(content_manager));
    // Note: The call `webkit_user_content_manager_register_script_message_handler(content_manager, "callbackHandler");`
    // is often not needed with modern WebKitGTK when using the `script-message-received::handler-name` signal.
    // The existence of `window.webkit.messageHandlers.callbackHandler` in JS is the key, which this signal usually ensures.

    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(web_view));

    if (html_content) {
        webkit_web_view_load_html(web_view, html_content, NULL);
        g_free(html_content); 
    } else {
        webkit_web_view_load_html(web_view, "<html><body><h1>Critical Error: HTML content generation failed unexpectedly.</h1></body></html>", NULL);
    }
    
    if (root_json != NULL) { 
        cJSON_Delete(root_json);
    }

    gtk_widget_grab_focus(GTK_WIDGET(web_view));
    gtk_widget_show_all(window_widget);
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;
    char *json_input_raw = NULL; 
    char *json_input_for_activate = NULL; 

    json_input_raw = read_stdin_all();

    if (json_input_raw == NULL && feof(stdin) && !ferror(stdin)) { 
        fprintf(stdout, "INFO: No input from stdin. Using default JSON message.\n");
        const char* default_json_const = 
            "{ \"window\": { \"title\": \"WhiteRails UI - Default\", \"width\": 600, \"height\": 400, \"fullscreen\": false }, "
            "\"custom_styles\": \"body { text-align: center; padding-top: 50px; } h1 { color: #88c0d0; } p { color: #d8dee9; } button {font-size: 16px; }\", "
            "\"components\": [ "
                "{ \"type\": \"label\", \"text\": \"Welcome to WhiteRails UI Runtime!\", \"attributes\": { \"style\": \"font-size: 24px; margin-bottom: 20px;\" } }, "
                "{ \"type\": \"label\", \"text\": \"No specific UI definition was provided via stdin.\\nDisplaying this default interface.\" }, "
                "{ \"type\": \"button\", \"text\": \"Test Interaction\", \"action\": { \"type\": \"test_event\", \"payload\": \"Default button clicked!\" } } "
            "] }";
        json_input_for_activate = g_strdup(default_json_const);
    } else if (json_input_raw == NULL) { 
        fprintf(stderr, "ERROR: Failed to read JSON from stdin. Exiting.\n");
        return 1; 
    } else if (strlen(json_input_raw) == 0) { 
        fprintf(stderr, "ERROR: Received empty input from stdin. Expected a JSON string. Exiting.\n");
        free(json_input_raw);
        return 1;
    }
    else { 
        json_input_for_activate = g_strdup(json_input_raw);
    }

    app = gtk_application_new("org.whiterails.ui_runtime", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), json_input_for_activate); 
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    if (json_input_raw != NULL) { 
        free(json_input_raw); 
    }
    if (json_input_for_activate != NULL) { 
        g_free(json_input_for_activate); 
    }

    return status;
}
