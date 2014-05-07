#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "dropbox_api.h"
#include "dropbox_urls.h"
#include "buffer.h"


#ifdef WIN32
	#include <io.h>
	#include <fcntl.h>
	#define pipe(fds) _pipe(fds,4096, _O_BINARY) 
	#define SSL_CERT curl_easy_setopt(curl, CURLOPT_CAPATH, "./ca-bundle.crt");
#else
	#define SSL_CERT
//	#define SSL_CERT curl_easy_setopt(curl, CURLOPT_CAPATH, "./ca-bundle.crt");
#endif
pthread_t threads[256];
buffer data_buffer;

int rest_build_param(char** param, const char * name, const char* value){
	*param = (char*)malloc(strlen(name)+strlen(value) + 2);
	return sprintf(*param, "%s=%s", name, value);
}

char * rest_escape(char * url){
	size_t len = strlen(url);
	size_t diff = 0;
	size_t current = len;
	int i;
	char * escaped = (char*) malloc(len + 1);
	escaped[0] = '\0';
	for ( i = 0; i < len; i ++){
		if (len+diff +3> current){
			current += 4;
			escaped = (char*)realloc(escaped,current+1);
		}
		char c = url[i];
		switch (c){
		case ' ':
			diff += 2;
			strcat(escaped,"%20");
			break;
		default:
			escaped[i+diff]=c;
			escaped[i+diff+1] = '\0';
			break;
		}
	}
	return escaped;
}

typedef struct {
	CURL * curl;
	FILE * file;
	char * url;
} run_curl_args;

void * run_curl(void* ptr){
	if (ptr != NULL){
		run_curl_args * args = (run_curl_args*) ptr;
		curl_easy_perform(args->curl);
		curl_easy_cleanup(args->curl);
		fclose(args->file);
		free(args->url);
	}
}
size_t ReadFileCB( void *contents, size_t size, size_t nmemb, void *userp){
	FILE * file = (FILE*) userp;
	if (file == NULL) file == stdin;
	return fread(contents,size,nmemb,file);
}

static size_t WriteFileCB(void * contents, size_t size, size_t nmemb, void * userp){
	FILE * file = (FILE*) userp;
	if (file == NULL) file == stdout;
	return fwrite(contents, size, nmemb, file);
}

static size_t WriteBufferCB(void *contents, size_t size, size_t nmemb, void *userp)
{ 
	if (userp!= NULL){
		size_t realsize = size * nmemb;
		buffer * data = (buffer*) userp;
		*data = buffer_append(*data,contents, realsize);
		return realsize;
	}
}

char * rest_build_url(char ** params, char* base){
	char * url; 
	int num_params;
	int i;
	int len = 0;
	for (i = 0; params[i] != NULL; i++){
		len += strlen(params[i]) + 1;
	}
	num_params = i;
	url = (char*) malloc(strlen(base) + len + 2);
	strcpy(url, base);
	strcat(url, "?");
	for ( i = 0; i < num_params; i ++){
		strcat(url, params[i]);
		if ( i< num_params-1)
			strcat(url, "&");
	}
	char *escaped = rest_escape(url);
	free(url);
	return escaped;
}

FILE * REST_GET	(char ** params, char * url){
	char * full_url = rest_build_url(params, url);

	/* set up pipes for reading/writing*/
	int fd[2];
	if (pipe(fd)!=0){ // uh oh, we have something wrong
		printf("could not create pipe\n");
		exit(1);
	}
	FILE * read = fdopen(fd[0], "r");
	FILE * write = fdopen(fd[1], "w");
	CURL * curl = curl_easy_init();
	SSL_CERT
	curl_easy_setopt(curl,CURLOPT_URL,full_url);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,(void*)write);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteFileCB);
	run_curl_args *args = malloc(sizeof(run_curl_args));
	args->curl = curl;
	args->file = write;
	args->url = url;
	pthread_create(&threads[fd[1]],NULL, run_curl, args);
	return read;
}

buffer REST_GET_BUFFER (char ** params, char * url){
	char * full_url = rest_build_url(params, url);
	CURL * curl = curl_easy_init();
	SSL_CERT
	buffer data = buffer_init(data,0);
	curl_easy_setopt(curl,CURLOPT_URL,full_url);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&data);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteBufferCB);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	free(full_url);
	return data;
}

