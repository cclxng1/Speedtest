#ifndef NETWORK_H
#define NETWORK_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t size;
} Response;

typedef struct {
    size_t sent;
    size_t total;
} UploadData;

size_t callback(void *ptr, size_t size, size_t nmemb, void *userdata);
size_t download_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
size_t upload_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
void detect_location(char *country, char *city);
double download_test(const char *host);
double upload_test(const char *host);

#endif