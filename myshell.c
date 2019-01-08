#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "parser.h"




int main(void) {
	char buf[1024]; /*string que introduce el usuario*/
	tline * line; /*informacion del string*/
	int i, j, jA, jB, k; /*contadores*/
	pid_t pid; /*pid del proceso (orden)*/
	int procB; /*num de linea de procesos en background*/
	pid_t **listBackground = (pid_t **)malloc(100 * sizeof(pid_t *));/*lista de las lineas de procesos en background */
	pid_t ppid; /*pid del padre*/
	char dir[1024]; /*direccion para el comando cd, etc.*/
	char dir_in[1024]; /*redireccion input*/
	char dir_out[1024]; /*redireccion output*/
	char dir_err[1024]; /*redireccion error*/
	FILE *file_in;
	FILE *file_out;
	FILE *file_err;
	int **p; /*lista de los pipes*/
	

	/*SHELL INTERFACE*/
	printf("msh> ");
	signal(SIGINT, SIG_IGN); /*ignorar ctrl+C*/
	signal(SIGQUIT, SIG_IGN); /*ignorar ctrl+\*/


	procB = 0;

	while (fgets(buf, 1024, stdin)) {

		if (strcmp(buf, "\n") == 0) {/*si es un salto de linea no hace nada*/
			printf("msh> ");
			continue;
		}

		line = tokenize(buf); /*extraer informacion del string recibido*/

		/*para los comandos que no tienen ruta*/
		/*comando cd*/
		if (strcmp(line->commands[0].argv[0], "cd") == 0) {

			if (line->commands[0].argc == 1) {
				strcpy(dir, getenv("HOME")); /*ruta de defecto*/
			}
			else {
				strcpy(dir, line->commands[0].argv[1]); /*ruta que ha introducido el usuario*/
			}

			if (chdir(dir) != 0) {
				fprintf(stderr, "Error en ejecuión del cd: %s\n", strerror(errno));
			}
		/*comando exit*/
		} else if (strcmp(line->commands[0].argv[0], "exit") == 0) {
			exit(0);
		/*no existe el comando*/
		} else if (line->commands[0].filename == NULL) { /*no encuentra la ruta del primer orden (no existe)*/
			fprintf(stderr, "%s: No se encuentra <<%s>>\n", line->commands[0].argv[0],line->commands[0].argv[0]);
			printf("msh> ");
			continue;
		}



		if (line->background) { /* comando & */
			listBackground[procB] = (pid_t *) malloc ((line->ncommands) * sizeof(pid_t));
			procB += 1;
		}

		/*en caso de que haya mas comandos, creo tanto pipes como (num de comando -1)*/
		if (line->ncommands > 1) {
			p = (int**)malloc((line->ncommands - 1) * sizeof(int*));
			for (i = 0; i < line->ncommands - 1; i++) {
				p[i] = (int *)malloc(2 * sizeof(int));
				pipe(p[i]);
			}
		}

		/*creacion de los procesos*/
		for (i = 0; i < line->ncommands; i++) {
			pid = fork();

			if (pid < 0) {
				fprintf(stderr, "Error en crear proceso: %s\n", strerror(errno));
				exit(1);
			} else if (pid == 0) { /*proceso hijo*/
				

				if (line->ncommands == 1) { /*un unico comando*/

					if (line->redirect_error != NULL) { /* comando >& fichero_error */
						strcpy(dir_err, line->redirect_error);
						file_err = fopen(dir_err, "w+"); /*abrir fichero en modo escritura, si no existe pues lo crea*/
						dup2(fileno(file_err), STDERR_FILENO);
					}

					if (line->redirect_input != NULL) { /* comando < fichero_entrada */
						strcpy(dir_in, line->redirect_input);
						file_in = fopen(dir_in, "r");
						if (file_in==NULL){
							fprintf(stderr, "%s: Error. %s\n", dir_in, strerror(errno));
							exit(1);
						} else {
							dup2(fileno(file_in), 0); /*redirigir la entrada estandar por el fd del file_in*/
						}
					}

					if (line->redirect_output != NULL) { /* comando < fichero_entrada */
						strcpy(dir_out, line->redirect_output);
						file_out = fopen(dir_out, "w+"); /*abrir fichero en modo escritura, si no existe pues lo crea*/
						dup2(fileno(file_out), 1); /*redirigir la entrada estandar por el fd del file_out*/
					}

					

					if (line->background) { /* comando & */
						listBackground[procB][i] = getpid();
					}

					execv(line->commands[i].filename, line->commands[i].argv); /*ejecutar*/

				}

				if (line->ncommands > 1) { /*mas comandos*/
					
					if (i == 0) {/*primer orden*/
						if (line->background) { /* comando & */
							listBackground[procB][i] = getpid();
						}

						if (line->redirect_input != NULL) {
							strcpy(dir_in, line->redirect_input);
							file_in = fopen(dir_in, "r");
							if (file_in==NULL){
								fprintf(stderr, "%s: Error. %s\n", dir_in, strerror(errno));
								exit(1);
							} else {
								dup2(fileno(file_in), 0);
							}
						}
						close(p[0][0]); /*cierro entrada*/
						dup2(p[0][1], STDOUT_FILENO); /*redirigir salida*/
						for (j = 1; j < line->ncommands - 1; j++) { /*cerrar los pipes que no usa*/
							close(p[j][0]);
							close(p[j][1]);
						}

						execv(line->commands[0].filename, line->commands[0].argv);

					}
					else {/*el resto de los comandos*/
						if (line->background) { /* comando & */
							listBackground[procB][i] = getpid();
						}

						if (i == line->ncommands - 1) { /*el ultimo comando*/

							if (line->redirect_output != NULL) { /* comando < fichero_entrada */
								strcpy(dir_out, line->redirect_output);
								file_out = fopen(dir_out, "w+"); 
								dup2(fileno(file_out), STDOUT_FILENO); 
							}

							if (line->redirect_error != NULL) { /* comando >& fichero_error */
								strcpy(dir_err, line->redirect_error);
								file_err = fopen(dir_err, "w+");
								dup2(fileno(file_err), STDERR_FILENO);
							}

							for (jB = 0; jB < line->ncommands - 2; jB++) { /*cerrar los pipes que no usa*/
								close(p[jB][0]);
								close(p[jB][1]);
							}

							close(p[i-1][1]); /*cierro salida del pipe*/
							dup2(p[i-1][0], STDIN_FILENO); /*redirigir entrada*/

							execv(line->commands[i].filename, line->commands[i].argv);

						} else { /*comandos entremedios*/
						
							for (jA = 0; jA < line->ncommands - 1; jA++) { /*cerrar los pipes que no usa*/
								if ((jA == (i-1))||(jA==i)){
									continue;
								}
								close(p[jA][0]);
								close(p[jA][1]);
							}

							close(p[i-1][1]); /*cierro salida del anterior*/
							close(p[i][0]); /*cierro entrada del siguiente*/
							
							dup2(p[i][1], 1);/*redirigir salida del siguiente*/
							dup2(p[i-1][0], 0); /*redirigir entrada del anterior*/

							execv(line->commands[i].filename, line->commands[i].argv);

						}
					} /*fin resto de los comandos*/

				} /*fin mas comandos*/
			/*fin hijos*/
			}
		}

		/*proceso padre*/

		/*cerrar pipes*/
		for (k = 0; k < line->ncommands - 1; k++) {
			close(p[k][0]);
			close(p[k][1]);
		}


		/*esperar hijos*/
		if (line->background) {
			/*no hay que esperar*/		
		} else {
			for (k = 0; k < line->ncommands; k++) {
				wait(NULL);			
			}
		}


		/*liberar memoria de los punteros*/
		if (line->ncommands > 1) {
			for (k = 0; k < line->ncommands - 1; k++) {
				free(p[k]);
			}
			free(p);
		}

		if (line->background) { /* comando & */
			for (k = 0; k < 100; k++) {
				free(listBackground[procB]);
			}
			free(listBackground);
		}

		printf("msh> ");
	}/*FIN SHELL INTERFACE*/
	return 0;
}
