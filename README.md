# Poste Italiane Simulation – C Implementation

> A multi-process simulation of an Italian post office built in C using POSIX IPC (shared memory, semaphores, System V message queues). Supports dynamic user addition, real-time scaling, comprehensive statistics (including CSV export), unit tests, and Docker.
> The choice of using english as for the documentation is given by person choice, if you find any issues let me know.
---

## Features

- **Multi-process architecture** with separate executables: director, ticket dispenser, operators, users, and `new_users`  
- **Real-time simulation** with configurable minute-to-nanosecond scaling  
- **Dynamic user addition** at runtime via the `new_users` tool  
- **POSIX IPC integration** using shared memory (`/poste_stats`, `/poste_stations`), semaphores, and System V message queues  
- **Comprehensive statistics** tracking daily, cumulative, per-service, operator utilization, pause counts, queue times  
- **Automatic CSV export** of final statistics to `./tmp/final_stats*.csv`  
- **Unit testing framework** for core modules (`test_time.c`, `test_shm_stats.c`)  
- **Docker support** for containerized deployment  
- **Configuration management** via files or environment variables  

---

## Repository Structure

```
├── configs/                   
│   ├── config_timeout.conf    # Sample timeout-trigger config  
│   └── config_explode.conf    # Sample explode-trigger config  
├── include/                   
│   ├── config.h               # Default parameters & g_config struct  
│   ├── poste.h                # Shared-memory data structures
│   ├── sharedmem.h             # Shared-memory functions wrapper
│   ├── msg_queue.h            # Message-Queues functions wrapper
│   ├── direttore.h            # Used solely for testing purposes on the direttore.c file
│   ├── erogatore_ticket.h     # Holds definitions used in the erogatore_ticket.c file
│   └── comunicazioni.h        # IPC communication definitions
├── src/                       
│   ├── direttore.c            # Director process, statistics & CSV export  
│   ├── erogatore_ticket.c     # Ticket dispenser logic  
│   ├── operatore.c            # Operator process logic  
│   ├── utente.c               # User process behavior  
│   ├── new_users.c            # Runtime user-addition client  
│   └── systems/               
│       ├── msg_queue.c        # System V message-queue wrapper  
│       ├── shared_mem.c       # POSIX shared-memory helper  
│       └── config.c           # Configuration loader implementation  
├── tests/                     
│   ├── smoke_test.sh          # End-to-end smoke script  
│   ├── test_time.c            # Unit test for time computations  
│   └── test_shm_stats.c       # Unit test for shared-memory stats  
├── msg/                       # Message queue key files
├── tmp/                       # CSV output directory (auto-created)
├── bin/                       # Compiled executables (auto-created)
├── obj/                       # Object files (auto-created)
├── makefile                   
├── Dockerfile                 
├── README.md                  
└── LICENSE                    
```

---

## Architecture

The simulation follows a **producer-consumer** pattern with multiple concurrent processes:

```
Director Process
├── spawns → Ticket Generator Process
├── spawns → N Operator Processes 
├── spawns → M User Processes
├── manages → Shared Resources (Statistics, Stations)
└── handles → Dynamic User Addition (via message queue)
```

**Process Communication Flow:**

- Users request tickets from the ticket generator via message queues
- Users present tickets to available operators at worker stations
- Operators process services and update shared statistics
- Director monitors simulation, reports statistics, handles user addition requests
- `new_users` client sends requests to director for adding users at runtime

---

## Build & Run

### Prerequisites

- **GCC compiler** with C99 support  
- **POSIX-compatible system** (Linux/Unix)  
- **Make utility**  
- **System V IPC** support (`ipcrm`, `ipcs`)  

### Compilation Flags

The project uses strict compilation flags for code quality:

```bash
gcc -Wvla -Wall -Wextra -Werror -g -std=c99
```

### Basic Build & Run

```bash
# Clean build
make clean
make all

# Clean IPCs and start simulation
make run

# Run with specific configurations
make run_explode
make run_timeout
```

