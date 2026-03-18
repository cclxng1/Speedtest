#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

// A struct to hold the API response
struct MemoryBlock {
    char *response;
    size_t size;
};

// The callback function
static size_t WriteMemoryCallback(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryBlock *mem = (struct MemoryBlock *)userp;

    char *ptr = realloc(mem->response, mem->size + realsize + 1);
    if(!ptr) return 0; // out of memory!

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0; // null-terminate the string

    return realsize;
}

int main(void) {
    CURL *curl_handle;
    CURLcode res;
    struct MemoryBlock chunk = { .response = malloc(1), .size = 0 };

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if(curl_handle) {
        // Replace with your preferred Geolocation API
        curl_easy_setopt(curl_handle, CURLOPT_URL, "http://ip-api.com/json/");
        
        // Pass our callback function
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        
        // Pass our struct to the callback
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl_handle);

        if(res == CURLE_OK) {
            printf("Location Data: %s\n", chunk.response);
        }

        curl_easy_cleanup(curl_handle);
        free(chunk.response);
    }

    curl_global_cleanup();
    return 0;
}