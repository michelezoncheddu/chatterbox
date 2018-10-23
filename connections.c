/**
 * @file   connections.c
 * @brief  Contiene le funzioni che implementano il protocollo 
 *           tra i clients ed il server
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#include <message.h>
#include <connections.h>
#include <util.h>

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
int openConnection(char *path, unsigned int ntimes, unsigned int secs) {
	int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd == -1) {
		perror("socket");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, UNIX_PATH_MAX);

	for (int i = 0; i < ntimes; ++i) {
		if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
			sleep(secs);
			continue;
		}
		return sock_fd;
	}
	perror("connect");
	return -1;
}

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
int readHeader(long fd, message_hdr_t *hdr) {
	return readn((int)fd, hdr, sizeof(message_hdr_t));
}

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
int readData(long fd, message_data_t *data) {
	int n = readn((int)fd, &(data->hdr), sizeof(message_data_hdr_t));
	if (n == -1)
		return -1;

	if (n == 0 || data->hdr.len == 0) // non devo leggere dati
		return n;
	
	MALLOC(data->buf, malloc(data->hdr.len), "data readData");
	
	return readn((int)fd, data->buf, data->hdr.len);
}

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
int readMsg(long fd, message_t *msg) {
	int n = readHeader(fd, &(msg->hdr));
	if (n > 0)
		return readData(fd, &(msg->data));
	return n;
}

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
int sendHeader(long fd, message_hdr_t *hdr) {
	return writen((int)fd, hdr, sizeof(message_hdr_t));
}

/**
 * @function sendData
 * @brief    Invia il body del messaggio al server
 *
 * @param fd   descrittore della connessione
 * @param data puntatore ai dati del messaggio
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *data) {
	// invio l'header, per far sapere la lunghezza dei dati al client
	int n = writen((int)fd, &(data->hdr), sizeof(message_data_hdr_t));
	if (n == -1)
		return -1;

	if (data->hdr.len == 0) // non ho dati da inviare
		return n;

	// invio i dati
	return writen((int)fd, data->buf, data->hdr.len);
}

/**
 * @function sendMsg
 * @brief invia il messaggio al client
 * 
 * @param fd  descrittore della connessione
 * @param msg puntatore al messaggio
 * 
 * @return <= 0 se c'e' stato un errore
 */
int sendMsg(long fd, message_t *msg) {
	int n = sendHeader(fd, &(msg->hdr));
	if (n > 0)
		n += sendData(fd, &(msg->data));
	return n;
}

/**
 * @function sendOp
 * @brief    invia al client l'header con l'operazione
 * 
 * @param fd descrittore della connessione
 * @param op operazione da comunicare
 * 
 * @return <= 0 se c'e' stato un errore
 */
int sendOp(long fd, op_t op) {
	message_hdr_t hdr;
	memset(&hdr, 0, sizeof(message_hdr_t));
	hdr.op = op;
	strncpy(hdr.sender, "server", 7); // MAX_NAME_LENGTH e' > 6
	return sendHeader(fd, &hdr);
}

/**
 * @function sendRequest
 * @brief    Invia un messaggio di richiesta al server 
 *
 * @param fd  descrittore della connessione
 * @param msg puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg) {
	return sendMsg(fd, msg);
}
