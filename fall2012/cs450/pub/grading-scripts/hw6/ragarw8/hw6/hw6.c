#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <math.h>
#include "hw6.h"
#include "cqueue.c"

char *sender_queue;
unsigned int window_size;
unsigned int rx_window_size;
unsigned int start_seq_no;
unsigned int rx_start_seq_no;
unsigned int rtt;
unsigned int deviation;
struct timeval timeout;

struct sockaddr_in peeraddr;

int sequence_number;
int expected_sequence_number = 0;

int min(int a, int b){
	return a<b?a:b;
}

unsigned timeval_to_msec(struct timeval *t) { 
	return t->tv_sec*1000+t->tv_usec/1000;
}

void msec_to_timeval(int millis, struct timeval *out_timeval) {
	out_timeval->tv_sec = millis/1000;
	out_timeval->tv_usec = (millis%1000)*1000;
}

unsigned current_msec() {
	struct timeval t;
	gettimeofday(&t,0);
	return timeval_to_msec(&t);
}

/* updates rtt and deviation estimates based on new sample */
void update_rtt(unsigned this_rtt) {
	// if this is the first packet, make an 'educated guess' as to the rtt and deviation
	if(sequence_number==0) {
		rtt = this_rtt;
		deviation = this_rtt/2;
	}
	else {
	  deviation = 0.7*deviation + 0.3*(abs(this_rtt - rtt));
	  rtt = 0.8*rtt + 0.2*this_rtt;
	}
	msec_to_timeval(rtt+4*deviation,&timeout);
}

int rel_connect(int socket,struct sockaddr_in *toaddr,int addrsize) {
		 peeraddr=*toaddr;
		 return 0;
}

int rel_rtt(int sock) {
		 return rtt;
}

struct senderQelements{
    char magic;
    char packet[MAX_PACKET];
    char ack_rxed:1;
    char retx:1;
    int packet_len;
    unsigned sent_time;
};


void send_all_unacked_packets_again(int sock){
    int n;
    n = getCurrentQSize();
    for(int i = 0; i < n ; i++){
        struct senderQelements *s;
        s = getNthElement(i);
        if(!s->ack_rxed){
            //todo:resend
            fprintf(stderr,"\r<RITESH>Retransmitted Packet - %d with rtt %d dev %d timeout - %d ms           \n",i+start_seq_no,rtt, deviation,timeval_to_msec(&timeout));
            s->retx = 1;
            //s->sent_time = current_msec(); //doesnt matter...will not be used if it is a retx
            sendto(sock, s->packet, s->packet_len, 0,(struct sockaddr*)&peeraddr,sizeof(peeraddr));
        }
    }
}

