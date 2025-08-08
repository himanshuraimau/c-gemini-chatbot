#ifndef AI_H
#define AI_H

#include <curl/curl.h>

// Define the struct completely in the header
struct ResponseData {
    char *data; // Response data
    size_t size; // Size of the response data
};

// Update function declaration to include history parameter
char* get_ai_response(const char* input, const char* history); // Function to get AI response
void cleanup_ai(); // Function to cleanup AI resources
void init_ai(); // Function to initialize AI resources

// Configuration setters
void ai_set_model(const char *model_name);
void ai_set_generation_params(double temperature, double top_p, int top_k, int max_output_tokens);
void ai_set_system_prompt(const char *prompt_text);
void ai_clear_system_prompt();

// Configuration getters
const char* ai_get_model();
double ai_get_temperature();
double ai_get_top_p();
int ai_get_top_k();
int ai_get_max_output_tokens();
const char* ai_get_system_prompt();

#endif
