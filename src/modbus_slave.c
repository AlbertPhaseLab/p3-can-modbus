// SPDX-License-Identifier: MIT
#include <stdio.h>
#include <stdlib.h>
#include <modbus.h>
#include <errno.h>

#define NUM_REGISTERS 16

int main(void)
{
    modbus_t *ctx = modbus_new_tcp("0.0.0.0", 502);
    if (ctx == NULL) {
        fprintf(stderr, "No se pudo crear el contexto Modbus\n");
        return 1;
    }

    modbus_mapping_t *mb_mapping = modbus_mapping_new(0, 0, NUM_REGISTERS, 0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Fallo al asignar los registros: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return 1;
    }

    /* Valor inicial de ejemplo en el registro 0, para poder leerlo sin escribir antes */
    mb_mapping->tab_registers[0] = 1234;

    int server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1) {
        fprintf(stderr, "No se pudo escuchar: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return 1;
    }

    printf("Esclavo Modbus TCP escuchando en el puerto 502...\n");

    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

    while (1) {
        modbus_tcp_accept(ctx, &server_socket);
        printf("Cliente conectado\n");

        while (1) {
            int rc = modbus_receive(ctx, query);
            if (rc == -1)
                break; /* cliente desconectado */

            printf("Peticion recibida (%d bytes)\n", rc);
            modbus_reply(ctx, query, rc, mb_mapping);
        }

        printf("Cliente desconectado\n");
    }

    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);
    return 0;
}
