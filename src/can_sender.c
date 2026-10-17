// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <interfaz> <id_hex> [byte1 byte2 ...]\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s vcan0 123 DE AD BE EF\n", argv[0]);
        return 1;
    }

    const char *ifname = argv[1];
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

    struct can_frame frame = {0};
    frame.can_id = (canid_t)strtol(argv[2], NULL, 16);

    int nbytes_data = argc - 3;
    if (nbytes_data > 8)
        nbytes_data = 8;
    frame.can_dlc = (__u8)nbytes_data;

    for (int i = 0; i < nbytes_data; i++)
        frame.data[i] = (__u8)strtol(argv[3 + i], NULL, 16);

    ssize_t nbytes = write(sock, &frame, sizeof(frame));
    if (nbytes != sizeof(frame)) {
        perror("write");
        return 1;
    }

    printf("Enviado: ID=0x%03X DLC=%d\n", frame.can_id, frame.can_dlc);

    close(sock);
    return 0;
}
