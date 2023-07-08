#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>

#define BUFFER_SIZE 4096

struct in6_addr in6addr_linklocal_allnodes = {
    { { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
};

void send_ndp_solicitation(int sock, const struct in6_addr *target_mac) {
    struct sockaddr_in6 dst_addr;
    memset(&dst_addr, 0, sizeof(struct sockaddr_in6));
    dst_addr.sin6_family = AF_INET6;
    dst_addr.sin6_addr = in6addr_linklocal_allnodes;

    struct nd_neighbor_solicit ns;
    memset(&ns, 0, sizeof(struct nd_neighbor_solicit));
    ns.nd_ns_hdr.icmp6_type = ND_NEIGHBOR_SOLICIT;
    ns.nd_ns_hdr.icmp6_code = 0;
    ns.nd_ns_target = *target_mac;

    struct iovec iov[1];
    iov[0].iov_base = &ns;
    iov[0].iov_len = sizeof(struct nd_neighbor_solicit);

    struct msghdr msg;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &dst_addr;
    msg.msg_namelen = sizeof(struct sockaddr_in6);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    sendmsg(sock, &msg, 0);
}

void process_ndp_response(int sock) {
    char buffer[BUFFER_SIZE];
    struct iovec iov[1];
    iov[0].iov_base = buffer;
    iov[0].iov_len = sizeof(buffer);

    struct sockaddr_in6 src_addr;
    memset(&src_addr, 0, sizeof(struct sockaddr_in6));
    struct msghdr msg;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_name = &src_addr;
    msg.msg_namelen = sizeof(struct sockaddr_in6);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    ssize_t recv_len = recvmsg(sock, &msg, 0);
    if (recv_len < 0) {
        perror("Error receiving NDP response");
        return;
    }

    struct nd_neighbor_advert *na = (struct nd_neighbor_advert *) buffer;
    if (na->nd_na_hdr.icmp6_type == ND_NEIGHBOR_ADVERT) {
        struct in6_addr *target_addr = &(na->nd_na_target);
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, target_addr, ip_str, sizeof(ip_str));
        printf("Discovered IPv6 address: %s\n", ip_str);
    }
}

int main(int argc, char *argv[]) {
    int sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    if (sock < 0) {
        perror("Error creating socket");
        exit(1);
    }

    // Set socket options for the desired network interface
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(struct sockaddr_in6));
    sa.sin6_family = AF_INET6;
    sa.sin6_addr = in6addr_any;
    sa.sin6_scope_id = if_nametoindex(argv[2]);

    if (bind(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr_in6)) < 0) {
        perror("Error binding socket");
        close(sock);
        exit(1);
    }

    struct in6_addr target_mac;
    if (inet_pton(AF_INET6, argv[1], &target_mac) <= 0) {
        perror("Error converting MAC address");
        close(sock);
        exit(1);
    }

    send_ndp_solicitation(sock, &target_mac);
    process_ndp_response(sock);

    close(sock);
    return 0;
}
