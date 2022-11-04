/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

// Forward Helper function
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);


int main(int argc, char** argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen; // unsigned int
    struct sockaddr_storage clientaddr; // 구조체, family, size, align

    /* Check command-line args */
    if (argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
    }

    listenfd = Open_listenfd(argv[1]); // listen socket 생성
    while(1) {
        clientlen = sizeof(clientaddr); 
        connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen); // 소켓과 주소 합침
        Getnameinfo((SA*)&clientaddr, clientlen, hostname, MAXLINE, // hostname과 port를 저장한다
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port); // 호스트와 port를 출력한다
        doit(connfd);
        Close(connfd);
    }
}


/* Transaction 실행 */
void doit (int fd)
{
    int is_static;
    struct stat sbuf; // 여러 데이터 형식에 맞는 struct
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio; // robust i/o, buffer structor

    /* Read request line and headers */
    Rio_readinitb(&rio, fd); // rio안의 소켓 data 추가
    Rio_readlineb(&rio, buf, MAXLINE); // rio data를 읽고 buf에 저장
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // buf에서 읽어와서 저장
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not implemented",
                    "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio); // 요청 헤더를 읽는다

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs); // 정적 or 동적 content 여부 체크
    if (stat(filename, &sbuf) < 0){                // 파일이 디스크 상에 있지 않다면 에러
        clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
        return;
    }

    if (is_static) { /* Server static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){ // 해당 파일이 읽기 권한을 가지고 있는지 확인
            clienterror(fd, filename, "403", "Forbidden",
                    "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);   // 클라이언트에게 제공
    }
    else{ /* Serve dynamic content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){ // 해당 파일이 읽기 권한을 가지고 있는지 확인
            clienterror(fd, filename, "403", "Forbidden",
                        "Tiny couldn't run the CGI program");

            return;
        }
        server_dynamic(fd, filename, cgiargs);    // 클라이언트에게 제공
    }
}


void clienterror(int fd, char* cause, char* errnum,
                    char* shortmsg, char* longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff""\r\n>", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n",body);
    

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}



