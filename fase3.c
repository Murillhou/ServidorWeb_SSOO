#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <errno.h>

#include "param.h"
#include "http.h"

void atender_cliente ( ConexionHTTP* cliente );
void atender_peticion ( ConexionHTTP* cliente, Peticion* peticion );
void procesar_cgi ( ConexionHTTP* cliente, Peticion* peticion, char* nombre_fichero );
void enviar_recurso ( ConexionHTTP* cliente, Peticion* peticion, char* nombre_fichero);

/* VARIABLES GLOBALES */
ParametrosServidor param;
int i = 0;
int j = 0;
int k = 0;
int p = 0;
int term = 0;
int term2 = 0;
int termMain = 0;
int alrm = 0;
int alrm2 = 0;
int empieza_hijo = 0;
int inactivos = 0; /* Procesos trabajador inactivos */

int estado_trabajador;
int error_trabajador;
int *pids;
pid_t pid_trabajador;
Peticion* peticion;
int estadoPeticion;
char nombre_fichero[MAXPATHLEN];
const char* CGI_EXT = ".cgi";
/* Paramateros del servidor */
char server_port[200];
char hostname[200];
char metodo[200];
char longitud[200];
char argumentos[200];
char prueba[200];
char tipocont[200];
int pini; 
int pmax;
int pinactmin;
int pinactmax;

/* tipo de dato booleano */
typedef int bool;
#define true 1
#define false 0
/* tipo de dato estado */
typedef int estado;
#define nousado 0
#define inactivo 1
#define activo 2
/* Dirección de memoria compartida */
void *memCompartida; 
/* Tipo de datos que se almacenará en la memoria compartida */
typedef struct { 
	estado est;
	int pid;
} EstadoTrabajadores;
/* Variable que nos permite manejar comodamente los datos de la memoria compartida */
EstadoTrabajadores *Tabla;

sigset_t mask;
sigset_t mask2;
sigset_t mask3;
sigset_t orig_mask;
/* MANEJADORES DE SEÑAL */
struct sigaction act;
static void manejador_SIGCHLD (int sig, siginfo_t *siginfo, void *context){
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador de sigchld por la terminación del proceso %i++++++++++++++++++++\n",siginfo->si_pid );
	for(i=0;i<pmax;i++){
		if(Tabla[i].pid == siginfo->si_pid){
			fprintf( stderr, "\n +++++++++++++++++se ha marcado el proceso %i, posicion %i, como no usado++++++++++++++++++++\n",siginfo->si_pid, i );
			Tabla[i].est = nousado;
			Tabla[i].pid = 0;
			p--;
		}	
	}
}
void manejador_SIGTERM(int sig){
	termMain = 1;
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGTERM (termMain a 1)++++++++++++++++++++\n" );
	
}
void manejador_SIGINT(int sig){
	termMain = 1;
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGINT (termMain a 1)++++++++++++++++++++\n" );
}
void manejador_SIGUSR1(int sig){
	empieza_hijo = 1;
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGUSR1 (empieza_hijo a 1)++++++++++++++++++++\n" );
}
void manejador_SIGCHLD2(int sig)	{
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SICGCHLD2 (VACIO)++++++++++++++++++++\n" );		
}
void manejador_SIGINT2(int sig)	{
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGINT2 (VACIO)++++++++++++++++++++\n" );
}
void manejador_SIGTERM2(int sig)	{
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGTERM2 (term a 1)++++++++++++++++++++\n" );
	term = 1;
}
void manejador_SIGALRM(int sig)	{
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGALRM (alrm a 1)++++++++++++++++++++\n" );
	alrm = 1;
}
void manejador_SIGALRM2(int sig)	{
	fprintf( stderr, "\n +++++++++++++++++se ha ejecutado el manejador SIGALRM2 (alrm2 a 1, alrm a 0)++++++++++++++++++++\n" );
	alrm2 = 1;
	alrm = 0;
}

