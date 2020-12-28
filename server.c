#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "threadpool.h"
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define BUFLEN 4000

//response numbers
#define Found 302 
#define BadRequest 400  
#define Forbidden 403
#define NotFound 404 
#define IntrnalError 500 
#define NotSupport 501
#define Ok 200
#define Dir0 0
#define File0 1

//headers for response
#define HTTP "HTTP/1.1 "

#define BadRequest400  "400 Bad Request"
#define found302 "302 Found"
#define forbidden403 "403 Forbidden"
#define notFound404 "404 Not Found"
#define intrnalError500 "500 Internal Server Error"
#define notSupport501 "501 Not supported"
#define ok200 "200 OK"

#define newLine "\r\n"
#define ServerWebserver "Server: webserver/1.0"
#define Date "Date: "
#define Location "Location: "
#define ContentType "Content-Type: text/html"
#define ContentLength "Content-Length: "
#define length302 "123"
#define length400 "113"
#define length403 "111"
#define length404 "112"
#define length500 "144"
#define length501 "129"
//dir con/file
#define connectionType "Content-Type: "
#define lastMOdified "Last-Modified: "
#define ConnectionClose "Connection: close"

//headers for body
#define html0 "<HTML><HEAD><TITLE>"
#define html1 "</TITLE></HEAD>"
#define html2 "<BODY><H4>"
#define html3 "</H4>"
#define html4  "</BODY></HTML>"

//headerd for table

#define htmltr0 "<tr><td><A HREF="
#define htmltr2 "</A></td><td>"
#define htmltr3 "</td>"
#define htmltr4 "<td>"
#define htmltr5  "</tr>"


int handaleNewAccept(void * new_fd);
char *get_mime_type(char *name);
void checkRequest(char *request, int new_fd);
void buildResponse(int new_sock_fd, int num, char *path);
void buildResponseFileOrDir(char * path, int new_fd);
void readFileAndSendToClient(char *filename, int newsockfd);
void handleASigPipe(int);
/*Yisrael Bar 10-26/12/20*/
int main(int argc, char* argv[]) {
    signal(SIGPIPE , handleASigPipe);
    //check that we got enghou argument
    if (argc <  4)
    {
        // fprintf(stderr,"ERROR, no port provided\n");
        fprintf(stderr, "Usage: server <port> <pool-size>\n");
        exit(EXIT_FAILURE);        
    }
    //socket
    int welcome_sock_fd,  new_sock_fd;
    struct sockaddr_in servr_addr;
    //in case you want client info..
    // struct sockaddr cli_addr;
    // socklen_t clilen;

    //make welcome socket =>tcp , get the socket fd
    welcome_sock_fd = socket(AF_INET , SOCK_STREAM, 0);
    if (welcome_sock_fd < 0)
    {
        perror("error opening welcome socket\n");
        exit(EXIT_FAILURE);
    }
    // init server addr
    servr_addr.sin_family = AF_INET;
    //htonl(INADDR_ANY)---------------need to check
    servr_addr.sin_addr.s_addr = INADDR_ANY;
    //check port number:   1024 < port <= 9999
    int port = atoi(argv[1]);
    if (port < 0 || port > 9999)
    {
        fprintf(stderr, "Usage: server <port> <pool-size>\n");
        close(welcome_sock_fd);
        exit(EXIT_FAILURE);
    }
    servr_addr.sin_port = htons(port);
    //connect welcome socket with port and ip
    if (bind(welcome_sock_fd , (struct sockaddr *) & servr_addr, sizeof(servr_addr)) < 0)
    {
        perror("error in bind\n");
        exit(EXIT_FAILURE);
    }

    //max 5 wait in welcome socket
    listen(welcome_sock_fd ,5);
    //wait for client
    int numOfAcceept = atoi(argv[3]);
    if (numOfAcceept <= 0)
    {
        printf("<max number of request> must be bigger then 0 \n");
        close(welcome_sock_fd);
        exit(EXIT_SUCCESS);

    }
    int *arrayNumOfAcceept =  (int *)malloc(sizeof(int)*numOfAcceept);
    if (arrayNumOfAcceept == NULL)
    {
        printf("Error in malloc of arrayNumOfAcceept\n");
        exit(EXIT_FAILURE);
    }
    
    int threadPoolSize = atoi(argv[2]);
    if (threadPoolSize <= 0)
    {
        printf("<pool size> must be bigger then 0 and smaller then 200\n");
        close(welcome_sock_fd);
        free(arrayNumOfAcceept);
        exit(EXIT_SUCCESS);
    }
    
    threadpool* t0 = create_threadpool(threadPoolSize);
    for (int i = 0; i < numOfAcceept ; i++)
    {
        new_sock_fd = accept(welcome_sock_fd , NULL, NULL);
        if (new_sock_fd < 0)
        {
            perror("error on accecpt\n");
        }
        arrayNumOfAcceept[i] = new_sock_fd;
        dispatch(t0,handaleNewAccept,&arrayNumOfAcceept[i]);
        // fflush(stdin); 
    }
    destroy_threadpool(t0);
    free(arrayNumOfAcceept);
    close(welcome_sock_fd);
    return 0;
}

