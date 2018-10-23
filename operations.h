#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <users.h>
#include <message.h>

/**
 * @file   operations.h
 * @brief  Contiene le funzioni che implementano le operazioni
 *           richieste dal client
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @function fsize
 * @brief    calcola la dimensione di un file
 * 
 * @param filename nome del file
 * 
 * @return la dimensione del file
 */
off_t fsize(const char *filename);

/**
 * @function registerOp
 * @brief    implementa l'operazione richiesta con REGISTER_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void registerOp(hash_t users, int fd, message_t msg);

/**
 * @function connectOp
 * @brief    implementa l'operazione richiesta con CONNECT_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void connectOp(hash_t users, int fd, message_t msg);

/**
 * @function postTxtOp
 * @brief    implementa l'operazione richiesta con POSTTXT_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postTxtOp(hash_t users, int fd, message_t msg);

/**
 * @function postTxtAllOp
 * @brief    implementa l'operazione richiesta con POSTTXTALL_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postTxtAllOp(hash_t users, int fd, message_t msg);

/**
 * @function postFileOp
 * @brief    implementa l'operazione richiesta con POSTFILE_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postFileOp(hash_t users, int fd, message_t msg);

/**
 * @function getFileOp
 * @brief    implementa l'operazione richiesta con GETFILE_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param req   messaggio di richiesta
 */
void getFileOp(hash_t users, int fd, message_t req);

/**
 * @function unregisterOp
 * @brief    implementa l'operazione richiesta con UNREGISTER_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void unregisterOp(hash_t users, int fd, message_t msg);

/**
 * @function createGroupOp
 * @brief    implementa l'operazione richiesta con CREATEGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void createGroupOp(hash_t users, int fd, message_t msg);

/**
 * @function addGroupOp
 * @brief    implementa l'operazione richiesta con ADDGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void addGroupOp(hash_t users, int fd, message_t msg);

/**
 * @function delGroupOp
 * @brief    implementa l'operazione richiesta con DELGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void delGroupOp(hash_t users, int fd, message_t msg);

#endif // OPERATIONS_H
