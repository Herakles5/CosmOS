#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <unistd.h>
#include <iostream>

int main() {
    int sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if(sock < 0) std::cout << "failed\n";
    else std::cout << "success\n";
    return 0;
}
