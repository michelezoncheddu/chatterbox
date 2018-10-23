#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <libgen.h>
#include <fcntl.h>
#include <pthread.h>

#include <stats.h>
#include <util.h>
#include <users.h>
#include <online.h>
#include <groups.h>
#include <message.h>
#include <connections.h>

/**
 * @file   operations.c
 * @brief  Contiene le funzioni che implementano le operazioni
 *           richieste dal client
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

extern statistics chattyStats; // definita in stats.h
extern config     conf;        // parametri di configurazione
extern char      *DirName;     // files directory
static int        notused;

/**
 * @function fsize
 * @brief    calcola la dimensione di un file
 * 
 * @param filename nome del file
 * 
 * @return la dimensione del file
 */
off_t fsize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

/**
 * @function registerOp
 * @brief    implementa l'operazione richiesta con REGISTER_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void registerOp(hash_t users, int fd, message_t msg) {
	// registro l'utente
	if (signUp(users, msg.hdr.sender, 0) == -1) {
		sendOp(fd, OP_NICK_ALREADY);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: nome %s gia' registrato\n", msg.hdr.sender);
		return;
	}
	printf("SERVER: %s registrato\n", msg.hdr.sender);
	if (addOnline(msg.hdr.sender, fd) == -1) { // troppi utenti online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: troppi utenti online, impossibile connettere %s\n", msg.hdr.sender);
		return;
	}
	sendOnlineList(msg.hdr.sender); // invio la lista di utenti online
}

/**
 * @function connectOp
 * @brief    implementa l'operazione richiesta con CONNECT_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void connectOp(hash_t users, int fd, message_t msg) {
	if (getOnline(msg.hdr.sender) != -1) { // utente gia' online
		sendOpAtomic(msg.hdr.sender, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s gia' collegato\n", msg.hdr.sender);
		return;
	}
	if (isRegistered(users, msg.hdr.sender) != 1) { // utente non registrato o nome di gruppo
		sendOp(fd, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: impossibile connettere %s\n", msg.hdr.sender);
		return;
	}
	if (addOnline(msg.hdr.sender, fd) == -1) { // impossibile aggiungere l'utente alla lista online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: troppi utenti online, impossibile connettere %s\n", msg.hdr.sender);
		return;
	}
	printf("SERVER: %s connesso\n", msg.hdr.sender);
	sendOnlineList(msg.hdr.sender); // invio la lista di utenti online				
}

/**
 * @function postTxtOp
 * @brief    implementa l'operazione richiesta con POSTTXT_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postTxtOp(hash_t users, int fd, message_t msg) {
	int res; // vale 1 se il destinatario e' un utente, 2 se e' un gruppo
	if (getOnline(msg.hdr.sender) == -1) { // mittente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi per poter inviare un messaggio\n", msg.hdr.sender);
		return;
	}
	res = 1;
	if (getOnline(msg.data.hdr.receiver) == -1) { // destinatario non online
		if (!(res = isRegistered(users, msg.data.hdr.receiver))) { // destinatario non registrato
			sendOp(fd, OP_NICK_UNKNOWN);
			chattyStats.nerrors++;
			printf("SERVER - ERRORE: utent o gruppo %s non registrato\n", msg.data.hdr.receiver);
			return;
		}
	}
	if (msg.data.hdr.len > conf.MaxMsgSize) { // messaggio troppo lungo
		sendOpAtomic(msg.hdr.sender, OP_MSG_TOOLONG);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: messaggio troppo lungo\n");
		return;
	}
	msg.hdr.op = TXT_MESSAGE;
	if (res == 1) // il destinatario e' un utente
		sendMessage(users, msg); // salvo il messaggio e provo ad inviarlo
	else { // il destinatario e' un gruppo
		res = sendMessageToGroup(users, msg);
		free(msg.data.buf);
	}
	if (res == -1) { // l'utente non appartiene al gruppo
		sendOpAtomic(msg.hdr.sender, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: l'utente non appartiene al gruppo\n");
		return;
	}
	sendOpAtomic(msg.hdr.sender, OP_OK); // invio l'ack al mittente
}

/**
 * @function postTxtAllOp
 * @brief    implementa l'operazione richiesta con POSTTXTALL_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postTxtAllOp(hash_t users, int fd, message_t msg) {
	if (getOnline(msg.hdr.sender) == -1) { // mittente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi per poter inviare un messaggio\n", msg.hdr.sender);
		return;
	}
	if (msg.data.hdr.len > conf.MaxMsgSize) { // messaggio troppo lungo
		sendOpAtomic(msg.hdr.sender, OP_MSG_TOOLONG);
		chattyStats.nerrors++;
		free(msg.data.buf);
		printf("SERVER - ERRORE: messaggio troppo lungo\n");
		return;
	}
	msg.hdr.op = TXT_MESSAGE;
	sendMessageAll(users, msg); // salvo il messaggio e lo invio in broadcast
	sendOpAtomic(msg.hdr.sender, OP_OK); // invio l'ack al mittente
	free(msg.data.buf);
}

/**
 * @function postFileOp
 * @brief    implementa l'operazione richiesta con POSTFILE_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void postFileOp(hash_t users, int fd, message_t msg) {
	int fd_file; // fd del file da salvare
	int res; // vale 1 se il destinatario e' un utente, 2 se e' un gruppo

	if (getOnline(msg.hdr.sender) == -1) { // mittente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi per poter inviare un file\n", msg.hdr.sender);
		return;
	}
	res = 1;
	if (getOnline(msg.data.hdr.receiver) == -1) { // destinatario non online
		if (!(res = isRegistered(users, msg.data.hdr.receiver))) { // destinatario non registrato
			sendOp(fd, OP_NICK_UNKNOWN);
			chattyStats.nerrors++;
			printf("SERVER - ERRORE: destinatario inesistente\n");
			return;
		}
	}
	if (msg.data.hdr.len > conf.MaxMsgSize) { // messaggio troppo lungo
		sendOpAtomic(msg.hdr.sender, OP_MSG_TOOLONG);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: messaggio troppo lungo\n");
		return;
	}
	msg.hdr.op = FILE_MESSAGE;

	SYSCALL(notused, chdir(DirName), "chdir"); // mi posiziono nella cartella dei file
	message_data_t file;
	
	// leggo il file
	if (readData(fd, &file) <= 0) {
		sendOpAtomic(msg.hdr.sender, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: leggendo il file\n");
		return;
	}

	if (file.hdr.len > conf.MaxFileSize * 1024) { // file troppo grande
		sendOpAtomic(msg.hdr.sender, OP_MSG_TOOLONG);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: file troppo grande\n");
		return;
	}

	SYSCALL(fd_file, open(basename(msg.data.buf), O_CREAT | O_TRUNC | O_RDWR, 0777), "open"); // creo il file
	SYSCALL(notused, writen(fd_file, file.buf, file.hdr.len), "writen"); // scrivo il file
	free(file.buf);

	sendOpAtomic(msg.hdr.sender, OP_OK); // invio l'ack al mittente
	if (res == 1) // il destinatario e' un utente
		sendMessage(users, msg); // salvo il messaggio e provo ad inviarlo
	else { // il destinatario e' un gruppo
		res = sendMessageToGroup(users, msg);
		free(msg.data.buf);
	}
	if (res == -1) { // l'utente non appartiene al gruppo
		sendOpAtomic(msg.hdr.sender, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: l'utente non appartiene al gruppo\n");
		return;
	}
	SYSCALL(notused, close(fd_file), "close");
}

/**
 * @function getFileOp
 * @brief    implementa l'operazione richiesta con GETFILE_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param req   messaggio di richiesta
 */
