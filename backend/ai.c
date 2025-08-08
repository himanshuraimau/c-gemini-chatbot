#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "ai.h"
#include <json-c/json.h>

#define CHAT_INPUT_MAX 20480 // Max input size
#define MAX_HISTORY 40000 // Max history size
#define API_MAX_TOKENS 307200 // API max tokens


// Define the struct completely in the source file

size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size *nmemb;
    struct ResponseData *resp = (struct ResponseData *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if(!ptr) {
        printf("Memory allocation failed!\n");
        return 0;
    }
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

// Runtime configuration state
static char current_model[128] = "gemini-1.5-pro";
static double current_temperature = 0.7;
static double current_top_p = 1.0;
static int current_top_k = 64;
static int current_max_output_tokens = 2048;
static char *system_prompt = NULL;

void ai_set_model(const char *model_name) {
    if (!model_name) return;
    strncpy(current_model, model_name, sizeof(current_model) - 1);
    current_model[sizeof(current_model) - 1] = '\0';
}

void ai_set_generation_params(double temperature, double top_p, int top_k, int max_output_tokens) {
    if (temperature < 0.0) temperature = 0.0;
    if (temperature > 2.0) temperature = 2.0;
    if (top_p < 0.0) top_p = 0.0;
    if (top_p > 1.0) top_p = 1.0;
    if (top_k < 1) top_k = 1;
    if (max_output_tokens < 1) max_output_tokens = 1;
    if (max_output_tokens > API_MAX_TOKENS) max_output_tokens = API_MAX_TOKENS;

    current_temperature = temperature;
    current_top_p = top_p;
    current_top_k = top_k;
    current_max_output_tokens = max_output_tokens;
}

void ai_set_system_prompt(const char *prompt_text) {
    if (system_prompt) {
        free(system_prompt);
        system_prompt = NULL;
    }
    if (prompt_text && prompt_text[0] != '\0') {
        system_prompt = strdup(prompt_text);
    }
}

void ai_clear_system_prompt() {
    if (system_prompt) {
        free(system_prompt);
        system_prompt = NULL;
    }
}

const char* ai_get_model() { return current_model; }
double ai_get_temperature() { return current_temperature; }
double ai_get_top_p() { return current_top_p; }
int ai_get_top_k() { return current_top_k; }
int ai_get_max_output_tokens() { return current_max_output_tokens; }
const char* ai_get_system_prompt() { return system_prompt ? system_prompt : ""; }

// Function to get user input
void get_user_input(char *buffer, int max_size) {
    printf("You: ");
    fgets(buffer, max_size, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
}

// Build JSON payload with optional history and system prompt and generationConfig
static char* create_json_payload(const char *input, const char *history) {
    // Create JSON objects
    struct json_object *root = json_object_new_object();

    // contents array
    struct json_object *contents = json_object_new_array();

    // If system prompt is set, add as the first part
    if (system_prompt && system_prompt[0] != '\0') {
        struct json_object *sys_content = json_object_new_object();
        struct json_object *sys_parts = json_object_new_array();
        struct json_object *sys_text_part = json_object_new_object();
        json_object_object_add(sys_text_part, "text", json_object_new_string(system_prompt));
        json_object_array_add(sys_parts, sys_text_part);
        json_object_object_add(sys_content, "role", json_object_new_string("user"));
        json_object_object_add(sys_content, "parts", sys_parts);
        json_object_array_add(contents, sys_content);
    }

    // Add combined history + current input as a single content
    char *combined_text = NULL;
    size_t hist_len = history ? strlen(history) : 0;
    size_t input_len = input ? strlen(input) : 0;
    size_t combined_len = hist_len + input_len + 64;
    combined_text = (char*)malloc(combined_len);
    if (!combined_text) {
        json_object_put(root);
        return strdup("Memory allocation error");
    }
    if (hist_len > MAX_HISTORY) {
        history = history + hist_len - MAX_HISTORY;
        hist_len = strlen(history);
    }
    snprintf(combined_text, combined_len, "Previous conversation:\n%s\nUser: %s", history ? history : "", input ? input : "");

    struct json_object *user_content = json_object_new_object();
    struct json_object *user_parts = json_object_new_array();
    struct json_object *user_text_part = json_object_new_object();
    json_object_object_add(user_text_part, "text", json_object_new_string(combined_text));
    json_object_array_add(user_parts, user_text_part);
    json_object_object_add(user_content, "parts", user_parts);
    json_object_array_add(contents, user_content);
    free(combined_text);

    json_object_object_add(root, "contents", contents);

    // generationConfig
    struct json_object *gen = json_object_new_object();
    json_object_object_add(gen, "temperature", json_object_new_double(current_temperature));
    json_object_object_add(gen, "topP", json_object_new_double(current_top_p));
    json_object_object_add(gen, "topK", json_object_new_int(current_top_k));
    json_object_object_add(gen, "maxOutputTokens", json_object_new_int(current_max_output_tokens));
    json_object_object_add(root, "generationConfig", gen);

    const char *json_c_str = json_object_to_json_string(root);
    char *json_payload = strdup(json_c_str);
    json_object_put(root);
    return json_payload;
}

void init_ai() {
    curl_global_init(CURL_GLOBAL_ALL); // Initialize CURL
}

void cleanup_ai() {
    curl_global_cleanup(); // Cleanup CURL
    ai_clear_system_prompt();
}

// Update get_ai_response to use history
char* get_ai_response(const char* input, const char* history) {
    CURL *curl;
    CURLcode res;
    struct ResponseData resp;
    const char *api_key_env = getenv("GEMINI_API_KEY");
    const char *api_key = api_key_env ? api_key_env : ""; // API key from env
    char url[256];
    
    resp.data = malloc(1);
    resp.size = 0;

    snprintf(url, sizeof(url), 
        "https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s",
        current_model, api_key);
    
    curl = curl_easy_init();
    if(!curl) {
        free(resp.data);
        return strdup("Error initializing CURL");
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    char *json_data = create_json_payload(input, history);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
    
    res = curl_easy_perform(curl);
    
    
    printf("Raw API Response:\n%s\n", resp.data);
    
    free(json_data);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if(res != CURLE_OK) {
        char *error = strdup(curl_easy_strerror(res));
        free(resp.data);
        return error;
    }

    // Parse the JSON response to extract the actual message
    struct json_object *parsed_json = json_tokener_parse(resp.data);
    if (!parsed_json) {
        free(resp.data);
        return strdup("Error parsing JSON response");
    }

    // Navigate through the JSON structure
    struct json_object *candidates = NULL, *first_candidate = NULL, *content = NULL, *parts = NULL, *first_part = NULL, *text = NULL;
    
    if (!json_object_object_get_ex(parsed_json, "candidates", &candidates) ||
        json_object_get_type(candidates) != json_type_array ||
        json_object_array_length(candidates) == 0) {
        const char *err = json_object_to_json_string_ext(parsed_json, JSON_C_TO_STRING_PRETTY);
        json_object_put(parsed_json);
        free(resp.data);
        return strdup(err);
    }

    first_candidate = json_object_array_get_idx(candidates, 0);
    json_object_object_get_ex(first_candidate, "content", &content);
    json_object_object_get_ex(content, "parts", &parts);
    first_part = json_object_array_get_idx(parts, 0);
    json_object_object_get_ex(first_part, "text", &text);

    // Get the actual response text
    const char *response_text = json_object_get_string(text);
    char *final_response = strdup(response_text ? response_text : "");

    // Cleanup
    json_object_put(parsed_json);
    free(resp.data);

    return final_response;
}