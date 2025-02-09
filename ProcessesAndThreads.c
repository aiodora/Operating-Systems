#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "a2_helper.h"
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

pthread_mutex_t m6;
pthread_cond_t c_start, c_end;
int start = 0, end = 0;

sem_t* sem8;
sem_t* sem6;

void* func_th6(void* id) {
    int th_id = *((int*)id);

    pthread_mutex_lock(&m6);

    if (th_id == 3) {
        start = 1;
        pthread_cond_signal(&c_start);
    } else if (th_id == 2) {
        while (!start) {
            pthread_cond_wait(&c_start, &m6);
        }
    } else if(th_id == 4) {
        //wait until thread 4 from process 8 ends
        sem_wait(sem6);
    }

    info(BEGIN, 6, th_id);

    if(th_id == 2) {
        info(END, 6, th_id);
        end = 1;
        pthread_cond_signal(&c_end);
    } else if(th_id == 3) {
        while(!end) {
            pthread_cond_wait(&c_end, &m6);
        }
        info(END, 6, th_id);
    } else if(th_id == 4) {
        info(END, 6, th_id);
        //annouce thread 3 from process 8 that thread 4 ended and it can start
        sem_post(sem8);
    } else {
        info(END, 6, th_id);
    } 

    pthread_mutex_unlock(&m6);
    return NULL;
}

sem_t semaphore5;
int num_running_th5 = 0;
int th11_start = 0;
int th11_done = 0;
pthread_mutex_t m5 = PTHREAD_MUTEX_INITIALIZER;

void P(sem_t *sem) {
    sem_wait(sem);
}

void V(sem_t *sem) {
    sem_post(sem);
}

void* func_th5(void* id) {
    int th_id = *((int*)id);

    if(th_id == 11) {
        P(&semaphore5);
        th11_start = 1;
        info(BEGIN, 5, th_id);
        pthread_mutex_lock(&m5);
        num_running_th5++;
        pthread_mutex_unlock(&m5);
    } else {
        while(!th11_start) {}
        if(!th11_done) {
            if(num_running_th5 < 5) {
                P(&semaphore5);
                info(BEGIN, 5, th_id);
                pthread_mutex_lock(&m5);
                num_running_th5++;
                pthread_mutex_unlock(&m5);
            } else {
                while(!th11_done) {}
                P(&semaphore5);
                info(BEGIN, 5, th_id);
                pthread_mutex_lock(&m5);
                num_running_th5++;
                pthread_mutex_unlock(&m5);
            }
        } 
        else {
            while(!th11_done) {}
            P(&semaphore5);
            info(BEGIN, 5, th_id);
            pthread_mutex_lock(&m5);
            num_running_th5++;
            pthread_mutex_unlock(&m5);
        }
    }

    if(th_id == 11) {
        while(num_running_th5 < 5) {} //sa nu-l las sa se termine inainte sa fie inca 4 threaduri 
        //dar na
        info(END, 5, 11);
        pthread_mutex_lock(&m5);
        num_running_th5--;
        pthread_mutex_unlock(&m5);
        th11_done = 1;
    } else {
        while(!th11_done) {}
        info(END, 5, th_id);
        pthread_mutex_lock(&m5);
        num_running_th5--;
        pthread_mutex_unlock(&m5);
    }

    V(&semaphore5);

    return NULL;
}

void* func_th8(void* id) {
    int th_id = *((int*)id);

    if(th_id == 3) {
        //thread 3 from process 8 must wait until thread 4 from process 6 finishes
        sem_wait(sem8);
    }

    info(BEGIN, 8, th_id);

    info(END, 8, th_id);

    if(th_id == 4) {
        //announce thread 4 from process 6 can start since thread 4 from process 8 has ended
        sem_post(sem6);
    }

    return NULL;
}

int main() {
    init();

    info(BEGIN, 1, 0);

    sem8 = sem_open("sem8", O_CREAT, 0644, 0);
    sem6 = sem_open("sem6", O_CREAT, 0644, 0);

    sem_post(sem8); sem_wait(sem8); 
    sem_post(sem6); sem_wait(sem6); 

    pid_t pid2 = fork();
    if(pid2 == 0) {
        info(BEGIN, 2, 0);

        pid_t pid9 = fork();
        if(pid9 == 0) {
            info(BEGIN, 9, 0);

            info(END, 9, 0);
            return 0;
        }

        waitpid(pid9, NULL, 0);

        info(END, 2, 0);
        return 0;
    }

    pid_t pid5 = fork();
    if(pid5 == 0) {
        info(BEGIN, 5, 0);
        pthread_t threads5[39];
        int id5[39];

        sem_init(&semaphore5, 0, 5);

        for(int i = 0; i < 39; i++) {
            id5[i] = i + 1;
            if(pthread_create(&threads5[i], NULL, func_th5, &id5[i]) != 0) {
                printf("Error creating a new thread");
	            exit(1);
            }
        }

        for(int i = 0; i < 39; i++) {
            pthread_join(threads5[i], NULL);
        }
        sem_destroy(&semaphore5);

        pid_t pid8 = fork();
        if(pid8 == 0) {
            info(BEGIN, 8, 0);

            pthread_t threads8[5];
            int id8[5];

            for(int i = 0; i < 5; i++) {
                id8[i] = i + 1;
                pthread_create(&threads8[i], NULL, func_th8, &id8[i]);
            }

            for(int i = 0; i < 5; i++) {
                pthread_join(threads8[i], NULL);
            }

            info(END, 8, 0);
            return 0;
        }

        waitpid(pid8, NULL, 0);

        info(END, 5, 0);
        return 0;
    }
    
    pid_t pid3 = fork();
    if(pid3 == 0) {
        info(BEGIN, 3, 0);

        pid_t pid4 = fork();
        if(pid4 == 0) {
            info(BEGIN, 4, 0);

            pid_t pid7 = fork();
            if(pid7 == 0) {
                info(BEGIN, 7, 0);

                info(END, 7, 0);
                return 0;
            }

            waitpid(pid7, NULL, 0);

            info(END, 4, 0);
            return 0;
        }

        pid_t pid6 = fork();
        if(pid6 == 0) {
            info(BEGIN, 6, 0);
            pthread_t threads6[4];
            int id[4];
            pthread_mutex_init(&m6, NULL);
            pthread_cond_init(&c_start, NULL);
            pthread_cond_init(&c_end, NULL);

            for(int i = 0; i < 4; i++) {
                id[i] = i + 1;
                pthread_create(&threads6[i], NULL, func_th6, &id[i]);
            }

            for(int i = 0; i < 4; i++) {
                pthread_join(threads6[i], NULL);
            }

            pthread_mutex_destroy(&m6);
            pthread_cond_destroy(&c_start);
            pthread_cond_destroy(&c_end);

            info(END, 6, 0);
            return 0;
        }

        waitpid(pid4, NULL, 0);
        waitpid(pid6, NULL, 0);

        info(END, 3, 0);
        return 0;
    }

    waitpid(pid2, NULL, 0);
    waitpid(pid3, NULL, 0);
    waitpid(pid5, NULL, 0);

    sem_close(sem8);
    sem_close(sem6);

    sem_unlink("sem8");
    sem_unlink("sem6");

    info(END, 1, 0);
    return 0;
}