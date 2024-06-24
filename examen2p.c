#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <semaphore.h>

// Estructura para representar un nodo de la cola
typedef struct nodo
{
    char id_auto[4]; // ID del auto
    int id; // ID numérico del auto
    int pos_puente; // Posición actual del auto en el puente (0-3)
    struct nodo* sig; // Puntero al siguiente nodo
} NODO_COLA;

// Estructura para representar una cola
typedef struct cola
{
    NODO_COLA* cabeza, * pie; // Punteros a la cabeza y al final de la cola
    int cant; // Cantidad de nodos en la cola
} COLA;

// Prototipos de funciones
void inicializar(COLA*);
NODO_COLA* crear(char[]);
void encolar(COLA*, char[]);
char* desencolar(COLA*);
int estaRepetido(COLA, COLA, char[]);
void incrementar(COLA*);
int vacio(COLA);
int cantidad(COLA);

// Variables globales para representar las colas y los recursos de sincronización
COLA puente, fila_izquierda, fila_derecha;
pthread_mutex_t lock; // Mutex para proteger las colas y el sentido actual
pthread_cond_t puente_libre; // Variable de condición para avisar que el puente está libre
sem_t semaforo_izquierda, semaforo_derecha; // Semáforos para controlar el acceso a las colas
int c2 = 0, coches_en_puente = 0, sentido_actual = -1; // -1: ninguno, 0: derecha, 1: izquierda
int entrada_comando(char[], char[]); // Función para procesar comandos del usuario
void status(int); // Función para mostrar el estado del puente y las colas
void posxy(int, int); // Función para posicionar el cursor en la pantalla
void car(char[], int); // Función para agregar un auto a una cola
void* starto(void*); // Función para simular el movimiento de los autos

// Función principal
int main(int argc, char const* argv[])
{
    system("clear"); // Limpia la pantalla

    pthread_t p2, p3; // Hilos para simular el movimiento de los autos

    // Inicializa los recursos de sincronización
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&puente_libre, NULL);
    sem_init(&semaforo_izquierda, 0, 0);
    sem_init(&semaforo_derecha, 0, 0);

    // Inicializa las colas
    inicializar(&fila_izquierda);
    inicializar(&fila_derecha);
    inicializar(&puente);

    // Variables para almacenar comandos e IDs de autos
    char comando[15];
    char id_auto[10];
    char end = '\0';

    do
    {
        do
        {
            // Solicita un comando al usuario
            puts("Ingrese un comando: ");
            comando[0] = end;
            id_auto[0] = end;
            scanf("%15[^\n]", comando);
            getchar();
            strcat(comando, &end);
            int accion = entrada_comando(comando, id_auto);

            // Procesa el comando
            switch (accion)
            {
            case 0:
                car(id_auto, 0); // Agrega un auto a la cola derecha
                break;
            case 1:
                car(id_auto, 1); // Agrega un auto a la cola izquierda
                break;
            case 3:
                status(-1); // Muestra el estado del puente y las colas
                break;
            case 4:
                break;
            default:
                printf("ERROR: Comando no valido\n\n");
            }
        } while (strcmp(comando, "start"));

        // Inicia la simulación
        status(-1);
        sleep(1);
        int izq = 1, der = 0;
        pthread_create(&p2, NULL, &starto, (void*)&izq);
        pthread_create(&p3, NULL, &starto, (void*)&der);
        pthread_join(p3, NULL);
        pthread_join(p2, NULL);
    } while (strcmp(comando, "stop"));

    // Libera los recursos de sincronización
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&puente_libre);
    sem_destroy(&semaforo_izquierda);
    sem_destroy(&semaforo_derecha);

    return 0;
}

// Función para agregar un auto a una cola
void car(char id_auto[], int sentido)
{
    COLA* cola = sentido ? &fila_izquierda : &fila_derecha;
    if (!estaRepetido(fila_izquierda, fila_derecha, id_auto))
    {
        encolar(cola, id_auto);
        printf("Auto agregado a la cola %s\n\n", sentido ? "izquierda" : "derecha");
        if (sentido)
        {
            sem_post(&semaforo_izquierda); // Incrementa el semáforo de la cola izquierda
        }
        else
        {
            sem_post(&semaforo_derecha); // Incrementa el semáforo de la cola derecha
        }
    }
    else
    {
        puts("ERROR: no se pueden agregar autos repetidos");
    }
}

// Función para mostrar el estado del puente y las colas
void status(int sentido)
{
    char bridge[] = "=======================================================";
    system("clear");
    int i;
    for (i = 1; i <= 5; i++)
    {
        posxy(i, 15);
        printf("%s", bridge);
    }
    NODO_COLA* auxd = fila_derecha.cabeza, * auxi = fila_izquierda.cabeza;
    i++;
    posxy(i, 1);
    printf("**Autos en espera a la izquierda**");
    posxy(i, 50);
    printf("**Autos en espera a la derecha**\n");
    i++;
    while (auxd || auxi)
    {
        if (auxi)
        {
            posxy(i, 1);
            printf("=> %s", auxi->id_auto);
            auxi = auxi->sig;
        }
        if (auxd)
        {
            posxy(i, 50);
            printf("<= %s", auxd->id_auto);
            auxd = auxd->sig;
        }
        i++;
    }
    i++;

    if (sentido != -1)
    {
        char raya_auto[2][11] = {{"<<<<<<<<<<"}, {">>>>>>>>>>"}};
        int j;
        int pos1[3] = {14, 32, 50}, pos2[3] = {50, 32, 14};
        incrementar(&puente);
        NODO_COLA* aux = puente.cabeza;
        while (aux)
        {
            if (aux->pos_puente > 0)
            {
                for (j = 2; j <= 4; j++)
                {
                    posxy(j, sentido ? pos1[aux->pos_puente - 1] : pos2[aux->pos_puente - 1]);
                    printf("%s", raya_auto[sentido]);
                }
                posxy(3, sentido ? pos1[aux->pos_puente - 1] + 2 : pos2[aux->pos_puente - 1] + 2);
                printf("%s", aux->id_auto);
            }
            aux = aux->sig;
        }
    }
    fflush(stdout);
    posxy(i, 1);
}

