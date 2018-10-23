#ifndef QUEUE_H_
#define QUEUE_H_

/**
 * @file   queue.h
 * @brief  Contiene le funzioni che implementano una coda
 *           utilizzabile concorrentemente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @struct node
 * @brief  nodo della lista
 * 
 * @var data puntatore ai dati del nodo
 * @var next puntatore al prossimo nodo della lista
 */
typedef struct node {
	void        *data;
	struct node *next;
} node_t;

/**
 * @struct queue
 * @brief  dati della coda
 * 
 * @var head puntatore alla testa della coda
 * @var tail puntatore alla fine della coda
 * @var quel lunghezza della coda
 */
typedef struct queue {
	node_t        *head;
	node_t        *tail;
	unsigned long  qlen;
} queue_t;

/**
 * @function initQueue
 * @brief    inizializza la coda
 * 
 * @return la coda inizializzata
 */
queue_t *initQueue();

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
int enqueue(queue_t *q, void *data);

/**
 * @function dequeue
 * @brief    estrae un elemento dalla coda
 * 
 * @param q puntatore alla coda
 * 
 * @return puntatore ai dati dell'elemento estratto
 */
void *dequeue(queue_t *q);

/**
 * @function freeQueue
 * @brief    libera la coda
 * 
 * @param q puntatore alla coda
 */
void freeQueue(queue_t *q);

#endif // QUEUE_H_
