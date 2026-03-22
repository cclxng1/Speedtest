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

typedef struct {
    size_t sent;
    size_t total;
} UploadData;

void print_usage() {
    printf("Usage: ./main [options]\n");
    printf("Options:\n");
    printf("  -a <host>  Run download speed test with specified server\n");
    printf("  -b <host>  Run upload speed test with specified server\n");
    printf("  -c         Find best server\n");
    printf("  -d         Detect your location\n");
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

int main(int argc, char *argv[]){

    char *data = read_file("speedtest_server_list.json");
    if (data == NULL){
        return 1;
    }

    cJSON *json = cJSON_Parse(data);
    int server_count = cJSON_GetArraySize(json);
    cJSON_Delete(json);
    Server *servers = malloc(server_count * sizeof(Server));
    if(!servers){
        printf("Memory allocation failed\n");
        free(data);
        return 1;
    }
    int count = parse_servers(data,servers);

    if (argc == 1) {
        print_usage();
        return 0;
        }

    char *country = calloc(128, 1);
    char *city = calloc(128, 1);
    Server results[128];
    int found = 0;
    int best = 0;

    int opt;
    while ((opt = getopt(argc, argv, "a:b:cdeh")) !=-1){
        switch (opt){
                case 'a' :
                printf("Download test with server: %s\n", optarg);
                double download = download_test(optarg);
                printf("Download speed: %.2f Mbps\n", download);
                break;

                case 'b' :
                printf("Upload test with server: %s\n", optarg);
                double upload = upload_test(optarg);
                printf("Upload speed: %.2f Mbps\n", upload);
                break;

                case 'c' :
                detect_location(country,city);
                printf("Your location: %s, %s\n", country, city);
                found = find_server_by_country(servers, count, country, results);
                if(found == 0){
                    printf("No servers found in %s\n", country);
                    break;
                }
                printf("Found %d servers in %s\n", found, country);
                best = find_best_server(results, found);
                if(results[best].latency < 0){
                    printf("No working servers found\n");
                    break;
                }
                printf("Best server: %s (%s) - %.2f ms\n", results[best].host, results[best].city, results[best].latency * 1000);
                break;

                case 'd' :
                printf("Finding your location...\n");
                detect_location(country,city);
                if(strlen(country) == 0){
                    printf("Failed to detect location\n");
                    break;
                }
                printf("Your location: %s, %s\n", country, city);
                break;

                case 'e' :
                printf("Starting full speed test...\n\n");
                printf("Detecting location...\n");
                detect_location(country, city);
                if(strlen(country) == 0){
                    printf("Failed to detect location\n");
                    break;
                }
                found = find_server_by_country(servers, count, country, results);
                if(found == 0){
                    printf("No servers found in %s\n", country);
                    break;
                }
                printf("Finding best server...\n");
                best = find_best_server(results, found);
                if(results[best].latency < 0){
                    printf("No working servers found\n");
                    break;
                }
                printf("Testing download speed...\n");
                double download_speed = download_test(results[best].host);
                printf("Testing upload speed...\n");
                double upload_speed = upload_test(results[best].host);
                printf("\n===== Results =====\n");
                printf("Location: %s, %s\n", country, city);
                printf("Server: %s (%s)\n", results[best].host, results[best].city);
                printf("Download: %.2f Mbps\n", download_speed);
                printf("Upload: %.2f Mbps\n", upload_speed);
                printf("===================\n");
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
    free(country);
    free(city);
    return 0;
}