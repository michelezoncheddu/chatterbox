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
 * @file   users.c
 * @brief  Contiene le funzioni che implementano la gestione
 *           degli utenti registrati in modo concorrente
 *           in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

extern config    conf;              // parametri di configurazione
static pthread_mutex_t *hash_mutex; // array di lock per la tabella hash
static long size;                   // dimensione della tabella hash
static int  mutsize;                // numero di celle riservate ad ogni hash_mutex

statistics chattyStats  = { 0,0,0,0,0,0,0 }; // definita in stats.h

/**
 * @function initUsers
 * @brief    funzione di inizializzazione della struttura per
 *             la gestione degli utenti registrati
 * 
 * @param n il numero massimo (consigliato) di utenti registrati
 * 
 * @return la tabella hash degli utenti registrati
 */
hash_t initUsers(int n) {
	int r; // risultato inizializzazione mutex

	/**
	 * Per distribuire in modo uniforme le lock nella tabella hash,
	 * calcolo la dimensione come mcm tra il parametro n e il numero di threads worker,
	 * moltiplicando tutto per 2, per avere un fattore di carico massimo di 0.5,
	 * con la quale la tabella hash funziona in modo ottimale.
	 */
	size = 2 * ((n * conf.ThreadsInPool) / mcd(n, conf.ThreadsInPool)); // size = 2 * mcm(n, ThreadsInPool)
	mutsize = size / conf.ThreadsInPool;
	hash_t table = calloc(size, sizeof(user_t*));
	if (!table)
		return NULL;

	// inizializzo le mutex della tabella hash
	MALLOC(hash_mutex, malloc(conf.ThreadsInPool * sizeof(pthread_mutex_t)), "hash_mutex initHash");
	for (int i = 0; i < conf.ThreadsInPool; ++i) {
		r = pthread_mutex_init(&hash_mutex[i], NULL);
		if (r != 0) {
			free(table);
			return NULL;
		}
	}
	return table;
}

/**
 * @function signUp
 * @brief    inserisce un utente o un gruppo all'interno della tabella hash
 * 
 * @param table   la tabella degli utenti
 * @param key     il nome dell'utente
 * @param isGroup indica se devo regustrare un gruppo o un utente
 * 
 * @return -1 se l'utente o il gruppo e' gia' registrato
 *          0 altrimenti
 */
int signUp(hash_t table, char *key, int isGroup) {
	history_t *h;
	user_t    *new;

	// se e' gia' registrato
	if (isRegistered(table, key))
		return -1;

	int pos = hash(key);
	MALLOC(new, malloc(sizeof(user_t)), "new signUp");

	if (!isGroup) { // e' un utente
		MALLOC(h, malloc(sizeof(history_t)), "h signUp");
		MALLOC(h->msgs, malloc(conf.MaxHistMsgs * sizeof(message_t)), "h->msgs signUp");
		MALLOC(h->sent, malloc(conf.MaxHistMsgs * sizeof(int)), "h->sent signUp");
	}
	else // e' un gruppo
		h = NULL;

	// inizializzo i dati
	strncpy(new->nick, key, MAX_NAME_LENGTH + 1);
	new->history = h;
	if (!isGroup) {
		h->start = -1;
		h->end   =  0;
		h->size  =  0;
	}
	
	// inserisco la struttura in testa alla lista di trabocco
	pthread_mutex_lock(&hash_mutex[pos / mutsize]);
	if (table[pos]) {
		new->next  = table[pos];
		table[pos] = new;
	}
	else {
		new->next  = NULL;
		table[pos] = new;
	}
	pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
	chattyStats.nusers++;
	return 0;
}

/**
 * @function unregisterUser
 * @brief    deregistra un utente o un gruppo,
 *             cancellandolo anche dalla struttura online
 * 
 * @param table la tabella degli utenti
 * @param key   il nome dell'utente o del gruppo
 */
