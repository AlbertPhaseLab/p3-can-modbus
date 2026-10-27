// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <modbus.h>

#define CAN_ID_TELEMETRY 0x100
#define NUM_REGISTERS    16
#define TELEMETRY_REGS   4

static int open_can(const char *ifname)
{
    int s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket CAN");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
        perror("ioctl SIOCGIFINDEX");
        close(s);
        return -1;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind CAN");
        close(s);
        return -1;
    }
    return s;
}

/* Telemetria: pares de bytes -> registros de 16 bits, big-endian */
static void handle_can_frame(const struct can_frame *f, modbus_mapping_t *map)
{
    if (f->can_id != CAN_ID_TELEMETRY)
        return;

    for (int i = 0; i < TELEMETRY_REGS; i++) {
        uint8_t hi = (2 * i     < f->can_dlc) ? f->data[2 * i]     : 0;
        uint8_t lo = (2 * i + 1 < f->can_dlc) ? f->data[2 * i + 1] : 0;
        map->tab_registers[i] = ((uint16_t)hi << 8) | lo;
    }

    printf("CAN 0x%03X -> regs 0-3 = %04X %04X %04X %04X\n",
           f->can_id,
           map->tab_registers[0], map->tab_registers[1],
           map->tab_registers[2], map->tab_registers[3]);
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    const char *ifname = (argc > 1) ? argv[1] : "vcan0";

    int can_fd = open_can(ifname);
    if (can_fd < 0)
        return 1;

    modbus_t *ctx = modbus_new_tcp("0.0.0.0", 502);
    if (ctx == NULL) {
        fprintf(stderr, "No se pudo crear el contexto Modbus\n");
        return 1;
    }

    modbus_mapping_t *map = modbus_mapping_new(0, 0, NUM_REGISTERS, 0);
    if (map == NULL) {
        fprintf(stderr, "Fallo al asignar registros: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    int server_fd = modbus_tcp_listen(ctx, 1);
    if (server_fd == -1) {
        fprintf(stderr, "No se pudo escuchar: %s\n", modbus_strerror(errno));
        modbus_mapping_free(map);
        modbus_free(ctx);
        return 1;
    }

    printf("Gateway listo: CAN en %s, Modbus TCP en el puerto 502\n", ifname);
    fflush(stdout);

    int client_fd = -1;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

    for (;;) {
        struct pollfd fds[2];
        fds[0].fd = can_fd;
        fds[0].events = POLLIN;
        fds[1].fd = (client_fd >= 0) ? client_fd : server_fd;
        fds[1].events = POLLIN;

        int n = poll(fds, 2, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        /* 1) Tramas CAN */
        if (fds[0].revents & POLLIN) {
            struct can_frame frame;
            ssize_t nbytes = read(can_fd, &frame, sizeof(frame));
            if (nbytes == (ssize_t)sizeof(frame))
                handle_can_frame(&frame, map);
        }

        /* 2) Modbus: nuevo cliente o peticion de un cliente ya conectado */
        if (fds[1].revents & (POLLIN | POLLHUP | POLLERR)) {
            if (client_fd < 0) {
                client_fd = modbus_tcp_accept(ctx, &server_fd);
                if (client_fd >= 0)
                    printf("Cliente Modbus conectado\n");
            } else {
                int rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    modbus_reply(ctx, query, rc, map);
                } else if (rc == -1) {
                    modbus_close(ctx);
                    client_fd = -1;
                    printf("Cliente Modbus desconectado\n");
                }
            }
            fflush(stdout);
        }
    }

    modbus_mapping_free(map);
    modbus_free(ctx);
    close(can_fd);
    return 0;
}
