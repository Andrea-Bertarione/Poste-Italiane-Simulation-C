#ifndef EROGATORE_TICKET_H
#define EROGATORE_TICKET_H

#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>

#define SERVICE_TIME_SPREAD 50
#define QUEUE_SIZE 1000

enum SERVICE_ID {
    I_R_PACCHI,
    I_R_LETTERE,
    P_V_BANCOPOSTA,
    P_BOLLETTINI,
    A_PRODOTTI_FINANZIARI,
    A_OROLOGI_BRACCIALETT
};

struct S_ticket {
    enum SERVICE_ID service;
    pid_t user;
    int ticket_number;
    int in_use;
};

struct S_ticket_queue {
    //Array of tickets
    struct S_ticket tickets_queue[QUEUE_SIZE];
    int ticket_counter;

    // Synchronization
    sem_t ticket_lock;  // Semaphore index for atomic updates
};

#define SHM_TICKET_NAME   "/poste_tickets"
#define SHM_TICKETS_SIZE   sizeof(struct S_ticket_queue)

#endif