buffer  REST_POST (char ** params, char * url){
	CURL * curl = curl_easy_init();
	char * post = rest_build_url(params,"");
	char * escaped_url = rest_escape(url);
	SSL_CERT
	buffer data = buffer_init(data,0);
	curl_easy_setopt(curl,CURLOPT_URL,escaped_url);
	int i = 0;
	curl_easy_setopt(curl,CURLOPT_POSTFIELDS,post);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA, &data);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteBufferCB);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	free(post);
	free(escaped_url);
	return data;
}

buffer REST_PUT_FILE (char** params, char* url, FILE * in){
	CURL * curl = curl_easy_init();
	buffer data = buffer_init(data,0);
	char * full_url = rest_build_url(params,url);
	free(full_url);
	printf("%s\n",full_url);
	curl_easy_setopt(curl,CURLOPT_URL,full_url);
	curl_easy_setopt(curl,CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl,CURLOPT_READDATA,in);
//	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,&data);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,WriteBufferCB);
	curl_easy_setopt(curl,CURLOPT_READFUNCTION,ReadFileCB); // for windows
	CURLcode res = curl_easy_perform(curl);
	free(full_url);
	curl_easy_cleanup(curl);
	if (res != CURLE_OK){
		printf("res = %d, data = %.*s\n",res,data.size,data.data);
		data = buffer_free(data);
	}
	return data;
}

FILE * db_files_get(char* path, const char* access_token){
	char* params[2];
	char * url = malloc(strlen(path) + strlen(FILES_GET) + 2);

	rest_build_param(&params[0],"access_token",access_token);
	sprintf(url, "%s%s", FILES_GET, path);
	params[1] = NULL;

	FILE * file = REST_GET(params,url);
	free(url);
	return file;
} 

json_object * db_files_put(char* path, const char* access_token, FILE * input_file){
	char* params[2];
	char * url = malloc(strlen(path) + strlen(FILES_PUT) + 2);

	rest_build_param(&params[0],"access_token",access_token);
	sprintf(url, "%s%s", FILES_PUT, path);
	params[1] = NULL;

	buffer resp = REST_PUT_FILE(params,url,input_file);
	free(url);
	free(params[0]);
	json_object * response = json_tokener_parse(resp.data);
	return response;
}

json_object * db_metadata (char* path, const char* access_token, bool list){
	char * params[3];
	int i;
	rest_build_param(&params[0],"access_token",access_token);
	char * url = malloc(strlen(path) + strlen(METADATA)+1);
	strcpy(url, METADATA);
	strcat(url, path);
	params[1] = list?"list=true":"list=false";
	params[2]=NULL;
	buffer resp= REST_GET_BUFFER(params,url);
	free(params[0]);
	free(url);
	json_object * response = json_tokener_parse(resp.data);
//	printf(json_object_to_json_string(response));
	return response;
}

json_object * db_longpoll (char* cursor, int timeout){
	char * params[3];
	int i;
	if (timeout > 480) timeout = 480;// max allowed
	if (timeout < 30)  timeout = 30;  // minimum allowed
	rest_build_param(&params[0],"cursor",cursor);
	params[1] = malloc(3 + strlen("timeout=")+1);
	sprintf(params[1],"timeout=%d",timeout);
	params[2] = NULL;
	buffer resp = REST_GET_BUFFER(params,LONGPOLL);
	free (params[0]);
	free (params[1]);
	json_object * response = json_tokener_parse(resp.data);
	printf(json_object_to_json_string(response));
	return response;
}
json_object * db_delta    (char* cursor, const char* access_token){
	char* params[3];
	if (cursor != NULL){
		rest_build_param(&params[1],"cursor",cursor);
	} else {
		params[1] = NULL;
	}
	rest_build_param(&params[0],"access_token",access_token);
	params[2] = NULL;

	buffer resp = REST_POST(params,DELTA);
	json_object * response;
	response = json_tokener_parse(resp.data);
//	printf(json_object_to_json_string(response));
	return response;
}

const char * db_authorize_token (char* token, char * client_id, char* client_secret){
	char * params[5];
	rest_build_param(&params[0], "code",token);
	rest_build_param(&params[1], "client_id",client_id);
	rest_build_param(&params[2], "client_secret",client_secret);
	rest_build_param(&params[3], "grant_type","authorization_code");
	params[4] = NULL;
	buffer resp = REST_POST(params,OAUTH_2_TOKEN);
	json_object * response; json_object *access_token;
	response = json_tokener_parse(resp.data);
	if (json_object_object_get_ex(response,"access_token",&access_token)){
		return json_object_get_string(access_token);
	}
	printf(json_object_to_json_string(response));printf("\n");
	return NULL;
}