int handaleNewAccept(void * new_fd){
    char buffer[BUFLEN];
    memset(buffer ,0 , BUFLEN);
    int new_sock_fd = *(int*)new_fd;
    //read the request from telnet or browser line
    int rc = read(new_sock_fd , buffer , BUFLEN);
    if (rc < 0)
    {
        buildResponse(new_sock_fd , IntrnalError, NULL);
        return -1;
    }
    char * pointer = buffer;
    //send the requset to check the body of it and call the right func accordingilly
    checkRequest(pointer , new_sock_fd);
    close(new_sock_fd);
    return 0;
}
//get a path of a file and put out the fromat of the file
char *get_mime_type(char *name){
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".au") == 0) return "audio/basic";
    if (strcmp(ext, ".wav") == 0) return "audio/wav";
    if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return NULL;
}

void checkRequest(char *request, int new_fd){
    if (request == NULL)
    {
        return;
    }
    int counter = 0;
    int index = 0;
    //count the words in the request
    for (int i = 0; i < BUFLEN; i++)
    {
        if (request[i] == ' ' )
        {
            counter++;
        }else if (request[i] ==  '\r' && request[i+1] ==  '\n')
        {
            counter++;
            index = i;
            break;
        }
    }
    //index = GET / HTTP/1.1 = 13
    if (index < 13)
    {
        //not enough letters to be a request
        buildResponse(new_fd, BadRequest, NULL); 
        return;
    }
    
    //check if the third word is ' HTTP/1.1' 
    char test[10];
    memset(test , 0 , 10);
    //i =  index - 9 =' HTTP/1.1'
    //copy to test the last 9 chars
    int j = 0;
    for (int i = index -  9 ; i < index; i++)
    {
        test[j] = request[i];
        j++;
    }
    //check if the last 9 chars are a match to to HTTP/1.1
    if(strcmp(test, " HTTP/1.1") != 0){
        if(strcmp(test, " HTTP/1.0") != 0){
            buildResponse(new_fd, BadRequest, NULL); 
            return; 
        }
    }
    // printf("this is from request: %s\n", test);
    //check there 3 words in the request
    if (counter != 3)
    {
        buildResponse(new_fd, BadRequest, NULL); 
        return;       
    }
    
    
    char request_line[BUFLEN];
    memset(request_line , 0 , BUFLEN);
    strncpy(request_line, request , index);
    // printf("request_line:  %s\n",request_line);
    // printf("counter: %d\n", counter);
    // if(strstr(request_line, "/favicon.ico")!= NULL )
    //     return;
    
    //if there is another method (POST)- not GET 
    char * check = strstr(request_line , "GET");
    if (check == NULL)
    {
        // return error message "501 not supported", as in file 501.txt
        buildResponse(new_fd, NotSupport, NULL);
        return;
    }
    
    //if there is no path
    check = strstr(request_line , "/");
    if (check == NULL)
    {
        // return error message "404 Not Found", as in file 404.txt.
        buildResponse(new_fd, NotFound, NULL);
        return;
    }
    if (strcmp(check , "/1.1")== 0 && strcmp(check , "/1.0")== 0 )
    {
        // printf("miss slase at the bignning\n");
        
        buildResponse(new_fd, NotFound, NULL);
        return;
    }
    
    char temp_path[BUFLEN];
    char *path = (char*)malloc(BUFLEN*sizeof(char));
    if (path == NULL)
    {
        buildResponse(new_fd, IntrnalError, NULL);
        return;
    }
    memset(path , 0 ,BUFLEN);
    memset(temp_path , 0 ,BUFLEN);
    
    
    path[0] = '.'; 
    //index- 13 ===>  ' HTTP/1.1\r\n\0 ' = 13  
    strncpy(temp_path, check, index - 13 );
    //if there is no path
    if (strcmp(temp_path, "") == 0 || temp_path == NULL)
    {
        // return error message "404 Not Found", as in file 404.txt. 
        buildResponse(new_fd, NotFound, NULL);
        free(path);
        return;
    }
    strcat(path , temp_path);
    // printf("temp path is: '%s'\n", temp_path);
    // printf("check is: '%s'\n", check);

    

    // printf("path is: '%s'\n", path);
    //if there is no path
    check = strstr(path , "/");
    if (check == NULL)
    {
        // return error message "404 Not Found", as in file 404.txt.
        buildResponse(new_fd, NotFound, NULL);
        free(path);
        return;
    }
    //check permissions all the path
    int i = 0 ;
    int counter0 = 0;
    int index0 =(int)strlen(path);
    while (i <  index0)
    {
        if (path[i]  == '/')
        {
            path[i] = '\0';
            counter0++; 
        }
        i++;
    }
    i =0;
    struct stat fs;
    while (i < counter0)
    {
        // printf("path: %s\n",path);

        if (stat(path, &fs) == -1){
            buildResponse(new_fd, NotFound, NULL);
            free(path);
            return;
        }
        if (S_ISREG(fs.st_mode)){ 
            if( fs.st_mode & S_IROTH ){
                // printf("read\n ");
            }else{
                buildResponse(new_fd, Forbidden, NULL);
                free(path);
                return;
            }
        }
        else if (S_ISDIR(fs.st_mode)){
            if( fs.st_mode & S_IXOTH ){
                // printf("execute\n");
            }else{
                buildResponse(new_fd, Forbidden, NULL);
                free(path);
                return;
            }
        }

        int j =0;
        while( j< index0){
            if (path[j] == '\0')
            {
                path[j] = '/';
                break;
            }
            j++;
        }
        i++;
    }
    
    // printf("path: %s\n",path);
    //it seemes the request is ok and move it to build response 
    ////----------------file and dir--------------
    buildResponseFileOrDir(path, new_fd);
    return;

}

