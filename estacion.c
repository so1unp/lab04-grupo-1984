#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

#include "comun.h"

#define COLA_ESTACION "/cola_estacion"
#define TAM_MENSAJE 100
#define SEM_HANGAR "/sem_hangar"

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

void registrar_evento(const char *evento)
{
    FILE *archivo;

    archivo = fopen("bitacora.txt", "a");

    if (archivo != NULL) {
        fprintf(archivo, "%s\n", evento);
        fclose(archivo);
    }
}

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

void avisar_naves_deuterio(void)
{
    int i;
    mqd_t cola_nave;
    char nombre_cola[30];
    char texto[] = "Estacion necesita deuterio";

    for (i = 0; i < MAX_CLIENTES; i++) {

        snprintf(
            nombre_cola,
            sizeof(nombre_cola),
            "/cola_nave_%d",
            i
        );

        cola_nave = mq_open(
            nombre_cola,
            O_WRONLY | O_NONBLOCK
        );

        if (cola_nave != (mqd_t)-1) {
            mq_send(
                cola_nave,
                texto,
                strlen(texto) + 1,
                0
            );

            mq_close(cola_nave);
        }
    }
}

int main(int argc, char *argv[])
{
    WINDOW *mapa;
    WINDOW *info;
    WINDOW *mensajes;

    struct mq_attr atributos;
    mqd_t cola;
    sem_t *sem_hangar;

    char mensaje[TAM_MENSAJE];
    char ultimo_mensaje[40] = "Sin mensajes";

    int fd;
    int tecla;
    int combustible = 100;
    int naves_hangar = 0;
    int dinero_total = 0;

    int precio_deut;
    int precio_mut;
    int precio_sem;
    int precio_ker;

    time_t ultimo_descuento;

    Sector *sector;

    (void) argc;
    (void) argv;

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

    atributos.mq_flags = 0;
    atributos.mq_maxmsg = 10;
    atributos.mq_msgsize = TAM_MENSAJE;
    atributos.mq_curmsgs = 0;

    mq_unlink(COLA_ESTACION);

    cola = mq_open(COLA_ESTACION, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &atributos);
    if (cola == (mqd_t)-1) {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }

    sem_unlink(SEM_HANGAR);

    sem_hangar = sem_open(SEM_HANGAR, O_CREAT, 0666, 3);
    if (sem_hangar == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    precio_deut = leer_valor_config("PRECIO_DEUTERIO", 10);
    precio_mut  = leer_valor_config("PRECIO_MUTEXIO", 20);
    precio_sem  = leer_valor_config("PRECIO_SEMAFORITA", 30);
    precio_ker  = leer_valor_config("PRECIO_KERNELIO", 40);
    registrar_evento("Estacion iniciada");

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    refresh();

    mapa = newwin(FILAS, COLUMNAS, 5, 25);
    info = newwin(10, 28, 5, 68);
    mensajes = newwin(8, 28, 16, 68);

    keypad(mapa, TRUE);
    wtimeout(mapa, 100);

    ultimo_descuento = time(NULL);

    while (combustible > 0) {
        if (mq_receive(cola, mensaje, TAM_MENSAJE, NULL) != -1) {
            if (strcmp(mensaje, "ENTRAR") == 0) {
                if (sem_trywait(sem_hangar) == 0) {
                    naves_hangar++;
                    registrar_evento("Nave ingreso al hangar");
                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "ENTRAR aceptado");
                } else {
                    registrar_evento("Ingreso rechazado: hangar lleno");
                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "Hangar lleno");
                }
            }

            if (strcmp(mensaje, "SALIR") == 0) {
                if (naves_hangar > 0) {
                    sem_post(sem_hangar);
                    naves_hangar--;
                    registrar_evento("Nave salio del hangar");
                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "SALIR aceptado");
                } else {
                    registrar_evento("Salida rechazada: hangar vacio");
                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "Hangar vacio");
                }
            }

            if (strncmp(mensaje, "VENDER", 6) == 0) {
                int deut;
                int mut;
                int sema;
                int ker;
                int total;

                if (sscanf(mensaje, "VENDER %d %d %d %d", &deut, &mut, &sema, &ker) == 4) {
                    total =
                        deut * precio_deut +
                        mut * precio_mut +
                        sema * precio_sem +
                        ker * precio_ker;

                    dinero_total += total;

                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "Venta: $%d", total);
                    registrar_evento("Venta de minerales");
                } else {
                    snprintf(ultimo_mensaje, sizeof(ultimo_mensaje), "Venta invalida");
                    registrar_evento("Venta invalida");
                }
            }
        }

        if (time(NULL) - ultimo_descuento >= 1) {
            combustible--;
            ultimo_descuento = time(NULL);
        }

      if (combustible == 20) {

    strcpy(
        ultimo_mensaje,
        "Solicitando deuterio"
    );

    avisar_naves_deuterio();

    registrar_evento(
        "Estacion solicita deuterio"
    );
   }

        dibujar_mapa(mapa, sector);

        werase(info);
        werase(mensajes);

        box(info, 0, 0);
        box(mensajes, 0, 0);

        mvwprintw(info, 0, 2, " ESTACION ");
        mvwprintw(mensajes, 0, 2, " MENSAJES ");

        mvwprintw(info, 2, 2, "Combustible: %d", combustible);
        mvwprintw(info, 3, 2, "Hangar: %d/3", naves_hangar);
        mvwprintw(info, 4, 2, "Dinero: $%d", dinero_total);
        mvwprintw(info, 6, 2, "Recibe mensajes");
        mvwprintw(info, 7, 2, "ENTRAR / SALIR");
        mvwprintw(info, 8, 2, "VENDER recursos");

        mvwprintw(mensajes, 2, 2, "Ultimo mensaje:");
        mvwprintw(mensajes, 3, 2, "%s", ultimo_mensaje);

        wrefresh(info);
        wrefresh(mensajes);

        tecla = wgetch(mapa);

        if (tecla == 'q') {
            break;
        }
    }

    registrar_evento("Estacion finalizada");

    delwin(mapa);
    delwin(info);
    delwin(mensajes);
    endwin();

    mq_close(cola);
    mq_unlink(COLA_ESTACION);

    sem_close(sem_hangar);
    sem_unlink(SEM_HANGAR);

    munmap(sector, sizeof(Sector));
    close(fd);

    printf("Estacion finalizada.\n");

    exit(EXIT_SUCCESS);
}
