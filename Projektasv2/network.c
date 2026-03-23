#include "network.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

size_t callback(void *ptr, size_t size, size_t nmemb, void *userdata){
    size_t bytes = size * nmemb;
    Response *resp = (Response *) userdata;
    char *temp = realloc(resp -> data, resp -> size + bytes + 1);
    if(!temp) return 0;
    resp -> data = temp;
    memcpy(resp -> data + resp -> size, ptr, bytes);
    resp -> size += bytes;
    resp ->data[resp->size] = '\0';
    return bytes;
}

size_t download_callback(void *ptr, size_t size, size_t nmemb, void *userdata){
    size_t bytes = size * nmemb;
    size_t *total = (size_t *) userdata;
    *total += bytes;
    return bytes;
}

size_t upload_callback(void *ptr, size_t size, size_t nmemb, void *userdata){
    UploadData *upload = (UploadData *) userdata;
    size_t buffer_size = size * nmemb;
    size_t remaining = upload -> total - upload -> sent;

    if (remaining == 0) return 0;

    size_t to_send = remaining < buffer_size ? remaining : buffer_size;
    memset(ptr, 'T', to_send);
    upload -> sent += to_send;
    return to_send;
}

size_t discard_callback(void *ptr, size_t size, size_t nmemb, void *usedata){
    return size * nmemb;
}

void detect_location(char *country, char *city){
    CURL *curl = curl_easy_init();
    if(!curl){
        printf("Error with curl\n");
        return;
    }
    Response resp = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json/");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    CURLcode res = curl_easy_perform(curl);
    if(res != CURLE_OK){
        printf("Failed to detect location: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return;
    }
    curl_easy_cleanup(curl);

    if(!resp.data){
        printf("Failed to get location data\n");
        return;
    }

    cJSON *json = cJSON_Parse(resp.data);
    if(json){
        cJSON *c = cJSON_GetObjectItem(json, "country");
        cJSON *ci = cJSON_GetObjectItem(json, "city");
        strcpy(country, c->valuestring);
        strcpy(city, ci->valuestring);
        cJSON_Delete(json);
    }
    free(resp.data);
}

double download_test(const char *host){
    CURLM *multi = curl_multi_init();
    int num_connections = 4;
    int running = 0;
    CURL *handles[4];
    size_t bytes[4] = {0, 0, 0, 0};
    char url[512];
    snprintf(url, sizeof(url), "http://%s/download?size=25000000", host);
    
    for(int i=0; i < num_connections; i++){
        handles[i] = curl_easy_init();
        curl_easy_setopt(handles[i], CURLOPT_URL, url);
        curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, download_callback);
        curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &bytes[i]);
        curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(handles[i], CURLOPT_USERAGENT, "TeltonikaTest");
        curl_easy_setopt(handles[i], CURLOPT_FOLLOWLOCATION, 1L);
        curl_multi_add_handle(multi, handles[i]);
    }

    do {
        curl_multi_perform(multi, &running);
        curl_multi_wait(multi, NULL, 0, 100, NULL);
    } while (running > 0);

    double total_time = 0;
    curl_easy_getinfo(handles[0], CURLINFO_TOTAL_TIME, &total_time);

    if(total_time == 0) {
        printf("Speed test failed: no data received\n");
        for(int i=0; i<num_connections; i++){
            curl_multi_remove_handle(multi, handles[i]);
            curl_easy_cleanup(handles[i]);
        }
        curl_multi_cleanup(multi);
        return 0;
    }

    size_t total_bytes = 0;
    for(int i=0; i < num_connections; i++){
        total_bytes += bytes[i];
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    curl_multi_cleanup(multi);

    double speed_mbps = (total_bytes * 8.0) / (total_time * 1000000);
    return speed_mbps;
}

double upload_test(const char *host){
    CURLM *multi = curl_multi_init();
    int num_connections = 4;
    int running = 0;
    CURL *handles[4];
    UploadData uploads[4];
    char url[512];
    snprintf(url, sizeof(url), "http://%s/upload", host);

    for(int i=0; i<num_connections; i++){
        uploads[i].sent = 0;
        uploads[i].total = 25000000;
        handles[i] = curl_easy_init();
        curl_easy_setopt(handles[i], CURLOPT_URL, url);
        curl_easy_setopt(handles[i], CURLOPT_POST, 1L);
        curl_easy_setopt(handles[i], CURLOPT_POSTFIELDSIZE, (long)uploads[i].total);
        curl_easy_setopt(handles[i], CURLOPT_READFUNCTION, upload_callback);
        curl_easy_setopt(handles[i], CURLOPT_READDATA, &uploads[i]);
        curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, discard_callback);
        curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(handles[i], CURLOPT_USERAGENT, "TeltonikaTest");
        curl_easy_setopt(handles[i], CURLOPT_FOLLOWLOCATION, 1L);
        curl_multi_add_handle(multi, handles[i]);
    }

    do {
        curl_multi_perform(multi, &running);
        curl_multi_wait(multi, NULL, 0, 100, NULL);
    } while (running > 0);

    double total_time = 0;
    curl_easy_getinfo(handles[0], CURLINFO_TOTAL_TIME, &total_time);

    if(total_time == 0) {
        printf("Speed test failed: no data received\n");
        for(int i=0; i<num_connections; i++){
            curl_multi_remove_handle(multi, handles[i]);
            curl_easy_cleanup(handles[i]);
        }
        curl_multi_cleanup(multi);
        return 0;
    }

    size_t total_bytes = 0;
    for(int i=0; i < num_connections; i++){
        total_bytes += uploads[i].sent;
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    curl_multi_cleanup(multi);

    double speed_mbps = (total_bytes * 8.0) / (total_time * 1000000);

    return speed_mbps;

}