void unregisterUser(hash_t table, char *key) {
	user_t *elem, *tmp;
	int pos = hash(key);

	pthread_mutex_lock(&hash_mutex[pos / mutsize]);
	elem = table[pos];
	// se devo cancellare la testa della lista di trabocco
	if (strncmp(elem->nick, key, MAX_NAME_LENGTH + 1) == 0) {
		freeHistory(elem->history);
		tmp = elem;
		table[pos] = elem->next;
		free(tmp);
		pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
		chattyStats.nusers--;
		return;
	}

	// cerco la struttura da eliminare
	tmp = elem->next;
	while (tmp && strncmp(tmp->nick, key, MAX_NAME_LENGTH + 1) != 0)
		tmp = tmp->next;
	if (!tmp) { // utente non trovato
		pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
		return;
	}

	// cancello la history e la struttura
	elem = tmp;
	freeHistory(elem->history);
	elem = elem->next;
	free(tmp);
	pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
	chattyStats.nusers--;
	deleteOnline(key);
}

/**
 * @function isRegistered
 * @brief    controlla se un utente o un gruppo e' presente
 *             tra gli utenti registrati
 * 
 * @param table la tabella degli utenti
 * @param key   il nome dell'utente o del gruppo
 * 
 * @return 0 se il nick non e' registrato
 *         1 se e' un utente
 *         2 se e' un gruppo
 */
int isRegistered(hash_t table, char *key) {
	int pos = hash(key), res;

	// cerco il nick
	pthread_mutex_lock(&hash_mutex[pos / mutsize]);
	user_t *elem = table[pos];
	while (elem && strncmp(elem->nick, key, MAX_NAME_LENGTH + 1) != 0)
		elem = elem->next;
	if (elem == NULL) { // nick non trovato
		pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
		return 0;
	}
	if (elem->history)
		res = 1; // e' un utente
	else
		res = 2; // e' un gruppo
	pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
	return res;
}

/**
 * @function sendMessage
 * @brief    inserisce un messaggio nella history,
 *             e se il destinatario e' online lo invia in modo atomico
 * 
 * @param table la tabella degli utenti
 * @param msg   il messaggio da inviare
 */
void sendMessage(hash_t table, message_t msg) {
	int pos = hash(msg.data.hdr.receiver), res;

	// cerco il destinatario (non posso non trovarlo)
	pthread_mutex_lock(&hash_mutex[pos / mutsize]);
	while (table[pos] && strncmp(table[pos]->nick, msg.data.hdr.receiver, MAX_NAME_LENGTH + 1) != 0)
		table[pos] = table[pos]->next;

	// inserisco il messaggio nella history
	history_t *h = table[pos]->history; // prelevo la history del destinatario
	if (h->start == -1) { // history non piena
		h->msgs[h->end] = msg;

		// provo ad inviare il messaggio
		res = sendMessageAtomic(msg);
		if (res > 0) { // messaggio inviato
			h->sent[h->end] = 1;
			if (msg.hdr.op == TXT_MESSAGE)
				chattyStats.ndelivered++;
			else
				chattyStats.nfiledelivered++;
		}
		else { // messaggio non inviato
			h->sent[h->end] = 0;
			if (msg.hdr.op == TXT_MESSAGE)
				chattyStats.nnotdelivered++;
			else
				chattyStats.nfilenotdelivered++;
		}
		h->end = (h->end + 1) % conf.MaxHistMsgs;
		if (h->end == 0)
			h->start = 0;
		h->size++;
	}
	else { // start == end, history piena
		if (h->msgs[h->end].data.buf)
			free(h->msgs[h->end].data.buf);
		h->msgs[h->end] = msg;

		// provo ad inviare il messaggio
		res = sendMessageAtomic(msg);
		if (res > 0) { // messaggio inviato
			h->sent[h->end] = 1;
			if (msg.hdr.op == TXT_MESSAGE)
				chattyStats.ndelivered++;
			else
				chattyStats.nfiledelivered++;
		}
		else { // messaggio non inviato
			h->sent[h->end] = 0;
			if (msg.hdr.op == TXT_MESSAGE)
				chattyStats.nnotdelivered++;
			else
				chattyStats.nfilenotdelivered++;
		}
		h->end = (h->end + 1) % conf.MaxHistMsgs;
		h->start = h->end;
	}
	table[pos]->history = h; // salvo le modifiche
	pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
}

