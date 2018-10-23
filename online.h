#ifndef ONLINE_H_
#define ONLINE_H_

#include <pthread.h>

#include <util.h>

/**
 * @file   online.h
 * @brief  Contiene le funzioni che implementano la gestione
 *           degli utenti online in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @struct online_t
 * @brief  dati di un utente online
 * 
 * @var nick  nome dell'utente
 * @var fd    fd della connessione legata all'utente
 * @var mutex lock per l'invio atomico di messsaggi all'utente
 */
typedef struct {
	char nick[MAX_NAME_LENGTH + 1];
	int  fd;
	pthread_mutex_t mutex;
} online_t;

/**
 * @function initOnline
 * @brief    inizializza la struttura per gli utenti online
 */
void initOnline();

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
int addOnline(char *nick, int fd);

/**
 * @function getOnline
 * @brief    cerca se un utente e' nella lista online
 * 
 * @param nick il nome dell'utente
 * 
 * @return -1 se l'utente non e' all'interno della lista
 *         la posizione altrimenti
 */
int getOnline(char *nick);

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
int getOnlineUnlocked(char *nick);

/**
 * @function removeOnline
 * @brief    rimuove un utente e il proprio fd dalla lista online
 * 
 * @param fd il fd dell'utente da rimuovere
 */
void removeOnline(int fd);

/**
 * @function deleteOnline
 * @brief cancella ogni occorrenza del nickname dell'utente dalla struttura online
 * 
 * @param nick il nome dell'utente
 */
void deleteOnline(char *nick);

/**
 * @function getOnlineList
 * @brief    crea la lista di utenti online
 * 
 * @param list la lista che dovra' essere scritta
 * 
 * @return il numero di utenti online
 */
int getOnlineList(char ***list);

/**
 * @function sendOnlineList
 * @brief    invia la lista di utenti online ad un certo fd
 * 
 * @param nick il nome del destinatario
 */
void sendOnlineList(char *nick);

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
int sendOpAtomic(char *nick, op_t op);

/**
 * @function sendMessageAtomic
 * @brief    invia un messaggio in modo atomico
 * 
 * @param msg il messaggio da inviare
 * 
 * @return > 0 se il destinatario e' online
 *          -1 altrimenti
 */
int sendMessageAtomic(message_t msg);

/**
 * @function freeOnline
 * @brief    elimina le strutture per gli utenti online
 */
void freeOnline();

#endif // ONLINE_H_
