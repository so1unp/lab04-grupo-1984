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

void inicializar_clientes(Sector *sector)
{
    int i;

    for (i = 0; i < MAX_CLIENTES; i++) {
        sector->clientes[i].activo = 0;
        sector->clientes[i].y = 0;
        sector->clientes[i].x = 0;
        sector->clientes[i].simbolo = '0' + i;
    }
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

    inicializar_clientes(sector);
}

void dibujar_mapa(WINDOW *mapa, Sector *sector)
{
    int i;
    int j;

    werase(mapa);
    box(mapa, 0, 0);

    mvwprintw(mapa, 0, 2, " SERVIDOR - MAPA COMPARTIDO ");

    for (i = 1; i < FILAS - 1; i++) {
        for (j = 1; j < COLUMNAS - 1; j++) {
            mvwaddch(mapa, i, j, sector->mapa[i][j]);
        }
    }

    wrefresh(mapa);
}

int main(int argc, char *argv[])
{
    int fd;
    int tecla;

    Sector *sector;
    WINDOW *mapa;

    (void) argc;
    (void) argv;

    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd, sizeof(Sector)) == -1) {
        perror("ftruncate");
        exit(EXIT_FAILURE);
    }

    sector = mmap(
        NULL,
        sizeof(Sector),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (sector == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    inicializar_mapa(sector);

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    refresh();

    mapa = newwin(FILAS, COLUMNAS, 5, 35);

    keypad(mapa, TRUE);
    wtimeout(mapa, 1000);

    while (1) {
        dibujar_mapa(mapa, sector);

        tecla = wgetch(mapa);

        if (tecla == 'q') {
            break;
        }
    }

    delwin(mapa);

    endwin();

    munmap(sector, sizeof(Sector));

    close(fd);

    printf("Servidor finalizado.\n");

    exit(EXIT_SUCCESS);
}
