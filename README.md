# Poste Italiane Emulation - C Implementation

A multi-process simulation of an Italian post office built in C using POSIX IPC mechanisms.

## Features

- **Multi-process architecture** with concurrent operators, users, and ticket dispensing
- **Real-time simulation** with configurable time scaling and duration
- **Comprehensive statistics** tracking service times, queue lengths, and operator efficiency
- **Flexible service configuration** with 6 different postal services
- **POSIX IPC integration** using shared memory, semaphores, and message queues
- **Docker support** for easy deployment and testing
- **Unit testing framework** with automated test suite

## Structure

```
├── src/
│   ├── direttore.c          # Main director process
│   ├── erogatore_ticket.c   # Ticket dispenser
│   ├── operatore.c          # Operator logic
│   ├── utente.c             # User behavior
│   └── systems/             # Core system utilities
│       ├── msg_queue.c      # Message queue implementation
│       └── shared_mem.c     # Shared memory management
├── include/                 # Header files and definitions
├── tests/                   # Unit tests and test utilities
├── docs/                    # Documentation
└── bin/                     # Compiled executables
```

## Architecture

The simulation follows a **producer-consumer** pattern with multiple concurrent processes:

```
Director Process
├── spawns → Ticket Generator Process
├── spawns → N Operator Processes  
├── spawns → M User Processes
└── manages → Shared Resources (Statistics, Stations, Tickets)
```

**Process Communication Flow:**
- Users request tickets from the ticket generator
- Users present tickets to available operators
- Operators process services and update statistics
- Director monitors and reports daily/total statistics

## Shared Memory

> Each memory segment has associated semaphores to ensure atomic operations

| Endpoint | Description |
|----------|-------------|
| `/poste_stats` | Holds all simulation statistics (daily, total, per-service) |
| `/poste_stations` | Shows currently working operators and station status |
| `/poste_tickets` | Manages active tickets being processed |

## Inter Process Communication

> Message queues are used instead of pipes to simplify process communication management

```
User ──(ticket request)────────▶ Ticket Generator
User ◀─(ticket number)────────── Ticket Generator

User ──(present ticket)────────▶ Operator
User ◀─(service done)─────────── Operator
```

## Build Requirements

- **GCC compiler** with C99 support
- **POSIX-compatible system** (Linux/Unix)
- **Make utility**
- **pthread library** (usually pre-installed)
- **realtime extensions** (`-lrt` flag support)

### Compilation Flags
The project uses strict compilation flags for code quality:
```bash
gcc -Wall -Wextra -Werror -g -std=c99
```

## Quick Start

### Basic Usage
```bash
make all
./bin/direttore
```

### With Docker
```bash
docker build -t poste-simulation .
docker run -it poste-simulation
```

### Development Workflow
```bash
# Clean build
make clean
make all

# Run tests
make test

# Clean IPCs and run
make run
```

## Configuration

The simulation supports multiple configuration parameters that can be set via:
- **Environment variables**
- **Configuration files**
- **Header definitions** in `include/poste.h`

Key configurable parameters:
- `SIM_DURATION` - Simulation length in days
- `NUM_OPERATORS` - Number of operator processes
- `NUM_USERS` - Number of user processes  
- `NUM_WORKER_SEATS` - Available service stations
- `N_NANO_SECS` - Time scaling factor

## Services Available

| Service | Average Time (minutes) |
|---------|----------------------|
| Invio e ritiro pacchi | 10 |
| Invio e ritiro lettere e raccomandate | 8 |
| Prelievi e versamenti Bancoposta | 6 |
| Pagamento bollettini postali | 8 |
| Acquisto prodotti finanziari | 20 |
| Acquisto orologi e braccialetti | 20 |

> Service times are randomized within ±50% of the base value

## Testing

```bash
# Run all tests
make test

# Run specific test modules
make unit

# Manual testing
./tests/test_time
./tests/test_shm_stats
```

## Statistics Output

The simulation provides comprehensive daily and cumulative statistics:

- **Service Metrics**: Users served, failed services, average times
- **Operator Metrics**: Active operators, break frequency, station utilization  
- **Performance Metrics**: Queue times, service efficiency, throughput
- **Per-Service Breakdown**: Individual statistics for each postal service

## Troubleshooting

### Common Issues

**IPC Resource Cleanup**
```bash
# Remove all IPCs if simulation crashes
ipcrm -a
```

**Permission Errors**
```bash
# Ensure proper permissions for shared memory
chmod +x bin/*
```

**Build Errors**
```bash
# Clean rebuild
make clean
make all
```

### Memory Leaks
The system automatically deallocates IPC resources on normal termination. Use `ipcrm -a` after abnormal termination.

## Docker Support