/* PROGRAMA PRINCIPAL */
int main(int argc, char** argv)
{	
	GestorHTTP * gHttp;
	ConexionHTTP* cliente;	
	bool maestro = 1;
	
	/* Procesar los parámetros de configuración */
	{
	analizar_linea_mandatos ( argc, argv, &param );
	if ( param.nivel_depuracion > 0 )
		fprintf ( stderr, "\nPuerto del Servidor:: %d\n",param.puerto_escucha );
	pini = param.num_inicial_trabajadores;
	pmax = param.num_max_trabajadores;
	pinactmax = param.num_max_trabajadores_ociosos;
	pinactmin = param.num_min_trabajadores_ociosos;
	fprintf( stderr, "\n PINI: %i, PMAX: %i, IMAX: %i, IMIN: %i \n", pini, pmax, pinactmax, pinactmin);
	}
	/* Crear gestorHTTP */
	{
	if ( (gHttp = http_crear ( param.puerto_escucha ) ) == NULL) {
		fprintf( stderr, "\n....ERROR: No se ha podido crear el gestor\n" );
		exit(1);
	}
	}
	/* Crear tabla de trabajadores en memoria compartida */
	{
	if ( (memCompartida = mmap ( NULL, (sizeof(EstadoTrabajadores))*pmax, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0) ) == MAP_FAILED) {
		perror ("Error al crear la Tabla de Trabajadores en memoria compartida");
		exit(-1);
	}
	Tabla = memCompartida;
	}
	/* Marcar todos los trabajadores como no usados */
	for(j=0; j<pmax; j++){
		Tabla[j].est = nousado;
	}
	/* Crear "Pini" trabajadores */
	for(j=0; (j<pini) && (maestro==1); j++){
		sigemptyset (&mask);
		sigaddset (&mask, SIGCHLD);
		/* bloquea SIGCHLD temporalmente, y guarda la mascara original */
		if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
			perror ("error en sigprocmask\n");
		}
		switch(pid_trabajador=fork()){
			case(pid_t)-1:
				j--;
				perror("error en fork\n");
				_exit(1);
				break;
			case(pid_t)0:
				maestro = 0;
			/*	fprintf( stderr, "\n (trabajador %i) SE HA CREADO EL PROCESO TRABAJADOR \n",getpid() );*/
				break;
			default:
				/* Se rellena la entrada de la tabla del proceso recien creado */
				p++;
				Tabla[j].est = inactivo;
				Tabla[j].pid = pid_trabajador;
			/*	fprintf( stderr, "\n (maestro) HE CREADO EL PROCESO TRABAJADOR CON PID %i, posicion %i, estado %i, \n",Tabla[j].pid,j, Tabla[j].est);*/
				/* restaura la mascara original */
				if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
					perror ("error en sigprocmask");
				}
				break;
		}
	}
	
	/* 
	 *
	 *
	 * Si el proceso es el maestro */
	if(maestro==1)
	{	
		/* Bucle para regular numero de trabajadores inactivos */
		do{	
			sleep(1);
			/* Contar procesos inactivos */
			inactivos = 0;
			for(j=0;j<pmax;j++){
				if((Tabla[j].est == inactivo)){
					inactivos++;
					/*fprintf ( stderr, "\n PROCESO INACTIVO CON PID %i, POSICION %i.Nº PROC: %i, Nº PROC INACT: %i\n", Tabla[j].pid, j, p, inactivos); */
				}
			}
			/* fprintf ( stderr, "\n NUMERO DE PROCESOS INACTIVOS %i.",inactivos); */
			
			/* Crear o destruir procesos trabajadores */
			if((inactivos > pinactmax)&&(pinactmax > pinactmin)){
				fprintf( stderr, "\n (maestro) NUMERO DE PROCESOS INACTIVOS SUPERIOR AL MAXIMO: %i", inactivos);
				
				sigemptyset (&mask);
				sigaddset (&mask, SIGCHLD);
				/* bloquea SIGCHLD temporalmente, y guarda la mascara original */
				if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
					perror ("error en sigprocmask\n");
				}
				/* Envia la señal SIGTERM a (inactivos - pinactmax) trabajadores */
				pids = (int *)malloc (pmax*sizeof(int));
				j=0; k=0;
				for(j=0;(j<pmax)&&(k<(inactivos - pinactmax));j++){
					if(Tabla[j].est == inactivo){
						pids[k]=Tabla[j].pid;
						if(kill(Tabla[j].pid, SIGTERM)==-1){ 
							perror("error en kill");
							_exit(1);
						}else{
							fprintf ( stderr, "\n (maestro) SE HA ENVIADO SEÑAL SIGTERM AL PROCESO CON PID %i, EN LA POSICION %i\n", Tabla[j].pid, j);
							k++;
						}
					}
				}
				free(pids);
				j=0;
				/* restaura la mascara original */
				if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
					perror ("error en sigprocmask");
				}
				/* Espera a que finalicen los trabajadores a los que mando morir */
				for(j=0;j<(inactivos - pinactmax);j++){
					do{
						error_trabajador = waitpid(pids[j],&estado_trabajador,0);
					}while((error_trabajador==-1) && (errno==EINTR));
				}
			}else if((inactivos < pinactmin)&&(pinactmin < pinactmax)&&(p < pmax))
				{
				fprintf( stderr, "\n (maestro) NUMERO DE PROCESOS INACTIVOS INFERIOR AL MINIMO: %i", inactivos);
				
				sigemptyset (&mask);
				sigaddset (&mask, SIGCHLD);
				/* bloquea SIGCHLD temporalmente, y guarda la mascara original */
				if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
					perror ("error en sigprocmask\n");
				}
		
				/* crea (pinact - inactivos) procesos nuevos */
				for(j=0;(j<(pinactmin - inactivos))&&(maestro==1);j++){
					switch(pid_trabajador=fork()){
						case(pid_t)-1:
							j--;
							perror("error en fork\n");
							_exit(1);
							break;
						case(pid_t)0:
							maestro = 0;
							fprintf( stderr, "\n (trabajador %i) SE HA CREADO EL PROCESO TRABAJADOR \n",getpid() );
							break;
						default:
							/* Se asigna una entrada de la tabla al proceso recien creado */
							for(j=0;(j<pmax);j++){	
								if(Tabla[j].est == 0){
									Tabla[j].est = 1;
									Tabla[j].pid = pid_trabajador;
									fprintf( stderr, "\n (maestro) COMO CONSECUENCIA HE CREADO EL PROCESO TRABAJADOR CON PID %i, posicion %i, estado %i.\n", Tabla[j].pid, j, Tabla[j].est);
									p++;
									j=pmax;
								}
							}
							break;
					} /* Fin switch */
				} /* Fin for */		
				/* restaura la mascara original */
				if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
					perror ("error en sigprocmask");
				}
			} /* fin else if */
		
			/* Captura de señales maestro*/
			act.sa_sigaction = &manejador_SIGCHLD;
			act.sa_flags = SA_SIGINFO;
			if (sigaction(SIGCHLD, &act, NULL) < 0) {
				perror ("error en sigaction");
				_exit(1);
			}
			if(signal(SIGTERM, manejador_SIGTERM)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			if(signal(SIGINT, manejador_SIGINT)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
		/* Mientras no se le ordene morir ( y se trate del proceso maestro) */
		}while((termMain != 1) && (maestro == 1)); /* fin do-while */

		/* Para finalizar, enviar a todos los trabajadores la señal de terminación */
		if(maestro == 1){ /* Se comprueba de nuevo que es el maestro */
			j=0; k=0;
			for(j=0;j<pmax;j++){
				if(Tabla[j].est != 0){
					if(kill(Tabla[j].pid, SIGTERM)==-1){ 
						perror("error en kill");
						_exit(1);
					}else {
						fprintf ( stderr, "\n (maestro) PARA FINALIZAR SE HA ENVIADO SEÑAL SIGTERM AL PROCESO CON PID %i, EN LA POSICION %i\n", Tabla[j].pid, j);
						k++;
					}
				}
			}
			/* Esperar a que todos los trabajadores hayan terminado su ejecución */
			for(j=0;j<k;j++){
				do{
					error_trabajador = wait(&estado_trabajador);
				}while((error_trabajador==-1) && (errno==EINTR));
			}
			fprintf ( stderr, "\n (maestro) TODOS LOS PROCESOS TRABAJADORES HAN TERMINADO\n");	
			
			/* destrucción del gestor http */
			http_destruir ( gHttp );			
			return(0);
		} /* fin if */
	} /* fin if(maestro==1) */

	/*
	 *
	 * 
	 * Si es un proceso trabajador */	
	if(maestro == 0){
		do{
			if(signal(SIGTERM, manejador_SIGTERM2)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			if(signal(SIGINT, SIG_IGN)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			if(signal(SIGUSR1, manejador_SIGUSR1)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			if(signal(SIGCHLD, manejador_SIGCHLD2)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			if(signal(SIGALRM, manejador_SIGALRM)==SIG_ERR){
				perror("error en signal");
				_exit(1);
			}
			/* Esperar conexion de cliente */
			/*fprintf ( stderr, "\n (trabajador %i) EL TRABAJADOR ESTA ESPERANDO UNA CONEXIÓN ENTRANTE.\n", getpid() );*/
			
			if(http_esperar_conexion ( gHttp, &cliente, 0 ) == HTTP_OK){
				if ( param.nivel_depuracion > 0 ) fprintf ( stderr, "\n (trabajador %i) Conexión de Cliente con IP %s establecida.\n",getpid(), http_obtener_ip_cliente ( cliente ) );
				/* Marcar mi posicion en la tabla de trabajadores como activo */
				for(i=0;i<pmax;i++){
					if(Tabla[i].pid == getpid()){
						Tabla[i].est = activo;
						fprintf ( stderr, "\n (trabajador %i) EL PROCESO TRABAJADOR EN LA POSICION %i, HA SIDO MARCADO COMO ACTIVO\n", getpid(), i);
						i=pmax;
					}
				}
				alarm(300);
				/* Repetir */
				do{
					if(signal(SIGTERM, manejador_SIGTERM2)==SIG_ERR){
						perror("error en signal");
						_exit(1);
					}
					term2 = 0;
					/* Leer peticion */
					estadoPeticion = http_leer_peticion ( cliente, &peticion, 1 );
					if(estadoPeticion == HTTP_OK){
						if ( param.nivel_depuracion > 0 )fprintf ( stderr, " (trabajador %i) Petición: IP=%s, RECURSO: %s\n",getpid(), peticion->ip_cliente, peticion->fichero );
						if ( strstr ( peticion->fichero, "/../" ) || ! strncmp ( peticion->fichero, "../", 3 ) ){
							http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );
							if ( param.nivel_depuracion > 0 )fprintf ( stderr, "\nTRABAJADOR PID %i: ... PETICION_ERROR: fichero no autorizado:: %s\n", getpid(), peticion->fichero);
							term2 = 1;
						}
						/* composición del path físico como concatenación de directorio_base y nombre del recurso */
						if ((term2 != 1) && (term != 1) && (strlen ( param.directorio_base ) + strlen ( peticion->fichero ) >= MAXPATHLEN )){
							http_enviar_codigo ( cliente, peticion, 500, "Nombre de recurso demasiado largo" );
							if ( param.nivel_depuracion > 0 )fprintf ( stderr, "\nTRABAJADOR PID %i: ... PETICION_ERROR: nombre de recurso demasiado largo: %s\n", getpid(), peticion->fichero);
							term2 = 1;
							}
							strcpy ( nombre_fichero, param.directorio_base );
							strcat ( nombre_fichero, peticion->fichero );
							/* procesar solicitud */
							if (strcmp ( peticion->fichero + strlen(peticion->fichero) - strlen ( CGI_EXT ), CGI_EXT )){
								enviar_recurso ( cliente, peticion, nombre_fichero );
							}else{
								procesar_cgi( cliente, peticion, nombre_fichero );
							}
							http_destruir_peticion ( peticion );
					}
					else if( estadoPeticion == HTTP_CLIENTE_DESCONECTADO){
						if ( param.nivel_depuracion > 0 ) fprintf ( stderr, "\nTRABAJADOR PID %i: ....ERROR: Detectado cliente desconectado\n",getpid());
						term2 = 1;
					}
					else if ( estadoPeticion == HTTP_ERROR){
						fprintf ( stderr, "\n TRABAJADOR PID %i: ...ERROR: Error en http_leer_peticion\n",getpid());
						term2 = 1;
					}
				}while((term2 != 1)&&(term != 1)&&(alrm==0)); 
				/* Hasta que el cliente se desconecte o se le ordene morir */
				
			/*	fprintf ( stderr, "\n (trabajador %i) EL PROCESO TRABAJADOR VA A PASAR A INACTIVO\n", getpid());*/
			
				/* Marcar mi posición en la tabla de trabajadores como INACTIVO */			
				for(i=0;i<pmax;i++){
					if(Tabla[i].pid == getpid()){
						Tabla[i].est = inactivo;
						fprintf ( stderr, "\n (trabajador %i) EL PROCESO TRABAJADOR EN LA POSICION %i, HA SIDO MARCADO COMO INACTIVO\n", Tabla[i].pid, i);
						i = pmax;
					}
				}
				/* cerrar conexión */
				http_cerrar_conexion ( cliente );
			}/* fin if(http_esperar_conexion...) */
			
		}while(term != 1); /* Hasta que el proceso reciba SIGTERM o SIGINT */	
	} /* fin if(maestro==0) */
	
	_exit(1);
} /* fin main() */

/*
 * Procesar el CGI solicitado si existe
 * 
 */
void procesar_cgi( ConexionHTTP* cliente, Peticion* peticion, char* nombre_fichero ){
	int estado_hijo;
	int error_hijo;
	pid_t pid_hijo;
	int tuberia_ph[2];
	int tuberia_hp[2];

	int fd;
	struct stat st;

	if(pipe(tuberia_ph)==-1){
		perror("error en pipe");
		_exit(1);
	}
	if(pipe(tuberia_hp)==-1){
		perror("error en pipe");
		_exit(1);
	}
		
	if ( stat ( nombre_fichero, &st ) == -1 ) {
		http_enviar_codigo ( cliente, peticion, 404, "No encontrado" );
		return;
	}
	
	if ( S_ISREG(st.st_mode) ) {
		if ( ( fd = open(nombre_fichero, O_RDONLY) ) == -1 ){
			http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );
			if ( param.nivel_depuracion > 0 )
				fprintf ( stderr, "...ERROR: Error en el acceso al recurso:: %s \n", nombre_fichero );
			return;
		}
		close(fd);		
		switch(pid_hijo=fork()){
			case(pid_t)-1:
				perror("error en fork\n");
				_exit(1);
				break;
			case(pid_t)0:
			/* proceso hijo */
				fprintf ( stderr, "\n EL HIJO SE EJECUTA \n");
				sigemptyset (&mask2);
				sigaddset (&mask2, SIGUSR1);
				while(empieza_hijo == 0){
						sigsuspend(&mask2);
				}
				fprintf ( stderr, "\n EL HIJO EMPIEZA \n");
				gethostname(hostname, 200); 
				fprintf ( stderr, "\n SE HA OBTENIDO EL HOSTNAME: %s \n",hostname);
				sprintf(server_port, "%d", param.puerto_escucha);
				fprintf ( stderr, "\n SE HA CONVERTIDO A STRING PUERTO: %s \n",server_port);				
				sprintf(metodo, "%d", peticion->metodo);
				fprintf ( stderr, "\n SE HA CONVERTIDO A STRING METODO: %s \n",metodo);
				sprintf(longitud, "%d", peticion->long_cuerpo);
				fprintf ( stderr, "\n SE HA CONVERTIDO A STRING LONGITUD: %s \n",longitud);
				if(peticion->argumentos != NULL){
					sprintf(argumentos, "%s", peticion->argumentos);
					fprintf ( stderr, "\n SE HA CONVERTIDO A STRING ARGUMENTOS: %s \n",argumentos);
				}
				fprintf ( stderr, "\n DIRECCIÓN REMOTA: %s \n",peticion->ip_cliente);
				fprintf ( stderr, "\n NOMBRE DEL SCRIPT: %s \n",peticion->fichero);
				fprintf ( stderr, "\n TIPO DE CONTENIDO: %s \n",peticion->content_type);
				
				if((setenv ("SERVER_NAME",hostname,1))==-1){
					perror("error estableciendo SERVER_NAME");
				}
				if((setenv ("GATEWAY_INTERFACE", "CGI/1.1",1))==-1){
					perror("error estableciendo GATEWAY_INTERFACE");
				}
				if((setenv ("SERVER_PORT", server_port, 1))==-1){
					perror("error estableciendo SERVER_PORT");
				}
				if((setenv ("SERVER_PROTOCOL", "HTTP/1.1", 1))==-1){
					perror("error estableciendo SERVER_PROTOCOL");
				}
				if((setenv ("REQUEST_METHOD", metodo, 1))==-1){
					perror("error estableciendo REQUEST_METHOD");
				}
				if((setenv ("SCRIPT_FILENAME", peticion->fichero, 1))==-1){
					perror("error estableciendo SCRIPT_FILENAME");
				}
				if(argumentos != NULL){
					if(setenv ("QUERY_STRING", argumentos, 1)==-1)
						perror("error estableciendo QUERY_STRING");
				}
				if((setenv ("REMOTE_ADDR", peticion->ip_cliente, 1))==-1){
					perror("error estableciendo REMOTE_ADDR");
				}			
				if(setenv ("CONTENT_TYPE", peticion->content_type, 1)==-1){
					perror("error estableciendo CONTENT_TYPE");
				}
				if(setenv ("CONTENT_LENGTH", longitud, 1)==-1){
					perror("error estableciendo CONTENT_LENGTH");
				}
				fprintf ( stderr, "\n SE HAN ESTABLECIDO LAS VARIABLES DE ENTORNO (HIJO) \n");
				close(tuberia_ph[0]);
				close(tuberia_hp[1]);
				dup2(tuberia_ph[1], STDIN_FILENO);
				dup2(tuberia_hp[0], STDOUT_FILENO);
				fprintf ( stderr, "\n SE VA A EJECUTAR EL CGI (HIJO) \n");
				execl(nombre_fichero, nombre_fichero, NULL);
				
				fprintf ( stderr, "\n EL CGI DEBIO DE EJECUTARSE MAL O NO EJECUTARSE (HIJO) \n");
				http_enviar_codigo ( cliente, peticion, 500, "Problema indeterminado (el hijo alcanzo el final)" );
				_exit(1);
				break;
				
			default:
			/* proceso padre */
				if(signal(SIGALRM, manejador_SIGALRM2)==SIG_ERR){
					perror("error en signal");
					_exit(1);
				}
				fprintf ( stderr, "\n(trabajador %i) EL PADRE SE EJECUTA.",getpid());
				close(tuberia_hp[0]);
				if((peticion->cuerpo) != NULL){	/* se escribe el cuerpo de la peticion en la tuberia_ph */
					if(write(tuberia_ph[0],peticion->cuerpo, strlen(peticion->cuerpo))!=(ssize_t)strlen(peticion->cuerpo)){
						perror("padre:error en write pipe");
						_exit(1);
					}
				}
				close(tuberia_ph[0]);
				if(kill(pid_hijo, SIGUSR1)==-1){ /* envío de señal al hijo para que ejecute la peticion */
					perror("error en kill");
					_exit(1);
				}
				fprintf ( stderr, "\n SEÑAL P->H ENVIADA (PADRE) \n");
				alrm2=0;
				alarm(60);
				do{
					/* espera de actividad en uno de sus hijos */
					error_hijo=wait(&estado_hijo);
					/* si se produce un error, y errno toma el valor EINTR, entonces se
					ha interrumpido a causa de una señal, y se relanza otra vez */
				}while((error_hijo==-1) && (errno==EINTR) && (alrm2 == 0));
				if (error_hijo != -1 ){	/* verifica si la actividad detectada es la finalización de algún hijo */
					fprintf ( stderr, "\n EL HIJO HA FINALIZADO (PADRE) \n");
					if(WIFEXITED(estado_hijo) || WIFSIGNALED(estado_hijo)){
						fprintf ( stderr, "\n SE VA A ENVIAR LA RESPUESTA (TUBERIA H-P) (PADRE PID %i) \n",getpid());
						if(http_enviar_respuesta ( cliente, peticion, tuberia_hp[1], 0) != HTTP_OK){
							fprintf ( stderr, "\n LA RESPUESTA ESTA VACÍA \n");
							http_enviar_codigo ( cliente, peticion, 500, "Problema indeterminado (no se pudo enviar la respuesta)" );
						}
						close(tuberia_hp[1]);
					}else{
						if(alrm2 == 1){
							if(kill(pid_hijo, SIGTERM)==-1){
								perror("error en kill");
								_exit(1);
							}else{
								do{
									error_hijo=wait(&estado_hijo);
								}while((error_hijo==-1) && (errno==EINTR));
				
							}
						}
						perror("error en la ejecución del CGI");
						_exit(1);
					}
				}else{
					perror("error en la ejecución del CGI");
					_exit(1);
				}
				break;
				/* fin del padre */
		}/* fin switch */
	}
	else if ( S_ISDIR(st.st_mode) ){
		if ( param.nivel_depuracion > 0 )
			fprintf ( stderr, "...INFO: Se ha solicitado un directorio:: %s \n", nombre_fichero );
	}
	else{
		if ( param.nivel_depuracion > 0 )
			fprintf ( stderr, "...ERROR: Se ha solicitado un recurso no permitido :: %s \n", nombre_fichero );
	
		http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );		
	}
}

