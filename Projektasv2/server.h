#ifndef SERVER_H
#define SERVER_H

typedef struct {
    char country[128];
    char city[128];
    char provider[256];
    char host[256];
    int id;
    double latency;
} Server;

int parse_servers(const char *json_data, Server *servers);
int find_server_by_country(Server *servers, int count, const char *country, Server *results);
int find_best_server(Server *servers, int count);

#endif