#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <iostream>
#include <vector>
#include <regex>
#include <string>

#include <pthread.h>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

std::string path;

void process_list(int sock,std::string path, std::vector<std::string> &list)
{
	std::regex rex;
	std::smatch match;
	std::string file;
	rex.assign("^GET\\s+(/[-=?&./a-zA-Z0-9]*)\\s+HTTP/1.(0|1)");
	if(!std::regex_search(list[0],match,rex))
	{
		std::string message = "HTTP/1.0 400 Bad Request\r\n\r\n";
		send(sock,message.c_str(),message.length(),0);
		return;
	}
	file = match[1];
	size_t pos = file.find("?");
	if(pos != std::string::npos)
	{
		file.erase(pos,file.length()-pos);
	}
	if(file.length() == 1)
	{
		file = "/index.html";
	}
	
	if(path.back() == '/')
	{
		path.erase(path.end()-1);
	}
	path+=file;
	int fd = open(path.c_str(),O_RDONLY);
	if(fd == -1)
	{
		std::string message = "HTTP/1.0 404 Not Found\r\n\r\n";
		send(sock,message.c_str(),message.length(),0);
		return;	
	}
	struct stat st;
	stat(path.c_str(),&st);
	for(int i = 1; i< list.size();i++)
	{
	}
	std::string message = "HTTP/1.0 200 OK\r\n";
	message +="Content-Type: text/html\r\n";
	message +="Content-Length: ";
	message +=std::to_string(st.st_size);
	message +="\r\n\r\n";
	void * filebuf = malloc(st.st_size);
	read(fd,filebuf,st.st_size);
	close(fd);
	send(sock,message.c_str(),message.length(),0);
	send(sock,filebuf,st.st_size,0);
	free(filebuf);
}

void *thread_proc(void * sock)
{
	int  ssocket = *((int*)(sock));
	char buf[1024];
	std::vector<std::string> request;
	bool go = true;
	bool nl = false;
	int fd = open("/home/box/log.txt",O_APPEND);
	do
	{
		memset(buf,0,1024);
		int size = recv(ssocket,buf,sizeof(buf),0);
		if(size == 1)
		{
			if(buf[0] == '\n')
			{
				go = false;
			}
		}
		else 
		{
			if(size == 2)
			{
				if(buf[0] == '\r' && buf[1] == '\n' && nl)
				{
					go = false;
				}
			}
			else 
			{
				if((buf[size-1] == '\n') &&( buf[size -3] == '\n') && (buf[size -2] == '\r') &&( buf[size-4] == '\r'))
				{
					go =false;
				}
				else 
				{
					if((buf[size-1] == '\n' && buf[size-2] == '\r') || (buf[size-1] == '\r'))
					{
						nl=true;
					}
				}
			}
		}
		write(fd,buf,size);
		request.push_back(buf);
	}while(go);
	close(fd);
	process_list(ssocket,path,request);
	shutdown(ssocket,SHUT_RDWR);
	close(ssocket);
}

int main(int argc, char **argv)
{
	int opt;
	int optcnt=0;
	char host[15];
	int port;
	int soc,ssocket;
	while((opt = getopt(argc,argv,"h:p:d:")) != -1)
	{
		switch (opt)
		{
			case 'h':
				++optcnt;
				strcpy(host,optarg);
				break;
			case 'p':
				++optcnt;
				port = atoi(optarg);
				break;
			case 'd':
				++optcnt;
				path=optarg;
				break;
		}
	}
	if(optcnt!=3)
	{
		std::cerr<<"Need parametrs"<<std::endl;
		return -1;
	}
	daemon(0,0);
	soc = socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in sa;
	memset(&sa,0,sizeof(sockaddr_in));
	sa.sin_family=AF_INET;
	sa.sin_port=htons(port);
	sa.sin_addr.s_addr = inet_addr(host);
	bind(soc,(sockaddr *)&sa,sizeof(sockaddr_in));
	listen(soc,SOMAXCONN);
	while(ssocket = accept(soc,0,0))
	{
		int s = ssocket;
		pthread_t thread;
		pthread_create(&thread,NULL,thread_proc,&s);
		pthread_detach(thread);
	}
	close(soc);
	return 0;
}
