#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>
#include "a2_helper.h"
#include "stdbool.h"

#define SNAME1 "/sema1"
#define SNAME2 "/sema2"

int sem_id_p2;
int sem_id_p9;

sem_t sem2;
sem_t sem9;

sem_t *named_sem1;
sem_t *named_sem2;

pthread_mutex_t lock9_13;
pthread_cond_t condEnd9_13;
bool isStartedP9[5];
bool isFinished9_13;

pthread_mutex_t lock9_other[5];
pthread_cond_t condStart9_other;

pthread_mutex_t lock21;
pthread_cond_t condStart23;

pthread_mutex_t lock22;
pthread_cond_t condEnd21;

bool is23Started;
bool is21Finished;

// decrement by 1 the sempahore sem
//    - ask for a permission, i.e. wait until a permission become available
void P(sem_t *sem)
{
    sem_wait(sem);
}

// increment by one the sempahore sem
//   - release a previously obtained persmission
void V(sem_t *sem)
{
    sem_post(sem);
}


void* execThreadP2(void* threadID) {
    int id = *((int*) threadID);

    if (id == 1) {
        // wait for t.23 to start
        pthread_mutex_lock(&lock21);

        while (!is23Started) {
            pthread_cond_wait(&condStart23, &lock21);
        }

        info(BEGIN, 2, id);

        info(END, 2, id);
        is21Finished = true;
        pthread_cond_signal(&condEnd21);


        pthread_mutex_unlock(&lock21);
    } else if (id == 3) {

        info(BEGIN, 2, id);
        is23Started = true;
        pthread_cond_signal(&condStart23);

        pthread_mutex_lock(&lock22);

        // wait for 21 to end
        while (!is21Finished) {
            pthread_cond_wait(&condEnd21, &lock22);
        }

        info(END, 2, id);

        pthread_mutex_unlock(&lock22);
    } else if (id == 2) {
        P(named_sem1);
        info(BEGIN, 2, id);
        info(END, 2, id);
        V(named_sem2);
    } else {
        info(BEGIN, 2, id);
        info(END, 2, id);

    }

    return NULL;
}

void* execThreadP5(void* threadID) {
    int id = *((int*) threadID);

    if (id == 1) {
        info(BEGIN, 5, id);
        info(END, 5, id);
        V(named_sem1);
    } else if (id == 5) {
        P(named_sem2);
        info(BEGIN, 5, id);
        info(END, 5, id);
    } else {
        info(BEGIN, 5, id);
        info(END, 5, id);

    }
    return NULL;
}

void* execThreadP9(void* threadID) {
    int id = *((int*) threadID);

    if (id >= 41 && id <= 45) {
        int offsetid = id - 41;

        P(&sem9);
        info(BEGIN, 9, id);

        isStartedP9[offsetid] = true;

        if (pthread_cond_signal(&condStart9_other)  != 0) {
            perror("Cannot signal the condition waiters");
            exit(7);
        }

        if (pthread_mutex_lock(&lock9_other[offsetid]) != 0) {
            perror("Cannot take the lock");
            exit(4);
        }

        // wait for 13 to end
        while (!isFinished9_13) {
            printf("T%d = Waiting..\n", id);

            if (pthread_cond_wait(&condEnd9_13, &lock9_other[offsetid]) != 0) {
                perror("Cannot wait for condition");
                exit(6);
            }
        }

        isStartedP9[offsetid] = false;
        info(END, 9, id);
        V(&sem9);
        pthread_mutex_unlock(&lock9_other[offsetid]);
    } else if (id == 13) {
        P(&sem9);
        info(BEGIN, 9, id);

        if (pthread_mutex_lock(&lock9_13) != 0) {
            perror("Cannot take the lock");
            exit(4);
        }

        bool result = true;
        int i;

        for (i=0; i < 5; i++)
            if (! isStartedP9[i]) {
                result = false;
                break;
            }

        // wait for other 5 threads to start
        while (!result) {

            if (pthread_cond_wait(&condStart9_other, &lock9_13) != 0) {
                perror("Cannot wait for condition");
                exit(6);
            }

            result = true;
            for (i=0; i < 5; i++)
                if (! isStartedP9[i]) {
                    result = false;
                    break;
                }


            printf("T13 = Waiting.. %d\n", result);
        }


        info(END, 9, id);
        isFinished9_13 = true;

        for (i=0; i < 5; i++)
            if (pthread_cond_signal(&condEnd9_13)  != 0) {
                perror("Cannot signal the condition waiters");
                exit(7);
            }


        pthread_mutex_unlock(&lock9_13);
        V(&sem9);
    } else {
        P(&sem9);
        info(BEGIN, 9, id);
        info(END, 9, id);
        V(&sem9);
    }



    return NULL;
}

