# Poste-Italiane-Emulation-C
### Descrizione del progetto: versione minima (voto max 24 su 30)
Si intende simulare il funzionamento di un ufficio postale. A tal fine sono presenti i seguenti processi e risorse.
• Processo direttore Ufficio Posta, che gestisce la simulazione, e mantiene le statistiche su richieste e servizi
erogati dall’ufficio. Genera gli sportelli, e gli operatori.
• Processo erogatore ticket: eroga ticket per specifici servizi. In particolare, dovranno essere implementate
le funzionalità per garantire almeno i servizi elencati in Tab. 1. Il tempo indicato è da considerarsi il valore
medio per erogare il servizio, e dovrà essere usato per generare un tempo casuale di erogazione nell’intorno
±50% del valore indicato.
Tabella 1: Elenco dei servizi forniti dall’ufficio postale.
servizio tempario (in minuti)
Invio e ritiro pacchi 10
Invio e ritiro lettere e raccomandate 8
Prelievi e versamenti Bancoposta 6
Pagamento bollettini postali 8
Acquisto prodotti finanziari 20
Acquisto orologi e braccialetti 20
• L’utente richiede un ticket specifico per uno dei servizi elencati in Tab. 1, attende il proprio turno, riceve la
prestazione richiesta e torna a casa.
• Esistono risorse di tipo sportello; ogni sportello è specializzato nel fornire un solo tipo di prestazione, che
varia ogni giorno della simulazione (vedi sopra elenco dei possibili servizi). Gli sportelli aprono e chiudono
secondo la disponibilit`a degli operatori.
• NOF WORKERS processi di tipo operatore: hanno un orario di lavoro, effettuano pause casuali.
• NOF USERS processi di tipo utente. Il processo utente decide se recarsi all’ufficio postale e sceglie il servizio
da richiedere.
#### Processo Direttore
Il processo direttore è responsabile dell’avvio della simulazione, della creazione delle risorse di tipo sportello,
dei processi operatore e utente, delle statistiche e della terminazione. Si noti bene che il processo direttore non
si occupa dell’aggiornamento delle statistiche, ma solo della loro lettura, secondo quanto indicato. All’avvio, il
processo direttore:
• crea un solo processo erogatore ticket.
• crea NOF WORKER SEATS risorse di tipo sportello.
• crea NOF WORKERS processi di tipo operatore.
• crea NOF USERS processi di tipo utente.
Successivamente il direttore avvia la simulazione, che avrà come durata SIM DURATION giorni, dove ciascun minuto
è simulato dal trascorrere di N NANO SECS nanosecondi.
La simulazione deve cominciare solamente quando tutti i processi erogatore, operatore e utente sono stati creati
e hanno terminato la fase di inizializzazione.
Alla fine di ogni giornata, il processo direttore dovr`a stampare le statistiche totali e quelle della giornata, che
comprendono:
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

