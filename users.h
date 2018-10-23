#ifndef USERS_H_
#define USERS_H_

#include <config.h>
#include <message.h>

/**
 * @file   users.h
 * @brief  Contiene le funzioni che implementano la gestione
 *           degli utenti registrati in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @struct history_t
 * @brief  history di un utente
 * 
 * @var msgs  array circolare di messaggi 
 * @var sent  array per indicare se ogni messaggio
 *              e' stato inviato oppure no (vedi statistiche)
 * @var start la posizione del primo messaggio
 * @var end   la posizione dell'ultimo messaggio
 * @var size  il numero di messaggi salvati
 */
typedef struct {
	message_t *msgs;
	int       *sent;
	int        start;
	int        end;
	int        size;
} history_t;

/**
 * @struct user
 * @brief  dati di un utente registrato
 * 
 * @var nick    nome dell'utente
 * @var history puntatore alla history dell'utente
 * @var next    puntatore al prossimo utente
 */
typedef struct user {
	char         nick[MAX_NAME_LENGTH + 1];
	history_t   *history;
	struct user *next;
} user_t;

// ridefinizione di tipo per comodita'
typedef user_t** hash_t;

/**
 * @function initUsers
 * @brief    funzione di inizializzazione delle strutture per
 *             la gestione degli utenti registrati
 * 
 * @param n il numero massimo (consigliato) di utenti registrati
 * 
 * @return la tabella hash degli utenti registrati
 */
hash_t initUsers(int n);

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
int signUp(hash_t table, char *key, int isGroup);

/**
 * @function unregisterUser
 * @brief    deregistra un utente o un gruppo,
 *             cancellandolo anche dalla struttura online
 * 
 * @param table la tabella degli utenti
 * @param key   il nome dell'utente o del gruppo
 */
void unregisterUser(hash_t table, char *key);

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
int isRegistered(hash_t table, char *key);

/**
 * @function sendMessage
 * @brief    inserisce un messaggio nella history,
 *             e se il destinatario e' online lo invia in modo atomico
 * 
 * @param table la tabella degli utenti
 * @param msg   il messaggio da inviare
 */
void sendMessage(hash_t table, message_t msg);

/**
 * @function sendMessageAll
 * @brief    inserisce un messaggio nella history di tutti gli utenti,
 *             e per ogni destinatario online, lo invia a tutti,
 *             tranne al mittente (in modo atomico)
 * 
 * @param table la tabella degli utenti
 * @param msg   il messaggio da inviare
 */
void sendMessageAll(hash_t table, message_t msg);

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
int sendMessageToGroup(hash_t table, message_t msg);

/**
 * @function sendHistory
 * @brief    invia al richiedente l'intera history presente sul server
 * 
 * @param table la tabella degli utenti
 * @param nick  il nome del destintario
 * @param fd    il fd del destinatario
 */
void sendHistory(hash_t table, char *key, int fd);

/**
 * @function freeHistory
 * @brief    cancella l'intera history di un utente
 * 
 * @param h il puntatore alla history da cancellare
 */
void freeHistory(history_t *h);

/**
 * @function freeUsers
 * @brief    cancella le strutture dati per gli utenti registrati
 * 
 * @param table la tabella degli utenti
 */
void freeUsers(hash_t table);

/**
 * @function hash
 * @brief funzione hash djb2 di Daniel Bernstein
 * 
 * @param str la stringa argomento della funzione
 * 
 * @return il valore hash calcolato, modulo
 *           la dimensione della tabella hash
 */
unsigned int hash(char *str);

#endif // USERS_H_
