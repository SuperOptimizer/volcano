#pragma once

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

// Define a structure to hold our buffer data
typedef struct {
    char* buffer;
    size_t size;
} DownloadBuffer;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    DownloadBuffer *mem = (DownloadBuffer *)userp;

    char *new_buffer = realloc(mem->buffer, mem->size + realsize + 1);  // +1 for null terminator
    if (!new_buffer) {
        return 0; // Signal error to curl
    }

    memcpy(new_buffer + mem->size, contents, realsize);
    mem->buffer = new_buffer;
    mem->size += realsize;
    mem->buffer[mem->size] = 0; // Null terminate

    return realsize;
}

static long download(const char* url, void** out_buffer) {
    CURL* curl;
    CURLcode res;
    long http_code = 0;

    // Initialize our buffer structure
    DownloadBuffer chunk = {
        .buffer = malloc(1),  // Initial buffer
        .size = 0
    };

    if (!chunk.buffer) {
        return -1;
    }
    chunk.buffer[0] = 0;  // Ensure null terminated

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.buffer);
        return -1;
    }

    // Set up curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    //TODO: with bearssl on windows I have to disable these
    // does that matter?
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(chunk.buffer);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Get HTTP response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        free(chunk.buffer);
        return -1;
    }

    *out_buffer = chunk.buffer;
    return chunk.size;
}