void rel_send(int sock, void *buf, int len)
{
    int flag_ack_checked;
    int flag_packet_dropped;
 	// make the packet = header + buf
	char packet[1400];
	struct hw6_hdr *hdr = (struct hw6_hdr*)packet;
	flag_ack_checked = 0;
	flag_packet_dropped = 0;
	memset(hdr,0,sizeof(struct hw6_hdr));
	hdr->sequence_number = htonl(sequence_number);
	memcpy(hdr+1,buf,len);

	fprintf(stderr,"\r<RITESH>Sent Packet - %d with rtt %d dev %d timeout - %d ms           \n",sequence_number,rtt, deviation,timeval_to_msec(&timeout));
	sendto(sock, packet, sizeof(struct hw6_hdr)+len, 0,(struct sockaddr*)&peeraddr,sizeof(peeraddr));

	// put packet in queue, flag = ack not rxed, seq_no
    struct senderQelements *s;
    s = calloc(sizeof(struct senderQelements),1);
    s->magic = 'S';
    s->ack_rxed = 0;
    s->retx = 0;
    s->packet_len = sizeof(struct hw6_hdr) + len;
    s->sent_time = current_msec();
    memcpy(s->packet, packet, s->packet_len);
    doQInsert((char *)s);

	// 1. If queue has no. of elements  == window size or if this is the close packet
    if(getCurrentQSize() == window_size){
        int ack_count;
        ack_count = 0;
	//      wait for acks from all
        while(ack_count < window_size) {
            if(getCurrentQSize() == 0){ //double check...otherwise we might go in infinite loop //remove
                break;
            }

            fd_set readset;
            FD_ZERO(&readset);
            FD_SET(sock,&readset); 

            struct timeval t = timeout; // select changes the timeout parameter on some platforms, so make a copy

            int rdy = select(FD_SETSIZE,&readset,0,0,&t);

            if(rdy==0) {
                //send all unacked packets again
                send_all_unacked_packets_again(sock);
                flag_packet_dropped = 1;
                // if we timed out, send again double the timeout value
                msec_to_timeval(min(5000,2*timeval_to_msec(&timeout)), &timeout);
                /*
                fprintf(stderr,"\rSent Packet %d with rtt %d dev %d timeout %d ms           ",sequence_number,rtt, deviation,timeval_to_msec(&timeout));
                sendto(sock, packet, sizeof(struct hw6_hdr)+len, 0,(struct sockaddr*)&peeraddr,sizeof(peeraddr));
                //s->retx=1;
                fprintf(stderr,"\r<RITESH>Retransmitting - Packet %d with rtt %d dev %d timeout %d ms           \n",sequence_number,rtt, deviation,timeval_to_msec(&timeout));
                */
            }
            else if(rdy==-1) {
                perror("select error");
            }
            else {
                char incoming[1400];
                struct sockaddr_in from_addr;
                unsigned int addrsize = sizeof(from_addr);
                int recv_count=recvfrom(sock,incoming,1400,0,(struct sockaddr*)&from_addr,&addrsize);
                if(recv_count<0) {
                    perror("When receiving packet.");
                    return;
                }

                struct hw6_hdr *hdr = (struct hw6_hdr*)incoming;
                if(ntohl(hdr->ack_number) >= start_seq_no && ntohl(hdr->ack_number) <= (start_seq_no + window_size - 1)) {
                    fprintf(stderr,"\r<RITESH>Got ack for %d       \n",ntohl(hdr->ack_number));
                    fprintf(stderr,"<RITESH> Ack rxed - %d , start_seq_no , redundant log\n", (ntohl(hdr->ack_number)) - start_seq_no, start_seq_no);
                    struct senderQelements *s;
                    s = getNthElement((ntohl(hdr->ack_number)) - start_seq_no);
                    fprintf(stderr,"<RITESH> nth element in Q packet->seq_no = %d\n", ntohl(((struct hw6_hdr *)s->packet)->sequence_number));
                    if(s->ack_rxed){
                        fprintf(stderr,"<RITESH> RE-Ack rxed - %d , doing nothing\n", (ntohl(hdr->ack_number)) - start_seq_no);
                    }
                    else{
                        ack_count++;
                        if(!s->retx){
                            // if this is an ack for our present packet, update the rtt and exit
                            update_rtt(current_msec() - s->sent_time);
                        }
                    }
                    s->ack_rxed = 1;
                    //break;
                }

                // if it's not an ack, it's the end of the stream. ACK it. 
                // Scenario: 1. Sender sends close, 2. rxer sends ack(it is lost), 3. rxer sends close, 4. sender recives close in rel_send()
                /*RITESH: todo?? Handle the close part here?? */
                if(! (hdr->flags & ACK)) {
                    // ack whatever we have so far
                    struct hw6_hdr ack;
                    ack.flags = ACK;
                    if(ntohl(hdr->sequence_number) == expected_sequence_number) {
                        expected_sequence_number++;
                    }
                    else {
                        fprintf(stderr,"Unexpected non-ACK in rel_send(), size %d. Acking what we have so far. \n",recv_count);
                    }
                    ack.ack_number = htonl(expected_sequence_number-1);
                    fprintf(stderr,"\r<RITESH>Exception: Sending ACK - %d  for a non-ack packet receivd        \n",ntohl(ack.ack_number));
                    sendto(sock, &ack, sizeof(ack), 0,(struct sockaddr*)&peeraddr,sizeof(peeraddr));
                }		 
            }
            flag_ack_checked = 1;
        }
    }
    /*
    else{
        //      return, so that user can send next packet.
        return;
    }
    */
    if(len == 0 && !flag_ack_checked){
        perror("Some problem");
        exit(2);
    }
    if(flag_ack_checked){
        while(getCurrentQSize() > 0){ //make_raghvan_change
            struct senderQelements *s;
            s = doQDelete();
            free(s);
        }
        start_seq_no = start_seq_no + window_size; //make_raghvan_change
	    if(start_seq_no != sequence_number+1){
	        exit(20); //assert_remove
        }
        if(flag_packet_dropped == 1){
            decrease_window_size();
        }
        else{
            increase_window_size();
        }
    }
	sequence_number++;
}