void make_p2_threads() {
    int i;
    int* state[2];
    int n = 5;
    pthread_t *t = (pthread_t*) malloc((n)*sizeof(pthread_t));
    int *id = (int*) malloc(n * sizeof(int));
    named_sem1 = sem_open(SNAME1, 0); /* Open a preexisting semaphore. */
    named_sem2 = sem_open(SNAME2, 0); /* Open a preexisting semaphore. */

    // create an unnamed, shared (1) semaphore, with initial value 3
    if (sem_init(&sem2, 1, 0) < 0) {
        perror("Error creating the semaphore");
        exit(2);
    }

    for (i=0; i < n; i++) {
        id[i] = i + 1;

        if (pthread_create(t+i, NULL, execThreadP2, &(id[i]))!=0) {
	         perror("Error creating a new thread");
	         exit(1);
        }

    }

    for(i=0; i < n; i++) {
        pthread_join(t[i], (void**) &state[0]);
    }


    sem_destroy(&sem2);
    sem_close(named_sem1);
    sem_close(named_sem2);
    free(t);
    free(id);
}

void make_p5_threads() {
    int i;
    int* state[2];
    int n = 5;
    pthread_t *t = (pthread_t*) malloc((n)*sizeof(pthread_t));
    int *id = (int*) malloc(n * sizeof(int));
    named_sem1 = sem_open(SNAME1, 0); /* Open a preexisting semaphore. */
    named_sem2 = sem_open(SNAME2, 0); /* Open a preexisting semaphore. */

    for (i=0; i < n; i++) {
        id[i] = i + 1;

        if (pthread_create(t+i, NULL, execThreadP5, &(id[i]))!=0) {
	         perror("Error creating a new thread");
	         exit(1);
        }
    }

    for(i=0; i < n; i++)
        pthread_join(t[i], (void**) &state[0]);

    sem_close(named_sem1);
    sem_close(named_sem2);
    free(t);
    free(id);
}

void make_p9_threads() {
    int i;
    int* state[2];
    int n = 45;
    int target_thread = 13;
    pthread_t *t = (pthread_t*) malloc((n)*sizeof(pthread_t));
    int *id = (int*) malloc(n * sizeof(int));

    // create an unnamed, shared (1) semaphore, with initial value 5
    if (sem_init(&sem9, 1, 6) < 0) { // todo: 5 or 6?
        perror("Error creating the semaphore");
        exit(2);
    }

    // create 45 - 6 threads
    for (i=0; i < n - 5; i++) {
        id[i] = i + 1;

        if (id[i] == target_thread) continue;

        if (pthread_create(t+i, NULL, execThreadP9, &id[i]) != 0) {
	         perror("Error creating a new thread");
	         exit(1);
        }
    }

    // join those threads
    for (i=0; i < n - 5; i++) {
        if (id[i] == target_thread) continue;

        pthread_join(t[i], (void**) &state[0]);
    }

    // create thread no. 13
    id[target_thread-1] = target_thread;
    if (pthread_create(t+target_thread-1, NULL, execThreadP9, &id[target_thread-1]) != 0) {
	         perror("Error creating a new thread");
	         exit(1);
        }


    // create 5 more threads
    for (i=n-5; i < n; i++) {
        id[i] = i + 1;

        if (pthread_create(t+i, NULL, execThreadP9, &id[i]) != 0) {
	         perror("Error creating a new thread");
	         exit(1);
        }
    }



    // join threads
    for (i=n-5; i < n; i++)
        pthread_join(t[i], (void**) &state[0]);


    pthread_join(t[target_thread-1], (void**) &state[0]);
    sem_destroy(&sem9);

    free(t);
    free(id);
}

