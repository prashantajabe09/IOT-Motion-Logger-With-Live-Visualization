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

#define SENSOR_BUFFER_SIZE	50

// Path to webpage
#define FILE_PATH	"/mnt/d/SELF/C_WORK_SPACE/index.txt"

FILE *fp;

char web_buffer[4096];
char sensor_reading[SENSOR_BUFFER_SIZE];
char rec_buffer[1024];
int buf_len = 0;
int byte_received;
int byte_tx = 0;
int accel_x = 0;
int accel_y = 1;
int accel_z = 98;
int gyro_x = 0;
int gyro_y = 12;
int gyro_z = 16;
int total_byte_to_tx = 0;

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

/*
*	create socket for listening the incoming connection request
*/
int get_listener_socket()
{
	struct addrinfo hints,*res,*p;
	struct sockaddr_storage client_addr;
	int status = 0;
	int listener_fd,client_fd;
	int yes = 1;

	memset(&hints,0,sizeof(hints));

	hints.ai_family 	= AF_INET;		// IPv4
	hints.ai_socktype 	= SOCK_STREAM;  
	hints.ai_protocol	= 0;
	hints.ai_flags 		= AI_PASSIVE;   // open to any network address on the host

	

	status = getaddrinfo(NULL,"10013",&hints,&res); // gives the linked list of addrinfo structure
	if (status != 0)
	{
		fprintf(stderr, "gai error: %s\n",gai_strerror(status));
		exit(1);
	}

	for (p = res; p != NULL;p = p->ai_next)
	{
		// Create socket descriptor to be used later for communication
		if ((listener_fd = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		// Set socket options. Set for Reuse, useful for connect after crashing.
		if (setsockopt(listener_fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) == -1)
		{
			perror("server: setsockopt");
			exit(1);			
		}

		// Binds the socket descriptor with the IP address.
		if (bind(listener_fd,p->ai_addr,p->ai_addrlen) == -1)
		{
			perror("server: bind");
			continue;
		}

		break;
	}

	// frees the memory used for linked list.
	freeaddrinfo(res);

	// socket starts listening for incoming connection request with upto 5 backlogs. after which kernel will start rejecting the request
	if (listen(listener_fd,5) == -1)
	{
		perror("server: listen");
		exit(1);		
	}	

	return listener_fd;
}

// return the IP address according to the domain
void* getaddr(struct sockaddr* sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// add the new client file descritor to the array of the file descritpor
void add_to_pfd_list(int client_fd,int *fd_count, struct pollfd *pfds)
{
	pfds[*fd_count].fd = client_fd;
	pfds[*fd_count].events = POLLIN | POLLOUT;
	pfds[*fd_count].revents = 0;

	//printf("poll count B: %d \n",*fd_count);
	//printf("Addr B: %p \n",fd_count);
	(*fd_count)++;
	//printf("poll count A: %d \n",*fd_count);
	//printf("Addr A: %p \n",fd_count);
}

// delete the closed file descriptor from array of file descriptor used for polling
void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	//Copy the one from the end over this one
	pfds[i] = pfds[*fd_count-1];
	(*fd_count)--;
}

// handles the new connection
void new_connection(int listener,int *fd_count,struct pollfd *pfds)
{
	struct sockaddr_storage client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	int client_fd;

	char remoteIP[INET6_ADDRSTRLEN];

	// accept the connection from the client.
	client_fd = accept(listener,(struct sockaddr *)&client_addr,&client_addr_len);

	if (client_fd == -1)
	{
		perror("socket:accept");
	}
	else
	{
		add_to_pfd_list(client_fd,fd_count,pfds);
		printf("pollserver: new connection from %s on socket %d\n",
		inet_ntop2(&client_addr, remoteIP, sizeof(remoteIP)),
					client_fd);

	}
}


// send the webpage on request from browser.
void send_web_page(int fd)
{
	char response[128];
	int fileLen = 0;
	int read_length = 0;
	int write_length = 0;
	fp = fopen(FILE_PATH,"r");
	if(! fp){
        perror("\n\n   bin file not found");
        exit(0);
    }

    // Go to the end of file
   	fseek(fp, 0, SEEK_END);

   	// number of bytes
	fileLen=ftell(fp);

	// return to the start of the file.
	fseek(fp, 0, SEEK_SET);

	// read file at size of 1 byte.
	read_length = fread(web_buffer, 1, fileLen, fp);
	
	// create buffer of http response
	write_length = snprintf(response, sizeof(response),
				   "HTTP/1.1 200 OK \r\n"
				"Content-Type: text/html \r\n"
				"Content-Length: %d \r\n"
				"\r\n",
				read_length);

	// Send the HTTP header
	if( send(fd,(char*)response,strlen(response),0) == -1)
	{
		perror("SERVER:SEND");
	}
	
	// Send the webpage body
	if( send(fd,(char*)web_buffer,strlen(web_buffer),0) == -1)
	{
		perror("SERVER:SEND");
	}
}

// handles the client data
void client_data(struct pollfd *pfds, int *i,int *fd_count)
{
	int byte_received = 0;
	
	// Check if Input event on socket.
	if (pfds[*i].revents & POLLIN)
	{
		// Dummy read
		byte_received =  recv(pfds[*i].fd,rec_buffer,sizeof(rec_buffer),MSG_PEEK);
		

		int sender_fd = pfds[*i].fd;
		if (byte_received <= 0)
		{
			recv(pfds[*i].fd,rec_buffer,sizeof(rec_buffer),0);
			if (byte_received == 0)
			{
				// Connectionclosed
				printf("pollserver: socket %d hungup\n", sender_fd);				
			}
			else
			{	
				perror("recv");
			}
			close(pfds[*i].fd);
			del_from_pfds(pfds, *i, fd_count);
			(*i)--;
		}
		else
		{
			recv(pfds[*i].fd,rec_buffer,sizeof(rec_buffer),0);

			if (strncmp(rec_buffer, "GET / ", 6) == 0)
			{
				send_web_page(pfds[*i].fd);

				// No more send from this fd
				shutdown(pfds[*i].fd, SHUT_WR);
			}
			else if (strncmp(rec_buffer, "GET /data", 9) == 0)
			{
   				char json_response[128];

   				int len = strlen(sensor_reading);

    			snprintf(json_response, sizeof(json_response),
     						   "HTTP/1.1 200 OK \r\n"
        						"Content-Type: text/plain \r\n"
        						"Content-Length: %d\r\n"
        						"\r\n"
        						"%s",
        						len,sensor_reading);

    			byte_tx = send(pfds[*i].fd, json_response, strlen(json_response), 0);

			}
			else if (strncmp(rec_buffer, "GET /favicon.ico", 15) == 0) 
			{
    			const char *resp = "HTTP/1.1 204 No Content\r\n\r\n";
    			send(pfds[*i].fd, resp, strlen(resp), 0);
    			shutdown(pfds[*i].fd, SHUT_WR);
    			//close(client_fd);
			}
			else 
			{
				// read sensor data received from esp8266
				byte_received = recv(pfds[*i].fd, sensor_reading + buf_len, SENSOR_BUFFER_SIZE - buf_len, 0);
				//printf("byte_received:%d \n",byte_received);
				if (byte_received > 0) {
    				buf_len += byte_received;
    				sensor_reading[buf_len] = '\0'; 

    				char *newline;
    				// logic to handle the incomplete data
    				while ((newline = strchr(sensor_reading, '\n')) != NULL) {
       					 *newline = '\0'; // terminate message

        				// shift remaining data to front of buffer
        				int remaining = buf_len - (newline - sensor_reading + 1);
       					 memmove(sensor_reading, newline + 1, remaining);
        				buf_len = remaining;
    				}
				}

			}
		}		
	}
	

}

// Handles the new connections
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
				client_data(pfds,&i,fd_count);
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
		process_connections(listener,&fd_count,pfds);

	}
	return 0;
}