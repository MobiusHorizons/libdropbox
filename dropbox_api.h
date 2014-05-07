#ifndef __DB_API_H__
#define __DB_API_H__
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdbool.h>

FILE * db_files_get(char* path, const char* access_token);
json_object * db_files_put(char* path, const char* access_token, FILE *   input_file);
json_object * db_metadata (char* path, const char* access_token, bool list);
json_object * db_delta    (char* cursor, const char* access_token);
json_object * db_longpoll (char* cursor,int timeout);
const char * db_authorize_token (char* token, char * client_id, char* client_secret);
#endif