```dockerfile
# Build image
docker build -t poste-simulation .

# Run simulation
docker run --rm -it poste-simulation
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## About

A Poste Italiane simulation university project for the Operating Systems course at University of Turin (Unito). 

**Course**: Sistemi Operativi  
**Institution**: Università degli Studi di Torino  
**Language**: C with POSIX IPC  

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

• L'utente richiede un ticket specifico per uno dei servizi elencati in Tab. 1, attende il proprio turno, riceve la prestazione richiesta e torna a casa.
• Esistono risorse di tipo sportello; ogni sportello è specializzato nel fornire un solo tipo di prestazione, che varia ogni giorno della simulazione (vedi sopra elenco dei possibili servizi). Gli sportelli aprono e chiudono secondo la disponibilità degli operatori.
• NOF_WORKERS processi di tipo operatore: hanno un orario di lavoro, effettuano pause casuali.
• NOF_USERS processi di tipo utente. Il processo utente decide se recarsi all'ufficio postale e sceglie il servizio da richiedere.

### Processo Direttore

Il processo direttore è responsabile dell'avvio della simulazione, della creazione delle risorse di tipo sportello, dei processi operatore e utente, delle statistiche e della terminazione. Si noti bene che il processo direttore non si occupa dell'aggiornamento delle statistiche, ma solo della loro lettura, secondo quanto indicato. All'avvio, il processo direttore:

• crea un solo processo erogatore ticket.
• crea NOF_WORKER_SEATS risorse di tipo sportello.
• crea NOF_WORKERS processi di tipo operatore.
• crea NOF_USERS processi di tipo utente.

> Successivamente il direttore avvia la simulazione, che avrà come durata SIM_DURATION giorni, dove ciascun minuto è simulato dal trascorrere di N_NANO_SECS nanosecondi. La simulazione deve cominciare solamente quando tutti i processi erogatore, operatore e utente sono stati creati e hanno terminato la fase di inizializzazione. Alla fine di ogni giornata, il processo direttore dovrà stampare le statistiche totali e quelle della giornata, che comprendono:

• il numero di utenti serviti totali nella simulazione
• il numero di utenti serviti in media al giorno
• il numero di servizi erogati totali nella simulazione
• il numero di servizi non erogati totali nella simulazione
• il numero di servizi erogati in media al giorno
• il numero di servizi non erogati in media al giorno
• il tempo medio di attesa degli utenti nella simulazione
• il tempo medio di attesa degli utenti nella giornata
• il tempo medio di erogazione dei servizi nella simulazione
• il tempo medio di erogazione dei servizi nella giornata
• le statistiche precedenti suddivise per tipologia di servizio
• il numero di operatori attivi durante la giornata;
• il numero di operatori attivi durante la simulazione;
• il numero medio di pause effettuate nella giornata e il totale di pause effettuate durante la simulazione;
• il rapporto fra operatori disponibili e sportelli esistenti, per ogni sportello per ogni giornata.

### Processo erogatore ticket

Su richiesta di un processo utente, il processo erogatore ticket si occupa di erogare il ticket relativo alla prestazione richiesta, secondo quanto indicato in Tabella 1.

### Risorse sportello

Ogni giorno lo sportello è associato a un tipo di servizio dal direttore: ogni giorno ci possono essere più sportelli che offrono lo stesso servizio, oppure ci possono essere dei servizi non offerti da alcuno sportello. Ogni sportello può essere occupato da un singolo operatore; la politica di associazione operatore-sportello è definita dal progettista e deve essere applicata all'inizio di ogni giornata.

### Processo operatore

All'avvio, ogni processo operatore viene creato in modo che sia in grado di erogare uno dei servizi citati in Sezione 5. Tale mansione resta invariata per tutta la simulazione. All'inizio di ogni giornata lavorativa, l'operatore:

• Compete con gli altri operatori per la ricerca di uno sportello libero tra quelli disponibili nell'ufficio postale che si occupano del servizio che lui è in grado di svolgere
• Se ne trova uno, lo occupa e comincia il proprio lavoro che terminerà alla fine della giornata lavorativa
• Con un massimo di NOF_PAUSE volte per tutta la simulazione, l'operatore può decidere (secondo un criterio scelto dal programmatore) di interrompere il servizio della giornata anticipatamente. In questo caso:
• termina di servire il cliente che stava servendo;
• lascia libero lo sportello occupato;
• aggiorna le statistiche.
Il processo operatore che al suo arrivo non trova uno sportello libero:
• resta in attesa che uno sportello si liberi (per una pausa di un altro operatore);
• torna a casa a fine giornata, e si ripresenta regolarmente il giorno dopo.

### Processo utente

Ogni processo utente si reca presso l'ufficio postale saltuariamente per richiedere un servizio tra quelli disponibili. Più in dettaglio, ogni giorno ogni processo utente:

• decide se recarsi o meno all'ufficio postale, secondo una probabilità P_SERV differente per ogni utente e scelta singolarmente in fase di creazione dell'utente in un intervallo compreso tra i valori [P_SERV_MIN, P_SERV_MAX].
• In caso affermativo
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
timeout: raggiungimento della durata impostata SIM_DURATION giorni
explode: numero di utenti in attesa al termine della giornata maggiore del valore EXPLODE_THRESHOLD
Il gruppo di studenti deve produrre configurazioni (file config_timeout.conf e config_explode.conf) in grado di generare la terminazione nei casi sopra descritti. Al termine della simulazione, l'output del programma deve riportare anche la causa di terminazione e le statistiche finali.

## Descrizione del progetto: versione "completa" (max 30)

In questa versione:

• un processo utente, quando decide di recarsi all'ufficio postale, genera una lista di al massimo N_REQUESTS richieste di servizi di vario tipo (il numero deve essere scelto in modo casuale per ogni utente, per ogni giorno). Quindi si reca all'ufficio postale dove richiederà in sequenza un ticket per ogni servizio nella lista (il ticket per il servizio i potrà essere richiesto solo quando il servizio i − 1 è stato completato).
• attraverso un nuovo eseguibile invocabile da linea di comando, deve essere possibile aggiungere alla simulazione altri N_NEW_USERS processi utente oltre a quelli inizialmente generati dal direttore;
• tutte le statistiche prodotte devono anche essere salvate in un file testo di tipo csv, in modo da poter essere utilizzate per una analisi futura

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