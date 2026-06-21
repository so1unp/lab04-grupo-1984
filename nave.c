#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <string.h>
#include <sys/mman.h>

#include "comun.h"

#define COLA_ESTACION "/cola_estacion"
#define TAM_MENSAJE 100

double oxigeno = 100.0;
int combustible = 100;
int y = 10;
int x = 20;
int jugando = 1;
int id_cliente = 0;

int sobre_asteroide = 0;

int deut = 0;
int mut = 0;
int sem = 0;
int ker = 0;

WINDOW *mapa;
WINDOW *mensajes;
WINDOW *minerales;

Sector *sector;

mqd_t cola_nave;
char nombre_cola_nave[30];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void dibujar_minerales(void)
{
    werase(minerales);
    box(minerales, 0, 0);

    mvwprintw(minerales, 0, 2, " [ Minerales ] ");
    mvwprintw(minerales, 2, 2, "Deuterio:   %d u", deut);
    mvwprintw(minerales, 3, 2, "Mutexio:    %d u", mut);
    mvwprintw(minerales, 4, 2, "Semaforita: %d u", sem);
    mvwprintw(minerales, 5, 2, "Kernelio:   %d u", ker);
    mvwprintw(minerales, 7, 2, "x: extraer");
    mvwprintw(minerales, 8, 2, "v: vender");

    wrefresh(minerales);
}

void dibujar_mapa_compartido(void)
{
    int i;
    int j;

    werase(mapa);
    box(mapa, 0, 0);

    for (i = 1; i < FILAS - 1; i++) {
        for (j = 1; j < COLUMNAS - 1; j++) {
            mvwaddch(mapa, i, j, (chtype) sector->mapa[i][j]);
        }
    }

    mvwprintw(mapa, 0, 2, " [ Nave %d | O2: %3d%% | Comb: %3d%% ] ",
              id_cliente, (int) oxigeno, combustible);

    wrefresh(mapa);
}

int enviar_mensaje_estacion(char *texto)
{
    mqd_t cola;

    cola = mq_open(COLA_ESTACION, O_WRONLY);

    if (cola == (mqd_t)-1) {
        mvwprintw(mensajes, 2, 2, "Estacion no disponible");
        wrefresh(mensajes);
        return 0;
    }

    mq_send(cola, texto, strlen(texto) + 1, 0);
    mq_close(cola);

    mvwprintw(mensajes, 2, 2, "Mensaje enviado          ");
    mvwprintw(mensajes, 3, 2, "%s                    ", texto);
    wrefresh(mensajes);

    return 1;
}

void revisar_mensajes_estacion(void)
{
    char aviso[TAM_MENSAJE];

    if (mq_receive(cola_nave, aviso, TAM_MENSAJE, NULL) != -1) {
        mvwprintw(mensajes, 5, 2, "%s                ", aviso);
        wrefresh(mensajes);
    }
}

void vender_recursos(void)
{
    char mensaje_venta[80];

    if (deut == 0 && mut == 0 && sem == 0 && ker == 0) {
        mvwprintw(mensajes, 4, 2, "No hay recursos      ");
        wrefresh(mensajes);
        return;
    }

    snprintf(mensaje_venta, sizeof(mensaje_venta),
             "VENDER %d %d %d %d", deut, mut, sem, ker);

    if (enviar_mensaje_estacion(mensaje_venta)) {
        deut = 0;
        mut = 0;
        sem = 0;
        ker = 0;

        mvwprintw(mensajes, 4, 2, "Recursos vendidos    ");
        wrefresh(mensajes);

        dibujar_minerales();
    }
}

void extraer_recurso(void)
{
    if (sobre_asteroide == 1 && combustible >= 5) {
        deut += 2;
        mut += 1;
        sem += 1;
        ker += 1;

        combustible -= 5;
        sobre_asteroide = 0;

        sector->mapa[y][x] = sector->clientes[id_cliente].simbolo;

        mvwprintw(mensajes, 4, 2, "Recursos extraidos   ");
        wrefresh(mensajes);

        dibujar_minerales();
        dibujar_mapa_compartido();
    } else {
        mvwprintw(mensajes, 4, 2, "No se puede extraer  ");
        wrefresh(mensajes);
    }
}