void buildResponse(int new_sock_fd, int num, char *path){
    // time: get time by a format 
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    //timebuf holds the correct format of the current time.
   //i call it request but it really a response
   //start build up a response of an error by types :  302, 400, 403, 404, 500,501
    char request [4000];
    request[0] = '\0';
    strcat(request , HTTP);
    // add the name of the error to the first line
    if (num == Found)
    {
        strcat(request , found302);
    }
    else if (num == BadRequest)
    {
        strcat(request , BadRequest400);
    }else if (num == Forbidden)
    {
        strcat(request , forbidden403);
    }
    else if (num == NotFound)
    {
        strcat(request , notFound404);
    }
    else if (num == IntrnalError)
    {
        strcat(request , intrnalError500);
    }
    else if (num == NotSupport)
    {
        strcat(request , notSupport501);
    }
    //add the server version and the date
    strcat(request , newLine);        
    strcat(request , ServerWebserver);        
    strcat(request , newLine);     
    strcat(request , Date);       
    strcat(request , timebuf);        
    strcat(request , newLine);  
    //Location: <path>/ 
    if (num == Found)
    {
        //in case of 302 we add the right location of the folder
        strcat(request , Location );      
        strcat(request , path );
        free(path);
        strcat(request , "/" );      
        strcat(request , newLine );          
    }
    
    strcat(request , ContentType);  
    strcat(request , newLine);     

    strcat(request , ContentLength);        
    if (num == Found)
    {
        strcat(request , length302);
    }
    else if (num == BadRequest)
    {
        strcat(request , length400);
    }else if (num == Forbidden)
    {
        strcat(request , length403);
    }
    else if (num == NotFound)
    {
        strcat(request , length404);
    }
    else if (num == IntrnalError)
    {
        strcat(request , length500);
    }
    else if (num == NotSupport)
    {
        strcat(request , length501);
    }
    strcat(request , newLine);     
    //we close connection right after the answer
    strcat(request , ConnectionClose); 
    strcat(request , newLine); 
    strcat(request , newLine); 
    //build an html format of the error - to display 
    if (num == Found)
    {
        strcat(request , html0);
        strcat(request , found302);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , found302);
        strcat(request , html3);
        strcat(request , newLine);        
        strcat(request , "Directories must end with a slash.");
        strcat(request , newLine); 
        strcat(request , html4);
    }
    else if (num == BadRequest)
    {
        strcat(request , html0);
        strcat(request , BadRequest400);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , BadRequest400);
        strcat(request , html3);
        strcat(request , newLine); 
        strcat(request , "Bad Request.");
        strcat(request , newLine); 
        strcat(request , html4);
    }else if (num == Forbidden)
    {
        strcat(request , html0);
        strcat(request , forbidden403);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , forbidden403);
        strcat(request , html3);
        strcat(request , newLine); 
        strcat(request , "Access denied.");
        strcat(request , newLine); 
        strcat(request , html4);
    }
    else if (num == NotFound)
    {
        strcat(request , html0);
        strcat(request , notFound404);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , notFound404);
        strcat(request , html3);
        strcat(request , newLine); 
        strcat(request , "File not found.");
        strcat(request , newLine); 
        strcat(request , html4);
    }
    else if (num == IntrnalError)
    {
        strcat(request , html0);
        strcat(request , intrnalError500);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , intrnalError500);
        strcat(request , html3);
        strcat(request , newLine);
        strcat(request , "Some server side error.");
        strcat(request , newLine); 
        strcat(request , html4);
    }
    else if (num == NotSupport)
    {
        strcat(request , html0);
        strcat(request , notSupport501);
        strcat(request , html1);
        strcat(request , newLine); 
        strcat(request , html2);
        strcat(request , notSupport501);
        strcat(request , html3);
        strcat(request , newLine);
        strcat(request , "Method is not supported.");
        strcat(request , newLine); 
        strcat(request , html4);    
    }
    strcat(request , newLine);
    // printf("response: \r\n%s \n\n", request);
    int rc = write(new_sock_fd ,request ,(int)strlen(request)); 
    if (rc < 0){
        printf("write to client failed in build response\n");
    }
    
}

