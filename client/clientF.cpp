//
//  clientF.cpp
//  Reliable UDP
//
//  Created by Snehil Vishwakarma on 10/13/15.
//  Copyright © 2015 Indiana University Bloomington. All rights reserved.
//

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fstream>
#include <time.h>

//GLOBAL DATA MEMBERS

struct r_udp                           //Structure of RELIABLE UDP HEADER
{
    bool ack,fin;
    unsigned int ackno,seqno;
    unsigned short int winsize;
    char data[512];
};

int ssthresh=64;                  //DEFAULT MAX WINDOW SIZE

//Functions used in program

int length(int no)           //Function calculating length of a number
{
    int q=0;
    if(no==0)
        return 1;
    while(no>0)
    {
        no=no/10;
        q++;
    }
    return q;
}
void error(const char *msg)       //Error message display and exit program
{
    perror(msg);
    exit(0);
}
r_udp formdata(bool start,char fname[],char mem[],unsigned short int cwind,int rackno,int rseqno)     //Function to form header to send to SERVER
{
    r_udp sobj;
    int w,x,y,z;
    sobj.ack=0;
    sobj.fin=0;
    if(start)
        sobj.ackno=rseqno;
    else
        sobj.ackno=rseqno+1;
    sobj.seqno=rackno+1;
    sobj.winsize=cwind;
    strncpy(sobj.data,fname,strlen(fname));
    
    if(sobj.ack==0)
        strncpy(mem,"0",1);
    else
        strncpy(mem,"1",1);
    strncat(mem," ",1);
    
    if(sobj.fin==0)
        strncat(mem,"0",1);
    else
        strncat(mem,"1",1);
    strncat(mem," ",1);
    
    z=3;
    x=sobj.ackno;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        strncat(mem," ",1);
        mem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(mem," ",1);
            w--;
        }
        while(x>0)
        {
            mem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    
    strncat(mem," ",1);
    
    z=z+y+1;
    x=sobj.seqno;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        mem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(mem," ",1);
            w--;
        }
        while(x>0)
        {
            mem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    strncat(mem," ",1);
    
    z=z+y+1;
    x=sobj.winsize;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        mem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(mem," ",1);
            w--;
        }
        while(x>0)
        {
            mem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    strncat(mem," ",1);
    
    strncat(mem,fname,strlen(fname));
    return sobj;
}
r_udp getdata(char mem[])       //Function to retreive received header information from buffer memory
{
    r_udp robj;
    int i,k;
    char temp[256];
    if(mem[0]=='0')       //Receiving ACK Flag
        robj.ack=0;
    else
        robj.ack=1;
    if(mem[2]=='0')       //Receiving FIN Flag
        robj.fin=0;
    else
        robj.fin=1;
    i=4;
    k=4;
    while(mem[k]!=' ')
        k++;
    bzero(temp,256);
    strncpy(temp, mem+i, k-i);       //Receiving ACK NO
    robj.ackno=atoi(temp);
    i=k+1;
    k=i;
    while(mem[k]!=' ')
        k++;
    bzero(temp,256);
    strncpy(temp, mem+i, k-i);       //Receiving SEQ NO
    robj.seqno=atoi(temp);
    i=k+1;
    k=i;
    while(mem[k]!=' ')
        k++;
    bzero(temp,256);
    strncpy(temp, mem+i, k-i);       //Receiving WINDOW SIZE
    robj.winsize=atoi(temp);
    i=k+1;
    k=(int)strlen(mem);
    bzero(robj.data,512);
    strncpy(robj.data,mem+i,k-i);       //Receiving DATA
    return robj;
}

//MAIN FUNCTION

