#include <microhttpd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include "ai.h"
#include "linked_list.h"

#define PORT 8080 // Server port

struct PostContext {
    char *buffer; // Buffer to store POST data
    size_t size; // Size of the buffer
};

struct LinkedList *chat_history; // Chat history linked list

static enum MHD_Result handle_post_data(void *coninfo_cls, 
                                      enum MHD_ValueKind kind,
                                      const char *key,
                                      const char *filename,
                                      const char *content_type,
                                      const char *transfer_encoding,
                                      const char *data,
                                      uint64_t off,
                                      size_t size) {
    struct PostContext *context = (struct PostContext *)coninfo_cls;
    
    // Acknowledge unused parameters
    (void)kind;
    (void)key;
    (void)filename;
    (void)content_type;
    (void)transfer_encoding;
    (void)off;
    
    char *new_buffer = realloc(context->buffer, context->size + size + 1);
    if (!new_buffer)
        return MHD_NO;

    context->buffer = new_buffer;
    memcpy(context->buffer + context->size, data, size);
    context->size += size;
    context->buffer[context->size] = '\0';
    
    return MHD_YES;
}

static void request_completed(void *cls,
                            struct MHD_Connection *connection,
                            void **con_cls,
                            enum MHD_RequestTerminationCode toe) {
    // Acknowledge unused parameters
    (void)cls;
    (void)connection;
    (void)toe;
    
    struct PostContext *context = *con_cls;
    if (context) {
        if (context->buffer)
            free(context->buffer);
        free(context);
        *con_cls = NULL;
    }
}

static struct MHD_Response* json_response_from_obj(struct json_object *obj) {
    const char *response_str = json_object_to_json_string(obj);
    char *response_copy = strdup(response_str);
    return MHD_create_response_from_buffer(strlen(response_copy), (void*)response_copy, MHD_RESPMEM_MUST_FREE);
}

