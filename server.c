#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>

#define BUF_LEN     1024    // Default buffer length
#define MAX_CONNECTIONS     12 // Maximum number of connections to server
#define ERROR_MSG   "HTTP/1.1 500 Internal Server Error" // TODO change this to dynamic type detection

void check_arguments(int argc){
    if (argc != 2) {
        fprintf(stderr, "usage: ./server port\n");
        exit(1);
    }
}

struct sockaddr_in create_serv_addr(char* argv[]){
    struct sockaddr_in servinfo;
    memset(&servinfo, 0, sizeof(servinfo));
    servinfo.sin_family = AF_INET;
	servinfo.sin_port = htons(atoi(argv[1]));
	servinfo.sin_addr.s_addr = INADDR_ANY;
    return servinfo;
}

// When we encounter an error
void print_and_exit(char *msg){
    perror(msg);
    exit(0);
}

int create_socket(){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        print_and_exit("create_socket: couldn't create socket\n");
    return sockfd;
}

void bind_socket_to_serv(struct sockaddr_in servinfo, int sockfd){
    int bind_result = bind(sockfd, (struct sockaddr*) &servinfo, sizeof(servinfo));
    if (bind_result < 0)
        print_and_exit("bind_socket_to_serv: problem with binding socket to server\n");
    
}

void enable_listen(int sockfd) {
    if (listen(sockfd, MAX_CONNECTIONS) < 0)
        print_and_exit("enable_listen: problem with listening\n");
}

int socket_of_new_client(int sockfd){
    struct sockaddr_in clientinfo;
    memset(&clientinfo, 0, sizeof(clientinfo));
    int client_len = sizeof(struct sockaddr_in);
    int client_socket = accept(sockfd, (struct sockaddr *) &clientinfo, &client_len);
    printf("socket_of_new_client %s:%hu\n", inet_ntoa(clientinfo.sin_addr), clientinfo.sin_port);

    if (client_socket < 0)
        print_and_exit("socket_of_new_client: error with accept\n");
    return client_socket;
}

int read_from_client(int new_client_socket, char* buf){
    memset(buf, 0, BUF_LEN);
    int num_bytes = read(new_client_socket, buf, BUF_LEN);
    if (num_bytes < 0)
        print_and_exit("read_from_client: error with reading from client\n");
    return num_bytes;
}

int write_to_client(int new_client_socket, char* buf, size_t length){
    int numbytes = write(new_client_socket, buf, length);
    if (numbytes < 0)
        print_and_exit("write_to_client: problem with writing to client\n");
    return numbytes;
}

char * parse_uri(char *buf){
    char *uri = strtok(NULL, " ");
    // delete the back slash
    if (strlen(uri) > 1)
        memmove(uri, uri+1, strlen(uri));
    return uri;
}
int does_file_exists(char *file_name){
    return access(file_name, F_OK) != -1;
}

char* parse_file_type(char *uri){
    char *file_type;
    if(strstr(uri,".htm"))
	{
		file_type = "text/html";
	}
	else if(strstr(uri,".txt")){
		file_type = "text/plain";
	}
	else if(strstr(uri,".png")){
		file_type = "image/png";
	}
	else if(strstr(uri,".gif")){
		file_type = "image/gif";
	}
	else if(strstr(uri,".jpg")){
		file_type = "image/jpg";
	}
	else if(strstr(uri,".css")){
		file_type = "text/css";
	}
	else if(strstr(uri,".js")){
		file_type = "application/javascript";
	}
    return file_type;
}
int get_file_size(char *file_name){
    struct stat st;
    stat(file_name, &st);
    return st.st_size;
}
void create_reply_header(char *uri, int new_client_socket){

    if (!does_file_exists(uri)){
        write_to_client(new_client_socket, ERROR_MSG, strlen(ERROR_MSG));
        printf("File does not exist = %s\n", uri);
        print_and_exit("create_reply_header: file does not exist\n");
    }
    
    int file_size = get_file_size(uri);
    char *file_type = parse_file_type(uri);
    printf("create_reply_header: file type is %s\n" ,file_type);
    char reply_header[BUF_LEN];
    memset(reply_header, 0, BUF_LEN);
    // TODO change hard valued http version to dynamic
    sprintf(reply_header,"HTTP/1.1 200 Document Follows\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", file_type,file_size);
    write_to_client(new_client_socket, reply_header, strlen(reply_header));
    printf("create_reply_header: header is %s\n", reply_header); //DEBUG
}
FILE* open_file(char *file_name, char* mode){
    FILE *file = fopen(file_name, mode);
    if (file == NULL)
        print_and_exit("open_file: Failed to open the file\n");
    fseek(file, 0, SEEK_SET);
    return file;
}
void send_response(char *uri, int new_client_socket){
	char filebuf[BUF_LEN];

	memset(filebuf,0,BUF_LEN);

	FILE *file_to_send = open_file(uri, "r");

	// read until EOF
	while(1)
	{
		int numbytes = fread(filebuf,1,BUF_LEN, file_to_send);
		if(numbytes == 0)
		{
			break;
		}
		numbytes = write_to_client(new_client_socket, filebuf, numbytes);
		if(numbytes ==-1)
		{
			write_to_client(new_client_socket, ERROR_MSG, strlen(ERROR_MSG));
			print_and_exit("send_response: failed to send packet\n");
		}
		memset(filebuf,0, BUF_LEN);
	}
	fclose(file_to_send);
}
void handle_http_get(char *buf, int new_client_socket, int sockfd){
    
    char *uri = parse_uri(buf);
    printf("handle_http_get: uri = %s\n", uri);
    if (!strcmp(uri, "/"))
        if (does_file_exists("index.html"))
            uri = "index.html";
        else if (does_file_exists("index.htm"))
            uri = "index.htm";
    create_reply_header(uri, new_client_socket);
    send_response(uri, new_client_socket);

}