void getFileOp(hash_t users, int fd, message_t req) {
	int size;      // dimensione del file da inviare
	int fd_file;   // fd del file da inviare
	char *buf;     // buffer nel quale salvare il file
	message_t msg; // wrapper per il file

	if (getOnline(req.hdr.sender) == -1) { // richiedente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi per poter scaricare un file\n", req.hdr.sender);
		return;
	}

	char *base = basename(req.data.buf);
	SYSCALL(notused, chdir(DirName), "chdir"); // mi posiziono nella cartella dei file
	fd_file = open(base, O_RDONLY); // provo ad aprire il file

	if (fd_file == -1) { // il file non esiste
		sendOpAtomic(req.hdr.sender, OP_NO_SUCH_FILE);
		chattyStats.nerrors++;
		return;
	}

	// leggo il file dal disco
	size = fsize(base); // ottengo la dimensione del file
	MALLOC(buf, malloc(size), "buf GETFILE_OP");
	SYSCALL(notused, readn(fd_file, buf, size), "readn fd_file");
	close(fd_file);
	setHeader(&(msg.hdr), OP_OK, "server");
	setData(&(msg.data), req.hdr.sender, buf, size);

	// invio il messaggio con il file
	if (sendMessageAtomic(msg) > 0) {
		chattyStats.nfiledelivered++;
		chattyStats.nfilenotdelivered--;
	}
	free(buf);
	free(req.data.buf);
}

