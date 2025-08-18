// include/direttore_core.h
#ifndef DIRETTORE_CORE_H
#define DIRETTORE_CORE_H

#include <semaphore.h>
#include "poste.h"

// TYPES
typedef struct S_poste_stats poste_stats;
typedef struct S_daily_stats daily_stats;
typedef struct S_service_stats service_stats;
typedef struct S_poste_stations poste_stations;
typedef struct S_worker_seat worker_seat;

int day_to_minutes(int days);
void start_new_day(int day, poste_stats* shared_stats, poste_stations* shared_stations);

#endif