### Add Users During Simulation

```bash
# Add 5 new users to running simulation
make add_users N=5

# Directly invoke the client
./bin/new_users --n-new-users 10
```

### Unit Tests

```bash
# Run all tests
make test

# Run specific test modules  
make unit

# Manual testing
./bin/test_time
./bin/test_shm_stats
```

### Docker Deployment

```bash
# Build container image
docker build -t poste-simulation .

# Run simulation in container
docker run --rm -it poste-simulation
```

---

## Configuration

### Configuration Files

Pass configuration files to override defaults:

```bash
./bin/direttore --config ./configs/config_timeout.conf
```

### Key Configuration Parameters

Set via configuration files or environment variables:

- **SIM_DURATION**: Simulation length in days (default: 5)
- **NUM_OPERATORS**: Number of operator processes (default: 10)  
- **NUM_USERS**: Number of user processes (default: 5)  
- **NUM_WORKER_SEATS**: Available service stations (default: 15)  
- **N_NANO_SECS**: Time scaling factor - nanoseconds per simulated minute (default: 50,000,000)  
- **WORKER_SHIFT_OPEN**: Opening hour (default: 8 AM)  
- **WORKER_SHIFT_CLOSE**: Closing hour (default: 8 PM)  
- **EXPLODE_MAX**: Max late users before explode termination (default: 30)  
- **MAX_N_REQUESTS**: Max services per user per day (default: 10)  
- **NOF_PAUSE**: Max operator early departures (default: 3)  

---

## Services Available

The simulation implements the 6 required postal services:

| Service | Average Time (minutes) |
|---------|------------------------|
| Invio e ritiro pacchi | 10 |
| Invio e ritiro lettere e raccomandate | 8 |
| Prelievi e versamenti Bancoposta | 6 |
| Pagamento bollettini postali | 8 |
| Acquisto prodotti finanziari | 20 |
| Acquisto orologi e braccialetti | 20 |

> **Note**: Service times are randomized within ±50% of the base value for realistic variation.

---

## Statistics & CSV Export

### Real-time Statistics

The director displays comprehensive statistics at the end of each simulated day:

- **Daily metrics**: served users, failed services, active operators, pauses taken  
- **Performance metrics**: average wait times, service times (general and per-service)  
- **Operator utilization**: ratio of active operators to available worker seats  
- **Service breakdown**: individual statistics for each of the 6 postal services  

### Final Statistics & CSV Export

At simulation end, all statistics are automatically exported to CSV format:

**File location**: `./tmp/final_stats.csv` (or `final_stats_1.csv`, etc., to avoid overwrites)

**CSV contents**:
- **Simulation Summary**: exit mode (timeout/explode), configuration parameters  
- **Global Statistics**: cumulative served/failed users, average wait/service times  
- **Per-Service Statistics**: breakdown for each of the 6 postal services  
- **Extra Information**: late users, total requests, detailed timing data  

---

## IPC Architecture

### Shared Memory Segments

| Endpoint | Description | Synchronization |
|----------|-------------|-----------------|
| `/poste_stats` | Global and daily statistics, simulation state | `stats_lock` semaphore |  
| `/poste_stations` | Worker seat status, operator assignments | `stations_lock` semaphore |

### Message Queues

- **Ticket requests**: Users ↔ Ticket Generator  
- **Service processing**: Users ↔ Operators  
- **Dynamic user addition**: `new_users` client ↔ Director  

### Semaphores

- **stats_lock**: Atomic statistics updates  
- **stations_lock**: Worker seat management  
- **open_poste_event**: Daily opening synchronization  
- **close_poste_event**: Daily closing synchronization  
- **day_update_event**: New day notifications  

---

## Troubleshooting

### Common Issues

**IPC Resource Cleanup**
```bash
# Remove all IPCs if simulation crashes
ipcrm -a

# Check current IPC status  
ipcs
```

**Permission Errors**
```bash
# Ensure proper permissions
chmod +x bin/*
```