static enum MHD_Result handle_request(void *cls,
                          struct MHD_Connection *connection,
                          const char *url,
                          const char *method,
                          const char *version,
                          const char *upload_data,
                          size_t *upload_data_size,
                          void **con_cls) {
    // Acknowledge unused parameters
    (void)cls;
    (void)version;
    
    if (*con_cls == NULL) {
        struct PostContext *context = calloc(1, sizeof(struct PostContext));
        if (!context) return MHD_NO;
        *con_cls = context;
        return MHD_YES;
    }

    if (strcmp(method, "POST") == 0 && strcmp(url, "/chat") == 0) {
        struct PostContext *context = *con_cls;
        
        if (*upload_data_size != 0) {
            handle_post_data(*con_cls, 0, NULL, NULL, NULL, NULL,
                           upload_data, 0, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        }

        // Parse JSON request
        struct json_object *parsed_json = json_tokener_parse(context->buffer ? context->buffer : "{}");
        struct json_object *message_obj = NULL;
        json_object_object_get_ex(parsed_json, "message", &message_obj);
        const char *message = message_obj ? json_object_get_string(message_obj) : "";

        // Get chat history before adding new message
        char *history = get_chat_history(chat_history, 10); // Last 10 messages
        
        // Add user message to chat history
        add_message(chat_history, message);

        // Get AI response with history context
        char *ai_response = get_ai_response(message, history);
        free(history);
        
        // Add AI response to chat history
        add_message(chat_history, ai_response);

        // Create JSON response after getting AI response
        struct json_object *response_obj = json_object_new_object();
        json_object_object_add(response_obj, "response", json_object_new_string(ai_response));
        
        struct MHD_Response *response = json_response_from_obj(response_obj);

        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        
        int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        json_object_put(response_obj);
        json_object_put(parsed_json);
        free(ai_response);
        
        return ret;
    }

    // Config GET
    if (strcmp(method, "GET") == 0 && strcmp(url, "/config") == 0) {
        struct json_object *cfg = json_object_new_object();
        json_object_object_add(cfg, "model", json_object_new_string(ai_get_model()));
        json_object_object_add(cfg, "temperature", json_object_new_double(ai_get_temperature()));
        json_object_object_add(cfg, "top_p", json_object_new_double(ai_get_top_p()));
        json_object_object_add(cfg, "top_k", json_object_new_int(ai_get_top_k()));
        json_object_object_add(cfg, "max_output_tokens", json_object_new_int(ai_get_max_output_tokens()));
        json_object_object_add(cfg, "system_prompt", json_object_new_string(ai_get_system_prompt()));

        struct MHD_Response *response = json_response_from_obj(cfg);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        json_object_put(cfg);
        return ret;
    }

    // Config POST
    if (strcmp(method, "POST") == 0 && strcmp(url, "/config") == 0) {
        struct PostContext *context = *con_cls;
        if (*upload_data_size != 0) {
            handle_post_data(*con_cls, 0, NULL, NULL, NULL, NULL,
                           upload_data, 0, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        }
        struct json_object *parsed_json = json_tokener_parse(context->buffer ? context->buffer : "{}");
        struct json_object *model = NULL, *temperature = NULL, *top_p = NULL, *top_k = NULL, *max_out = NULL, *sys_prompt = NULL;
        if (json_object_object_get_ex(parsed_json, "model", &model)) {
            ai_set_model(json_object_get_string(model));
        }
        if (json_object_object_get_ex(parsed_json, "temperature", &temperature) ||
            json_object_object_get_ex(parsed_json, "top_p", &top_p) ||
            json_object_object_get_ex(parsed_json, "top_k", &top_k) ||
            json_object_object_get_ex(parsed_json, "max_output_tokens", &max_out)) {
            double t = temperature ? json_object_get_double(temperature) : ai_get_temperature();
            double p = top_p ? json_object_get_double(top_p) : ai_get_top_p();
            int k = top_k ? json_object_get_int(top_k) : ai_get_top_k();
            int m = max_out ? json_object_get_int(max_out) : ai_get_max_output_tokens();
            ai_set_generation_params(t, p, k, m);
        }
        if (json_object_object_get_ex(parsed_json, "system_prompt", &sys_prompt)) {
            const char *sp = json_object_get_string(sys_prompt);
            if (sp && strlen(sp) > 0) { ai_set_system_prompt(sp); } else { ai_clear_system_prompt(); }
        }

        struct json_object *ok = json_object_new_object();
        json_object_object_add(ok, "status", json_object_new_string("ok"));
        struct MHD_Response *response = json_response_from_obj(ok);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        json_object_put(ok);
        json_object_put(parsed_json);
        return ret;
    }

    // Clear chat history
    if (strcmp(method, "POST") == 0 && strcmp(url, "/clear") == 0) {
        // Free old list and create a new one
        free_list(chat_history);
        chat_history = create_list();
        struct json_object *ok = json_object_new_object();
        json_object_object_add(ok, "status", json_object_new_string("cleared"));
        struct MHD_Response *response = json_response_from_obj(ok);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        json_object_put(ok);
        return ret;
    }

    // Health check
    if (strcmp(method, "GET") == 0 && strcmp(url, "/health") == 0) {
        struct json_object *h = json_object_new_object();
        json_object_object_add(h, "status", json_object_new_string("ok"));
        struct MHD_Response *response = json_response_from_obj(h);
        MHD_add_response_header(response, "Content-Type", "application/json");
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        json_object_put(h);
        return ret;
    }

    struct MHD_Response *response;
    enum MHD_Result ret;
    
    // Handle CORS preflight
    if (strcmp(method, "OPTIONS") == 0) {
        response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
        MHD_add_response_header(response, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        return ret;
    }

    // Serve static files for GET requests
    if (strcmp(method, "GET") == 0) {
        const char *filename = url;
        if (strcmp(url, "/") == 0) {
            filename = "/index.html";
        }
        
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "../frontend%s", filename);
        
        FILE *file = fopen(filepath, "rb");
        if (!file) {
            const char *not_found = "404 Not Found";
            response = MHD_create_response_from_buffer(strlen(not_found),
                                                     (void*)not_found,
                                                     MHD_RESPMEM_PERSISTENT);
            ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
            MHD_destroy_response(response);
            return ret;
        }
        
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char *buffer = malloc(size);
        fread(buffer, 1, size, file);
        fclose(file);
        
        response = MHD_create_response_from_buffer(size, buffer, MHD_RESPMEM_MUST_FREE);
        
        // Set content type based on file extension
        if (strstr(filename, ".html")) {
            MHD_add_response_header(response, "Content-Type", "text/html");
        } else if (strstr(filename, ".css")) {
            MHD_add_response_header(response, "Content-Type", "text/css");
        } else if (strstr(filename, ".js")) {
            MHD_add_response_header(response, "Content-Type", "application/javascript");
        }
    } else {
        // Handle POST request for chat
        const char *json_response = "{\"response\": \"Hello from C server!\"}";
        response = MHD_create_response_from_buffer(strlen(json_response),
                                                 (void*)json_response,
                                                 MHD_RESPMEM_PERSISTENT);
        MHD_add_response_header(response, "Content-Type", "application/json");
    }
    
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

int main() {
    init_ai(); // Initialize AI
    chat_history = create_list(); // Create chat history list
    
    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, PORT, NULL, NULL,
                            &handle_request, NULL,
                            MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
                            MHD_OPTION_END);
    
    if (daemon == NULL) {
        return 1;
    }
    
    printf("Server running on port %d\n", PORT);
    getchar();
    
    MHD_stop_daemon(daemon);
    cleanup_ai(); // Cleanup AI
    free_list(chat_history); // Free chat history list
    return 0;
}