int main(int argc, char *argv[])
{
    //Data Members
    
    int sfd,mode=1;
    long int n;
    socklen_t len;
    struct sockaddr_in sadd,cadd;
    struct hostent *hp;
    std::ofstream fi;
    char fname[256],mem[1024],fmem[1024];
    unsigned short int cwind,cwindF;
    r_udp sobj;
    
    fd_set  fdset;
    struct timeval timeout,timeout1;
    
    if (argc != 5)          //Check for having exactly 4 command line arguments for the client
    {
        error("Incorrect number of parameters!\n");
        exit(1);
    }
    
    sfd= socket(AF_INET, SOCK_DGRAM, 0);          //Creating DATAGRAM Socket for UDP connection
    if (sfd < 0)
        error("socket");
    
    sadd.sin_family = AF_INET;
    hp = gethostbyname(argv[1]);
    if (hp==0)
        error("Unknown host");
    
    bcopy((char *)hp->h_addr,
          (char *)&sadd.sin_addr,
          hp->h_length);
    sadd.sin_port = htons(atoi(argv[2]));
    
    len=sizeof(cadd);
    
    bzero(fname,256);
    strcpy(fname,argv[3]);
    
    r_udp *robj=new r_udp[cwind];
    int si=0,start=0,ci=0,count=0,DONE=0;
    bool isopen=0;
    
    cwindF=(unsigned short int)atoi(argv[4]);
    
    if(ssthresh>cwindF)         //MINIMUM of CLIENT_WINDOW and CONGESTION_WINDOW
    {
        ssthresh=cwindF;
    }
    
    int *seqrecv=new int[ssthresh];
    
    robj[si].ack=1;
    robj[si].ackno=0;
    robj[si].seqno=0;
    robj[si].fin=0;
    
    bzero(mem,1024);
    sobj=formdata(1,fname,mem,cwindF,robj[si].ackno,robj[si].seqno);
    //Send FILE REQUEST to SERVER
    n=sendto(sfd,mem,
             strlen(mem),0,(const struct sockaddr *)&sadd,len);
    if (n < 0)
        error("Sendto");
    
    FD_ZERO(&fdset);
    //FD_CLR(0, &fdset);
    FD_SET(sfd, &fdset);
    
    timeout.tv_sec=8;
    timeout.tv_usec=0;
    timeout1.tv_sec=8;
    timeout1.tv_usec=0;
    
    cwind=1;
    
    while (select(sfd+1,&fdset, NULL, NULL, &timeout1) && robj[si].fin==0)
    {
        
        if(ssthresh>cwindF)         //MINIMUM of CLIENT_WINDOW and CONGESTION_WINDOW
        {
            ssthresh=cwindF;
        }
        
        if(isopen==0)
        {
            bzero(mem,1024);
            //RECEIVING 1st PACKET from SERVER
            n = recvfrom(sfd,mem,1024,0,(struct sockaddr *)&cadd,&len);
            if (n < 0)
                std::cout<<"Error: reading from socket";
            robj[si]=getdata(mem);
            count=robj[si].seqno;
        }
        
        if(robj[si].ackno==0)       //BREAKING if "file does not exist" packet received
        {
            isopen=0;
            std::cout<<fname<<" FILE DOES NOT EXIST ON SERVER";
            break;
        }
        else
        {
            if(isopen==0)       //To see if FILE is open. FILE opened just once
            {
                fi.open(fname,std::ios::binary);
                fi.write(robj[si].data,strlen(robj[si].data));
                isopen=1;
                start=robj[si].seqno;
                start+=(strlen(robj[si].data));
                seqrecv[si]=robj[si].ackno;
                si++;
                
            }
            else
            {
                do
                {
                    bzero(mem,1024);
                    //RECEIVING PACKETS from SERVER
                    n = recvfrom(sfd,mem,1024,0,(struct sockaddr *)&cadd,&len);
                    if (n < 0)
                        std::cout<<"Error: reading from socket";
                    robj[si]=getdata(mem);
                    
                    bzero(fmem,1024);
                    strncpy(fmem,robj[si].data,strlen(robj[si].data));
                    
                    start=start+1+(int)(strlen(robj[si].data));
                    
                    if (start==robj[si].ackno)    //WRITING only if CORRECT SEQUENCE NO received
                    {
                        fi.write(fmem,strlen(fmem));
                    }
                    else
                        break;
                    seqrecv[si]=robj[si].ackno;
                    
                    if(robj[si].fin==1)
                    {
                        DONE=1;
                        break;
                    }
                    si++;
                    
                }while(robj[si].fin==0 && si<cwind && select(sfd+1,&fdset, NULL, NULL, &timeout));
                //RECEIVING till eiter FIN PACKET or RECEIVING WINDOW FULL or TIMEOUT
            }
        }
        
        //SENDING collective acknowledgments
        
        while (ci<si /*&& select(sfd+1,&fdset, NULL, NULL, &timeout)*/)
        {
            bzero(mem,1024);
            bzero(fname,256);
            sobj=formdata(0,fname,mem,cwind,count,seqrecv[ci]);
            //Sending ACKNOWLEDGMENT packets to Server
            n=sendto(sfd,mem,
                     strlen(mem),0,(const struct sockaddr *)&sadd,len);
            if (n < 0)
                error("Sendto");
            ci++;
        }
        if(DONE)
            break;
        
        if(mode==1)         //SLOW START
        {
            if(ci==si && cwind<ssthresh)        //SLOW START GROWING EXPONENTIALLY
                cwind*=2;
            else if(ci!=si)                 //TIMEOUT
            {
                ssthresh=cwind/2;
                cwind=1;
            }
            else if(cwind>=ssthresh)        //Switch to CONGESTION AVOIDANCE
                mode=2;
            
        }
        else if(mode==2)            //CONGESTION AVOIDANCE
        {
            if(ci!=si)                  //TIMEOUT
            {
                ssthresh=cwind/2;
                cwind=1;
                mode=1;
            }
            else if(ci==si)              //SLOW START GROWING LINEARLY
            {
                cwind+=1;
            }
        }
        
        si=0;
        ci=0;
        
    }
    
    if(isopen)
        fi.close();              //CLOSE the file when all packets received
    
    //Clearing dynamically allocated memories
    
    delete []robj;
    delete []seqrecv;
    
    close(sfd);         //Closing the socket
    return 0;
}
