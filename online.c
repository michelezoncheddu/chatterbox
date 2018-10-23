#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <util.h>
#include <connections.h>
#include <users.h>
#include <groups.h>
#include <stats.h>
#include <online.h>

/**
 * @file   online.c
 * @brief  Contiene le funzioni che implementano la gestione
 *           degli utenti online in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

extern statistics       chattyStats;                              // statistiche del server
extern unsigned int     MaxOnlineUsers;                           // parametro di configurazione
static online_t        *online;                                   // array per utenti online
static pthread_mutex_t  online_mutex = PTHREAD_MUTEX_INITIALIZER; // per l'array utenti online

/**
 * @function initOnline
 * @brief    inizializza la struttura per gli utenti online
 */
void initOnline() {
	int r;

	// inizializzo la struttura online
	MALLOC(online, malloc(MaxOnlineUsers * sizeof(online_t)), "online initHash");
	for (int i = 0; i < MaxOnlineUsers; ++i)
		online[i].fd = -1;
	
	// inizializzo le mutex della struttura online
	for (int i = 0; i < MaxOnlineUsers; ++i) {
		r = pthread_mutex_init(&online[i].mutex, NULL);
		if (r != 0)
			exit(EXIT_FAILURE);
	}
}

/**
 * @function addOnline
 * @brief    aggiunge un utente e il proprio fd alla lista online
 * 
 * @param nick il nome dell'utente
 * @param fd   il fd dell'utente
 * 
 * @return -1 se non e' possibile aggiungere altri utenti online
 *         la posizione nell'array online, altrimenti
 */
int addOnline(char *nick, int fd) {
	int i = 0;

	// cerco uno spazio libero
	pthread_mutex_lock(&online_mutex);
	while (online[i].fd != -1)
		i++;

	if (i == MaxOnlineUsers) { // array pieno
		pthread_mutex_unlock(&online_mutex);
		return -1;
	}

	// aggiungo fd e nick dell'utente alla struttura online
	online[i].fd = fd;
	strncpy(online[i].nick, nick, MAX_NAME_LENGTH + 1);
	chattyStats.nonline++;
	pthread_mutex_unlock(&online_mutex);
	return i;
}

/**
 * @function getOnline
 * @brief    cerca se un utente e' nella lista online
 * 
 * @param nick il nome dell'utente
 * 
 * @return -1 se l'utente non e' all'interno della lista
 *         la posizione altrimenti
 */
int getOnline(char *nick) {
	int i = 0;

	// cerco un fd valido dell'utente
	pthread_mutex_lock(&online_mutex);
	while (i < MaxOnlineUsers && (online[i].fd == -1 || strncmp(online[i].nick, nick, MAX_NAME_LENGTH + 1) != 0))
		i++;
	
	if (i == MaxOnlineUsers) { // utente non online
		pthread_mutex_unlock(&online_mutex);
		return -1;
	}
	pthread_mutex_unlock(&online_mutex);
	return i; // posizione dell'utente
}

/**
 * @function getOnlineUnlocked
 * @brief    cerca se un utente e' nella lista online,
 *             senza acquisire le lock sulla struttura
 * 
 * @param nick il nome dell'utente
 * 
 * @return -1 se l'utente non e' all'interno della lista
 *         la posizione altrimenti
 */
int getOnlineUnlocked(char *nick) {
	int i = 0;

	// cerco un fd valido dell'utente
	while (i < MaxOnlineUsers && (online[i].fd == -1 || strncmp(online[i].nick, nick, MAX_NAME_LENGTH + 1) != 0))
		i++;
	
	if (i == MaxOnlineUsers) // utente non online
		return -1;
	return i; // posizione dell'utente
}

/**
 * @function removeOnline
 * @brief    rimuove un utente e il proprio fd dalla lista online
 * 
 * @param fd il fd dell'utente da rimuovere
 */
void removeOnline(int fd) {
	int i = 0;

	// cerco il fd dell'utente
	pthread_mutex_lock(&online_mutex);
	while (i < MaxOnlineUsers && online[i].fd != fd)
		i++;

	if (i == MaxOnlineUsers) { // fd non trovato, non faccio nulla
		pthread_mutex_unlock(&online_mutex);
		return;
	}

	// mutua esclusione di precisione per via delle operazioni Atomic
	pthread_mutex_lock(&online[i].mutex);
	online[i].fd = -1; // invalido il fd
	pthread_mutex_unlock(&online[i].mutex);
	chattyStats.nonline--;
	pthread_mutex_unlock(&online_mutex);
}

