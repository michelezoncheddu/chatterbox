#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <util.h>
#include <users.h>
#include <config.h>
#include <message.h>
#include <groups.h>

/**
 * @file   groups.c
 * @brief  Contiene le funzioni che implementano la gestione
 *           dei gruppi in modo concorrente
 * @author Michele Zoncheddu 545227
 * 
 * Si dichiara che il contenuto di questo file e'
 *   in ogni sua parte opera originale dell'autore
 */

static group_t         *groups;                                   // la struttura gruppi
static pthread_mutex_t  groups_mutex = PTHREAD_MUTEX_INITIALIZER; // per la struttura groups
extern unsigned int     MaxGroups;                                // parametro di configurazione


/**
 * @function initGroups
 * @brief    alloca e inizializza le strutture dati
 *             per la gestione dei gruppi
 */
void initGroups() {
	// inizializzo i gruppi
	MALLOC(groups, malloc(MaxGroups * sizeof(group_t)), "groups initHash");
	for (int i = 0; i < MaxGroups; ++i) {
		MALLOC(groups[i].name, malloc(MAX_NAME_LENGTH + 1), "groups name");
		MALLOC(groups[i].creator, malloc(MAX_NAME_LENGTH + 1), "groups creator");
		groups[i].members = NULL;
		groups[i].nmembers = 0;
	}
}

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
int createGroup(char *groupName, char *creator) {
	int       i = 0;
	member_t *name;

	// cerco la prima posizione libera
	pthread_mutex_lock(&groups_mutex);
	while (i < MaxGroups && groups[i].nmembers > 0)
		i++;
	if (i == MaxGroups) { // spazio esaurito
		pthread_mutex_unlock(&groups_mutex);
		return -1;
	}
	// imposto il nome del gruppo e il creatore
	strncpy(groups[i].name, groupName, MAX_NAME_LENGTH + 1);
	strncpy(groups[i].creator, creator, MAX_NAME_LENGTH + 1);
	MALLOC(name, malloc(sizeof(member_t)), "name createGroup");
	MALLOC(name->nick, malloc(MAX_NAME_LENGTH + 1), "name.nick createGroup");
	strncpy(name->nick, creator, MAX_NAME_LENGTH + 1);
	name->next = NULL;
	groups[i].members = name;
	groups[i].nmembers++;
	pthread_mutex_unlock(&groups_mutex);
	return 1;
}

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
int addToGroup(char *groupName, char *user) {
	member_t *new, *tmp;
	int       i = 0;

	// cerco il gruppo
	pthread_mutex_lock(&groups_mutex);
	while (i < MaxGroups && (groups[i].nmembers == 0 || strncmp(groups[i].name, groupName, MAX_NAME_LENGTH + 1) != 0))
		i++;
	if (i == MaxGroups) { // gruppo non trovato
		pthread_mutex_unlock(&groups_mutex);
		return 0;
	}

	// controllo che non sia gia' presente all'interno del gruppo
	tmp = groups[i].members;
	while (tmp) {
		if (strncmp(tmp->nick, user, MAX_NAME_LENGTH + 1) == 0)
			break;
		tmp = tmp->next;
	}
	if (tmp) { // gia' iscritto
		pthread_mutex_unlock(&groups_mutex);
		return -1;
	}

	MALLOC(new, malloc(sizeof(member_t)), "new addToGroup");
	MALLOC(new->nick, malloc(MAX_NAME_LENGTH + 1), "new->nick addToGroup");
	strncpy(new->nick, user, MAX_NAME_LENGTH + 1);
	new->next = groups[i].members;
	groups[i].members = new;
	groups[i].nmembers++;
	pthread_mutex_unlock(&groups_mutex);
	return 1;
}

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
int removeFromGroup(char *groupName, char *user) {
	int i = 0;
	member_t *member, *prev = NULL, *tmp;

	// cerco il gruppo 
	pthread_mutex_lock(&groups_mutex);
	while (strncmp(groups[i].name, groupName, MAX_NAME_LENGTH + 1) != 0)
		i++;

	if (i == MaxGroups) { // gruppo non trovato
		pthread_mutex_unlock(&groups_mutex);
		return 0;
	}

	// il creatore sta abbandonando il gruppo e non ci sono "eredi"
	if (strncmp(groups[i].creator, user, MAX_NAME_LENGTH + 1) == 0 && groups[i].nmembers == 1) {
		pthread_mutex_unlock(&groups_mutex);
		return deleteGroup(groupName, user);
	}

	// cancello l'utente
	member = groups[i].members;
	while (member) {
		if (strncmp(member->nick, user, MAX_NAME_LENGTH + 1) == 0) // utente trovato
			break;
		prev = member;
		member = member->next;
	}
	if (!member) { // utente non trovato
		pthread_mutex_unlock(&groups_mutex);
		return -1;
	}
	if (!prev) { // devo cancellare la testa della lista
		tmp = member;
		groups[i].members = member->next;
		free(tmp->nick);
		free(tmp);

		/* se il creatore si sta cancellando, eredita i diritti
		   l'utente che si e' iscritto subito dopo di lui */
		if (strncmp(groups[i].creator, user, MAX_NAME_LENGTH + 1) == 0)
			strncpy(groups[i].creator, groups[i].members->nick, MAX_NAME_LENGTH + 1);
	}
	else { // elemento centrale
		tmp = member;
		prev->next = member->next;
		free(member->next);
		free(member);

		/* se il creatore si sta cancellando, eredita i diritti
		   l'utente che si e' iscritto subito dopo di lui */
		if (strncmp(groups[i].creator, user, MAX_NAME_LENGTH + 1) == 0)
			strncpy(groups[i].creator, prev->nick, MAX_NAME_LENGTH + 1);
	}
	groups[i].nmembers--;
	pthread_mutex_unlock(&groups_mutex);
	return 1;
}

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
int deleteGroup(char *groupName, char *user) {
	int i = 0;
	member_t *member, *tmp;
	
	// cerco il gruppo 
	pthread_mutex_lock(&groups_mutex);
	while (strncmp(groups[i].name, groupName, MAX_NAME_LENGTH + 1) != 0)
		i++;

	if (i == MaxGroups) { // gruppo non trovato
		pthread_mutex_unlock(&groups_mutex);
		return 0;
	}
	
	// controllo che l'operazione sia richiesta dal creatore del gruppo
	if (strncmp(groups[i].creator, user, MAX_NAME_LENGTH + 1) != 0) {
		pthread_mutex_unlock(&groups_mutex);
		return -1;
	}

	// cancello tutto il gruppo
	member = groups[i].members;
	while (member) {
		free(member->nick);
		tmp = member;
		member = member->next;
		free(tmp);
		groups[i].nmembers--;

	}
	groups[i].members = member;

	pthread_mutex_unlock(&groups_mutex);
	return 1;
}

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
int getMembers(char *groupName, char ***list) {
	int       i = 0;
	char    **local_list;
	member_t *tmp;

	// cerco il gruppo
	pthread_mutex_lock(&groups_mutex);
	while (strncmp(groups[i].name, groupName, MAX_NAME_LENGTH + 1))
		i++;
	
	if (i == MaxGroups) { // gruppo non trovato
		pthread_mutex_unlock(&groups_mutex);
		return -1;
	}
	
	MALLOC(local_list, malloc(groups[i].nmembers * sizeof(char*)), "local_list getMembers");
	for (int j = 0; j < groups[i].nmembers; ++j)
		MALLOC(local_list[j], malloc(MAX_NAME_LENGTH + 1), "local_list getMembers")

	tmp = groups[i].members;
	i = 0;
	while (tmp) {
		strncpy(local_list[i++], tmp->nick, MAX_NAME_LENGTH + 1);
		tmp = tmp->next;
	}
	*list = local_list;
	pthread_mutex_unlock(&groups_mutex);
	return i;
}

/**
 * @function freeGroups
 * @brief    dealloca tutti i gruppi, i loro membri
 *             e l'array dei gruppi
 */
void freeGroups() {
	member_t *name_elem, *name_tmp;
	// cleanup
	for (int i = 0; i < MaxGroups; ++i) {
		free(groups[i].name);
		free(groups[i].creator);
		name_elem = groups[i].members;
		while (name_elem) {
			free(name_elem->nick);
			name_tmp = name_elem;
			name_elem = name_elem->next;
			free(name_tmp);
		}
	}
	free(groups);
}
