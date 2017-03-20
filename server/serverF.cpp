//
//  serverF.cpp
//  Reliable UDP
//
//  Created by Snehil Vishwakarma on 10/13/15.
//  Copyright © 2015 Indiana University Bloomington. All rights reserved.
//

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fstream>
#include <arpa/inet.h>
#include <time.h>

//GLOBAL DATA MEMBERS

struct r_udp                     //Structure of RELIABLE UDP HEADER
{
    bool ack,fin;
    unsigned int ackno,seqno;
    unsigned short int winsize;
    char data[512];
};

std::ifstream fi;                //ifstream object to OPEN the file
int ssthresh=64;                  //DEFAULT MAX WINDOW SIZE

//Functions used in program

int length(int no)     //Function calculating length of a number
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
void error(const char *msg)  //Error message display and exit program
{
    perror(msg);
    exit(0);
}
r_udp getdata(char mem[])  //Function to retreive received header information from buffer memory
{
    int i,k;
    char temp[256],fname[256],fmem[1024];
    r_udp robj;
    
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
    bzero(fmem,1024);
    strncpy(fmem,mem+i,k-i);
    
    bzero(fname,256);
    strcpy(fname,fmem);
    bzero(robj.data,512);
    strncpy(robj.data,fname,strlen(fname));       //Receiving DATA
    
    return robj;
    
}
r_udp formdata(bool finish,int ack_add,bool err,int tack,char fmem[],unsigned short int swind,int rseqno,char data[])    //Function to form header to send to CLIENT
{
    int w,x,y,z;
    r_udp sobj;
    
    sobj.ack=1;
    if(finish)
        sobj.fin=1;
    else
        sobj.fin=0;
    if (err)
        sobj.ackno=0;
    else
        sobj.ackno=rseqno+ack_add;
    sobj.seqno=tack;
    sobj.winsize=swind;
    
    bzero(fmem,1024);
    if(sobj.ack==0)
        strncpy(fmem,"0",1);
    else
        strncpy(fmem,"1",1);
    strncat(fmem," ",1);
    
    if(sobj.fin==0)
        strncat(fmem,"0",1);
    else
        strncat(fmem,"1",1);
    strncat(fmem," ",1);
    
    z=3;
    x=sobj.ackno;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        strncat(fmem," ",1);
        fmem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(fmem," ",1);
            w--;
        }
        while(x>0)
        {
            fmem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    
    strncat(fmem," ",1);
    
    z=z+y+1;
    x=sobj.seqno;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        fmem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(fmem," ",1);
            w--;
        }
        while(x>0)
        {
            fmem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    strncat(fmem," ",1);
    
    z=z+y+1;
    x=sobj.winsize;
    y=length(x);
    z=z+y;
    
    if(x==0)
    {
        fmem[z]=x+48;
        z--;
    }
    else
    {
        w=y;
        while(w>0)
        {
            strncat(fmem," ",1);
            w--;
        }
        while(x>0)
        {
            fmem[z]=(x%10)+48;
            x=x/10;
            z--;
        }
    }
    strncat(fmem," ",1);
    strncat(fmem,data,strlen(data));
    strncpy(sobj.data, data, strlen(data));       //Copying DATA to send to CLIENT
    return sobj;
}

time_t Ertt=1,Drtt=0,Timeout=10;

void timeoutcalc(time_t Srtt)     //FUNCTION to calculate TIMEOUT
{
    Ertt=((0.875)*Ertt)+((0.125)*Srtt);     //Calculating ESTIMATED_TIMEOUT
    if(difftime(Ertt,Srtt)>=0)
        Drtt=((0.75)*Drtt)+((0.25)*(difftime(Ertt,Srtt)));     //Calculating DEVIATION
    else
        Drtt=((0.75)*Drtt)+((0.25)*(difftime(Srtt,Ertt)));     //Calculating DEVIATION
    Timeout=Ertt+4*Drtt;     //Calculating TIMEOUT
}

//MAIN FUNCTION