/**
 * @function deleteOnline
 * @brief cancella ogni occorrenza del nickname dell'utente dalla struttura online
 * 
 * @param nick il nome dell'utente
 */
void deleteOnline(char *nick) {
	// cancello ogni traccia del nick dagli utenti online
	pthread_mutex_lock(&online_mutex);
	for (int i = 0; i < MaxOnlineUsers; ++i)
		if (strncmp(online[i].nick, nick, MAX_NAME_LENGTH + 1) == 0) {
			for (int j = 0; j < MAX_NAME_LENGTH; ++j)
				online[i].nick[j] = '\0';
			online[i].fd = -1;
		}
	pthread_mutex_lock(&online_mutex);
}

/**
 * @function getOnlineList
 * @brief    crea la lista di utenti online
 * 
 * @param list la lista che dovra' essere scritta
 * 
 * @return il numero di utenti online
 */
int getOnlineList(char ***list) {
	int k = 0;
	char **local_list;

	pthread_mutex_lock(&online_mutex);
	MALLOC(local_list, calloc(chattyStats.nonline, sizeof(char*)), "local_list getOnlineList");
	for (int i = 0; i < chattyStats.nonline; ++i)
		MALLOC(local_list[i], calloc(1, MAX_NAME_LENGTH + 1), "local_list getOnlineList");

	// copio la lista di utenti online
	for (int i = 0; i < MaxOnlineUsers; ++i)
		if (online[i].fd != -1)
			strncpy(local_list[k++], online[i].nick, MAX_NAME_LENGTH + 1);
	
	*list = local_list;
	pthread_mutex_unlock(&online_mutex);
	return k;
}

/**
 * @function sendOnlineList
 * @brief    invia la lista di utenti online ad un certo fd
 * 
 * @param nick il nome del destinatario
 */
void sendOnlineList(char *nick) {
	message_t  reply;
	char      *list;
	int        k = 0;

	pthread_mutex_lock(&online_mutex);
	MALLOC(list, calloc(chattyStats.nonline, MAX_NAME_LENGTH + 1), "list sendUserList");
	// copio gli utenti online
	for (int i = 0; i < MaxOnlineUsers; ++i)
		if (online[i].fd != -1) {
			strncpy((list + k * (MAX_NAME_LENGTH + 1)), online[i].nick, MAX_NAME_LENGTH + 1);
			k++;
		}
	pthread_mutex_unlock(&online_mutex);
	setHeader(&(reply.hdr), OP_OK, "server");
	setData(&(reply.data), nick, list, chattyStats.nonline * (MAX_NAME_LENGTH + 1));
	sendMessageAtomic(reply);
	free(list);
}

/**
 * @function sendOpAtomic
 * @brief    invia un header con l'operazione
 *             specificata, atomicamente
 * 
 * @param nick il nome del destinatario
 * @param op   l'operazione
 * 
 * @return > 0 se il destinatario e' online
 *          -1 altrimenti
 */
int sendOpAtomic(char *nick, op_t op) {
	int pos, n;
	pthread_mutex_lock(&online_mutex);
	if ((pos = getOnlineUnlocked(nick)) != -1) { // destinatario online
		pthread_mutex_lock(&online[pos].mutex);
		pthread_mutex_unlock(&online_mutex);
		n = sendOp(online[pos].fd, op); // qua ho solo il lock sullo specifico client
		pthread_mutex_unlock(&online[pos].mutex);
		return n;
	}
	pthread_mutex_unlock(&online_mutex);
	return -1;
}

/**
 * @function sendMessageAtomic
 * @brief    invia un messaggio in modo atomico
 * 
 * @param msg il messaggio da inviare
 * 
 * @return > 0 se il destinatario e' online
 *          -1 altrimenti
 */
int sendMessageAtomic(message_t msg) {
	int pos, n;
	pthread_mutex_lock(&online_mutex);
	if ((pos = getOnlineUnlocked(msg.data.hdr.receiver)) != -1) { // destinatario online
		pthread_mutex_lock(&online[pos].mutex);
		pthread_mutex_unlock(&online_mutex);
		n = sendMsg(online[pos].fd, &msg);
		pthread_mutex_unlock(&online[pos].mutex);
		return n;
	}
	pthread_mutex_unlock(&online_mutex);
	return -1;
}

/**
 * @function freeOnline
 * @brief    elimina le strutture per gli utenti online
 */
void freeOnline() {
	// elimino la struttura online
	for (int i = 0; i < MaxOnlineUsers; ++i)
		pthread_mutex_destroy(&online[i].mutex);
	free(online);
}