int rel_socket(int domain, int type, int protocol) {
	/* start out with large timeout and rtt values */
	rtt = 500;
	deviation = 50;
	timeout.tv_sec = 0; 
	timeout.tv_usec = 700000; // rtt + 4*deviation ms
	sequence_number = 0;

    window_size = 1;
    expected_sequence_number = 0;
	start_seq_no = 0;

	return socket(domain, type, protocol);
}

void increase_window_size()
{
    window_size *= 2;
}

void decrease_window_size()
{
    if(window_size > 1)
        window_size /= 2;
}

unsigned int last_packet_seq_no_sent_up = -1;

struct rxerQelements{
    char magic;
    int flag_has_data;
    char packet[MAX_PACKET];
    char ack_sent:1;
    int packet_len;
}rlist[100];

//will return -1, if not found, otherwise will return the index, which can be used with getNthElement
int isPacketAvailableinQueueold(int seq_no) //user by rel_recv
{
    int count;
    struct hw6_hdr *hdr;
    struct rxerQelements *r;
    int n = getCurrentQSize();
    int i;
    count =0;
    for(i=0; i < n-1; i++){
        r = getNthElement(i);
        if (!r->flag_has_data) continue;
        hdr = (struct hw6_hdr *)r->packet;
        if(hdr->sequence_number == seq_no)
            return count;
        count++;
    }
    return -1;
}
//
//will return -1, if not found, otherwise will return the index, which can be used with getNthElement
int isPacketAvailableinQueue(int seq_no) //user by rel_recv
{
    struct rxerQelements *r;
    r = getNthElement(0);
    if (r->flag_has_data) 
        return 0;
    else 
        return -1;
}
        