/**
 * @function sendMessageAll
 * @brief    inserisce un messaggio nella history di tutti gli utenti,
 *             e per ogni destinatario online, lo invia a tutti,
 *             tranne al mittente (in modo atomico)
 * 
 * @param table la tabella degli utenti
 * @param msg   il messaggio da inviare
 */
void sendMessageAll(hash_t table, message_t msg) {
	int res, nonline;
	char **list = NULL;

	// memorizzo gli utenti online
	nonline = getOnlineList(&list);

	// inserisco il messaggio nella history di tutti gli utenti
	for (int i = 0; i < size; ++i) {
		pthread_mutex_lock(&hash_mutex[i / mutsize]);
		if (table[i] == NULL) { // elemento vuoto
			pthread_mutex_unlock(&hash_mutex[i / mutsize]);
			continue;
		}
		// se incontro "me stesso" e la lista continua, vado avanti
		if (table[i]->next && strncmp(table[i]->nick, msg.hdr.sender, MAX_NAME_LENGTH + 1) == 0) {
			table[i] = table[i]->next;
			pthread_mutex_unlock(&hash_mutex[i / mutsize]);
			i--; // il for rimane sullo stesso indice i
			continue;
		}
		history_t *h    = table[i]->history; // prelevo la history dell'utente
		message_t  hmsg = h->msgs[h->end];   // variabile temporanea per ridurre la verbosita'
		if (h->start == -1) { // history non piena
			hmsg = msg;
			// copio i dati del messaggio
			strncpy(hmsg.data.hdr.receiver, table[i]->nick, MAX_NAME_LENGTH + 1);
			MALLOC(hmsg.data.buf, malloc(hmsg.data.hdr.len), "hmsg.data.buf sendMessageAll");
			strncpy(hmsg.data.buf, msg.data.buf, msg.data.hdr.len);
			h->msgs[h->end] = hmsg; // salvo le modifiche

			// se il destinatario è online, provo ad inviare il messaggio
			if (isIn(table[i]->nick, list, nonline)) {
				res = sendMessageAtomic(hmsg);
				if (res > 0) { // messaggio inviato
					h->sent[h->end] = 1;
					chattyStats.ndelivered++;
				}
				else { // messaggio non inviato
					h->sent[h->end] = 0;
					chattyStats.nnotdelivered++;
				}
			}
			else { // destinatario non online
				h->sent[h->end] = 0;
				chattyStats.nnotdelivered++;
			}
			h->end = (h->end + 1) % conf.MaxHistMsgs;
			if (h->end == 0)
				h->start = 0;
			h->size++;
		}
		else { // start == end, history piena
			if (hmsg.data.buf)
				free(hmsg.data.buf);
			hmsg = msg;
			strncpy(hmsg.data.hdr.receiver, table[i]->nick, MAX_NAME_LENGTH + 1);
			MALLOC(hmsg.data.buf, malloc(hmsg.data.hdr.len), "hmsg.data.buf sendMessageAll");
			strncpy(hmsg.data.buf, msg.data.buf, msg.data.hdr.len); // copio i dati del messaggio
			h->msgs[h->end] = hmsg; // salvo le modifiche

			// se il destinatario è online, provo ad inviare il messaggio
			if (isIn(table[i]->nick, list, nonline)) {
				res = sendMessageAtomic(hmsg);
				if (res > 0) { // messaggio inviato
					h->sent[h->end] = 1;
					chattyStats.ndelivered++;
				}
				else { // messaggio non inviato
					h->sent[h->end] = 0;
					chattyStats.nnotdelivered++;
				}
			}
			else { // destinatario non online
				h->sent[h->end] = 0;
				chattyStats.nnotdelivered++;
			}
			h->end = (h->end + 1) % conf.MaxHistMsgs;
			h->start = h->end;
		}
		table[i]->history = h; // salvo le modifiche
		// se la lista di trabocco continua...
		if (table[i]->next) {
			table[i] = table[i]->next;
			pthread_mutex_unlock(&hash_mutex[i / mutsize]);
			i--; // il for rimane sullo stesso indice i
		}
		else
			pthread_mutex_unlock(&hash_mutex[i / mutsize]);
	}
	// dealloco la lista di utenti online
	for (int i = 0; i < nonline; ++i)
		free(list[i]);
	free(list);
}