#### Processo erogatore ticket
Su richiesta di un processo utente, il processo erogatore ticket si occupa di erogare il ticket relativo alla
prestazione richiesta, secondo quanto indicato in Tabella 1.
#### Risorse sportello
Ogni giorno lo sportello è associato a un tipo di servizio dal direttore: ogni giorno ci possono essere più sportelli
che offrono lo stesso servizio, oppure ci possono essere dei servizi non offerti da alcuno sportello.
Ogni sportello può essere occupato da un singolo operatore; la politica di associazione operatore-sportello è
definita dal progettista e deve essere applicata all’inizio di ogni giornata.
#### Processo operatore
All’avvio, ogni processo operatore viene creato in modo che sia in grado di erogare uno dei servizi citati in Sezione 5.
Tale mansione resta invariata per tutta la simulazione. All’inizio di ogni giornata lavorativa, l’operatore:
• Compete con gli altri operatori per la ricerca di uno sportello libero tra quelli disponibili nell’ufficio postale
che si occupano del servizio che lui è in grado di svolgere
• Se ne trova uno, lo occupa e comincia il proprio lavoro che terminerà alla fine della giornata lavorativa
• Con un massimo di NOF PAUSE volte per tutta la simulazione, l’operatore pu`o decidere (secondo un criterio
scelto dal programmatore) di interrompere il servizio della giornata anticipatamente. In questo caso:
– termina di servire il cliente che stava servendo;
– lascia libero lo sportello occupato;
– aggiorna le statistiche.
Il processo operatore che al suo arrivo non trova uno sportello libero:
• resta in attesa che uno sportello si liberi (per una pausa di un altro operatore);
• torna a casa a fine giornata, e si ripresenta regolarmente il giorno dopo.
#### Processo utente
Ogni processo utente si reca presso l’ufficio postale saltuariamente per richiedere un servizio tra quelli disponibili.
Più in dettaglio, ogni giorno ogni processo utente:
• decide se recarsi o meno all’ufficio postale, secondo una probabilità P SERV differente per ogni utente e scelta
singolarmente in fase di creazione dell’utente in un intervallo compreso tra i valori [P SERV MIN, P SERV MAX].
• In caso affermativo
– Stabilisce il servizio di cui vuole usufruire (secondo un criterio stabilito dall’utente);
– Stabilisce un orario (secondo un criterio stabilito dall’utente);
– Si reca all’ufficio postale;
– Controlla se quel giorno l’ufficio postale può servire richieste per il tipo di servizio scelto;
– Se sì, ottiene un ticket per l’apposito servizio;
– Attende il proprio turno e l’erogazione del servizio;
– Torna a casa e attende il giorno successivo.
Se al termine della giornata l’utente si trova ancora in coda, abbandona l’ufficio rinunciando all’erogazione del
servizio. Il numero di servizi non erogati `e uno dei parametri da monitorare.

#### Terminazione
La simulazione termina in una delle seguenti circorstanze:
timeout raggiungimento della durata impostata SIM DURATION giorni
explode numero di utenti in attesa al termine della giornata maggiore del valore EXPLODE THRESHOLD
Il gruppo di studenti deve produrre configurazioni (file config timout.conf e config explode.conf) in grado di
generare la terminazione nei casi sopra descritti.
Al termine della simulazione, l’output del programma deve riportare anche la causa di terminazione e le
statistiche finali.

---

### Descrizione del progetto: versione “completa” (max 30)
In questa versione:
• un processo utente, quando decide di recarsi all’ufficio postale, genera una lista di al massimo N REQUESTS
richieste di servizi di vario tipo (il numero deve essere scelto in modo casuale per ogni utente, per ogni giorno).
Quindi si reca all’ufficio postale dove richiederà in sequenza un ticket per ogni servizio nella lista (il ticket per
il servizio i potrà essere richiesto solo quando il servizio i − 1 `e stato completato).
• attraverso un nuovo eseguibile invocabile da linea di comando, deve essere possibile aggiungere alla simulazione
altri N NEW USERS processi utente oltre a quelli inizialmente generati dal direttore;
• tutte le statistiche prodotte devono anche essere salvate in un file testo di tipo csv, in modo da poter essere
utilizzate per una analisi futura
### Configurazione
Tutti i parametri di configurazione sono letti a tempo di esecuzione, da file o da variabili di ambiente. Quindi,
un cambiamento dei parametri non deve determinare una nuova compilazione dei sorgenti (non è consentito inserire
i parametri uno alla volta da terminale una volta avviata la simulazione).
8 Requisiti implementativi
Il progetto (sia in versione “minimal” che “normal”) deve
• evitare l’attesa attiva
• utilizzare almeno memoria condivisa, semafori e un meccanismo di comunicazione fra processi a scelta fra
code di messaggi o pipe,
• essere realizzato sfruttando le tecniche di divisione in moduli del codice (per esempio, i vari processi devono
essere lanciati da eseguibili diversi con execve(...)),
• essere compilato mediante l’utilizzo dell’utility make
• massimizzare il grado di concorrenza fra processi
• deallocare le risorse IPC che sono state allocate dai processi al termine del gioco
• essere compilato con almeno le seguenti opzioni di compilazione:
gcc -Wvla -Wextra -Werror
• poter eseguire correttamente su una macchina (virtuale o fisica) che presenta parallelismo (due o pi`u processori).
Per i motivi introdotti a lezione, ricordarsi di definire la macro GNU SOURCE o compilare il progetto con il flag
-D GNU SOURCE.
