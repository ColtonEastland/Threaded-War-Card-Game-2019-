#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

/* Structure for player so we can keep track of their cards */
typedef struct _player {
	int id;
	int card1;
	int card2;
} player_t;
 

/* Initialize a deck with 52 cards, this is a shared resource for all 3 players */
int cards[52];
int cards_size;

/* This is a shared variable to check if someone won */
int winner;

/* We need a mutex to be able to protect the critical sections of our code */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* This semaphore will make the players wait for the dealer to finish before they make a move */
sem_t dealer_semaphore;

/* A log file for tracing */
FILE *file;

player_t player1;
player_t player2;
player_t player3;

/* Get a card from the deck, this is a shared method between player threads */
int get_card() {
	int i;
	int card;
	
	card = cards[0];
	
	for(i = 0; i < cards_size - 1; i++)
		cards[i] = cards[i + 1];
		
	cards_size--;
	return card;
}

/* Return the card to the deck (back of deck, a shared method between player threads */
void return_card(int card) {
	cards[cards_size++] = card;
}

/* Print the content of the deck */
void print_deck() {
	int i;
	
	printf("DECK: ");
	
	for(i = 0; i < cards_size; i++) {
		printf("%d ", cards[i]);
	}
	
	printf("\n");
}

/* Log the deck to file */
void log_deck() {
	int i;
	
	fprintf(file, "DECK: ");
	
	for(i = 0; i < cards_size; i++) {
		fprintf(file, "%d ", cards[i]);
	}
	
	fprintf(file, "\n");
}

/* There will only be 1 dealer, what it does is it shuffles the cards */
void *handle_dealer_thread(void *args) {
	int i;
	int j;
	int temp;
	
	/* At this points, players are waiting for this dealer to finish its job */
	fprintf(file, "Dealer: Shuffling cards...\n");
			
	/* All players finished, we assume someone won already, reshuffle the cards */
	for(i = 0; i < 52; i++) {
		j = i + rand() / (RAND_MAX / (52 - i) + 1);
		temp = cards[i];
		cards[i] = cards[j];
		cards[j] = temp;
	}
	
	cards_size = 52;
	winner = -1;
	
	print_deck();
	
	player1.card1 = get_card();
	player2.card1 = get_card();
	player3.card1 = get_card();
			
	/* Now we make the players play, 1 notification for each player */
	sem_post(&dealer_semaphore);
	sem_post(&dealer_semaphore);
	sem_post(&dealer_semaphore);
	return (void *)NULL;
}

/* Run a player, 3 insances of this are created */
void *handle_player_thread(void *args) {
	player_t *player;
	int win;
	
	player = (player_t *)(args);
	win = -1;
		
	/* Wait for the dealer to distribut 1 card for all players */
	sem_wait(&dealer_semaphore);
	
	/* Okay player plays until somebody wins */
	while(1) {
		pthread_mutex_lock(&mutex);
		
		if(winner != -1) {
			/* Stop if somebody won */
			pthread_mutex_unlock(&mutex);
			break;
		}
		
		/* Print the initial card */
		if(player->card1 != -1) {
			fprintf(file, "PLAYER %d: hand %d\n", player->id, player->card1);
		} else {
			fprintf(file, "PLAYER %d: hand %d\n", player->id, player->card2);
		}
		
		/* Get a new card */
		if(player->card1 == -1) {
			player->card1 = get_card();
			fprintf(file, "PLAYER %d: draws %d\n", player->id, player->card1);
		} else {
			player->card2 = get_card();
			fprintf(file, "PLAYER %d: draws %d\n", player->id, player->card2);
		}
		
		/* Print both cards now */
		fprintf(file, "PLAYER %d: hand %d %d\n", player->id, player->card1, player->card2); 
		
		printf("PLAYER %d: hand %d %d\n", player->id, player->card1, player->card2);
		
		/* Check if we won */
		if(player->card1 == player->card2) {
			/* Stop if we won */
			printf("PLAYER %d WIN: yes\n", player->id); 
			winner = player->id;
			pthread_mutex_unlock(&mutex);
			break;			
		}
		
		printf("PLAYER %d WIN: no\n", player->id); 
		
		/* If we haven't won, discard one of the cards randomly */
		if(rand() % 2 == 0) {
			fprintf(file, "PLAYER %d: discards %d\n", player->id, player->card1);
			return_card(player->card1);
			player->card1 = -1;
		} else {
			fprintf(file, "PLAYER %d: discards %d\n", player->id, player->card2);
			return_card(player->card2);
			player->card2 = -1;
		}
		
		/* Print final hand */
		if(player->card1 != -1) {
			fprintf(file, "PLAYER %d: hand %d\n", player->id, player->card1);
		} else {
			fprintf(file, "PLAYER %d: hand %d\n", player->id, player->card2);
		}
		
		/* Print final deck */
		print_deck();		
		log_deck();
		pthread_mutex_unlock(&mutex);
	}
	
	/* Exit round */
	if(winner == player->id) {
		fprintf(file, "PLAYER %d: wins and exits round\n", player->id);
	} else {
		fprintf(file, "PLAYER %d: exits round\n", player->id);
	}
	
	return (void *)NULL;
}

/* Entry point of the program */
int main(int argc, char *argv[]) {
	int round = 0;
	int i;
	int j;
	
	pthread_t player1_thread;
	pthread_t player2_thread;
	pthread_t player3_thread;
	pthread_t dealer_thread;
		
	srand((unsigned)time(NULL));
		
	/* Initialize the deck, there are 4 suits and 13 cards each suit */
	cards_size = 0;
	
	for(i = 0; i < 4; i++) {
		for(j = 1; j <= 13; j++) {
			cards[cards_size++] = j;
		}
	}
	
	/* Create the players */
	player1.id = 1;
	player1.card1 = -1;
	player1.card2 = -1;
	
	player2.id = 2;
	player2.card1 = -1;
	player2.card2 = -1;

	player3.id = 3;
	player3.card1 = -1;
	player3.card2 = -1;
	
	/* open file for logging */
	file = fopen("log.txt", "w");
		
	/* Play 3 rounds */
	for(round = 0; round < 3; round++) {
		fprintf(file, "ROUND %d\n", (round + 1));
		printf("ROUND %d\n", (round + 1));
		
		/* Initialize the semaphores, we make the players wait for the dealer at start */
		sem_init(&dealer_semaphore, 0, 0);
		
		/* Start the player threads */
		pthread_create(&player1_thread, NULL, &handle_player_thread, &player1);
		pthread_create(&player2_thread, NULL, &handle_player_thread, &player2);
		pthread_create(&player3_thread, NULL, &handle_player_thread, &player3);
		
		/* Start the dealer thread */
		pthread_create(&dealer_thread, NULL, &handle_dealer_thread, NULL);
		
		/* Wait for everybody to finish, before starting next round */
		pthread_join(player1_thread, NULL);
		pthread_join(player2_thread, NULL);
		pthread_join(player3_thread, NULL);
		pthread_join(dealer_thread, NULL);
		
		fprintf(file, "\n");
		printf("\n");
	}
	
	fclose(file);
		
	return 0;
}

