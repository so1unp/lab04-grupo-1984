#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ncurses.h>

#include "comun.h"

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
    int fd;
    Sector *sector;
    WINDOW *mapa;

    fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
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

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    mapa = newwin(FILAS, COLUMNAS, 1, 1);

    while (1) {
        dibujar_mapa(mapa, sector);

        if (getch() == 'q') {
            break;
        }
    }

    delwin(mapa);
    endwin();

    munmap(sector, sizeof(Sector));
    close(fd);

    return 0;
}