/**
 * @function sendMessageToGroup
 * @brief    invia un messaggio a tutti i membri di un gruppo
 *             in modo atomico verso ogni destinatario
 * 
 * @param table la tabella degli utenti
 * @param msg   il messaggio da inviare
 * 
 * @return -1 se il mittente non appartiene al gruppo
 *          0 altrimenti
 */
int sendMessageToGroup(hash_t table, message_t msg) {
	int res, list_len, pos, k = 0;
	char  **list = NULL;
	user_t *elem;

	// ottengo la lista dei membri del gruppo
	list_len = getMembers(msg.data.hdr.receiver, &list);

	// verifico che il mittente appartenga al gruppo
	while (k < list_len && strncmp(list[k], msg.hdr.sender, MAX_NAME_LENGTH + 1) != 0)
		k++;
	if (k == list_len) { // il mittente non appartiene al gruppo
		for (int i = 0; i < list_len; ++i)
			free(list[i]);
		free(list);
		return -1;
	}

	// inserisco il messaggio nella history degli utenti del gruppo
	for (int i = 0; i < list_len; ++i) {
		pos = hash(list[i]);
		pthread_mutex_lock(&hash_mutex[i / mutsize]);
		elem = table[pos];
		// cerco l'utente
		while (elem) {
			// se e' diverso dal mittente
			history_t *h    = elem->history;   // prelevo la history dell'utente
			message_t  hmsg = h->msgs[h->end]; // variabile temporanea per ridurre la verbosita'
			if (h->start == -1) { // history non piena
				hmsg = msg;
				// copio i dati del messaggio
				strncpy(hmsg.data.hdr.receiver, elem->nick, MAX_NAME_LENGTH + 1);
				MALLOC(hmsg.data.buf, malloc(hmsg.data.hdr.len), "hmsg.data.buf sendMessageAll");
				strncpy(hmsg.data.buf, msg.data.buf, msg.data.hdr.len);
				h->msgs[h->end] = hmsg; // salvo le modifiche

				// provo ad inviare il messaggio
				res = sendMessageAtomic(hmsg);
				if (res > 0) { // messaggio inviato
					h->sent[h->end] = 1;
					if (hmsg.hdr.op == TXT_MESSAGE)
						chattyStats.ndelivered++;
					else
						chattyStats.nfiledelivered++;
				}
				else { // messaggio non inviato
					h->sent[h->end] = 0;
					if (hmsg.hdr.op == TXT_MESSAGE)
						chattyStats.nnotdelivered++;
					else
						chattyStats.nfilenotdelivered++;
				}
				h->end = (h->end + 1) % conf.MaxHistMsgs;
				if (h->end == 0)
					h->start = 0;
				h->size++;
			}
			else { // start == end, history piena
				if (hmsg.data.buf)
					free(hmsg.data.buf);
				hmsg = msg;
				strncpy(hmsg.data.hdr.receiver, elem->nick, MAX_NAME_LENGTH + 1);
				MALLOC(hmsg.data.buf, malloc(hmsg.data.hdr.len), "hmsg.data.buf sendMessageAll");
				strncpy(hmsg.data.buf, msg.data.buf, msg.data.hdr.len); // copio i dati del messaggio
				h->msgs[h->end] = hmsg; // salvo le modifiche

				// provo ad inviare il messaggio
				res = sendMessageAtomic(hmsg);
				if (res > 0) { // messaggio inviato
					h->sent[h->end] = 1;
					if (hmsg.hdr.op == TXT_MESSAGE)
						chattyStats.ndelivered++;
					else
						chattyStats.nfiledelivered++;
				}
				else { // messaggio non inviato
					h->sent[h->end] = 0;
					if (hmsg.hdr.op == TXT_MESSAGE)
						chattyStats.nnotdelivered++;
					else
						chattyStats.nfilenotdelivered++;
				}
				h->end = (h->end + 1) % conf.MaxHistMsgs;
				h->start = h->end;
			}
			elem = elem->next; // in caso di collisioni
		}
		pthread_mutex_unlock(&hash_mutex[i / mutsize]);
	}
	// dealloco la lista di utenti
	for (int i = 0; i < list_len; ++i)
		free(list[i]);
	free(list);
	return 0;
}