void buildResponseFileOrDir(char * path, int new_fd){    
    //init
    bool flag_dir = false;
    bool flag_file = false;
    char lastModified[128];
    //if there is only ' / '
    if (strcmp(path, "./") == 0)
    {
        //return the dir
        flag_dir = true;
    }
    extern int errno ;
    //stat - to get data on file or folder
    struct stat fs;
    if (stat(path, &fs) == -1){
        if (errno == 13)
        {
            buildResponse(new_fd, Forbidden, NULL);
        }else
            buildResponse(new_fd, NotFound, NULL);
        free(path);
        // printf("stat0 == -1\n");
        return;
    }
    
    //size of file or folder
    int content_length = (int)fs.st_size;
    char * file_folder_data = (char*)malloc(content_length*sizeof(char));
    if (file_folder_data ==  NULL)
    {
        buildResponse(new_fd, IntrnalError, NULL);
        printf("error in malloc in build buildResponseFileOrDir - file_folder_data\n");
        free(path);
        return;
    }
    memset(file_folder_data , 0, content_length);

    // check if the file is not regular
    if(S_ISREG(fs.st_mode) || S_ISDIR(fs.st_mode) ){
        //its ok
    }else
    {
        // in case the file is not regular
        buildResponse(new_fd , Forbidden ,NULL);
        free(path);
        free(file_folder_data);
        return;
    }
    // printf("Owner permissions: ");
    // if( fs.st_mode & S_IRUSR )
    //     printf("read ");
    //  printf("Group permissions: ");
    // if( fs.st_mode & S_IRGRP )
    //     printf("read ");
    // printf("Others permissions: ");
    // if( fs.st_mode & S_IROTH )
    //     printf("read ");
    // printf("Others permissions: ");
    //  if( fs.st_mode & S_IXOTH )
    //     printf("execute");

    //check if its a file or folder
    if (S_ISREG(fs.st_mode)){
        flag_file = true;
         // check access
        if( fs.st_mode & S_IROTH )
        { 
            // printf("there is permissions to others to read\n");
        }else{
            //if there is no access
            buildResponse(new_fd, Forbidden, NULL);
            // printf("there is no access \n");
            free(path);
            free(file_folder_data);
            return;
        }
    }
    
    //build a table in html page of all files
    if (S_ISDIR(fs.st_mode)){
        flag_dir = true;
        if( fs.st_mode & S_IXOTH )
        { 
            // printf("there is permissions to others to execute\n");
        }else{
            //if there is no access
            buildResponse(new_fd, Forbidden, NULL);
            // printf("there is no access \n");
            free(path);
            free(file_folder_data);
            return;
        }
        //check if its a folder if does have '/' at the end
        if (path[strlen(path)-1] != '/')
        {
            // printf("missing slase\n");
            free(file_folder_data);
            buildResponse(new_fd , Found , path);
            return;
        }
        
        DIR *folder;
        struct dirent *entry;
        folder = opendir(path);
        if(folder == NULL)
        {
            buildResponse(new_fd, IntrnalError, NULL);
            // printf("dir == NULL\n");
            free(path);
            free(file_folder_data);
            return;
        }
        // build an html of folder
        strcat(file_folder_data , html0);
        strcat(file_folder_data , "Index of ");
        strcat(file_folder_data , path);
        strcat(file_folder_data , html1);
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , html2);
        strcat(file_folder_data , "Index of ");
        strcat(file_folder_data , path);
        strcat(file_folder_data , html3);
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , "<table CELLSPACING=8>");
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , "<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>");
        strcat(file_folder_data , newLine);
        
        //go throw the folder , file by file/folder - to build a table of them
        while( (entry = readdir(folder)) )
        {
            char temp0[1];
            temp0[0] = '"'; 
            temp0[1] = '\0';
            strcat(file_folder_data , htmltr0);
            strcat(file_folder_data , temp0);
            strcat(file_folder_data , entry->d_name);
            strcat(file_folder_data , temp0);
            strcat(file_folder_data , ">");
            strcat(file_folder_data , entry->d_name);
            strcat(file_folder_data , htmltr2);
            //get data of file/folder by stat
            struct stat fs1 ;
            char d_name[500];
            memset(d_name , 0, 500);
            strcat(d_name , path);
            strcat(d_name , entry->d_name);
            if (stat(d_name, &fs1) == -1){
                buildResponse(new_fd, IntrnalError, NULL);
                // printf("stat == -1\n");
                free(path);
                free(file_folder_data);
                closedir(folder);
                return;
            }
            strftime(lastModified, sizeof(lastModified), RFC1123FMT, gmtime(&fs1.st_mtime));
            strcat(file_folder_data , lastModified);

            strcat(file_folder_data , htmltr3);
            strcat(file_folder_data , htmltr4);
            if (S_ISREG(fs1.st_mode)){
                char temp [20];
                sprintf(temp , "%d", (int)fs1.st_size);
                strcat(file_folder_data , temp);    
            }
            strcat(file_folder_data , htmltr3);
            strcat(file_folder_data , htmltr5);
            strcat(file_folder_data , newLine);
            //serce for index.html inside the folder-------------------
            if (strcmp(entry->d_name , "index.html") == 0)
            {
                // printf("found index.html\n");
                content_length = (int)fs1.st_size;
                //"./index.html" || d_name
                strcpy(path, d_name);
                fs = fs1;
                flag_dir = false;
                flag_file = true;
                // check access         
                if( fs1.st_mode & S_IROTH )
                { 
                    // printf("there is permissions to others to read\n");
                }else{
                    //if there is no access
                    buildResponse(new_fd, Forbidden, NULL);
                    // printf("there is no access \n");
                    free(path);
                    free(file_folder_data);
                    closedir(folder);
                    return;
                }
                break;
            }   
        }
        //done adding the files/folders and add the footer of the table
        strcat(file_folder_data , "</table>");
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , "<HR>");
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , "<ADDRESS>webserver/1.0</ADDRESS>");
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , newLine);
        strcat(file_folder_data , html4);

        if (closedir(folder) == -1)
        {
            buildResponse(new_fd, IntrnalError, NULL);        
        }
        content_length = strlen(file_folder_data);
    }
     // time: 
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    //timebuf holds the correct format of the current time.
    
    char *request = (char*)malloc((1000+content_length)*sizeof(char));
    if (request == NULL)
    {
        buildResponse(new_fd, IntrnalError, NULL);                    
        printf("error in malloc in build buildResponseFileOrDir - request\n");
        free(file_folder_data);
        free(path);
        return;
    }
    for (int i = 0; i < 1000+content_length; i++)
    {
        request[i] = '\0';
    }
    

    strcat(request , HTTP);
    strcat(request , ok200);     
    strcat(request , newLine);        
    strcat(request , ServerWebserver);        
    strcat(request , newLine);     
    strcat(request , Date);       
    strcat(request , timebuf);        
    strcat(request , newLine);  
    bool flag_type = false;
    if (flag_file)
    {
        //send to get mim type the path without the '.' at the begining
        char *temp_path = &path[1];
        char * type = get_mime_type(temp_path);
        if (type == NULL)
        {   //the type of file dont exist in get mim type -- so we remove content type header
            flag_type = true;
        }
        if (!flag_type)
        {
            strcat(request , connectionType);  
            strcat(request , type);
            strcat(request , newLine);     
        }
        
    }else if (flag_dir)
    {
        strcat(request , ContentType);  
        strcat(request , newLine);     
    }

    strcat(request , ContentLength);        
    char content_length_temp[20];
    sprintf(content_length_temp,"%d" ,content_length);
    strcat(request , content_length_temp);  
            
    strcat(request , newLine);     
    strcat(request , lastMOdified);
    //get last modified date of file or folder--from stat
    strftime(lastModified, sizeof(lastModified), RFC1123FMT, gmtime(&fs.st_mtime));

    strcat(request , lastModified);     
    strcat(request , newLine);     
    
    strcat(request , ConnectionClose); 
    strcat(request , newLine); 
    strcat(request , newLine); 

    //if its a file or folder -----send data
    if(flag_dir){
        strcat(request , file_folder_data); 
    }

    // printf("response: \r\n%s \n\n", request);
    // printf("request length: %d\n", (int)strlen(request));
    int rc = write(new_fd ,request ,strlen(request));
    if(rc < 0 )
        return;
    if (flag_file)
    {
        readFileAndSendToClient(path, new_fd);    
    }
   
    free(path);
    free(file_folder_data);
    free(request);

}