/**
 * @function unregisterOp
 * @brief    implementa l'operazione richiesta con UNREGISTER_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void unregisterOp(hash_t users, int fd, message_t msg) {
	int res; // vale 1 se devo cancellare un utente, 2 se un gruppo

	if (getOnline(msg.hdr.sender) == -1) { // richiedente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi\n", msg.hdr.sender);
		return;
	}
	if (!(res = isRegistered(users, msg.data.hdr.receiver))) {
		sendOp(fd, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: utente o gruppo inesistente\n");
		return;
	}
	if (res == 1) { // devo cancellare un utente
		unregisterUser(users, msg.hdr.sender);
		printf("SERVER: utente %s deregistrato\n", msg.hdr.sender);
	}
	else { // devo cancellare un gruppo
		if ((res = deleteGroup(msg.data.hdr.receiver, msg.hdr.sender)) < 1) {
			if (res == -1) {
				sendOp(fd, OP_FAIL);
				printf("SERVER - ERRORE: solo il creatore del gruppo puo' cancellarlo\n");
			}
			else {
				sendOp(fd, OP_NICK_UNKNOWN);
				printf("SERVER - ERRORE: gruppo inesistente\n");
			}
			chattyStats.nerrors++;
			return;
		}
		printf("SERVER: gruppo %s cancellato\n", msg.data.hdr.receiver);
	}
	sendOp(fd, OP_OK); // invio l'ack al mittente
}

/**
 * @function createGroupOp
 * @brief    implementa l'operazione richiesta con CREATEGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void createGroupOp(hash_t users, int fd, message_t msg) {
	if (getOnline(msg.hdr.sender) == -1) { // mittente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi\n", msg.hdr.sender);
		return;
	}
	
	// creo il gruppo
	if (signUp(users, msg.data.hdr.receiver, 1) == -1) {
		sendOpAtomic(msg.hdr.sender, OP_NICK_ALREADY);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: gruppo %s gia' registrato\n", msg.data.hdr.receiver);
		return;
	}
	if (createGroup(msg.data.hdr.receiver, msg.hdr.sender) == -1) {
		sendOpAtomic(msg.hdr.sender, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: troppi gruppi presenti, impossibile crearne altri\n");
		return;
	}
	// mando l'ack
	sendOpAtomic(msg.hdr.sender, OP_OK);
}

/**
 * @function addGroupOp
 * @brief    implementa l'operazione richiesta con ADDGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void addGroupOp(hash_t users, int fd, message_t msg) {
	int res; // risultato dell'operazione di inserimento nel gruppo

	if (getOnline(msg.hdr.sender) == -1) { // richiedente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi\n", msg.hdr.sender);
		return;
	}
	if (isRegistered(users, msg.data.hdr.receiver) != 2) {
		sendOp(fd, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: gruppo %s inesistente\n", msg.data.hdr.receiver);
		return;
	}
	// provo ad aggiungerlo al gruppo
	if ((res = addToGroup(msg.data.hdr.receiver, msg.hdr.sender)) < 1)  {
		if (res == -1) {
			sendOpAtomic(msg.hdr.sender, OP_NICK_ALREADY);
			printf("SERVER - ERRORE: utente gia' presente all'interno del gruppo\n");
		}
		else { // il gruppo potrebbe essere stato cancellato un istante prima
				//   della chiamata di addToGroup
			sendOpAtomic(msg.hdr.sender, OP_NICK_UNKNOWN);
			printf("SERVER - ERRORE: gruppo inesistente\n");
		}
		chattyStats.nerrors++;
		return;
	}
	// mando l'ack
	sendOpAtomic(msg.hdr.sender, OP_OK);
}

/**
 * @function delGroupOp
 * @brief    implementa l'operazione richiesta con DELGROUP_OP
 * 
 * @param users tabella degli utenti
 * @param fd    fd del richiedente
 * @param msg   messaggio di richiesta
 */
void delGroupOp(hash_t users, int fd, message_t msg) {
	int res; // risultato dell'operazione di eliminazione dal gruppo

	if (getOnline(msg.hdr.sender) == -1) { // richiedente non online
		sendOp(fd, OP_FAIL);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: %s deve collegarsi\n", msg.hdr.sender);
		return;
	}

	// controllo che il gruppo esista
	if (isRegistered(users, msg.data.hdr.receiver) != 2) {
		sendOpAtomic(msg.hdr.sender, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		printf("SERVER - ERRORE: gruppo inesistente\n");
		return;
	}

	// provo a rimuovere l'utente dal gruppo
	if ((res = removeFromGroup(msg.data.hdr.receiver, msg.hdr.sender)) < 1) {
		sendOpAtomic(msg.hdr.sender, OP_NICK_UNKNOWN);
		chattyStats.nerrors++;
		if (res == -1)
			printf("SERVER - ERRORE: non sei all'interno del gruppo\n");
		else // il gruppo potrebbe essere stato cancellato un istante prima
				//   della chiamata di removeFromGroup
			printf("SERVER - ERRORE: gruppo inesistente\n");
		return;
	}
	sendOpAtomic(msg.hdr.sender, OP_OK); // invio l'ack al mittente
}
