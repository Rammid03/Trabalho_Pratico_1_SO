#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#define NUM_CLIENTES 5
#define NUM_RECURSOS 3

int disponivel[NUM_RECURSOS];
int maximo[NUM_CLIENTES][NUM_RECURSOS];
int alocado[NUM_CLIENTES][NUM_RECURSOS];
int necessario[NUM_CLIENTES][NUM_RECURSOS];

pthread_mutex_t mutex;
volatile sig_atomic_t keep_running = 1; // Controla a execução das threads

// Handler para SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    keep_running = 0;
}

// Verifica se o sistema está em um estado seguro
int esta_seguro() {
    int trabalho[NUM_RECURSOS];
    int finalizado[NUM_CLIENTES] = {0};

    // Inicializa vetor de trabalho com recursos disponíveis
    for (int i = 0; i < NUM_RECURSOS; i++)
        trabalho[i] = disponivel[i];

    int count = 0;
    while (count < NUM_CLIENTES) {
        int encontrou = 0;
        for (int i = 0; i < NUM_CLIENTES; i++) {
            if (!finalizado[i]) {
                int j;
                // Verifica se as necessidades do cliente podem ser atendidas
                for (j = 0; j < NUM_RECURSOS; j++) {
                    if (necessario[i][j] > trabalho[j])
                        break;
                }
                if (j == NUM_RECURSOS) { // Cliente pode terminar
                    for (int k = 0; k < NUM_RECURSOS; k++)
                        trabalho[k] += alocado[i][k];
                    finalizado[i] = 1;
                    encontrou = 1;
                    count++;
                }
            }
        }
        if (!encontrou) // Nenhum cliente pode terminar
            return 0;
    }
    return 1;
}

// Solicita recursos para um cliente
int solicitar_recursos(int cliente, int solicitacao[]) {
    pthread_mutex_lock(&mutex);

    // Verifica se a solicitação é válida
    for (int i = 0; i < NUM_RECURSOS; i++) {
        if (solicitacao[i] > necessario[cliente][i]) {
            pthread_mutex_unlock(&mutex);
            return -1; // Solicitação excede necessidade
        }
        if (solicitacao[i] > disponivel[i]) {
            pthread_mutex_unlock(&mutex);
            return -2; // Recursos insuficientes
        }
    }

    // Aloca recursos temporariamente
    for (int i = 0; i < NUM_RECURSOS; i++) {
        disponivel[i] -= solicitacao[i];
        alocado[cliente][i] += solicitacao[i];
        necessario[cliente][i] -= solicitacao[i];
    }

    // Verifica se o estado é seguro
    if (!esta_seguro()) {
        // Reverte a alocação
        for (int i = 0; i < NUM_RECURSOS; i++) {
            disponivel[i] += solicitacao[i];
            alocado[cliente][i] -= solicitacao[i];
            necessario[cliente][i] += solicitacao[i];
        }
        pthread_mutex_unlock(&mutex);
        return -3; // Estado inseguro
    }

    pthread_mutex_unlock(&mutex);
    return 0; // Sucesso
}

// Libera recursos de um cliente
int liberar_recursos(int cliente, int liberacao[]) {
    pthread_mutex_lock(&mutex);

    // Verifica se a liberação é válida
    for (int i = 0; i < NUM_RECURSOS; i++) {
        if (liberacao[i] > alocado[cliente][i]) {
            pthread_mutex_unlock(&mutex);
            return -1; // Liberação excede alocação
        }
    }

    // Atualiza as estruturas
    for (int i = 0; i < NUM_RECURSOS; i++) {
        disponivel[i] += liberacao[i];
        alocado[cliente][i] -= liberacao[i];
        necessario[cliente][i] += liberacao[i];
    }

    pthread_mutex_unlock(&mutex);
    return 0; // Sucesso
}

// Função executada por cada thread cliente
void* cliente_thread(void* arg) {
    int cliente = *(int*)arg;
    int requisicao[NUM_RECURSOS];
    int liberacao[NUM_RECURSOS];

    while (keep_running) {
        sleep(rand() % 3 + 1);

        // Gera solicitação aleatória
        for (int i = 0; i < NUM_RECURSOS; i++)
            requisicao[i] = rand() % (necessario[cliente][i] + 1);

        int result = solicitar_recursos(cliente, requisicao);
        if (result == 0) {
            printf("Cliente %d solicitou: ", cliente);
            for (int i = 0; i < NUM_RECURSOS; i++)
                printf("%d ", requisicao[i]);
            printf("\n");
        } else {
            printf("Cliente %d: Solicitação negada: ", cliente);
            for (int i = 0; i < NUM_RECURSOS; i++)
                printf("%d ", requisicao[i]);
            if (result == -1)
                printf("(Excede necessidade)\n");
            else if (result == -2)
                printf("(Recursos insuficientes)\n");
            else
                printf("(Estado inseguro)\n");
        }

        sleep(rand() % 3 + 1);

        // Gera liberação aleatória
        for (int i = 0; i < NUM_RECURSOS; i++)
            liberacao[i] = rand() % (alocado[cliente][i] + 1);

        if (liberar_recursos(cliente, liberacao) == 0) {
            printf("Cliente %d liberou: ", cliente);
            for (int i = 0; i < NUM_RECURSOS; i++)
                printf("%d ", liberacao[i]);
            printf("\n");
        }
    }

    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != NUM_RECURSOS + 1) {
        printf("Uso correto: %s <recurso1> <recurso2> <recurso3>\n", argv[0]);
        return -1;
    }

    // Configura handler para SIGINT
    signal(SIGINT, sigint_handler);

    srand(time(NULL));
    pthread_mutex_init(&mutex, NULL);

    // Valida e inicializa recursos disponíveis
    for (int i = 0; i < NUM_RECURSOS; i++) {
        char *endptr;
        disponivel[i] = strtol(argv[i + 1], &endptr, 10);
        if (*endptr != '\0' || disponivel[i] < 0) {
            printf("Erro: Argumento %s inválido. Use números não negativos.\n", argv[i + 1]);
            pthread_mutex_destroy(&mutex);
            return -1;
        }
    }

    // Exibe estado inicial
    printf("Recursos disponíveis: ");
    for (int i = 0; i < NUM_RECURSOS; i++)
        printf("%d ", disponivel[i]);
    printf("\n");

    // Inicializa matrizes
    for (int i = 0; i < NUM_CLIENTES; i++) {
        for (int j = 0; j < NUM_RECURSOS; j++) {
            maximo[i][j] = rand() % (disponivel[j] + 1);
            alocado[i][j] = 0;
            necessario[i][j] = maximo[i][j];
        }
    }

    // Exibe matriz máximo
    printf("Matriz máximo:\n");
    for (int i = 0; i < NUM_CLIENTES; i++) {
        printf("Cliente %d: ", i);
        for (int j = 0; j < NUM_RECURSOS; j++)
            printf("%d ", maximo[i][j]);
        printf("\n");
    }

    // Cria threads
    pthread_t threads[NUM_CLIENTES];
    int ids[NUM_CLIENTES];

    for (int i = 0; i < NUM_CLIENTES; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, cliente_thread, &ids[i]);
    }

    // Aguarda término (controlado por SIGINT)
    for (int i = 0; i < NUM_CLIENTES; i++)
        pthread_join(threads[i], NULL);

    // Limpeza
    pthread_mutex_destroy(&mutex);
    printf("Programa finalizado.\n");
    return 0;
}