void send_http_post(char *uri, int new_client_socket, char* post_data){
    if (!does_file_exists) {
        write_to_client(new_client_socket, ERROR_MSG, strlen(ERROR_MSG));
        print_and_exit("send_http_post: File not found\n");
    }

    // POST header
    char post_header[BUF_LEN];
    memset(post_header, 0, BUF_LEN);
    sprintf(post_header,"HTTP/1.1 200 Document Follows\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n", get_file_size(uri));
    write_to_client(new_client_socket, post_header, BUF_LEN);

    // POST BODY OPENING
    char *body_op = "<html><body><pre><h1>";
    write_to_client(new_client_socket, body_op, strlen(body_op));

    // POST DATA
    write_to_client(new_client_socket, post_data, strlen(post_data));

    // POST BODY CLOSING
    char *body_close = "</h1></pre>";
    write_to_client(new_client_socket, body_close, strlen(body_close));

    // POST FILE CONTENT
    send_response(uri, new_client_socket);

}
void handle_http_post(char *buf, int new_client_socket, int sockfd){
    char *uri = parse_uri(buf);
    char* request_no_uri = strtok(NULL, "\0"); // Get everything in the post request except uri
    char *escape_seq_str = strstr(request_no_uri, "\r\n\r\n");
    int escape_first_char_index = escape_seq_str - request_no_uri;
    char* post_data = request_no_uri + escape_first_char_index + 4;

    send_http_post(uri, new_client_socket, post_data);
}

void handle_http_error(char *buf, int new_client_socket, int sockfd){
    printf("handle_http_error: unsupported operation = %s\n", buf);
    write_to_client(new_client_socket, ERROR_MSG, strlen(ERROR_MSG));
    exit(0);
}
void handle_http_request(int new_client_socket, int sockfd){
    char buf[BUF_LEN];
    int num_bytes = read_from_client(new_client_socket, buf);
    char *request_method = strtok(buf, " ");
    printf("Handling request_method %s\n", request_method); // DEBUG
    if (strncmp(request_method, "GET", 3) == 0)
        handle_http_get(buf,new_client_socket, sockfd);
    else if (strcmp(request_method, "POST") == 0)
        handle_http_post(buf, new_client_socket, sockfd);
    else
        handle_http_error(buf, new_client_socket, sockfd);
    

}
void handle_new_connections(int sockfd){
    int new_client_socket;
    printf("Server is ready to accept connections now\n"); //DEBUG
    while(1){
        int new_client_socket = socket_of_new_client(sockfd);
        int new_process = fork();
        if (new_process == 0){
            handle_http_request(new_client_socket, sockfd);
            
            exit(0);
             
        }
        close(new_client_socket);
           
    }
    
}
int main(int argc, char **argv){
    check_arguments(argc);
    struct sockaddr_in servinfo = create_serv_addr( argv);
    int sockfd = create_socket();
    bind_socket_to_serv(servinfo, sockfd);
    enable_listen(sockfd);
    handle_new_connections(sockfd); // The main loop
    //close(sockfd);

}
