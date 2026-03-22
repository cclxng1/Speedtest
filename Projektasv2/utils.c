#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

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