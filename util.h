#ifndef UTIL_H_
#define UTIL_H_

#include <unistd.h>
#include <errno.h>

#include <queue.h>
#include <users.h>

/**
 * @file   util.h
 * @brief  Contiene definizioni di macro e di funzioni di utilita'
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

#define SYSCALL(r, c, e) \
	if((r = c) == -1) { perror(e); exit(errno); }

#define LIBCALL(r, c, e) \
	if((r = c) != 0) { errno=r; perror(e); exit(EXIT_FAILURE); }

#define FUNCALL(r, c, e) \
	if((r = c) <= 0) { errno=r; fprintf(stderr, "ERROR %d: function %s \n", r, e); exit(EXIT_FAILURE); }

#define MALLOC(r, c, e) \
	if((r = c) == NULL) { fprintf(stderr, "ERROR: malloc %s \n", e); exit(EXIT_FAILURE); }

/**
 * @function readn
 * @brief    legge esattamente size byte
 * 
 * @param fd   il fd dal quale bisogna leggere
 * @param buf  il buffer nel quale salvare i dati
 * @param size il numero di byte da leggere
 * 
 * @return -1 se c'e' stato un errore
 *        >=0 altrimenti
 */
static inline int readn(int fd, void *buf, size_t size) {
	size_t left = size;
	int r;
	char *bufptr = (char*)buf;
	while (left > 0) {
		if ((r = read(fd, bufptr, left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0;
		left   -= r;
		bufptr += r;
	}
	return size;
}

/**
 * @function writen
 * @brief    scrive esattamente size byte
 * 
 * @param fd   il fd nel quale bisogna scrivere
 * @param buf  il buffer dal quale leggere i dati da inviare
 * @param size il numero di byte da scrivere
 * 
 * @return -1 se c'e' stato un errore
 *        >=0 altrimenti
 */
static inline int writen(int fd, void *buf, size_t size) {
	size_t left = size;
	int r;
	char *bufptr = (char*)buf;
	while (left > 0) {
		if ((r = write(fd, bufptr, left)) == -1) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0) return 0;  
		left   -= r;
		bufptr += r;
	}
	return size;
}

/**
 * @function mcd
 * @brief    calcola il massimo comun divisore tra due numeri
 *             utilizzando l'algoritmo di Euclide
 * 
 * @param a primo parametro
 * @param b secondo parametro
 * 
 * @return il massimo comun divisore tra a e b
 */
static inline int mcd(int a, int b) {
	int r;
	while (b != 0) {
		r = a % b;
		a = b; 
		b = r;
	}
	return a;
}

/**
 * @function isIn
 * @brief    verifica la presenza di una stringa
 *             in un array di stringhe
 * 
 * @param key   la stringa da cercare
 * @param array l'array nel quale cercare
 * @param n     la dimensione dell'array
 * 
 * @return 0 se key non e' presente nell'array
 *         1 altrimenti
 */
static inline int isIn(char *key, char **array, int n) {
	for (int i = 0; i < n; ++i)
		if (strncmp(key, array[i], MAX_NAME_LENGTH + 1) == 0)
			return 1;
	return 0;
}

#endif // UTIL_H_