void *soporte_vital(void *arg)
{
    (void) arg;

    while (jugando) {
        sleep(1);

        pthread_mutex_lock(&mutex);

        if (oxigeno > 0 && jugando) {
            oxigeno -= 1.5;
            dibujar_mapa_compartido();
        }

        if (oxigeno <= 0 && jugando) {
            jugando = 0;
            mvwprintw(mapa, 10, 15, " GAME OVER ");
            mvwprintw(mensajes, 2, 2, "Sin oxigeno!");
            wrefresh(mapa);
            wrefresh(mensajes);
        }

        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int posicion_ocupada_por_cliente(int fila, int columna)
{
    int i;

    for (i = 0; i < MAX_CLIENTES; i++) {
        if (i != id_cliente &&
            sector->clientes[i].activo &&
            sector->clientes[i].y == fila &&
            sector->clientes[i].x == columna) {
            return 1;
        }
    }

    return 0;
}

void registrar_cliente(void)
{
    int inicio_y;
    int inicio_x;

    inicio_y = 2 + id_cliente;
    inicio_x = 2;

    if (inicio_y >= FILAS - 1) {
        inicio_y = 2;
    }

    y = inicio_y;
    x = inicio_x;

    sector->clientes[id_cliente].activo = 1;
    sector->clientes[id_cliente].y = y;
    sector->clientes[id_cliente].x = x;
    sector->clientes[id_cliente].simbolo = (char)('0' + id_cliente);

    sector->mapa[y][x] = sector->clientes[id_cliente].simbolo;
}

void liberar_cliente(void)
{
    if (sobre_asteroide == 1) {
        sector->mapa[y][x] = 'A';
    } else {
        sector->mapa[y][x] = ' ';
    }

    sector->clientes[id_cliente].activo = 0;
}

int main(int argc, char *argv[])
{
    int fd;
    int ch;
    pthread_t thread;
    struct mq_attr atributos;

    if (argc < 2) {
        printf("Uso: ./nave <id 0-9>\n");
        return 1;
    }

    id_cliente = atoi(argv[1]);

    if (id_cliente < 0 || id_cliente >= MAX_CLIENTES) {
        printf("El id debe estar entre 0 y 9\n");
        return 1;
    }

    fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        perror("shm_open");
        printf("Primero tenes que ejecutar ./servidor\n");
        exit(EXIT_FAILURE);
    }

    sector = mmap(NULL, sizeof(Sector), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (sector == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    if (sector->clientes[id_cliente].activo) {
        printf("Ya existe una nave con ese id\n");
        return 1;
    }

    atributos.mq_flags = 0;
    atributos.mq_maxmsg = 10;
    atributos.mq_msgsize = TAM_MENSAJE;
    atributos.mq_curmsgs = 0;

    snprintf(nombre_cola_nave, sizeof(nombre_cola_nave), "/cola_nave_%d", id_cliente);
    mq_unlink(nombre_cola_nave);

    cola_nave = mq_open(nombre_cola_nave, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &atributos);
    if (cola_nave == (mqd_t)-1) {
        perror("mq_open nave");
        exit(EXIT_FAILURE);
    }

    registrar_cliente();

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    mapa = newwin(FILAS, COLUMNAS, 1, 1);
    mensajes = newwin(10, 30, 1, 42);
    minerales = newwin(10, 30, 11, 42);

    keypad(mapa, TRUE);
    wtimeout(mapa, 100);

    box(mensajes, 0, 0);
    mvwprintw(mensajes, 0, 2, " [ Mensajes ] ");
    mvwprintw(mensajes, 2, 2, "e: entrar | s: salir");
    mvwprintw(mensajes, 3, 2, "v: vender");
    wrefresh(mensajes);

    dibujar_minerales();
    dibujar_mapa_compartido();

    pthread_create(&thread, NULL, soporte_vital, NULL);

    while (jugando) {
        int nuevo_y = y;
        int nuevo_x = x;
        int movio = 0;

        ch = wgetch(mapa);

        revisar_mensajes_estacion();

        if (ch == 'q') {
            jugando = 0;
            break;
        }

        if (ch == 'e') {
            enviar_mensaje_estacion("ENTRAR");
        }

        if (ch == 's') {
            enviar_mensaje_estacion("SALIR");
        }

        if (ch == 'v') {
            pthread_mutex_lock(&mutex);
            vender_recursos();
            pthread_mutex_unlock(&mutex);
        }

        if (ch == 'x') {
            pthread_mutex_lock(&mutex);
            extraer_recurso();
            pthread_mutex_unlock(&mutex);
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

        pthread_mutex_lock(&mutex);

        if (movio &&
            combustible > 0 &&
            nuevo_y > 0 && nuevo_y < FILAS - 1 &&
            nuevo_x > 0 && nuevo_x < COLUMNAS - 1 &&
            sector->mapa[nuevo_y][nuevo_x] != 'E' &&
            posicion_ocupada_por_cliente(nuevo_y, nuevo_x) == 0) {

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

            sector->mapa[y][x] = sector->clientes[id_cliente].simbolo;

            sector->clientes[id_cliente].y = y;
            sector->clientes[id_cliente].x = x;

            combustible--;
            dibujar_mapa_compartido();
        }

        if (combustible <= 0 && jugando) {
            jugando = 0;
            mvwprintw(mapa, 10, 15, " GAME OVER ");
            mvwprintw(mensajes, 2, 2, "Sin combustible!");
            wrefresh(mapa);
            wrefresh(mensajes);
        }

        pthread_mutex_unlock(&mutex);
    }

    pthread_join(thread, NULL);

    liberar_cliente();

    delwin(mapa);
    delwin(mensajes);
    delwin(minerales);
    endwin();

    mq_close(cola_nave);
    mq_unlink(nombre_cola_nave);

    munmap(sector, sizeof(Sector));
    close(fd);

    return 0;
}