/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */

#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES    10
#define MAX_SLEEPING    3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <message.h>

/**
 * @file   connections.h
 * @brief  Contiene le funzioni che implementano il protocollo 
 *           tra i clients ed il server
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @function openConnection
 * @brief    Apre una connessione AF_UNIX verso il server 
 *
 * @param path   path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs   tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char *path, unsigned int ntimes, unsigned int secs);

/**
 * @function readHeader
 * @brief    Legge l'header del messaggio
 *
 * @param fd  descrittore della connessione
 * @param hdr puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readHeader(long fd, message_hdr_t *hdr);

/**
 * @function readData
 * @brief    Legge il body del messaggio
 *
 * @param fd   descrittore della connessione
 * @param data puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readData(long fd, message_data_t *data);

/**
 * @function readMsg
 * @brief    Legge l'intero messaggio
 *
 * @param fd  descrittore della connessione
 * @param msg puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int readMsg(long fd, message_t *msg);

/**
 * @function sendHeader
 * @brief    invia l'header del messaggio
 * 
 * @param fd  descrittore della connessione
 * @param hdr puntatore all'header del messaggio
 * 
 * @return <= 0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa)
 */
int sendHeader(long fd, message_hdr_t *hdr);

/**
 * @function sendData
 * @brief    Invia il body del messaggio al server
 *
 * @param fd   descrittore della connessione
 * @param data puntatore ai dati del messaggio
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *data);

/**
 * @function sendMsg
 * @brief invia il messaggio al client
 * 
 * @param fd  descrittore della connessione
 * @param msg puntatore al messaggio
 * 
 * @return <= 0 se c'e' stato un errore
 */
int sendMsg(long fd, message_t *msg);

/**
 * @function sendOp
 * @brief    invia al client l'header con l'operazione
 * 
 * @param fd descrittore della connessione
 * @param op operazione da comunicare
 * 
 * @return <= 0 se c'e' stato un errore
 */
int sendOp(long fd, op_t op);

/**
 * @function sendRequest
 * @brief    Invia un messaggio di richiesta al server 
 *
 * @param fd  descrittore della connessione
 * @param msg puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg);

#endif /* CONNECTIONS_H_ */
