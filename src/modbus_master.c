// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <modbus.h>

static void usage(const char *prog)
{
    fprintf(stderr, "Uso:\n");
    fprintf(stderr, "  %s <ip> <puerto> r <direccion> [cantidad]\n", prog);
    fprintf(stderr, "  %s <ip> <puerto> w <direccion> <valor>\n", prog);
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    char op = argv[3][0];
    int addr = atoi(argv[4]);

    modbus_t *ctx = modbus_new_tcp(ip, port);
    if (ctx == NULL) {
        fprintf(stderr, "No se pudo crear el contexto Modbus\n");
        return 1;
    }

    modbus_set_slave(ctx, 1);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Conexion fallida: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    int rc = 0;

    if (op == 'r') {
        int count = (argc > 5) ? atoi(argv[5]) : 1;
        if (count < 1 || count > 16) {
            fprintf(stderr, "Cantidad invalida (1-16)\n");
            rc = 1;
        } else {
            uint16_t regs[16];
            int n = modbus_read_registers(ctx, addr, count, regs);
            if (n == -1) {
                fprintf(stderr, "Lectura fallida: %s\n", modbus_strerror(errno));
                rc = 1;
            } else {
                for (int i = 0; i < n; i++)
                    printf("Registro %d = %u\n", addr + i, regs[i]);
            }
        }
    } else if (op == 'w') {
        if (argc < 6) {
            usage(argv[0]);
            rc = 1;
        } else {
            int value = atoi(argv[5]);
            if (modbus_write_register(ctx, addr, value) == -1) {
                fprintf(stderr, "Escritura fallida: %s\n", modbus_strerror(errno));
                rc = 1;
            } else {
                printf("Escrito registro %d = %d\n", addr, value);
            }
        }
    } else {
        fprintf(stderr, "Operacion desconocida: %c\n", op);
        rc = 1;
    }

    modbus_close(ctx);
    modbus_free(ctx);
    return rc;
}
