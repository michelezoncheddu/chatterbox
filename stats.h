/*
 * chatterbox Progetto del corso di LSO 2017/2018
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */

#if !defined(MEMBOX_STATS_)
#define MEMBOX_STATS_

#include <stdio.h>
#include <time.h>

/**
 * @file   stats.h
 * @brief  Contiene le strutture dati che definiscono il formato
 *           delle statistiche generate dal server
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @struct statistics
 * @brief  statistiche del server
 *
 * @var nusers            numero utenti registrati
 * @var nonline           numero utenti online
 * @var ndelivered        numero messaggi inviati
 * @var nnotdelivered     numero messaggi non inviati
 * @var nfiledelivered    numero file inviati
 * @var nfilenotdelivered numero file non inviati
 * @var nerrors           numero errori
 */
typedef struct {
    unsigned long nusers;                       // n. di utenti registrati
    unsigned long nonline;                      // n. di utenti connessi
    unsigned long ndelivered;                   // n. di messaggi testuali consegnati
    unsigned long nnotdelivered;                // n. di messaggi testuali non ancora consegnati
    unsigned long nfiledelivered;               // n. di file consegnati
    unsigned long nfilenotdelivered;            // n. di file non ancora consegnati
    unsigned long nerrors;                      // n. di messaggi di errore
} statistics;

/* aggiungere qui altre funzioni di utilita' per le statistiche */


/**
 * @function printStats
 * @brief    Stampa le statistiche nel file passato come argomento
 *
 * @param fout descrittore del file aperto in append.
 *
 * @return 0 in caso di successo, -1 in caso di fallimento 
 */
static inline int printStats(FILE *fout) {
    extern statistics chattyStats;

    if (fprintf(fout, "%ld - %ld %ld %ld %ld %ld %ld %ld\n",
		(unsigned long)time(NULL),
		chattyStats.nusers, 
		chattyStats.nonline,
		chattyStats.ndelivered,
		chattyStats.nnotdelivered,
		chattyStats.nfiledelivered,
		chattyStats.nfilenotdelivered,
		chattyStats.nerrors
		) < 0) return -1;
    fflush(fout);
    return 0;
}

#endif /* MEMBOX_STATS_ */