/*
 * Enviar el recurso estático solicitado o el fichero por defecto
 * si se trata de un directorio (NO implementado)
 */
void enviar_recurso( ConexionHTTP* cliente, Peticion* peticion, char* nombre_fichero ){
	int fd;
	struct stat st;
	
	if ( stat ( nombre_fichero, &st ) == -1 ) {
		http_enviar_codigo ( cliente, peticion, 404, "No encontrado" );
		return;
	}
			
	if ( S_ISREG(st.st_mode) ) {
		if ( ( fd = open(nombre_fichero, O_RDONLY) ) == -1 ){
			http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );
			if ( param.nivel_depuracion > 0 )fprintf ( stderr, "...ERROR: Error en el acceso al recurso:: %s \n", nombre_fichero );
			return;
		}
		
		http_enviar_respuesta ( cliente, peticion, fd, 1 );
		close(fd);
		
		if ( param.nivel_depuracion > 0 )fprintf ( stderr, " Recurso %s enviado\n", nombre_fichero );
	}
	else if ( S_ISDIR(st.st_mode) ){
		if ( param.nivel_depuracion > 0 )fprintf ( stderr, "...INFO: Se ha solicitado un directorio:: %s \n", nombre_fichero );
		strcat( nombre_fichero, "index.html");
		if ( ( fd = open(nombre_fichero, O_RDONLY) ) == -1 ){
			http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );
			if ( param.nivel_depuracion > 0 )fprintf ( stderr, "...ERROR: Error en el acceso al recurso:: %s \n", nombre_fichero );
			return;
		}
		http_enviar_respuesta ( cliente, peticion, fd, 1 );
		close(fd);
		
		if ( param.nivel_depuracion > 0 )fprintf ( stderr, " Recurso %s enviado\n", nombre_fichero );
	}
	else{
		if ( param.nivel_depuracion > 0 )fprintf ( stderr, "...ERROR: Se ha solicitado un recurso no permitido :: %s \n", nombre_fichero );
		http_enviar_codigo ( cliente, peticion, 403, "No autorizado" );
	}
}

