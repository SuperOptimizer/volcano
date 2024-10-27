#pragma once

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    size_t *current_size = (size_t*)((char**)userp + 1);
    char **buffer = userp;

    char *new_buffer = (char*)realloc(*buffer, *current_size + realsize + 1);
    if (!new_buffer) {
        return 0; // Signal error to curl
    }

    memcpy(new_buffer + *current_size, contents, realsize);
    *buffer = new_buffer;
    *current_size += realsize;
    (*buffer)[*current_size] = 0; // Null terminate

    return realsize;
}

static long download(const char* url, void** out_buffer) {
    CURL* curl;
    CURLcode res;
    long http_code = 0;

    // Initialize buffer and size
    char* buffer = malloc(1);  // Initial buffer
    size_t current_size = 0;          // Current size
    void* callback_data[2] = { &buffer, &current_size }; // Pack buffer and size together

    if (!buffer) {
        return -1;
    }

    curl = curl_easy_init();
    if (!curl) {
        free(buffer);
        return -1;
    }

    // Set up curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callback_data);
    //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    //curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // Perform the request
    res = curl_easy_perform(curl);

    // Check for errors
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        free(buffer);
        curl_easy_cleanup(curl);
        return -1;
    }

    // Get HTTP response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        free(buffer);
        return -1;
    }

    *out_buffer = buffer;
    return current_size;
}