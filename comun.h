#ifndef COMUN_H
#define COMUN_H

#define FILAS 20
#define COLUMNAS 40

#define MAX_CLIENTES 10

#define SHM_NAME "/cosmikernel_mapa"

typedef struct {
    int activo;
    int y;
    int x;
    char simbolo;
} Cliente;

typedef struct {
    char mapa[FILAS][COLUMNAS];
    int cantidad_asteroides;
    int cantidad_estaciones;
    Cliente clientes[MAX_CLIENTES];
} Sector;

#endif