/**
 * @function sendHistory
 * @brief    invia al richiedente l'intera history presente sul server
 * 
 * @param table la tabella degli utenti
 * @param nick  il nome del destintario
 * @param fd    il fd del destinatario
 */
void sendHistory(hash_t table, char *key, int fd) {
	int pos = hash(key);
	message_data_t data;
	memset(&data, 0, sizeof(message_data_t));
	data.buf = calloc(1, sizeof(size_t));
	strncpy(data.hdr.receiver, key, MAX_NAME_LENGTH + 1);

	pthread_mutex_lock(&hash_mutex[pos / mutsize]);
	// cerco l'utente (non posso non trovarlo)
	while (table[pos] && strncmp(table[pos]->nick, key, MAX_NAME_LENGTH + 1) != 0)
		table[pos] = table[pos]->next;

	history_t *h = table[pos]->history; // prelevo la history dell'utente
	sendOpAtomic(key, OP_OK);
	*(data.buf) = h->size;
	data.hdr.len = sizeof(size_t);
	sendData(fd, &data); // invio il numero di messaggi che inviero'
	free(data.buf);

	if (h->start == h->end) { // history piena
		for (int i = h->start; i < h->start + conf.MaxHistMsgs; ++i)
			if (sendMessageAtomic(h->msgs[i % conf.MaxHistMsgs]) > 0) { // provo ad inviare il messaggio
				if (h->sent[i % conf.MaxHistMsgs] == 0) { // se non era ancora stato inviato
					h->sent[i % conf.MaxHistMsgs] = 1;
					chattyStats.nnotdelivered--;
				}
				chattyStats.ndelivered++;
			}
	}
	else { // history non piena
		for (int i = 0; i < h->end; ++i)
			if (sendMessageAtomic(h->msgs[i]) > 0) { // provo ad inviare il messaggio
				if (h->sent[i] == 0) { // se non era ancora stato inviato
					h->sent[i] = 1;
					chattyStats.nnotdelivered--;
				}
				chattyStats.ndelivered++;
			}
	}
	pthread_mutex_unlock(&hash_mutex[pos / mutsize]);
}

/**
 * @function freeHistory
 * @brief    cancella l'intera history di un utente
 * 
 * @param h il puntatore alla history da cancellare
 */
void freeHistory(history_t *h) {
	if (!h) // se e' un gruppo
		return;
	if (h->start == h->end) { // history piena
		for (int i = h->start; i < h->start + conf.MaxHistMsgs; ++i)
			if (h->msgs[i % conf.MaxHistMsgs].data.buf)
				free(h->msgs[i % conf.MaxHistMsgs].data.buf);
	}
	else { // history non piena
		for (int i = 0; i < h->end; ++i)
			if (h->msgs[i].data.buf)
				free(h->msgs[i].data.buf);
	}
	free(h->msgs);
	free(h->sent);
	free(h);
}

/**
 * @function freeUsers
 * @brief    cancella la struttura dati per gli utenti registrati
 * 
 * @param table la tabella degli utenti
 */
void freeUsers(hash_t table) {
	user_t *user_elem, *user_tmp;

	// libero la hash di ogni utente
	for (int i = 0; i < size; ++i) {
		user_elem = table[i];
		while (user_elem) {
			user_tmp = user_elem;
			user_elem = user_elem->next;
			freeHistory(user_tmp->history);
			free(user_tmp);
		}
	}
	free(table);

	// elimino le mutex
	for (int i = 0; i < conf.ThreadsInPool; ++i)
		pthread_mutex_destroy(&hash_mutex[i]);
	free(hash_mutex);
}

/**
 * @function hash
 * @brief funzione hash djb2 di Daniel Bernstein
 * 
 * @param str la stringa argomento della funzione
 * 
 * @return il valore hash calcolato, modulo
 *           la dimensione della tabella hash
 */
unsigned int hash(char *str) {
	unsigned int hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash % size;
}
