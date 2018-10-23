#ifndef GROUPS_H_
#define GROUPS_H_

/**
 * @file   groups.h
 * @brief  Contiene le funzioni che implementano la gestione
 *           dei gruppi in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

/**
 * @struct member
 * @brief  membro di un gruppo
 * 
 * @var nick nome del membro
 * @var next puntatore al prossimo membro
 */
typedef struct member {
	char          *nick;
	struct member *next;
} member_t;

/**
 * @struct group_t
 * @brief  dati di un gruppo
 * 
 * @var name     nome del gruppo
 * @var creator  nome del creatore del gruppo
 * @var members  lista di membri del gruppo
 * @var nmembers numero di membri del gruppo
 */
typedef struct {
	char     *name;
	char     *creator;
	member_t *members;
	int       nmembers;
} group_t;

/**
 * @function initGroups
 * @brief    alloca e inizializza le strutture dati
 *             per la gestione dei gruppi
 */
void initGroups();

/**
 * @function createGroup
 * #brief    crea un nuovo gruppo
 * 
 * @param groupName il nome del gruppo che si vuole creare
 * @param creator   il nome del creatore
 * 
 * @return  1 se e' stato possibile creare il gruppo
 *         -1 altrimenti
 */
int createGroup(char *groupName, char *creator);

/**
 * @function addToGroup
 * @brief    aggiunge un nuovo utente al gruppo
 * 
 * @param groupName il nome del gruppo
 * @param user      il nome dell'utente
 * 
 * @return 1 se l'utente e' stato aggiunto correttamente
 *         0 se il gruppo non esiste
 *        -1 se l'utente e' gia' all'interno del gruppo
 */
int addToGroup(char *groupName, char *user);

/**
 * @function removeFromGroup
 * @brief    rimuove un utente dal gruppo
 * 
 * @param groupName il nome del gruppo
 * @param user      il nome dell'utente
 * 
 * @return 1 se l'utente e' stato rimosso dal gruppo
 *         0 se il gruppo non esiste
 *        -1 se l'utente non appartiene al gruppo
 */
int removeFromGroup(char *groupName, char *user);

/**
 * @function deleteGroup
 * @brief    cancella un gruppo, rimuovendo tutti i partecipanti
 * 
 * @param groupName il nome del gruppo
 * @param user      il creatore del gruppo
 * 
 * @return 1 se il gruppo e' stato cancellato
 *         0 se il gruppo non esiste
 *        -1 se user non e' il creatore del gruppo
 */
int deleteGroup(char *groupName, char *user);

/**
 * @function getMembers
 * @brief    crea la lista dei membri del gruppo
 * 
 * @param groupName il nome del gruppo
 * @param list puntatore all'array di stringhe
 * 
 * @return -1 se il gruppo non esiste
 *           i > 0 il numero di membri del gruppo
 */
int getMembers(char *groupName, char ***list);

/**
 * @function freeGroups
 * @brief    dealloca tutti i gruppi, i loro membri
 *             e l'array dei gruppi
 */
void freeGroups();

#endif // GROUPS_H
