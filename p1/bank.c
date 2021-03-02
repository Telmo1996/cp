/*Miembros del equipo: Telmo Fernández Corujo
						Anna Taboada Pardiñas*/
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
	int num_accounts;				// number of accounts
	int *accounts;					// balance array
	pthread_mutex_t * mutex_arr;	// mutex array
	pthread_cond_t *cond_retirada;	// array de condiciones
	int stop_retiradas;				// marca cuando dejar de esperar
};

struct args {
	int		thread_num;       // application defined thread #
	int		delay;			  // delay between operations
	int		iterations;       // number of operations
	int     net_total;        // total amount deposited by this thread
	int		total_retirado;	  // total retirado por el thread
	struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info {
	pthread_t    id;        // id returned by pthread_create()
	struct args *args;      // pointer to the arguments
};

// Threads run on this function
void *deposit(void *ptr)
{
	struct args *args =  ptr;
	int amount, account, balance;

	while(args->iterations--) {
		amount  = rand() % MAX_AMOUNT;
		account = rand() % args->bank->num_accounts;

		printf("Thread %d depositing %d on account %d\n",
			args->thread_num, amount, account);

		//lock
		pthread_mutex_lock(&args->bank->mutex_arr[account]);
		balance = args->bank->accounts[account];
		if(args->delay) usleep(args->delay); // Force a context switch

		balance += amount;
		if(args->delay) usleep(args->delay);

		args->bank->accounts[account] = balance;
		if(args->delay) usleep(args->delay);

		//unlock
		pthread_mutex_unlock(&args->bank->mutex_arr[account]);

		//Finaliza la sesión crítica
		args->net_total += amount;
	}
	return NULL;
}

void *transferencia (void *ptr){
	struct args *args = ptr;
	int amount=0, acc1, acc2;
	int acc1balance, acc2balance;

	while(args->iterations--){
		acc1 = rand() % args->bank->num_accounts;
		acc2 = rand() % args->bank->num_accounts;
		while(acc1 == acc2)
			acc2 = rand() % args->bank->num_accounts;
		
		//Lock de los threads en orden para evitar interbloqueo
		if(&args->bank->mutex_arr[acc1]<&args->bank->mutex_arr[acc2]){
			pthread_mutex_lock(&args->bank->mutex_arr[acc1]);
			pthread_mutex_lock(&args->bank->mutex_arr[acc2]);
		}else{
			pthread_mutex_lock(&args->bank->mutex_arr[acc2]);
			pthread_mutex_lock(&args->bank->mutex_arr[acc1]);
		}

		if(args->bank->accounts[acc1] != 0)
			amount = rand() % args->bank->accounts[acc1];
		
		printf("Thread %d transfering %d from account %d to account %d\n",
			args->thread_num, amount,acc1, acc2);

		
		acc1balance = args->bank->accounts[acc1];
		acc2balance = args->bank->accounts[acc2];
		if(args->delay) usleep(args->delay); // Force a context switch

		acc1balance -= amount;
		acc2balance += amount;
		if(args->delay) usleep(args->delay); // Force a context switch

		args->bank->accounts[acc1] = acc1balance;
		args->bank->accounts[acc2] = acc2balance;
		if(args->delay) usleep(args->delay); // Force a context switch

		pthread_mutex_unlock(&args->bank->mutex_arr[acc1]);
		pthread_mutex_unlock(&args->bank->mutex_arr[acc2]);
	
	}

	return NULL;
}

void *retirada(void *ptr){
	struct args *args = ptr;
	int amount, acc, balance;

	amount = rand() % MAX_AMOUNT;
	acc = rand() % args->bank->num_accounts;

	pthread_mutex_lock(&args->bank->mutex_arr[acc]);

	while((balance = args->bank->accounts[acc]) < amount){
		pthread_cond_wait(&args->bank->cond_retirada[acc],
			&args->bank->mutex_arr[acc]);
		if(args->bank->stop_retiradas){
			pthread_mutex_unlock(&args->bank->mutex_arr[acc]);
			return NULL;
		}
	}

	balance -= amount;
	if(args->delay) usleep(args->delay);

	args->bank->accounts[acc] = balance;
	if(args->delay) usleep(args->delay);

	//unlock
	pthread_mutex_unlock(&args->bank->mutex_arr[acc]);

	//Finaliza la sesión crítica
	args->total_retirado += amount;

	printf("Thread %d ha retirado %d de la cuenta %d\n",
		args->thread_num, amount, acc);
	
	return NULL;
}

// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(
	struct options opt, struct bank *bank, void* (*fun)(void*))
{
	int i;
	struct thread_info *threads;

