#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ncurses.h>

#include "comun.h"

int leer_valor_config(const char *clave, int valor_por_defecto)
{
    FILE *archivo;
    char linea[100];
    char nombre[50];
    int valor;

    archivo = fopen("config.txt", "r");

    if (archivo == NULL) {
        return valor_por_defecto;
    }

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        if (sscanf(linea, "%49[^=]=%d", nombre, &valor) == 2) {
            if (strcmp(nombre, clave) == 0) {
                fclose(archivo);
                return valor;
            }
        }
    }

    fclose(archivo);

    return valor_por_defecto;
}

void inicializar_mapa(Sector *sector)
{
    int i;
    int j;
    int cantidad;
    int filas_asteroides[5] = {3, 5, 8, 12, 16};
    int columnas_asteroides[5] = {8, 20, 30, 15, 25};

    for (i = 0; i < FILAS; i++) {
        for (j = 0; j < COLUMNAS; j++) {
            sector->mapa[i][j] = ' ';
        }
    }

    sector->cantidad_estaciones =
        leer_valor_config("ESTACIONES", 1);

    sector->cantidad_asteroides =
        leer_valor_config("ASTEROIDES", 5);

    if (sector->cantidad_asteroides > 5) {
        sector->cantidad_asteroides = 5;
    }

    if (sector->cantidad_asteroides < 0) {
        sector->cantidad_asteroides = 0;
    }

    if (sector->cantidad_estaciones > 0) {
        sector->mapa[10][10] = 'E';
    }

    cantidad = sector->cantidad_asteroides;

    for (i = 0; i < cantidad; i++) {
        sector->mapa[filas_asteroides[i]][columnas_asteroides[i]] = 'A';
    }
}