int main(int argc, char *argv[])
{
    
    int sfd,j,tack,start,tot,mode=1,round=0;
    long int n;
    socklen_t len;
    struct sockaddr_in sadd,cadd;
    char mem[1024],fmem[1024];
    unsigned short int swind,swindF;
    r_udp robj;
    time_t timer1=0,timer2=0,sample=0;
    int countr=0,pcountr=0;
    bool swtch=false;
    float percent=0;
    
    srand(2);     //Randomising 2 to get 50% drop probability while simulating LOSS & DELAY
    
    fd_set  fdset;
    struct timeval timeout,timeout1;
    
    FD_ZERO(&fdset);
    //FD_CLR(0, &fdset);
    FD_SET(sfd, &fdset);
    
    timeout.tv_sec=2;
    timeout.tv_usec=0;
    timeout1.tv_sec=2;
    timeout1.tv_usec=0;
    
    if (argc != 3)          //Check for having exactly 2 command line arguments for the client
    {
        error("Incorrect number of parameters!\n");
        exit(1);
    }
    
    sfd=socket(AF_INET, SOCK_DGRAM, 0);          //Creating DATAGRAM Socket for UDP connection
    if (sfd < 0)
        error("Opening socket");
    
    bzero((char *) &sadd, sizeof(sadd));
    sadd.sin_family=AF_INET;
    sadd.sin_addr.s_addr=INADDR_ANY;
    sadd.sin_port=htons(atoi(argv[1]));
    if (bind(sfd,(struct sockaddr *)&sadd,sizeof(sadd))<0)          //Bind the socket to SERVER ADDRESS
        error("binding");
    
    len = sizeof(cadd);
    //swindF=(unsigned short int)atoi(argv[2]);
    swind=1;
    
    bzero(mem,1024);
    n = recvfrom(sfd,mem,1024,0,(struct sockaddr *)&cadd,&len);         //Receive FILE REQUEST from CLIENT
    if (n < 0)
        std::cout<<"Error: reading from socket";
    
    
    robj=getdata(mem);
    swindF=robj.winsize;         //Setting SERVER_WINDOW as CLIENT_WINDOW
    start=robj.seqno;
    
    
    int si=0,ci=0;
    
    if(ssthresh>swindF)         //MINIMUM of SERVER_WINDOW and CONGESTION_WINDOW
    {
        ssthresh=swindF;
    }
    
    r_udp *sobj=new r_udp[ssthresh];
    int *ackrecv=new int[ssthresh];
    int *startarr=new int[ssthresh];
    
    fi.open(robj.data,std::ios::binary);            //Open the FILE
    tack=0;
    
    if(fi.is_open())            //Only allow sending if the file EXISTS
    {
        fi.seekg(0,std::ios::end);
        j=(int)fi.tellg();
        tot=j;
        fi.seekg(0);
        
        while(j>0 /*&& select(sfd+1,&fdset, NULL, NULL, &timeout1)*/)
        {
            if(ssthresh>swindF)         //MINIMUM of SERVER_WINDOW and CONGESTION_WINDOW
            {
                ssthresh=swindF;
            }
            
            while(si<swind && j>0)      //To send all collective packets before starting receving acknowledgments
            {
                if(j>=511)
                {
                    bzero(fmem,1024);
                    bzero(sobj[si].data,512);
                    fi.read(sobj[si].data,511);
                    tack++;
                    sobj[si]=formdata(0,511,0,tack,fmem,swind,start,sobj[si].data);
                    
                    ackrecv[si]=sobj[si].ackno+1;
                    
                    startarr[si]=start-1;
                    start+=512;
                    j=j-511;
                    //Taking time before sending the packets
                    time(&timer1);
                    if(rand()+1)      //DROPPING Packets while sending for SIMULATING LOSS and DELAY
                    {       //Sending when data size greater than packet size
                        n = sendto(sfd,fmem,strlen(fmem),
                                   0,(struct sockaddr *)&cadd,len);
                        if (n  < 0)
                            error("sendto");
                    }
                    si++;
                    
                }
                else
                {
                    bzero(fmem,1024);
                    bzero(sobj[si].data,512);
                    fi.read(sobj[si].data,j);
                    tack++;
                    sobj[si]=formdata(1,j,0,tack,fmem,swind,start,sobj[si].data);
                    //Taking time before sending the packets
                    time(&timer1);
                    //Sending when data size greater than packet size
                    n = sendto(sfd,fmem,strlen(fmem),
                               0,(struct sockaddr *)&cadd,len);
                    if (n  < 0)
                        error("sendto");
                    
                    startarr[si]=start-1;
                    start+=(strlen(sobj[si].data)+1);
                    j=j-(int)(strlen(sobj[si].data));
                    si++;
                    
                }
                if(j==0)
                {
                    //Condition when all file packets sent
                    si=0;
                    break;
                }
            }
            
            //Receiving collective acknowledgments
            
            while(ci<si /*&& select(sfd+1,&fdset, NULL, NULL, &timeout)*/)
            {
                bzero(mem,1024);
                //Receiving ACKNOWLEDGMENT packets from Client
                n = recvfrom(sfd,mem,1024,0,(struct sockaddr *)&cadd,&len);
                if (n < 0)
                    std::cout<<"Error: reading from socket";
                if(ci==0)
                {
                    time(&timer2);
                    sample=difftime(timer2,timer1);     //Taking SAMPLE TIME
                    if(sample<=Timeout)
                    {
                        timeoutcalc(sample);        //Calculate TIMEOUT
                    }
                    else
                        break;
                    
                }
                robj=getdata(mem);
                if(ackrecv[ci]!=robj.ackno)
                {
                    break;
                }
                else
                    ci++;
            }
            
            
            if(round==0 && ci==0)
            {
                round++;
            }
            else if(round==1 && ci==0)
                break;
            
            if(ci==si)          //Received all EXPECTED ACKNOWLEDGMENTS
            {
                
            }
            else if(ci<si)     //DID NOT received all EXPECTED ACKNOWLEDGEMENTS
            {
                fi.seekg(startarr[ci]);
                start=startarr[ci];
                j=tot-(start-(1*(si-ci)));
            }
            if(si!=0 && ci!=0)
            {
                pcountr=ci;
                countr+=pcountr;
                
                std::cout<<"\n Number of packets ackED this instance: "<<pcountr;
                std::cout<<"\n Number of packets expected to be ackED: "<<si;
                
                if(ci!=0)
                {
                    percent=(pcountr*100)/si;
                    std::cout<<"\n Percentage of packets sent this instance: "<<percent<<"%";
                }
            }
            
            if(mode==1)         //SLOW START
            {
                if(ci==si && swind<ssthresh)        //SLOW START GROWING EXPONENTIALLY
                    swind*=2;
                else if(ci!=si)                 //TIMEOUT
                {
                    ssthresh=swind/2;
                    swind=1;
                }
                else if(swind>=ssthresh)        //Switch to CONGESTION AVOIDANCE
                {
                    mode=2;
                    swtch=true;
                }
                
            }
            else if(mode==2)            //CONGESTION AVOIDANCE
            {
                if(ci!=si)                  //TIMEOUT
                {
                    ssthresh=swind/2;
                    swind=1;
                    mode=1;
                    swtch=true;
                }
                else if(ci==si)              //SLOW START GROWING LINEARLY
                {
                    swind+=1;
                }
            }
            
            if(swtch==true)
            {
                std::cout<<"\n Mode right now: ";
                if(mode==1)
                    std::cout<<" Slow Start! ";
                else if(mode==2)
                    std::cout<<" Congestion Avoidance! ";
                swtch=false;
            }
            
            si=0;
            ci=0;
            //std::cout<<"\n\nCHECK!";
            
        }
        fi.close();         //CLOSE the file when all packets sent
        
    }
    else            //Send a packet telling the CLIENT that the FILE doesn't exist
    {
        bzero(fmem,1024);
        bzero(sobj[si].data,512);
        sobj[si]=formdata(1,0,1,tack,fmem,swind,start,sobj[si].data);
        n = sendto(sfd,fmem,strlen(fmem),
                   0,(struct sockaddr *)&cadd,len);
        if (n  < 0)
            error("sendto");
    }
    
    //Clearing dynamically allocated memories
    
    delete []sobj;
    delete []ackrecv;
    delete []startarr;
    std::cout<<"\n";
    
    close(sfd);         //Closing the socket
    return 0;
}


