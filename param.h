/*
 * Módulo para la gestión de las opciones de ejecución
 */

#ifndef _PARAM_H_
#define _PARAM_H_

/* estructura de datos que contendrá los parámetros de configuración del servidor web */

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
 * analiza las opciones indicadas en la línea de mandatos y rellena los parámetros del servidor web
 */

extern void analizar_linea_mandatos ( int argc, char** argv, ParametrosServidor* parametros );

#endif
