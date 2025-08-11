
#include "../include/poste.h"

//List of available services
const char* services[NUM_SERVICE_TYPES] = {
    "Invio e ritiro pacchi",
    "Invio e ritiro lettere e raccomandate",
    "Prelievi e versamenti Bancoposta",
    "Pagamento bollettini postali",
    "Acquisto prodotti finanziari",
    "Acquisto orologi e braccialetti"
};

//List of durations(Minutes) refenced to services
const int* services_duration = {
    10,
    8,
    6,
    8,
    20,
    20
};

int main() {
    
}