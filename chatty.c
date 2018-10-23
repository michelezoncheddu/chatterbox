/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */

/**
 * @file   chatty.c
 * @brief  File principale del server chatterbox
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>

#include <stats.h>
#include <util.h>
#include <queue.h>
#include <users.h>
#include <online.h>
#include <operations.h>
#include <groups.h>
#include <message.h>
#include <connections.h>

/**
 * @struct thArgs_t
 * @brief  parametri per i thread del pool e per il listener
 * 
 * @var q     puntatore alla coda
 * @var pipe  array di due posizioni per la pipe di ritorno
 * @var table tabella per gli utenti
 */
typedef struct {
	queue_t *q;
	int     *pipe;
	hash_t   table;
} thArgs_t;

// variabili globali
static volatile sig_atomic_t stop  = 0; // flag di interruzione
static volatile sig_atomic_t stats = 0; // flag di stampa statistiche
config                       conf;      // definita in config.h
static int                   notused;

// parametri di configurazione
unsigned int MaxUsers;
unsigned int MaxOnlineUsers;
unsigned int MaxGroups;
char *UnixPath;
char *DirName;
char *StatFileName;

/**
 * @function usage
 * @brief    funzione di assistenza
 * 
 * @param progname il nome del programma
 */
static void usage(const char *progname) {
	fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
	fprintf(stderr, "  %s -f conffile\n", progname);
}

/**
 * @function updateMax
 * @brief    trova il massimo file descriptor nel set
 * 
 * @param set   l'insieme di fd
 * @param fdmax il massimo fd corrente
 * 
 * @return il nuovo massimo nel set
 */
static int updateMax(fd_set set, int fdmax) {
	for (int i = (fdmax-1); i >= 0; --i)
		if (FD_ISSET(i, &set))
			return i;
	return -1;
}

/**
 * @function signalHandler
 * @brief    thread per la gestione di segnali
 * 
 * @param arg puntatore al parametro (insieme di segnali da "ascoltare")
 * 
 * @return valore di terminazione della funzione
 */
static void *sigHandler(void *arg) {
	sigset_t set = *(sigset_t*)arg;
	int r, sig;
	while (1) {
		r = sigwait(&set, &sig);
		if (r != 0) {
			errno = r;
			perror("sigwait");
			return NULL;
		}
		if (sig == SIGINT || sig == SIGTERM || sig == SIGQUIT)
			stop = 1;
		else if (sig == SIGUSR1)
			stats = 1;
	}
	return NULL; // mai raggiunto
}

/**
 * @function listener
 * @brief    thread che attende connessioni da parte di utenti
 * 
 * @param args puntatore al parametro (struttura thArgs_t)
 * 
 * @return valore di terminazione della funzione
 */
static void *listener(void *args) {
	queue_t *q        = ((thArgs_t*)args) -> q;
	int      readpipe = ((thArgs_t*)args) -> pipe[0];
	int      fd_sock;
	struct sockaddr_un addr;
	FILE *stats_file;

	// preparazione socket
	SYSCALL(fd_sock, socket(AF_UNIX, SOCK_STREAM, 0), "socket");
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, UnixPath);
	SYSCALL(notused, bind(fd_sock, (struct sockaddr *)&addr, sizeof(addr)), "bind");
	SYSCALL(notused, listen(fd_sock, conf.MaxConnections), "listen");
	
	// creazione insieme di file descriptor
	fd_set set, rdset;
	int fd_client, fd_max;
	FD_ZERO(&set);
	FD_SET(readpipe, &set);
	FD_SET(fd_sock, &set);
	fd_max = fd_sock; // i file descriptor sono assegnati in ordine crescente

	// ciclo di ascolto
	struct timeval t, t_tmp; // timer per la select
	t.tv_sec  = 0;
    t.tv_usec = 50000; // 50 ms
	while (1) {
		rdset = set;
		t_tmp = t;
		SYSCALL(notused, select(fd_max + 1, &rdset, NULL, NULL, &t_tmp), "select");
		if (stop) // devo terminare
			break;
		if (stats) { // stampa statistiche
			stats_file = fopen(StatFileName, "w");
			if (!stats_file)
				exit(EXIT_FAILURE);
			printStats(stats_file);
			fclose(stats_file);
			stats = 0;
		}
		for (int fd = 0; fd <= fd_max; ++fd) {
			if (FD_ISSET(fd, &rdset)) {
				if (fd == readpipe) { // un worker ha terminato
					int *buf;
					MALLOC(buf, malloc(sizeof(int)), "buf listener");
					SYSCALL(notused, readn(readpipe, buf, sizeof(int)), "readn");
					FD_SET(*buf, &set);
					if (*buf > fd_max)
						fd_max = *buf;
					free(buf);
				}
				else if (fd == fd_sock) { // tentativo di connessione al server
					SYSCALL(fd_client, accept(fd_sock, NULL, 0), "accept");
					FD_SET(fd_client, &set);
					if (fd_client > fd_max)
						fd_max = fd_client;
				}
				else { // un client ha scritto
					int *tmp;
					MALLOC(tmp, malloc(sizeof(int)), "tmp listener");
					*tmp = fd;
					FUNCALL(notused, enqueue(q, tmp), "enqueue");
					FD_CLR(fd, &set); // non lo "ascolto" fino a quando un worker non soddisfa la sua richiesta
					if (fd == fd_max)
						fd_max = updateMax(set, fd_max);
				}
			}
		}
	}

	// protocollo di terminazione
	for (int i = 0; i < conf.ThreadsInPool; ++i)
		FUNCALL(notused, enqueue(q, END), "enqueue");
	close(fd_sock);
	pthread_exit(NULL);	
}