**Build Errors**
```bash
# Clean rebuild
make clean
make all
```

**Message Queue Issues**
```bash
# Check if key files exist
ls -la msg/

# Ensure processes start in correct order
# (director should start before new_users client)
```

### Memory Management

The system automatically deallocates IPC resources on normal termination. After crashes or abnormal termination, use `ipcrm -a` to clean up orphaned resources.

---

## Implementation Requirements Compliance

### ✅ Versione Completa (30/30 Points)

**Core Requirements**:
- ✅ Multi-process architecture with separate executables  
- ✅ All 6 required postal services with correct timing  
- ✅ Director, operators, users, ticket dispenser processes  
- ✅ Shared memory, semaphores, and message queue IPC  
- ✅ Daily and cumulative statistics tracking  
- ✅ Timeout and explode termination conditions  

**Advanced Requirements**:
- ✅ Multiple service requests per user (N_REQUESTS)  
- ✅ Dynamic user addition via `new_users` executable  
- ✅ CSV statistics export functionality  

**Technical Requirements**:
- ✅ No busy waiting (proper semaphore synchronization)  
- ✅ Modular code with separate executables  
- ✅ Makefile-based build system  
- ✅ Maximum concurrency between processes  
- ✅ Proper IPC resource cleanup  
- ✅ Strict compilation flags (`-Wvla -Wall -Wextra -Werror`)  
- ✅ Multi-processor compatibility  

---

## Performance

Successfully tested on:
- **Virtual Machine**: 2 cores, 4GB RAM  
- **Load**: 60+ concurrent users, 10 operators  
- **Performance**: Stable operation with minimal slowdown  

The simulation demonstrates excellent scalability and resource management under realistic post office workloads.

---

## License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## About

**Course**: Sistemi Operativi (Operating Systems)  
**Institution**: Università degli Studi di Torino  
**Language**: C with POSIX IPC  
**Author**: Andrea Bertarione  (1097211)

A comprehensive simulation demonstrating advanced systems programming concepts including multi-process coordination, IPC mechanisms, shared memory management, and real-time system design.

---

## Development Notes

