/*
 * membox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */

/**
 * @file   config.h
 * @brief  File contenente alcune define con valori massimi utilizzabili
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

#if !defined(CONFIG_H_)
#define CONFIG_H_

#define MAX_NAME_LENGTH 32

/* aggiungere altre define qui */

#define END NULL

// to avoid warnings like "ISO C forbids an empty translation unit"
typedef int make_iso_compilers_happy;

/**
 * @struct config
 * @brief  parametri di configurazione
 * 
 * @var ThreadsInPool  numero di thread worker
 * @var MaxConnections numero massimo di connessioni in coda
 * @var MaxHistMsgs    numero massimo di messaggi conservati 
 *                       nella history di ogni utente
 * @var MaxMsgSize     lunghezza massima di un messaggio testuale
 * @var MaxFileSize    dimensione massima di un file
 */
typedef struct {
	unsigned int ThreadsInPool;
	unsigned int MaxConnections;
	unsigned int MaxHistMsgs;
	unsigned int MaxMsgSize;
	unsigned int MaxFileSize;
} config;

#endif /* CONFIG_H_ */
