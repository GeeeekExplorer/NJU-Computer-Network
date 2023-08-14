#include<stdio.h>
#include<string.h>
#include<assert.h>
#include<sys/time.h>
#include<linux/if_packet.h>
#include<sys/socket.h>
#include<sys/ioctl.h>
#include<net/if.h>
#include<net/ethernet.h>
#include<net/if_arp.h>
#include<netinet/in.h>
#include<netinet/ip.h>
#include<netinet/ip_icmp.h>
#include<arpa/inet.h>
#include<errno.h>

int sockfd = -1;
int addr_len = sizeof(struct sockaddr_ll);
char send_buf[256];
char recv_buf[256];
struct sockaddr_ll dest_ll;
struct sockaddr_ll src_ll;
struct sockaddr_in dest_in;
struct ip *ip_header;
struct icmp *icmp_header;
struct in_addr destination;
char gateway[16];
struct in_addr netmask;
char interface[14];

struct route_item{
	char destination[16];
	char gateway[16];
	char netmask[16];
	char interface[16];
}route_info[2];

struct arp_table_item{
	char ip_addr[16];
	char mac_addr[18];
}arp_table[1];

struct device_item{
	char interface[14];
	char ip_addr[16];
	char mac_addr[18];
}device[1];

//��ʼ������ 
void init()
{
	FILE *file=fopen("PC2.txt","r");
	int i;
	for(i = 0; i < 2; i++)
	{
		fscanf(file,"%s",route_info[i].destination);
		fscanf(file,"%s",route_info[i].gateway);
		fscanf(file,"%s",route_info[i].netmask);
		fscanf(file,"%s",route_info[i].interface);
	}
	for(i = 0; i < 1; i++)
	{
		fscanf(file,"%s",arp_table[i].ip_addr);
		fscanf(file,"%s",arp_table[i].mac_addr);
	}
	for(i = 0; i < 1; i++)
	{
		fscanf(file,"%s",device[i].interface);
		fscanf(file,"%s",device[i].ip_addr);
		fscanf(file,"%s",device[i].mac_addr);
	}
	fclose(file);
}

//У����㷨
int checksum(unsigned short *buf, int len)
{    
	int nleft = len;    
	int sum = 0;    
	unsigned short *w = (unsigned short *)buf;    
	unsigned short answer = 0;   
	//��ICMP��ͷ������������2�ֽ�Ϊ��λ�ۼ����� 
	while (nleft > 1)    
	{       
		sum += *w++;       
		nleft -= 2;    
	}     
	//��ICMP��ͷΪ�������ֽڣ���ʣ�����һ�ֽڡ������һ���ֽ���Ϊһ��2�ֽ����ݵĸ��ֽڣ����2�ֽ����ݵĵ��ֽ�Ϊ0�������ۼ�    
	if (nleft == 1)    
	{            
	    *((unsigned char *)&answer) = *((unsigned char *)w);        
		sum += answer;    
	}     
	sum = (sum>>16) + (sum & 0xffff);    
	sum += (sum >> 16);    
	answer = ~sum;     
	return answer;
}

//�������ӿ���Ϣ 
int getifindex(char *if_name)
{
	int temp = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	struct ifreq req;
	memset(&req, 0, sizeof(req));
	strncpy(req.ifr_name, if_name, IFNAMSIZ - 1);
	ioctl(temp, SIOCGIFINDEX, &req);
	return req.ifr_ifindex;
}

//��� 
int unpack()
{
    ip_header = (struct ip *)recv_buf;
    icmp_header = (struct icmp *)(recv_buf + ip_header->ip_hl * 4);
    if(icmp_header->icmp_type == ICMP_ECHO)
    {
	    unsigned char *p = src_ll.sll_addr;
		printf("receive an ICMP request packet from %.2x:%02x:%02x:%02x:%02x:%02x\n",p[0],p[1],p[2],p[3],p[4],p[5]);
	    printf("src: %s, ",inet_ntoa(ip_header->ip_src));
		printf("dst: %s\n",inet_ntoa(ip_header->ip_dst));
		int i; 
	    for(i = 0; i < 1; i++)
	    	if(strcmp(inet_ntoa(ip_header->ip_dst),device[i].ip_addr) == 0)
	    		break;
	    return i;
	}
	else
		return -1;
}

//��Ӧ 
void reply()
{
	memcpy(send_buf,recv_buf,256);
	ip_header = (struct ip *)send_buf;
	struct in_addr temp;
	temp = ip_header->ip_dst;
	ip_header->ip_dst = ip_header->ip_src;
	ip_header->ip_src = temp; 
	icmp_header = (struct icmp *)(send_buf + ip_header->ip_hl * 4);
	icmp_header->icmp_type = ICMP_ECHOREPLY; 
	icmp_header->icmp_cksum = 0;
	icmp_header->icmp_cksum = checksum((unsigned short *)icmp_header, 64); 
	printf("reply to it\n\n");
	assert(sendto(sockfd, send_buf, 64, 0, (struct sockaddr *)&src_ll, sizeof(struct sockaddr_ll)) > 20);
}

int main(int argc, char *argv[])
{
	init();
	sockfd = socket(AF_PACKET,SOCK_DGRAM,htons(ETH_P_IP));
	assert(sockfd >= 0);
	src_ll.sll_family = AF_PACKET;
	src_ll.sll_protocol = htons(ETH_P_IP);
	src_ll.sll_halen = ETH_ALEN;
	while(1)
	{
		if(recvfrom(sockfd, recv_buf, 64, 0, (struct sockaddr *)&src_ll, &addr_len) > 0)
		{
			if((src_ll.sll_hatype == ARPHRD_ETHER)&&(src_ll.sll_pkttype == PACKET_HOST))
			{
				int i = unpack(); 
				if(i == 0)
					reply();
			}
		}
	}
}
