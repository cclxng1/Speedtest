#include "server.h"
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

int parse_servers(const char *json_data, Server *servers){
    cJSON *json = cJSON_Parse(json_data);
    if (!json){
        printf("Error parsing JSON\n");
        return 0;
    }
    int count = 0;
    int array_size = cJSON_GetArraySize(json);
    for(int i=0; i<array_size; i++){
        cJSON *item = cJSON_GetArrayItem(json, i);
        cJSON *country = cJSON_GetObjectItem(item, "country");
        cJSON *city = cJSON_GetObjectItem(item, "city");
        cJSON *provider = cJSON_GetObjectItem(item, "provider");
        cJSON *host = cJSON_GetObjectItem(item, "host");
        cJSON *id = cJSON_GetObjectItem(item, "id");

        strcpy(servers[count].country, country->valuestring);
        strcpy(servers[count].city, city->valuestring);
        strcpy(servers[count].provider, provider->valuestring);
        strcpy(servers[count].host, host->valuestring);
        servers[count].id = id->valueint;

        count++;

    }
    cJSON_Delete(json);
    return count;
}

int find_server_by_country(Server *servers, int count, const char *country, Server *results){
    int found = 0;
    for (int i=0; i<count; i++){
        if(strcmp(servers[i].country, country) == 0){
            results[found] = servers[i];
            found++;
        }
    }
    return found;
}

int find_best_server(Server *servers, int count){
    CURLM *multi = curl_multi_init();
    CURL **handles = malloc(count * sizeof(CURL *));
    int running = 0;
    int best = 0;

    for(int i=0; i<count; i++){
        handles[i] = curl_easy_init();
        char url[512];
        snprintf(url, sizeof(url), "http://%s", servers[i].host);
        curl_easy_setopt(handles[i], CURLOPT_URL, url);
        curl_easy_setopt(handles[i], CURLOPT_NOBODY, 1L);
        curl_easy_setopt(handles[i], CURLOPT_TIMEOUT, 3L);
        curl_multi_add_handle(multi, handles[i]);
    }

    do {
        curl_multi_perform(multi, &running);
        curl_multi_wait(multi, NULL, 0 , 100, NULL);
    } while (running > 0);

    for(int i=0; i<count; i++){
        double total_time = 0;
        CURLcode res;
        res = curl_easy_getinfo(handles[i], CURLINFO_TOTAL_TIME, &total_time);
        if(res == CURLE_OK && total_time < 2.5){
            servers[i].latency = total_time;
        } else {
            servers[i].latency = -1;
        }
        if(servers[i].latency > 0 && (servers[best].latency < 0 || servers[i].latency < servers[best].latency)){
            best = i;
        }
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    free(handles);
    curl_multi_cleanup(multi);
    return best;
}