- Use `make run` for normal development testing  
- Use `make run_explode` and `make run_timeout` to test termination conditions  
- Monitor IPC resources with `ipcs` during development  
- CSV files are timestamped to preserve multiple test runs  
- Unit tests validate core time calculation and shared memory functions (I wasn't able to produce more tests in time, but atleast what i did should work)

For detailed implementation specifics, see the source code comments and the original project requirements in the repository.

---

<details>
<summary>📜 Original Project Requirements (click to expand)</summary>

# Poste-Italiane-Emulation-C

## Descrizione del progetto: versione minima (voto max 24 su 30)

Si intende simulare il funzionamento di un ufficio postale. A tal fine sono presenti i seguenti processi e risorse.

• Processo direttore Ufficio Posta, che gestisce la simulazione, e mantiene le statistiche su richieste e servizi erogati dall'ufficio. Genera gli sportelli, e gli operatori.
• Processo erogatore ticket: eroga ticket per specifici servizi. In particolare, dovranno essere implementate le funzionalità per garantire almeno i servizi elencati in Tab. 1.

> Il tempo indicato è da considerarsi il valore medio per erogare il servizio, e dovrà essere usato per generare un tempo casuale di erogazione nell'intorno ±50% del valore indicato.

### Tabella 1: Elenco dei servizi forniti dall'ufficio postale.

| servizio | tempario (in minuti) |
|----------|---------------------|
| Invio e ritiro pacchi | 10 |
| Invio e ritiro lettere e raccomandate | 8 |
| Prelievi e versamenti Bancoposta | 6 |
| Pagamento bollettini postali | 8 |
| Acquisto prodotti finanziari | 20 |
| Acquisto orologi e braccialetti | 20 |

- L'utente richiede un ticket specifico per uno dei servizi elencati in Tab. 1, attende il proprio turno, riceve la prestazione richiesta e torna a casa.
- Esistono risorse di tipo sportello; ogni sportello è specializzato nel fornire un solo tipo di prestazione, che varia ogni giorno della simulazione (vedi sopra elenco dei possibili servizi). Gli sportelli aprono e chiudono secondo la disponibilità degli operatori.
- NOF_WORKERS processi di tipo operatore: hanno un orario di lavoro, effettuano pause casuali.
- NOF_USERS processi di tipo utente. Il processo utente decide se recarsi all'ufficio postale e sceglie il servizio da richiedere.

### Processo Direttore

Il processo direttore è responsabile dell'avvio della simulazione, della creazione delle risorse di tipo sportello, dei processi operatore e utente, delle statistiche e della terminazione. Si noti bene che il processo direttore non si occupa dell'aggiornamento delle statistiche, ma solo della loro lettura, secondo quanto indicato. All'avvio, il processo direttore:

- crea un solo processo erogatore ticket.
- crea NOF_WORKER_SEATS risorse di tipo sportello.
- crea NOF_WORKERS processi di tipo operatore.
- crea NOF_USERS processi di tipo utente.

> Successivamente il direttore avvia la simulazione, che avrà come durata SIM_DURATION giorni, dove ciascun minuto è simulato dal trascorrere di N_NANO_SECS nanosecondi. La simulazione deve cominciare solamente quando tutti i processi erogatore, operatore e utente sono stati creati e hanno terminato la fase di inizializzazione. Alla fine di ogni giornata, il processo direttore dovrà stampare le statistiche totali e quelle della giornata, che comprendono:

- il numero di utenti serviti totali nella simulazione
- il numero di utenti serviti in media al giorno
- il numero di servizi erogati totali nella simulazione
- il numero di servizi non erogati totali nella simulazione
- il numero di servizi erogati in media al giorno
- il numero di servizi non erogati in media al giorno
- il tempo medio di attesa degli utenti nella simulazione
- il tempo medio di attesa degli utenti nella giornata
- il tempo medio di erogazione dei servizi nella simulazione
- il tempo medio di erogazione dei servizi nella giornata
- le statistiche precedenti suddivise per tipologia di servizio
- il numero di operatori attivi durante la giornata;
- il numero di operatori attivi durante la simulazione;
- il numero medio di pause effettuate nella giornata e il totale di pause effettuate durante la simulazione;
- il rapporto fra operatori disponibili e sportelli esistenti, per ogni sportello per ogni giornata.

### Processo erogatore ticket

Su richiesta di un processo utente, il processo erogatore ticket si occupa di erogare il ticket relativo alla prestazione richiesta, secondo quanto indicato in Tabella 1.

### Risorse sportello

Ogni giorno lo sportello è associato a un tipo di servizio dal direttore: ogni giorno ci possono essere più sportelli che offrono lo stesso servizio, oppure ci possono essere dei servizi non offerti da alcuno sportello. Ogni sportello può essere occupato da un singolo operatore; la politica di associazione operatore-sportello è definita dal progettista e deve essere applicata all'inizio di ogni giornata.

### Processo operatore

All'avvio, ogni processo operatore viene creato in modo che sia in grado di erogare uno dei servizi citati in Sezione 5. Tale mansione resta invariata per tutta la simulazione. All'inizio di ogni giornata lavorativa, l'operatore:

- Compete con gli altri operatori per la ricerca di uno sportello libero tra quelli disponibili nell'ufficio postale che si occupano del servizio che lui è in grado di svolgere
- Se ne trova uno, lo occupa e comincia il proprio lavoro che terminerà alla fine della giornata lavorativa
- Con un massimo di NOF_PAUSE volte per tutta la simulazione, l'operatore può decidere (secondo un criterio scelto dal programmatore) di interrompere il servizio della giornata anticipatamente. In questo caso:
- termina di servire il cliente che stava servendo;
- lascia libero lo sportello occupato;
- aggiorna le statistiche.
Il processo operatore che al suo arrivo non trova uno sportello libero:
- resta in attesa che uno sportello si liberi (per una pausa di un altro operatore);
- torna a casa a fine giornata, e si ripresenta regolarmente il giorno dopo.

### Processo utente

Ogni processo utente si reca presso l'ufficio postale saltuariamente per richiedere un servizio tra quelli disponibili. Più in dettaglio, ogni giorno ogni processo utente:

- decide se recarsi o meno all'ufficio postale, secondo una probabilità P_SERV differente per ogni utente e scelta singolarmente in fase di creazione dell'utente in un intervallo compreso tra i valori [P_SERV_MIN, P_SERV_MAX].
- In caso affermativo
  i. Stabilisce il servizio di cui vuole usufruire (secondo un criterio stabilito dall'utente);
  ii. Stabilisce un orario (secondo un criterio stabilito dall'utente);
  iii. Si reca all'ufficio postale;
  iv. Controlla se quel giorno l'ufficio postale può servire richieste per il tipo di servizio scelto;
  v. Se sì, ottiene un ticket per l'apposito servizio;
  vi. Attende il proprio turno e l'erogazione del servizio;
  vii. Torna a casa e attende il giorno successivo.

Se al termine della giornata l'utente si trova ancora in coda, abbandona l'ufficio rinunciando all'erogazione del servizio. Il numero di servizi non erogati è uno dei parametri da monitorare.

### Terminazione

La simulazione termina in una delle seguenti circostanze:
- timeout: raggiungimento della durata impostata SIM_DURATION giorni
- explode: numero di utenti in attesa al termine della giornata maggiore del valore EXPLODE_THRESHOLD
> Il gruppo di studenti deve produrre configurazioni (file config_timeout.conf e config_explode.conf) in grado di generare la terminazione nei casi sopra descritti. Al termine della simulazione, l'output del programma deve riportare anche la causa di terminazione e le statistiche finali.

## Descrizione del progetto: versione "completa" (max 30)

In questa versione:

- un processo utente, quando decide di recarsi all'ufficio postale, genera una lista di al massimo N_REQUESTS richieste di servizi di vario tipo (il numero deve essere scelto in modo casuale per ogni utente, per ogni giorno). Quindi si reca all'ufficio postale dove richiederà in sequenza un ticket per ogni servizio nella lista (il ticket per il servizio i potrà essere richiesto solo quando il servizio i − 1 è stato completato).
- attraverso un nuovo eseguibile invocabile da linea di comando, deve essere possibile aggiungere alla simulazione altri N_NEW_USERS processi utente oltre a quelli inizialmente generati dal direttore;
- tutte le statistiche prodotte devono anche essere salvate in un file testo di tipo csv, in modo da poter essere utilizzate per una analisi futura

## Configurazione

Tutti i parametri di configurazione sono letti a tempo di esecuzione, da file o da variabili di ambiente. Quindi, un cambiamento dei parametri non deve determinare una nuova compilazione dei sorgenti (non è consentito inserire i parametri uno alla volta da terminale una volta avviata la simulazione).

## Requisiti implementativi

Il progetto (sia in versione "minimal" che "normal") deve

1. evitare l'attesa attiva
2. utilizzare almeno memoria condivisa, semafori e un meccanismo di comunicazione fra processi a scelta fra code di messaggi o pipe,
3. essere realizzato sfruttando le tecniche di divisione in moduli del codice (per esempio, i vari processi devono essere lanciati da eseguibili diversi con execve(...)),
4. essere compilato mediante l'utilizzo dell'utility make
5. massimizzare il grado di concorrenza fra processi
6. deallocare le risorse IPC che sono state allocate dai processi al termine del gioco
7. essere compilato con almeno le seguenti opzioni di compilazione:

```bash
gcc -Wvla -Wextra -Werror
```

8. poter eseguire correttamente su una macchina (virtuale o fisica) che presenta parallelismo (due o più processori). Per i motivi introdotti a lezione, ricordarsi di definire la macro GNU_SOURCE o compilare il progetto con il flag

```bash
-D_GNU_SOURCE
```

</details>
