#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <cjson/cJSON.h>
#include "server.h"
#include "network.h"
#include "utils.h"

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
    Server *results = malloc(server_count * sizeof(Server));
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
                printf("Server: %s (%s) - %.2f ms\n", results[best].host, results[best].city, results[best].latency * 1000);
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
    free(results);
    free(country);
    free(city);
    return 0;
}