/**
 * @function worker
 * @brief    thread del pool, soddisfa le richieste dei client
 * 
 * @param args puntatore al parametro (struttura thArgs_t)
 * 
 * @return valore di terminazione della funzione
 */
static void *worker(void *args) {
	queue_t        *q         = ((thArgs_t*)args) -> q;
	int             writepipe = ((thArgs_t*)args) -> pipe[1];
	hash_t          users     = ((thArgs_t*)args) -> table;
	int            *fd_client, n;
	message_t      *req;   // richiesta dal client
	
	while (1) {
		fd_client = (int*)dequeue(q); // estraggo il fd del client da servire
		if (fd_client == END) // devo terminare
			break;

		MALLOC(req, malloc(sizeof(message_t)), "req worker");
		n = readMsg(*fd_client, req); // leggo la richiesta del client

		if (n > 0) {
			switch (req->hdr.op) { // scelgo l'operazione (operations.c)
			case REGISTER_OP:
				registerOp(users, *fd_client, *req);
				break;
			
			case CONNECT_OP:
				connectOp(users, *fd_client, *req);
				break;
			
			case POSTTXT_OP:
				postTxtOp(users, *fd_client, *req);
				break;
			
			case POSTTXTALL_OP:
				postTxtAllOp(users, *fd_client, *req);
				break;
			
			case POSTFILE_OP:
				postFileOp(users, *fd_client, *req);
				break;
			
			case GETFILE_OP:
				getFileOp(users, *fd_client, *req);
				break;

			case GETPREVMSGS_OP:
				sendHistory(users, req->hdr.sender, *fd_client);
				break;
			
			case USRLIST_OP:
				sendOnlineList(req->hdr.sender);
				break;
			
			case UNREGISTER_OP:
				unregisterOp(users, *fd_client, *req);
				break;
			
			case DISCONNECT_OP:
				removeOnline(*fd_client);
				SYSCALL(notused, close(*fd_client), "close");
				break;

			case CREATEGROUP_OP:
				createGroupOp(users, *fd_client, *req);
				break;
			
			case ADDGROUP_OP:
				addGroupOp(users, *fd_client, *req);
				break;
			
			case DELGROUP_OP:
				delGroupOp(users, *fd_client, *req);
				break;
			
			default:
				printf("SERVER - ERRORE: operazione non riconosciuta\n");
			}
			
			// comunico al listener che il client e' stato servito
			SYSCALL(notused, writen(writepipe, fd_client, sizeof(int)), "writen");
		}
		else { // client disconnesso (n <= 0)
			removeOnline(*fd_client);
			printf("SERVER: fd %d disconnesso\n", *fd_client);
			SYSCALL(notused, close(*fd_client), "close");
		}
		free(req);
		free(fd_client);
	}
	pthread_exit(NULL);
}

/**
 * @function loadConfig
 * @brief    parser di file di configurazione per chatty
 * 
 * @param conf_path percorso del file di configurazione da utilizzare
 */