int main() {
    sem_unlink(SNAME1);
    sem_unlink(SNAME2);
    if (!sem_open(SNAME1, O_CREAT, 0644, 0)) {
        perror("semaphore open.");
        exit(-1);
    }

    if (!sem_open(SNAME2, O_CREAT, 0644, 0)) {
        perror("semaphore open.");
        exit(-1);
    }

    init();

    info(BEGIN, 1, 0);

    pid_t pid2 = fork();

    switch (pid2) {
        case -1:
            // error case
            perror("Cannot create a new child");
            exit(1);
        case 0:
            // p2
            info(BEGIN, 2, 0);
            printf("P2, PID=%d, PPID=%d\n", getpid(), getppid());
            pid_t pid7 = fork();
            switch (pid7) {
                case -1:
                    // error case
                    perror("Cannot create a new child");
                    exit(1);
                case 0:
                    // p7
                    info(BEGIN, 7, 0);
                    printf("P7, PID=%d, PPID=%d\n", getpid(), getppid());

                    info(END, 7, 0);
                    break;
                default:
                    // p2
                    make_p2_threads();
                    printf("P2 Waited for PID=%d\n", wait(NULL));
                    info(END, 2, 0);
            }

            exit(2);
            break;
        default:
            // p1
            printf("P1, PID=%d\n", getpid());
            pid_t pid3 = fork();

            switch (pid3) {
                case -1:
                    // error case
                    perror("Cannot create a new child");
                    exit(1);
                case 0:
                    // p3
                    info(BEGIN, 3, 0);
                    printf("P3, PID=%d, PPID=%d\n", getpid(), getppid());
                    pid_t pid5 = fork();
                    switch (pid5) {
                        case -1:
                            // error case
                            perror("Cannot create a new child");
                            exit(1);
                        case 0:
                            // p5
                            info(BEGIN, 5, 0);
                            printf("P5, PID=%d, PPID=%d\n", getpid(), getppid());
                            make_p5_threads();
                            info(END, 5, 0);
                            break;
                        default:
                            // p3
                            ;
                            pid_t pid6 = fork();
                            switch (pid6) {
                                case -1:
                                    // error case
                                    perror("Cannot create a new child");
                                    exit(1);
                                case 0:
                                    // p6
                                    info(BEGIN, 6, 0);
                                    printf("P6, PID=%d, PPID=%d\n", getpid(), getppid());
                                    pid_t pid8 = fork();
                                    switch (pid8) {
                                        case -1:
                                            // error case
                                            perror("Cannot create a new child");
                                            exit(1);
                                        case 0:
                                            // p8
                                            info(BEGIN, 8, 0);
                                            printf("P8, PID=%d, PPID=%d\n", getpid(), getppid());
                                            info(END, 8, 0);
                                            break;
                                        default:
                                            ;
                                            // p6
                                            pid_t pid9 = fork();
                                            switch (pid9) {
                                                case -1:
                                                    // error case
                                                    perror("Cannot create a new child");
                                                    exit(1);
                                                case 0:
                                                    // p9
                                                    info(BEGIN, 9, 0);
                                                    printf("P9, PID=%d, PPID=%d\n", getpid(), getppid());
                                                    make_p9_threads();
                                                    info(END, 9, 0);
                                                    break;
                                                default:
                                                    // p6
                                                    printf("1.P6 Waited for PID=%d\n", wait(NULL));
                                                    printf("2.P6 Waited for PID=%d\n", wait(NULL));
                                                    info(END, 6, 0);
                                            }

                                    }
                                    break;
                                default:
                                    printf("1.P3 Waited for PID=%d\n", wait(NULL));
                                    printf("2.P3 Waited for PID=%d\n", wait(NULL));
                                    info(END, 3, 0);
                            }


                    }
                    exit(3);
                    break;
                default:
                    ;
                    // p1
                    pid_t pid4 = fork();
                    switch (pid4) {
                        case -1:
                            // error case
                            perror("Cannot create a new child");
                            exit(1);
                        case 0:
                            // p4
                            info(BEGIN, 4, 0);
                            printf("P4, PID=%d, PPID=%d\n", getpid(), getppid());
                            info(END, 4, 0);
                            break;
                        default:
                            // p1

                            printf("2.P1 Waited for PID=%d\n", wait(NULL));
                            printf("3.P1 Waited for PID=%d\n", wait(NULL));
                            printf("4.P1 Waited for PID=%d\n", wait(NULL));
                            info(END, 1, 0);

                    }

                    //exit(4);
            }
    }


    return 0;
}
