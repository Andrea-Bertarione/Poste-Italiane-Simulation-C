// include/direttore_core.h
#ifndef DIRETTORE_CORE_H
#define DIRETTORE_CORE_H

#include <semaphore.h>
#include "poste.h"

int day_to_minutes(int days);
void start_new_day(int day, struct S_poste_stats* shared_stats, struct S_poste_stations* shared_stations);

#endif