void readFileAndSendToClient(char *path, int new_fd){
	FILE *fd;    
	size_t size =1000;
  	//make it unsigned char because we send files and not string - inside there is special chars
    unsigned char buf[size];
    //put zreos in the buf
    bzero(buf,sizeof(buf));
    //open the file with rb- read binary
  	if ((fd = fopen(path,"rb")) < 0)
    { 
        printf("fopen failed --in read from file\n");
        buildResponse(new_fd, IntrnalError , NULL);
        return;
    }
    //go to the head of the file
    fseek(fd,0,SEEK_SET);
	int num_in_byte=1;
    //read and write to client
	while(num_in_byte>0){
        //read the file
		if( (num_in_byte = fread (buf,1,sizeof(buf), fd))< 0){
			printf("read from file failed--in read from file\n");
            buildResponse(new_fd, IntrnalError , NULL);
            return;
		}
        //write to client
		if(num_in_byte > 0 && buf != NULL){
            buf[size] = '\0';
			if( write(new_fd, buf, sizeof(buf)) < 0){ 
                printf("write to client failed --in read from file\n");
                buildResponse(new_fd, IntrnalError , NULL);
                fclose(fd);
                return;
			}
			bzero(buf,sizeof(buf));
		}
	}
	fclose(fd);
}
void handleASigPipe(int sig_pipe){
    // printf("found a sig pipe\n");
}