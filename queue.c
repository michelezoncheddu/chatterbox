#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <util.h>
#include <queue.h>

/**
 * @file   queue.c
 * @brief  Contiene le funzioni che implementano una coda
 *           utilizzabile concorrentemente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

// lock per l'utilizzo della coda
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * variabile di condizione per la sospensione fino
 * all'inserimento di un elemento nella coda vuota
 */
static pthread_cond_t full  = PTHREAD_COND_INITIALIZER;

/**
 * @function initQueue
 * @brief    inizializza la coda
 * 
 * @return la coda inizializzata
 */
queue_t *initQueue() {
	queue_t *q = malloc(sizeof(queue_t));
	if (!q)
		return NULL;

	q->head = q->tail = NULL;
	q->qlen = 0;
	return q;
}

/**
 * @function enqueue
 * @brief    inserisce un elemento all'interno della coda
 * 
 * @param q    puntatore alla coda
 * @param data puntatore ai dati da inserire nel nodo
 * 
 * @return -1 se non e' stato possibile creare un nuovo nodo
 *          1 altrimenti
 */
int enqueue(queue_t *q, void *data) {
	node_t *new = malloc(sizeof(node_t));
	if (!new)
		return -1;

	new->data = data;
	new->next = NULL;
	pthread_mutex_lock(&mutex);
	if (q->qlen == 0)
		q->head = q->tail = new;
	else {
		q->tail->next = new;
		q->tail = new;
	}
	q->qlen++;
	pthread_cond_signal(&full);
	pthread_mutex_unlock(&mutex);
	return 1;
}

/**
 * @function dequeue
 * @brief    estrae un elemento dalla coda
 * 
 * @param q puntatore alla coda
 * 
 * @return puntatore ai dati dell'elemento estratto
 */
void *dequeue(queue_t *q) {
	void *data;
	pthread_mutex_lock(&mutex);
	while (q->qlen == 0)
		pthread_cond_wait(&full, &mutex);
	if (q->qlen == 1) {
		data = q->head->data;
		free(q->head);
		q->head = q->tail = NULL;
	}
	else {
		node_t *tmp = q->head;
		data = q->head->data;
		q->head = q->head->next;
		free(tmp);
	}
	q->qlen--;
	pthread_mutex_unlock(&mutex);
	return data;
}

/**
 * @function freeQueue
 * @brief    libera la coda
 * 
 * @param q puntatore alla coda
 */
void freeQueue(queue_t *q) {
	if (q->qlen == 0) 
		free(q);
	else {
		node_t *elem, *tmp;
		elem = q->head;
		while (elem) {
			tmp = elem;
			elem = elem->next;
			free(tmp->data);
			free(tmp);
			q->qlen--;
		}
		free(q);
	}
}
