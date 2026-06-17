#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "comun.h"

int y = 10;
int x = 20;
int jugando = 1;
int sobre_asteroide = 0;

WINDOW *mapa;
Sector *sector;

void dibujar_mapa_compartido(void)
{
    int i;
    int j;

    werase(mapa);
    box(mapa, 0, 0);

    for (i = 1; i < FILAS - 1; i++) {
        for (j = 1; j < COLUMNAS - 1; j++) {
            mvwaddch(mapa, i, j, sector->mapa[i][j]);
        }
    }

    wrefresh(mapa);
}

int main(void)
{
    int fd;
    int ch;

    fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    sector = mmap(NULL, sizeof(Sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sector == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    mapa = newwin(FILAS, COLUMNAS, 1, 1);

    keypad(mapa, TRUE);
    wtimeout(mapa, 100);

    sector->mapa[y][x] = 'O';

    dibujar_mapa_compartido();

    while (jugando) {
        int nuevo_y = y;
        int nuevo_x = x;
        int movio = 0;

        ch = wgetch(mapa);

        if (ch == 'q') {
            jugando = 0;
            break;
        }

        if (ch == KEY_UP) {
            nuevo_y--;
            movio = 1;
        }

        if (ch == KEY_DOWN) {
            nuevo_y++;
            movio = 1;
        }

        if (ch == KEY_LEFT) {
            nuevo_x--;
            movio = 1;
        }

        if (ch == KEY_RIGHT) {
            nuevo_x++;
            movio = 1;
        }

        if (movio &&
            nuevo_y > 0 && nuevo_y < FILAS - 1 &&
            nuevo_x > 0 && nuevo_x < COLUMNAS - 1 &&
            sector->mapa[nuevo_y][nuevo_x] != 'E') {

            if (sobre_asteroide == 1) {
                sector->mapa[y][x] = 'A';
            } else {
                sector->mapa[y][x] = ' ';
            }

            y = nuevo_y;
            x = nuevo_x;

            if (sector->mapa[y][x] == 'A') {
                sobre_asteroide = 1;
            } else {
                sobre_asteroide = 0;
            }

            sector->mapa[y][x] = 'O';

            dibujar_mapa_compartido();
        }
    }

    if (sobre_asteroide == 1) {
        sector->mapa[y][x] = 'A';
    } else {
        sector->mapa[y][x] = ' ';
    }

    delwin(mapa);
    endwin();

    munmap(sector, sizeof(Sector));

    return 0;
}