#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

char rec_buffer[1024];
int byte_received;
int byte_tx = 0;
int accel_x = 0;
int accel_y = 1;
int accel_z = 98;
int gyro_x = 0;
int gyro_y = 12;
int gyro_z = 16;
int total_byte_to_tx = 0;
char http_response[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<!DOCTYPE html>\r\n"
"<html>\r\n"
"<head>\r\n"
"  <meta charset=\"utf-8\">\r\n"
"  <title>IoT Sensor Page</title>\r\n"
"  <script>\r\n"
"    function getData() {\r\n"
"      fetch('/data')\r\n"
"        .then(response => response.json())\r\n"
"        .then(data => {\r\n"
"		 console.log(data); "
"          document.getElementById(\"result\").innerText = \"Accel: \" + data.accel + \" | Gyro: \" + data.gyro;"
"        })\r\n"
"        .catch(err => {\r\n"
"          console.error(\"Error fetching data:\", err);\r\n"
"          document.getElementById(\"result\").innerText = \"Error fetching data\";\r\n"
"        });\r\n"
"    }\r\n"
"  </script>\r\n"
"</head>\r\n"
"<body>\r\n"
"  <h1>IoT Sensor Dashboard</h1>\r\n"
"  <button onclick=\"getData()\">Get Sensor Value</button>\r\n"
"  <p id=\"result\">Not requested yet</p>\r\n"
"</body>\r\n"
"</html>\r\n";

char webpage[] =
"<!DOCTYPE html>\n"
"<html>\n"
"<head><title>My Test Page</title></head>\n"
"<body>\n"
"<h1>Hello, World!</h1>\n"
"<p>This is a simple webpage served from C server.</p>\n"
"</body>\n"
"</html>\n";


/*
 * Convert socket to IP address string.
 * addr: struct sockaddr_in or struct sockaddr_in6
 */
 const char *inet_ntop2(void *addr, char *buf, size_t size)
 {
	 struct sockaddr_storage *sas = addr;
	 struct sockaddr_in *sa4;
	 struct sockaddr_in6 *sa6;
	 void *src;

	 switch (sas->ss_family) {
	 case AF_INET:
		 sa4 = addr;
		 src = &(sa4->sin_addr);
		 break;
	 case AF_INET6:
		 sa6 = addr;
		 src = &(sa6->sin6_addr);
		 break;
	 default:
	 	return NULL;
	 }

	 return inet_ntop(sas->ss_family, src, buf, size);
}


int get_listener_socket()
{
	struct addrinfo hints,*res,*p;
	struct sockaddr_storage client_addr;
	int status = 0;
	int listener_fd,client_fd;
	int yes = 1;

	hints.ai_family 	= AF_INET;
	hints.ai_socktype 	= SOCK_STREAM;
	hints.ai_protocol	= 0;
	hints.ai_flags 		= AI_PASSIVE;

	memset(&hints,0,sizeof(hints));

	status = getaddrinfo(NULL,"10060",&hints,&res);
	if (status != 0)
	{
		fprintf(stderr, "gai error: %s\n",gai_strerror(status));
		exit(1);
	}

	for (p = res; p != NULL;p = p->ai_next)
	{
		if ((listener_fd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(listener_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) == -1)
		{
			perror("server: setsockopt");
			exit(1);			
		}
		if (bind(listener_fd,p->ai_addr,p->ai_addrlen) == -1)
		{
			perror("server: bind");
			continue;
		}

		break;
	}

	freeaddrinfo(res);

	if (listen(listener_fd,5) == -1)
	{
		perror("server: listen");
		exit(1);		
	}	

	return listener_fd;
}

void* getaddr(struct sockaddr* sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void add_to_pfd_list(int client_fd,int *fd_count, struct pollfd *pfds)
{
	pfds[*fd_count].fd = client_fd;
	pfds[*fd_count].events = POLLIN | POLLOUT;
	pfds[*fd_count].revents = 0;

	printf("poll count B: %d \n",*fd_count);
	printf("Addr B: %p \n",fd_count);
	(*fd_count)++;
	printf("poll count A: %d \n",*fd_count);
	printf("Addr A: %p \n",fd_count);
}

void new_connection(int listener,int *fd_count,struct pollfd *pfds)
{
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len;
	int client_fd;

	char remoteIP[INET6_ADDRSTRLEN];


	client_fd = accept(listener,(struct sockaddr *)&client_addr,&client_addr_len);

	if (client_fd == -1)
	{
		perror("socket:accept");
	}
	else
	{
		//printf("poll count A: %d \n",*fd_count);
		add_to_pfd_list(client_fd,fd_count,pfds);
		//printf("poll count B: %d \n",*fd_count);
		printf("pollserver: new connection from %s on socket %d\n",
		inet_ntop2(&client_addr, remoteIP, sizeof(remoteIP)),
					client_fd);

	}
}



void send_web_page(int fd)
{
	printf("Sending Webpage..\n");
	if( send(fd,(char*)http_response,sizeof(http_response),0) == -1)
	{
		perror("SERVER:SEND");
	}
	// if ( send(fd,(char*)webpage,sizeof(webpage),0) == -1)
	// {
	// 	perror("SERVER:SEND");
	// }
}

void client_data(struct pollfd *pfds, int *i)
{
	int byte_received = 0;
	//printf("i:%d \n",*i);
	if (pfds[*i].revents & POLLIN)
	{
		printf("INPUT \n");
		byte_received =  recv(pfds[*i].fd,rec_buffer,sizeof(rec_buffer),0);
		printf("BR:%d \n",byte_received);
		printf("%s \n",rec_buffer);
		if (byte_received == -1)
		{
			perror("server:rec");
		}
		else if (byte_received == 0)
		{
			printf("Client connection Closed \n");
			// TO DO: to remove the fd from the poll list.
		}

		else
		{
			if (strncmp(rec_buffer, "GET / ", 6) == 0)
			{
				// send the webpage
				send_web_page(pfds[*i].fd);

			}
			else if (strncmp(rec_buffer, "GET /data", 9) == 0)
			{
    			//int r = rand() % 100;  // random number 0–99
    			//printf("REACHED \n");
   				char json_response[128];
   				char temp_buffer[50];
   				int len = 0;
   				len = snprintf(temp_buffer,sizeof(temp_buffer),"{\"accel\": [%d,%d,%d],\"gyro\": [%d,%d,%d]}",accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z);
    			snprintf(json_response, sizeof(json_response),
     						   "HTTP/1.1 200 OK \r\n"
        						"Content-Type: application/json \r\n"
        						"Content-Length: %d\r\n"
        						"\r\n"
        						"%s",
        						len,temp_buffer );
    			//printf("%s \n",json_response);
    			byte_tx = send(pfds[*i].fd, json_response, strlen(json_response), 0);
    			// if (byte_tx == -1)
    			// {
    			// 	printf("TX ERROR:%d \n",byte_tx);
    			// 	printf("TOTAL BYTES TO TX: %ld \n",strlen(json_response));
    			// }
    			// else
    			// {
    			// 	printf("TX ERROR:%d \n",byte_tx);
    			// 	printf("TOTAL BYTES TO TX: %ld \n",strlen(json_response));
    			// 	printf("%s \n",json_response);    				
    			// }

			}

			else 
			{
				
			}
		}		
	}
	

}


void process_connections(int listener, int *fd_count,struct pollfd *pfds)
{
	//loop through the element of the pfds to check for the events
	int i;
	for (i = 0;i < *fd_count;i++)
	{
		if (pfds[i].revents & (POLLIN | POLLHUP))
		{
			if (pfds[i].fd == listener)
			{
				// call the new connection
				new_connection(listener,fd_count,pfds);
			}			
			else 
			{
				// call handle client data
				//printf("Browser Query \n");
				client_data(pfds,&i);
			}
		}
	}

}



int main(void)
{

	int listener;
	int fd_count = 0;
	

	struct pollfd *pfds = malloc (sizeof(*pfds) * 5);

	listener = get_listener_socket();

	if (listener == -1)
	{
		fprintf(stderr, "error getting listening socket\n");
        exit(1);
	}

	pfds[0].fd = listener;
	pfds[0].events = POLLIN;
	fd_count = 1;

	puts("pollserver: waiting for connections...");


	while(1)
	{
		int poll_count = poll(pfds,fd_count,-1);
		int i = 0;
		if (poll_count == -1) 
		{
			perror("poll");
            exit(1);
		}
		//printf("Entering the process_connections \n");
		process_connections(listener,&fd_count,pfds);
		// close(listener);
		// for( i = 0;i < poll_count;i++)
		// {
		// 	close(pfds[i].fd);
		// }
		

	}
	return 0;
}