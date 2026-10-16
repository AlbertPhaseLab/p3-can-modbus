// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(int argc, char *argv[])
{
    const char *ifname = (argc > 1) ? argv[1] : "vcan0";

    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        return 1;
    }

    struct sockaddr_can addr = {0};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    printf("Escuchando en %s...\n", ifname);

    struct can_frame frame;
    while (1) {
        ssize_t nbytes = read(sock, &frame, sizeof(frame));
        if (nbytes < 0) {
            perror("read");
            break;
        }
        if (nbytes < (ssize_t)sizeof(frame)) {
            fprintf(stderr, "trama incompleta\n");
            continue;
        }

        printf("ID=0x%03X DLC=%d DATA=", frame.can_id, frame.can_dlc);
        for (int i = 0; i < frame.can_dlc; i++)
            printf("%02X ", frame.data[i]);
        printf("\n");
    }

    close(sock);
    return 0;
}
