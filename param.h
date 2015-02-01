/*
 * M�dulo para la gesti�n de las opciones de ejecuci�n
 */

#ifndef _PARAM_H_
#define _PARAM_H_

/* estructura de datos que contendr� los par�metros de configuraci�n del servidor web */

typedef struct {
	int puerto_escucha;
	char* directorio_base;
	int num_inicial_trabajadores;
	int num_max_trabajadores;
	int num_min_trabajadores_ociosos;
	int num_max_trabajadores_ociosos;
	int nivel_depuracion;
} ParametrosServidor;


/*
 * analiza las opciones indicadas en la l�nea de mandatos y rellena los par�metros del servidor web
 */

extern void analizar_linea_mandatos ( int argc, char** argv, ParametrosServidor* parametros );

#endif
