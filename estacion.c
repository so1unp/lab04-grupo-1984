#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <unistd.h>

#include "comun.h"

#define COLA_ESTACION "/cola_estacion"
#define TAM_MENSAJE 20
#define CAPACIDAD_HANGAR 3
#define COMBUSTIBLE_INICIAL 100
#define UMBRAL_COMBUSTIBLE 40

void dibujar_mapa(WINDOW *mapa, Sector *sector)
{
    int i;
    int j;

    werase(mapa);
    box(mapa, 0, 0);

    mvwprintw(mapa, 0, 2, " MAPA COMPARTIDO ");

    for (i = 1; i < FILAS - 1; i++) {
        for (j = 1; j < COLUMNAS - 1; j++) {
            mvwaddch(mapa, i, j, sector->mapa[i][j]);
        }
    }

    wrefresh(mapa);
}

int main(void)
{
    WINDOW *mapa;
    WINDOW *info;

    struct mq_attr atributos;
    mqd_t cola;

    char mensaje[TAM_MENSAJE];
    char estado[60] = "Sin solicitudes";

    int fd;
    int tecla;

    int naves_almacenadas = 0;
    int combustible = COMBUSTIBLE_INICIAL;
    int pedido_ayuda_enviado = 0;

    Sector *sector;

    fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        printf("Primero tenes que ejecutar ./servidor\n");
        exit(EXIT_FAILURE);
    }

    sector = mmap(NULL,
                  sizeof(Sector),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  0);

    if (sector == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    atributos.mq_flags = 0;
    atributos.mq_maxmsg = 10;
    atributos.mq_msgsize = TAM_MENSAJE;
    atributos.mq_curmsgs = 0;

    mq_unlink(COLA_ESTACION);

    cola = mq_open(COLA_ESTACION,
                   O_CREAT | O_RDONLY | O_NONBLOCK,
                   0666,
                   &atributos);

    if (cola == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    refresh();

    mapa = newwin(FILAS, COLUMNAS, 5, 25);
    info = newwin(12, 35, 5, 68);

    keypad(mapa, TRUE);
    wtimeout(mapa, 1000);

    while (combustible > 0) {

        combustible--;

        if (combustible < UMBRAL_COMBUSTIBLE &&
            pedido_ayuda_enviado == 0) {

            pedido_ayuda_enviado = 1;

            snprintf(estado,
                     sizeof(estado),
                     "AYUDA: combustible bajo");
        }

        if (mq_receive(cola,
                       mensaje,
                       TAM_MENSAJE,
                       NULL) != -1) {

            if (strcmp(mensaje, "ENTRAR") == 0) {

                if (naves_almacenadas < CAPACIDAD_HANGAR) {

                    naves_almacenadas++;

                    snprintf(estado,
                             sizeof(estado),
                             "Nave almacenada");
                }
                else {

                    snprintf(estado,
                             sizeof(estado),
                             "Acceso negado: hangar lleno");
                }
            }

            if (strcmp(mensaje, "SALIR") == 0) {

                if (naves_almacenadas > 0) {

                    naves_almacenadas--;

                    snprintf(estado,
                             sizeof(estado),
                             "Nave salio del hangar");
                }
            }
        }

        dibujar_mapa(mapa, sector);

        werase(info);
        box(info, 0, 0);

        mvwprintw(info, 0, 2, " ESTACION ");

        mvwprintw(info,
                  2,
                  2,
                  "Naves: %d/%d",
                  naves_almacenadas,
                  CAPACIDAD_HANGAR);

        mvwprintw(info,
                  4,
                  2,
                  "Combustible: %d%%",
                  combustible);

        mvwprintw(info,
                  6,
                  2,
                  "Estado:");

        mvwprintw(info,
                  7,
                  2,
                  "%s",
                  estado);

        if (pedido_ayuda_enviado) {

            mvwprintw(info,
                      9,
                      2,
                      "Pedido de ayuda enviado");
        }

        mvwprintw(info,
                  10,
                  2,
                  "q: salir");

        wrefresh(info);

        tecla = wgetch(mapa);

        if (tecla == 'q') {
            break;
        }
    }

    delwin(mapa);
    delwin(info);

    endwin();

    mq_close(cola);
    mq_unlink(COLA_ESTACION);

    munmap(sector, sizeof(Sector));
    close(fd);

    printf("Estacion finalizada\n");

    return 0;
}