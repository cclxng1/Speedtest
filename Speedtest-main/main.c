#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

typedef struct {
    char country[128];
    char city[128];
    char provider[256];
    char host[256];
    int id;
    double latency;
} Server;   

typedef struct {
    char *data;
    size_t size;
} Response;

void print_usage() {
    printf("Usage: ./main [options]\n");
    printf("Options:\n");
    printf("  -a <host>  Run download speed test with specifiedx     server\n");
    printf("  -b <host>  Run upload speed test with specified server\n");
    printf("  -c         Detect your location\n");
    printf("  -d         Find best server\n");
    printf("  -e         Run full automatic test\n");
    printf("  -h         Show this help message\n");
}

char* read_file(const char *filename){
    FILE *file = fopen(filename, "r");
    if (!file){
        printf("Cannot open file");
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(size+1);
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;

} 

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

void detect_location(char *country, char *city){
    CURL *curl = curl_easy_init();
    if(!curl){
        printf("Error with curl\n");
        return;
    }
    Response resp = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL, "http://ip-api.com/json/");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    cJSON *json = cJSON_Parse(resp.data);
    if(json){
        cJSON *c = cJSON_GetObjectItem(json, "country");
        cJSON *ci = cJSON_GetObjectItem(json, "city");
        printf("Your location: %s, %s\n", c->valuestring, ci->valuestring);
        strcpy(country, c->valuestring);
        strcpy(city, ci->valuestring);
        cJSON_Delete(json);
    }
    free(resp.data);
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
    CURL *handles[count];
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
        if(res == CURLE_OK){
            servers[i].latency = total_time;
            printf("Server %s - %.2f ms\n", servers[i].host, total_time * 1000);
        } else {
            servers[i].latency = 999;
        }
        if(servers[i].latency < servers[best].latency){
            best = i;
        }
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    curl_multi_cleanup(multi);
    return best;
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
        curl_easy_setopt(handles[i], CURLOPT_USERAGENT, "SpeedTest");
        curl_multi_add_handle(multi, handles[i]);
    }

    do {
        curl_multi_perform(multi, &running);
        curl_multi_wait(multi, NULL, 0, 100, NULL);
    } while (running > 0);

    double total_time = 0;
    curl_easy_getinfo(handles[0], CURLINFO_TOTAL_TIME, &total_time);
    
    size_t total_bytes = 0;
    for(int i=0; i < num_connections; i++){
        printf("Connection %d: %zu bytes\n", i, bytes[i]);
        total_bytes += bytes[i];
        curl_multi_remove_handle(multi, handles[i]);
        curl_easy_cleanup(handles[i]);
    }

    curl_multi_cleanup(multi);

    double speed_mbps = (total_bytes * 8.0) / (total_time * 1000000);
    printf("Download speed: %.2f Mbps \n", speed_mbps);
    return speed_mbps;
}

int main(int argc, char *argv[]){

    char *data = read_file("speedtest_server_list.json");
    if (data == NULL){
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    int server_count = cJSON_GetArraySize(json);
    cJSON_Delete(json);
    Server *servers = malloc(server_count * sizeof(Server));
    int count = parse_servers(data,servers);
    printf("Loaded %d servers\n", count);
    //for(int i=0;i<100;i++){
    //printf("Result: %s, %s - %s\n", servers[1].country, servers[2].city, servers[3].host);
    //printf("Second: %s, %s - %s\n", servers[1].country, servers[1].city, servers[1].host);
    //}

    if (argc == 1) {
        print_usage();
        return 0;
        }

    char *country = malloc(128);
    char *city = malloc(128);
    Server results[128];

    int opt;
    while ((opt = getopt(argc, argv, "a:b:cdeh")) !=-1){
        switch (opt){
                case 'a' :
                printf("Download test with server: %s\n", optarg);
                download_test(optarg);
                break;
                case 'b' :
                printf("Upload test with server: %s\n", optarg);
                break;
                case 'c' :
                detect_location(country,city);
                int found = find_server_by_country(servers, count, country, results);
                printf("Found %d servers in %s\n", found, country);
                int best = find_best_server(results, found);
                printf("Best server: %s (%s) - %.2f ms\n", results[best].host, results[best].city, results[best].latency * 1000);
                break;
                case 'd' :
                printf("Finding best server...\n");
                break;
                case 'e' :
                printf("Running full test...\n");
                break;
                case 'h':
                print_usage();
                return 0;
                default:
                printf("unknown option\n");
                return 1;

        }
    }
    free(servers);
    free(data);
    return 0;
    
}