int rel_recv(int sock, void * buffer, size_t length) {
    static int first_time = 1;
    if(first_time == 1){
        for(int i=0; i<100; i++){  //insert 100 blank elements in queue
            struct rxerQelements *r;
            r = calloc(1,sizeof(struct rxerQelements));
            r->magic = 'R';
            r->flag_has_data = 0;
            r->ack_sent = 0;
            r->packet_len = 0;
            if(r == NULL){
                perror("<RITESH> no memory for rxer window\n");
                exit(45);
            }
            doQInsert(r);
        }
        first_time = 0;
    }
    char packet[MAX_PACKET];
    memset(&packet,0,sizeof(packet));
    struct hw6_hdr* hdr=(struct hw6_hdr*)packet;	
    unsigned int addrlen=sizeof(peeraddr);

    struct rxerQelements *r;
    int index;

    while(-1 ==( index = isPacketAvailableinQueue(last_packet_seq_no_sent_up + 1))){
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, NULL, 0); //wait indefinetly
        int recv_count = recvfrom(sock, packet, MAX_PACKET, 0, (struct sockaddr*)&peeraddr, &addrlen);
        fprintf(stderr,"Got packet: %d                  \r\n",ntohl(hdr->sequence_number));
        if(ntohl(hdr->sequence_number) <= (last_packet_seq_no_sent_up + 100 -1)) // -1 to be safe, if cqueue uses full elemements than change it to -0
        {
            struct hw6_hdr ack;
            ack.flags = ACK;
            ack.ack_number = hdr->sequence_number;
            sendto(sock, &ack, sizeof(ack), 0, (struct sockaddr*)&peeraddr, addrlen); //send ack - normal
            fprintf(stderr,"Sent ACK : %d                  \r\n",ntohl(hdr->sequence_number));

            struct rxerQelements *r;
            r = NULL;
            if(ntohl(hdr->sequence_number) > (last_packet_seq_no_sent_up) || last_packet_seq_no_sent_up == -1){
                r = getNthElement(ntohl(hdr->sequence_number) - (last_packet_seq_no_sent_up+1));
            }

            if(r == NULL || r->flag_has_data){
                //retransmit
                fprintf(stderr,"%d Retransmit  detected  - ignoring -  Sent ACK in above log : %d                  \r\n",ntohl(hdr->sequence_number),ntohl(ack.ack_number));
            }
            else{
                r->flag_has_data = 1;
                memcpy(r->packet, packet, recv_count);
                r->packet_len = recv_count;
                //store it in window;
            }
        }
        else{
            fprintf(stderr,"Not Sending ACK (window full- last_packet_seq_no_sent_up  = %d) : packet seq no - %d                  \r\n",last_packet_seq_no_sent_up,ntohl(hdr->sequence_number));
        }
    }
    if(index!=0){
        perror("index != 0\n");
        exit(44);
    }
    r = doQDelete(); //  deletefromQ() should return packet with sq_no = last_packet_seq_no_sent_up+1
    hdr = (struct hw6_hdr *)r->packet;
    fprintf(stderr,"Sent packet upwards: %d                  \r\n",ntohl(hdr->sequence_number));
    memcpy(buffer, r->packet+sizeof(struct hw6_hdr), r->packet_len-sizeof(struct hw6_hdr));
    last_packet_seq_no_sent_up = ntohl(hdr->sequence_number);
    int plen = r->packet_len;
    //reuse calloced element..dont free n malloc again
    doQInsert(r); // make space for next packet
    memset(r,0,sizeof(struct rxerQelements));
    r->magic = 'R';
    r->flag_has_data = 0;//better safe than sorry
    //todo probably:if packet is a close packet, ack whatever we have so far
    return plen - sizeof(struct hw6_hdr); //give packet to user
}



int rel_close(int sock) {
    fprintf(stderr, "<RITESH>, rel_close called.\n");
    fprintf(stderr, "<RITESH>, rel_close: sending close and waiting for ack of close packet(packet_len = 0).\n");

	// an empty packet signifies end of file
	rel_send(sock,0,0);

	fprintf(stderr,"\nSent EOF. Now in final wait state.\n");
    fprintf(stderr, "<RITESH>, rel_close: rel_send ended, waiting for 2 more seconds to process close packet.\n");

	struct timeval t;
	t.tv_sec = 2;
	t.tv_usec = 0;

	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
	int wait_start = current_msec();
	
	// why wait for not more than 2 secs? In the below case, the ack might have been lost, and we dont want to wait unnecessarily at step 5.
    /* 
     * Scenario: 1. Sender sends close, 2. rxer sends ack(it is lost), 3. rxer sends close, 4. sender recives close in rel_send(), 
     * 5. Sender will wait for ack of the packet it has sent, and will not receive it (in this case).
     */
	// wait for 2 seconds
	while(current_msec() - wait_start < 2000) {		
		rel_recv(sock,0,0);
	}
    fprintf(stderr, "<RITESH>, rel_close: exiting after waiting for 2 secs or packet recieved.\n");

	close(sock);
}