// Función para simular el movimiento de los autos
void* starto(void* data)
{
    int lado = *(int*)data; // 0: derecha, 1: izquierda
    int autos_pasados = 0;

    while (!vacio(fila_izquierda) || !vacio(fila_derecha) || !vacio(puente))
    {
        COLA* cola = lado ? &fila_izquierda : &fila_derecha;
        if (!vacio(*cola))
        {
            // Bloquea el mutex y espera que el puente esté libre
            pthread_mutex_lock(&lock);
            sentido_actual = lado;
            for (int i = 0; i < 4 && !vacio(*cola); i++)
            {
                // Agrega los autos al puente
                encolar(&puente, desencolar(cola));
                if (cantidad(puente) > 3) desencolar(&puente); // Elimina el auto que esté en la última posición
                status(lado);
                sleep(1);
                autos_pasados++;
            }
            // Espera a que los autos del puente salgan
            while (!vacio(puente))
            {
                if (puente.cabeza->pos_puente == 3) desencolar(&puente);
                status(lado);
                sleep(1);
            }
            sentido_actual = -1; // El puente está libre
            pthread_cond_broadcast(&puente_libre); // Desbloquea los hilos que esperan en la condición
            pthread_mutex_unlock(&lock); // Libera el mutex

            // Cambia de sentido cada 4 autos
            if (autos_pasados >= 4)
            {
                autos_pasados = 0;
                lado = !lado;
            }
        }
        else
        {
            lado = !lado; // Cambia de lado si la cola actual está vacía
            sleep(1);
        }
    }
    return NULL;
}

// Función para posicionar el cursor en la pantalla
void posxy(int x, int y)
{
    printf("\033[%d;%df", x, y);
    fflush(stdout);
}

// Función para inicializar una cola
void inicializar(COLA* c)
{
    c->cabeza = c->pie = NULL;
    c->cant = 0;
}

// Función para crear un nodo de la cola
NODO_COLA* crear(char nombre[])
{
    NODO_COLA* temp = (NODO_COLA*)malloc(sizeof(NODO_COLA));
    strcpy(temp->id_auto, nombre);
    temp->pos_puente = 0;
    temp->sig = NULL;
    return temp;
}

// Función para agregar un nodo al final de la cola
void encolar(COLA* cola, char nombre[])
{
    NODO_COLA* nodo = crear(nombre);
    if ((*cola).cabeza == NULL) (*cola).cabeza = nodo;
    else (*cola).pie->sig = nodo;
    (*cola).pie = nodo;
    cola->cant++;
}

// Función para eliminar el primer nodo de la cola
char* desencolar(COLA* cola)
{
    char* ret = NULL;
    if (cola != NULL)
    {
        if ((cola->cabeza) != NULL)
        {
            ret = (char*)calloc(7, sizeof(char));
            strcpy(ret, cola->cabeza->id_auto);
            NODO_COLA* aux = cola->cabeza;
            cola->cabeza = cola->cabeza->sig;
            free(aux);
            if (cola->cabeza == NULL) cola->pie = NULL;
            cola->cant--;
        }
    }
    return ret;
}

// Función para verificar si un ID de auto ya existe en las colas
int estaRepetido(COLA cola_izq, COLA cola_der, char id[])
{
    NODO_COLA* actual_izq = cola_izq.cabeza;
    NODO_COLA* actual_der = cola_der.cabeza;
    while (actual_izq != NULL)
    {
        if (!strcmp(actual_izq->id_auto, id)) return 1;
        actual_izq = actual_izq->sig;
    }
    while (actual_der != NULL)
    {
        if (!strcmp(actual_der->id_auto, id)) return 1;
        actual_der = actual_der->sig;
    }
    return 0;
}

// Función para incrementar la posición de los autos en el puente
void incrementar(COLA* cola)
{
    NODO_COLA* aux = cola->cabeza;
    while (aux)
    {
        if (aux->pos_puente >= 3 || aux->pos_puente < 0) aux->pos_puente = -1;
        else aux->pos_puente++;
        aux = aux->sig;
    }
}

// Función para verificar si una cola está vacía
int vacio(COLA c) { return c.cabeza ? 0 : 1; }

// Función para obtener la cantidad de nodos en una cola
int cantidad(COLA c) { return c.cant; }

// Función para procesar comandos del usuario
int entrada_comando(char comando[], char id_auto[])
{
    char aux[4] = "", direccion[4];

    strncpy(aux, comando, 3);
    aux[3] = '\0';

    if (!strcmp(aux, "car") && strlen(comando) > 3)
    {
        sprintf(id_auto, "%s%02i", "auto", ++c2);
        id_auto[6] = '\0';
        strncpy(direccion, &comando[4], 3);
        direccion[3] = '\0';
        if (!strcmp(direccion, "izq")) return 1;
        if (!strcmp(direccion, "der")) return 0;
    }
    else if (!strcmp(comando, "status")) return 3;
    else if (!strcmp(comando, "start")) return 4;
    return -1;
}