	printf("creating %d threads\n", opt.num_threads);
	threads = malloc(sizeof(struct thread_info) * opt.num_threads);

	if (threads == NULL) {
		printf("Not enough memory\n");
		exit(1);
	}

	// Create num_thread threads running swap()
	for (i = 0; i < opt.num_threads; i++) {
		threads[i].args = malloc(sizeof(struct args));

		threads[i].args -> thread_num = i;
		threads[i].args -> net_total  = 0;
		threads[i].args -> total_retirado = 0;
		threads[i].args -> bank       = bank;
		threads[i].args -> delay      = opt.delay;
		threads[i].args -> iterations = opt.iterations;

		if (0 != pthread_create(&threads[i].id, NULL, fun, threads[i].args)) {
			printf("Could not create thread #%d", i);
			exit(1);
		}
	}

	return threads;
}

// Print the final balances of accounts and threads
void print_deposits(struct bank *bank, struct thread_info *thrs, int num_threads) {
	int total_deposits=0;
	printf("\nNet deposits by thread\n");

	for(int i=0; i < num_threads; i++) {
		printf("%d: %d\n", i, thrs[i].args->net_total);
		total_deposits += thrs[i].args->net_total;
	}
	printf("Total: %d\n", total_deposits);
}

void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
	int bank_total=0, total_reti=0;
    printf("\nAccount balance\n");
	for(int i=0; i < bank->num_accounts; i++) {
		printf("%d: %d\n", i, bank->accounts[i]);
		bank_total += bank->accounts[i];
	}
	printf("Total: %d\n", bank_total);

    printf("\nTotal retirado\n");
	for(int i=0; i < bank->num_accounts; i++) {
		printf("%d: %d\n", i, thrs[i].args->total_retirado);
		total_reti += thrs[i].args->total_retirado;
	}
	printf("Total: %d\n", total_reti);

}

/*// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);

	print_balances(bank, threads, opt.num_threads);

	for (int i = 0; i < opt.num_threads; i++){
		free(threads[i].args);
		pthread_mutex_destroy(&bank->mutex_arr[i]);
	}
	free(threads);
}*/

void wait_no_balance(struct options opt, struct bank *bank, struct thread_info *threads) {
	// Wait for the threads to finish
	for (int i = 0; i < opt.num_threads; i++)
		pthread_join(threads[i].id, NULL);

	for (int i = 0; i < opt.num_threads; i++){
		free(threads[i].args);
		pthread_mutex_destroy(&bank->mutex_arr[i]);
	}
	free(threads);
	/*free(bank->accounts);
	free(bank->mutex_arr);*/
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
	bank->num_accounts = num_accounts;
	bank->accounts     = malloc(bank->num_accounts * sizeof(int));
	bank->mutex_arr = malloc(bank->num_accounts * sizeof(pthread_mutex_t));
	bank->stop_retiradas = 0;
	bank->cond_retirada = malloc(bank->num_accounts * sizeof(pthread_cond_t));

	for(int i=0; i < bank->num_accounts; i++){
		bank->accounts[i] = 0;
	
		//inicializar los mutex
		if (pthread_mutex_init(&bank->mutex_arr[i], NULL) != 0){
			printf("\n mutex init failed\n");
			exit(1);
		}

		pthread_cond_init(&bank->cond_retirada[i], NULL);
	}
		
}

int main (int argc, char **argv)
{
	struct options      opt;
	struct bank         bank;
	struct thread_info *thrs;
	struct thread_info *thrs_trans;
	struct thread_info *thrs_reti;

	srand(time(NULL));

	// Default values for the options
	opt.num_threads  = 5;
	opt.num_accounts = 10;
	opt.iterations   = 100;
	opt.delay        = 10;

	read_options(argc, argv, &opt);

	init_accounts(&bank, opt.num_accounts);

	thrs = start_threads(opt, &bank, deposit);
	thrs_trans = start_threads(opt, &bank, transferencia);
	thrs_reti = start_threads(opt, &bank, retirada);

    wait_no_balance(opt, &bank, thrs);
    wait_no_balance(opt, &bank, thrs_trans);

	bank.stop_retiradas = 1;
	for(int i=0; i<bank.num_accounts; i++)
		pthread_cond_broadcast(&bank.cond_retirada[i]);

	wait_no_balance(opt, &bank, thrs_reti);

	print_deposits(&bank, thrs, opt.num_threads);
	print_balances(&bank, thrs_reti, opt.num_threads);

	return 0;
}