void loadConfig(char *conf_path) {
	FILE *conf_file = fopen(conf_path, "r");
	char *buf;
	int maxSize = 128;
	if (!conf_file) {
		printf("SERVER - ERRORE: impossibile aprire il file %s\n", conf_path);
		exit(EXIT_FAILURE);
	}

	// assumo che il nome di ogni variabile e dei file non superi i 128 caratteri
	MALLOC(buf, malloc(maxSize + 1), "buf loadConfig");
	MALLOC(UnixPath, malloc(maxSize + 1), "UnixPath loadConfig");
	MALLOC(DirName, malloc(maxSize + 1), "DirName loadConfig");
	MALLOC(StatFileName, malloc(maxSize + 1), "StatFileName loadConfig");

	// ciclo di parsing
	while (fscanf(conf_file, "%s", buf) >= 0) {
		if (strncmp(buf, "ThreadsInPool",  maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &conf.ThreadsInPool) > 0){} else
		if (strncmp(buf, "MaxConnections", maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &conf.MaxConnections) > 0){} else
		if (strncmp(buf, "MaxHistMsgs",    maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &conf.MaxHistMsgs) > 0){} else
		if (strncmp(buf, "MaxMsgSize",     maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &conf.MaxMsgSize) > 0){} else
		if (strncmp(buf, "MaxFileSize",    maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &conf.MaxFileSize) > 0){} else
		if (strncmp(buf, "MaxUsers",       maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &MaxUsers) > 0){} else
		if (strncmp(buf, "MaxOnlineUsers", maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &MaxOnlineUsers) > 0){} else
		if (strncmp(buf, "MaxGroups",      maxSize + 1) == 0 && fscanf(conf_file, "%*s %u", &MaxGroups) > 0){} else
		if (strncmp(buf, "UnixPath",       maxSize + 1) == 0 && fscanf(conf_file, "%*s %s", UnixPath) > 0){} else
		if (strncmp(buf, "DirName",        maxSize + 1) == 0 && fscanf(conf_file, "%*s %s", DirName) > 0){} else
		if (strncmp(buf, "StatFileName",   maxSize + 1) == 0 && fscanf(conf_file, "%*s %s", StatFileName) > 0){}
	}
	free(buf);
	fclose(conf_file);
}

/**
 * @function main
 * @brief    metodo main di chatty
 * 
 * @param argc numero di argomenti
 * @param argv array di argomenti
 * 
 * @return valore di terminazione della funzione
 */
int main(int argc, char *argv[]) {
	// controllo argomenti
	if (argc < 3) {
		usage("chatty");
		return 0;
	}

	// importo le configurazioni dal file
	loadConfig(argv[2]);
	
	sigset_t sigset; // signal mask
	// anche se non sono chiamate di sistema, restituiscono -1 e settano errno, quindi uso SYSCALL
	SYSCALL(notused, sigemptyset(&sigset), "sigemptyset"); // resetto tutti i bit
	SYSCALL(notused, sigaddset(&sigset, SIGINT), "sigaddset"); // aggiungo i segnali alla maschera
	SYSCALL(notused, sigaddset(&sigset, SIGTERM), "sigaddset");
	SYSCALL(notused, sigaddset(&sigset, SIGQUIT), "sigaddset");
	SYSCALL(notused, sigaddset(&sigset, SIGPIPE), "sigaddset");
	SYSCALL(notused, sigaddset(&sigset, SIGUSR1), "sigaddset");
	LIBCALL(notused, pthread_sigmask(SIG_BLOCK, &sigset, NULL), "pthread_sigmask"); // maschero i segnali

	// creazione sigHandler
	pthread_t sigHandlertid;
	LIBCALL(notused, pthread_create(&sigHandlertid, NULL, sigHandler, &sigset), "pthread_create");

	// creazione pipe di ritorno
	int retpipe[2];
	SYSCALL(notused, pipe(retpipe), "pipe");

	// creazione coda
	queue_t *q;
	MALLOC(q, initQueue(), "initQueue");

	// creazione tabella hash
	hash_t users;
	MALLOC(users, initUsers(MaxUsers), "initUsers");

	// inizializzazione struttua online
	initOnline();

	// inizializzazione struttura gruppi
	initGroups();

	// ignoro l'errore, posso andare avanti anche se non era gia' presente nessun socket 
	unlink(UnixPath);

	// creazione thread listener
	pthread_t listid;
	thArgs_t  args;
	args.q     = q;
	args.pipe  = retpipe;
	args.table = users;
	LIBCALL(notused, pthread_create(&listid, NULL, listener, &args), "pthread_create");

	// creazione thread worker
	pthread_t *worktid;
	MALLOC(worktid, malloc(conf.ThreadsInPool * sizeof(pthread_t)), "worktid main");
	for (int i = 0; i < conf.ThreadsInPool; ++i)
		LIBCALL(notused, pthread_create(&worktid[i], NULL, worker, &args), "pthread_create");
	
	// attesa thread listener
	LIBCALL(notused, pthread_join(listid, NULL), "pthread_join");

	// terminazione sigHandler (sigwait e' un cancellation point)
	LIBCALL(notused, pthread_cancel(sigHandlertid), "pthread_cancel");
	LIBCALL(notused, pthread_join(sigHandlertid, NULL), "pthread_join");

	// attesa thread worker
	for (int i = 0; i < conf.ThreadsInPool; ++i)
		LIBCALL(notused, pthread_join(worktid[i], NULL), "pthread_join");
	
	// cleanup
	free(UnixPath);
	free(DirName);
	free(StatFileName);
	free(worktid);
	freeQueue(q);
	freeUsers(users);
	freeOnline();
	freeGroups();
	close(retpipe[0]);
	close(retpipe[1]);
	printf("\nSERVER TERMINATO\n");
	return 0;
}
