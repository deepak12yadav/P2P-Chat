#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h> 
#include <arpa/inet.h>
#include<fcntl.h>
#include <sys/sendfile.h>
#include<bits/stdc++.h>
#include <cstdio>
using namespace std;

#define BUFSIZE 1024
#define PORT 8888

struct USER_INFO{
	map<string,pair<string,string> >nametoip;
	map<string,string>iptoname;
	void add(string name,string ip,string port){
		nametoip[name]={ip,port};
		iptoname[ip]=name;
	}
};
struct USER_INFO User_info;

struct dynamicinfo{
	set<int>Client_fd;	//connected clients
	map<int,string>soc_name;
	map<string,int>name_soc;
	void add(int a,string name){
		Client_fd.insert(a);
		soc_name[a]=name;
		name_soc[name]=a;
	}
	void del(int a){
		Client_fd.erase(a);
		close(a);
	}
};

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(){
	//Hard Coding user_info
	User_info.add("baba","10.145.213.21","8888");
	User_info.add("deepak","10.145.189.191","8888");
	User_info.add("ayush","10.5.27.111","8888");

	//---------------------------initializations--------------------------------------------------------------
	int master_socket,opt,activity,max_sd,new_socket,sd,valread,status;	//master_socket for listening
	struct dynamicinfo Dynamic;
	struct sockaddr_in address; /* server's addr */
	char message_s[BUFSIZE];
	fd_set readfds,writefds;
	int addrlen = sizeof(address);
	char *message = "Hey, you welcome in the p2p chat  \r\n";

	//--------------------------------------------------------------------------------------------------------

	master_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (master_socket < 0) 
	    error("ERROR opening socket");

	//set master socket to allow multiple connections , this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    /*
	* build the server's Internet address
	*/
	bzero((char *) &address, sizeof(address));

	/* this is an Internet address */
	address.sin_family = AF_INET;

	/* let the system figure out our IP address */
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	/* this is the port we will listen on */
	address.sin_port = htons((unsigned short)PORT);

	/* 
	* bind: associate the parent socket with a port 
	*/
	if (bind(master_socket, (struct sockaddr *) &address, sizeof(address)) < 0) 
		error("ERROR on binding");

	cout<<"*****Welcome to the chat application *****\nYou are listening on port "<<PORT<<endl;

	//try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 5) < 0)
    {
        error("listen");
    }

    while(1){
    	//clear the socket set
        FD_ZERO(&readfds);
  
        //add master socket to set
        FD_SET(master_socket, &readfds);
        FD_SET(0,&readfds);
        FD_SET(1,&writefds);
        max_sd = master_socket;
        //add child sockets to set
        for(auto sd:Dynamic.Client_fd){
        	if(sd > 0)
                FD_SET( sd , &readfds);
             
            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }
        struct timeval timeout;      
	    timeout.tv_sec = 60;
	    timeout.tv_usec = 0;
        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , &timeout);
        if ((activity < 0) && (errno!=EINTR)) 
        {
            printf("select error");
            continue;                          
        }
        //If timout happens
        if(activity==0){
        	cout<<"Timeout happens , all clients are disconnected \n";
        	Dynamic.Client_fd.clear();
        	continue;
        }
        if(FD_ISSET(1, &writefds)){
        	bzero(message_s,BUFSIZE);
            if((write(1,message_s,BUFSIZE))<0){
            	error("Eror writing to STDOUT");
            }
        }
        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) 
        {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                error("accept");
            }

            string m_ip=string(inet_ntoa(address.sin_addr));
            string m_port=to_string(ntohs(address.sin_port));
            // cout<<"connection request from :"<<m_ip<<" "<<m_port<<endl;
            //Check if the user is allowed to connect
            if(User_info.iptoname.find(m_ip)==User_info.iptoname.end()){
            	char* notallowed="You are not registered in this chat , kinldy contact the administrator\n";
            	if( send(new_socket, notallowed, strlen(notallowed), 0) != strlen(notallowed) ) 
	            {
	                perror("error while sending ");
	            }
	            continue;
            }
          
            cout<<"You are connected with :*** ip::"<<m_ip<<" port::"<<m_port<<" *** at socket -- "<<new_socket<<endl;
        
            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) ) 
            {
                perror("error while sending ");
            }
            //************************Assuming it is one of the user we have inofrmation about 
            Dynamic.add(new_socket,User_info.iptoname[m_ip]);
        }
        if (FD_ISSET(0 ,&readfds)) 
        {
            bzero(message_s,BUFSIZE);
            if((read(0,message_s,BUFSIZE))<0){
            	error("Eror reading from STDIN");
            }
            char input[BUFSIZE];
            strcpy(input,message_s);
            char* temp=strtok(input,"/");
            if(User_info.nametoip.find(string(temp))!=User_info.nametoip.end()){
            	if(Dynamic.name_soc.find(string(temp))!=Dynamic.name_soc.end()){
            		if( send(Dynamic.name_soc[string(temp)], message_s, strlen(message_s), 0) != strlen(message_s) ) 
		            {
		                perror("error while sending");
		            }
            	}
            	else{
            		struct addrinfo hints,*servinfo; // server's address
            		int sockfd;
            		char* hostname=(char*)User_info.nametoip[string(temp)].first.c_str();
            		char* port=(char*)User_info.nametoip[string(temp)].second.c_str();
            		memset(&hints, 0, sizeof(hints));

					hints.ai_family = AF_INET; // use IPv4 
					hints.ai_socktype = SOCK_STREAM;
					hints.ai_flags = AI_PASSIVE; // fill in my IP for me
            		if((status=getaddrinfo(hostname, port, (struct addrinfo*)&hints, &servinfo))!=0)
					{
						error("[ERROR] getaddrinfo()");						
					}

					struct addrinfo *p;		//To traverse the linked list 
					for(p=servinfo;p!=NULL;p=p->ai_next){
						if((sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol))==-1){
							perror("[ERROR] Client : socket");
							continue;
						}
						if(connect(sockfd, p->ai_addr, p->ai_addrlen)==-1){
							close(sockfd);
							perror("[ERROR] client: connect");
							continue;
						}
						break;
					}
					if(p==NULL){
						error("[ERROR] Failed to connect\n");
						
					}

					char ip4[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &(((struct sockaddr_in*)p->ai_addr)->sin_addr), ip4, INET_ADDRSTRLEN);
					cout<<"Connecting to :"<<temp<<" at ip:"<<hostname<<" port :"<<port<<"	socketfd  "<<sockfd<<endl;
					freeaddrinfo(servinfo); // all done with this structure
					if( send(sockfd,message_s, strlen(message_s), 0) != strlen(message_s) ) 
		            {
		                perror("error while sending ");
		            }
		            Dynamic.add(sockfd,string(temp));
            	}
            }
            else{
            	cout<<"Friend not in the list\n";
            	continue;
            }
        }
          
        //else its some IO operation on some other socket :)
        for (auto i:Dynamic.Client_fd) 
        {
            sd = i;
              
            if (FD_ISSET( sd , &readfds)) 
            {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , message_s, BUFSIZE)) == 0)
                {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                      
                    Dynamic.del(sd);
                }
                  
                //Echo back the message that came in
                else
                {
                    //set the string terminating NULL byte on the end of the data read
                    message_s[valread] = '\0';
                    cout<<Dynamic.soc_name[i]<<" :  ";
                    printf("%s\n", message_s);
                }
            }
        }

    }	
    return 0;
}