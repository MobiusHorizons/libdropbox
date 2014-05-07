#include "dropbox_api.h"
#include <string.h>

const char * JSON_GET_STRING(json_object * obj, char * object){
	if (json_object_object_get_ex(obj,object,&obj)){
		return json_object_get_string(obj);
	}
	return NULL; 
}

const char * init(char * client_key,char * client_secret){
	FILE * state = fopen("access_token.txt", "r");
	const char * access_token = NULL;
	if (state != NULL){
		char *at = malloc(128);
		at = fgets(at, 128, state);
		if (at != NULL){
			int len = strlen(at);
			at[len-1] = '\0';
			access_token = at;
		}
		fclose(state);
	}
	if (access_token == NULL){
		char  token[128];
		printf("go to https://www.dropbox.com/1/oauth2/authorize?response_type=code&client_id=%s and copy the code here\n",client_key);
		if (fgets(token,128,stdin) == NULL) exit(1);
		int len = strlen(token);
		if (token[len-1] == '\n') token[len-1] = '\0';
		access_token = db_authorize_token(token,client_key,client_secret);
		FILE * state = fopen("access_token.txt","w");
		fprintf(state ,"%s\n",access_token);
		fclose(state);
	}
	if (access_token == NULL) exit(-1);
	return access_token;
}

void do_put(char* in, char* out, const char* access_token){
	printf("doing put %s %s\n",in,out);
	FILE * file = fopen(in,"r");
	if (file == NULL){
		printf("could not open file '%s'\n",in);	
		exit(-1);
	}
	json_object * upload = db_files_put(out,access_token,file);
	printf("uploaded '%s' to '%s'\n",in,JSON_GET_STRING(upload,"path"));
	fclose(file);
}

void do_ls(char* file, const char *access_token){
	int i;
	json_object * metadata = db_metadata(file,access_token,true);
	json_object * contents;
	if (json_object_object_get_ex(metadata,"contents",&contents)){
		for ( i = 0; i < json_object_array_length(contents); i ++){
			json_object * value = json_object_array_get_idx(contents,i);
			json_object * path;
			if (json_object_object_get_ex(value,"path",&path)){
				printf("%s\n",json_object_get_string(path));
			}
		}
	}
}

void do_metadata(char * file, const char*access_token){
	json_object * metadata = db_metadata(file,access_token,true);
	printf("%s\n", json_object_to_json_string(metadata));
}

void do_get(char * file, const char * access_token){
		FILE * out_file = db_files_get(file,access_token);
		char  resp[1024];
		int i;
		do {
			i = fread(resp, 1, 1024, out_file);
			printf("%.*s", i,resp);
		} while(i);
		fclose(out_file);
}

int main(int argc, char** argv){
	char * client_key    = "gmq6fs74fuw1ead";
	char * client_secret = "ia87pt0ep6dvb7y";
	curl_global_init(CURL_GLOBAL_DEFAULT);
	const char * access_token = init(client_key,client_secret);
	printf("access_token = %s\n",access_token);
	if (argc > 2 && strcmp(argv[1],"ls")== 0 ){
		if (argc== 2) do_ls("/",access_token);
		else do_ls(argv[2],access_token);
	} else if (argc >=2 && strcmp(argv[1],"metadata")==0){
		do_metadata(argv[2],access_token);
	} else if (argc == 4 && strcmp(argv[1],"put")==0){
		do_put(argv[2],argv[3],access_token);
	} else if (argc == 3 && strcmp(argv[1],"get")==0){
		do_get(argv[2],access_token);
	} else {
		printf("usage dropbox [command] [args]\n"
			"commands:\n"
			"\tls [dir]\n"
			"\tmetadata [file/dir]\n"
			"\tput [local] [remote]\n"
			"\tget [remote]\n"
		);
	}

/*	json_object * delta = db_delta(NULL,access_token);
	const char * cursor;
	if (json_object_object_get_ex(delta,"cursor",&temp_obj)){
		cursor = json_object_get_string(temp_obj);
	}
	printf("cursor = %s\n",cursor);*/
	curl_global_cleanup();
}
