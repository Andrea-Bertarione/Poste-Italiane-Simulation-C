#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <comunications.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#define PREFIX "\e[1;36m[N-NEW-USERS]:\e[0m"

// Types
typedef struct S_new_users_request new_users_request;
typedef struct S_new_users_done new_users_done;

#ifndef UNIT_TEST
int main(const int argc, const char *argv[]) {
    int N_NEW_USERS = 1;
    if (argc > 1) {
        if (strcmp(argv[1], "--n-new-users") == 0 && argc == 3) {
            N_NEW_USERS = atoi((char *)argv[2]);
        }
    }

    if (N_NEW_USERS < 1) {
        printf(PREFIX " %d is an invalid number, using default value\n", N_NEW_USERS);
        fflush(stdout);
        N_NEW_USERS = 1;
    }

    key_t key = ftok(KEY_NEW_USERS, proj_ID_USERS);
    if (key == -1) { perror("ftok"); return 1; }
    mq_id qid = mq_open(key, 0666, 0666);
    if (qid < 0) {
        perror("mq_open");
        return 1;
    }

    printf(PREFIX " Adding %d new users to the Poste simulation\n", N_NEW_USERS);
    fflush(stdout);

    new_users_request request_msg;
    request_msg.N_NEW_USERS = N_NEW_USERS;
    request_msg.sender_pid = getpid();

    if (mq_send(qid, MSG_TYPE_ADD_USERS_REQUEST, &request_msg, sizeof(new_users_request)) < 0) {
        perror("mq_send request");
        return 1;
    }
    printf(PREFIX " Message sent to Direttore, awaiting response\n");

    new_users_done res;
    ssize_t n = mq_receive(qid, getpid(), &res, sizeof(res) , 0);
    if (n < 0) {
        if (errno == EINTR) return 1;  // interrupted by signal
        perror("msgrcv");
    }

    if (!res.status) {
        // Error
        printf(PREFIX " Received response message with failure status\n");
        return EXIT_FAILURE;
    }

    // Success
    printf(PREFIX " Received response message with success status\n");
    return EXIT_SUCCESS;
}
#endif  // UNIT_TEST