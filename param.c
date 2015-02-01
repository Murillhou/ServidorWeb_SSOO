#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>
#include <string.h>
#include <sys/param.h>
#include "param.h"

static void imprimir_instrucciones_uso_y_salir ( char* nombre_programa, int status )
{
  fprintf(stderr, "OPCIONES DE USO:\n\n");
  fprintf(stderr, "-p[ort] PUERTO: puerto de escucha\n");
  fprintf(stderr, "[-b[ase] DIRECTORIO]: directorio base de las páginas web (longitud inferior a %d caracteres) [.]\n", MAXPATHLEN);
  fprintf(stderr, "[[-Pini|-I] NUMERO]: número inicial de trabajadores [3]\n");
  fprintf(stderr, "[[-Pmax|-M] NUMERO]: número máximo de trabajadores [20]\n");
  fprintf(stderr, "[-[Imi]n NUMERO]: número mínimo de trabajadores ociosos [2]\n");
  fprintf(stderr, "[[-Imax|-m] NUMERO]: número máximo de trabajadores ociosos [5]\n");
  fprintf(stderr, "[-h[elp]]: muestra esta información de uso\n");
  fprintf(stderr, "[-[debu]g [Nivel]]: muestra información de depuración[0]");
  fprintf(stderr, "\nTodos los argumentos, salvo el puerto, son opcionales.\nEntre corchetes se indica su valor por defecto.\n\n");
  fprintf(stderr, "Ejemplos de uso:\n\n");
  fprintf(stderr, "%s -p 80 -b /usr/local/www/data -I 3 -n 2 -m 5 -M 20 -g3\n", nombre_programa);
  fprintf(stderr, "%s -port 80 -base /usr/local/www/data -Pini 3 -Imin 2 -Imax 5 -Pmax 20 -debug=3\n\n\n", nombre_programa);
  exit ( status );
}

void analizar_linea_mandatos ( int argc, char** argv, ParametrosServidor* parametros )
{	static const struct option opciones[] = {
		{ "port", required_argument, NULL, 'p' },
		{ "base", required_argument, NULL, 'b' },
		{ "debug", optional_argument, NULL, 'g' },
		{ "Pini", required_argument, NULL, 'I' },
		{ "Pmax", required_argument, NULL, 'M' },
		{ "Imin", required_argument, NULL, 'n' },
		{ "Imax", required_argument, NULL, 'm' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 } };
	int c;
	char* fin;

	parametros->puerto_escucha = 0;
	parametros->directorio_base = ".";
	parametros->num_inicial_trabajadores = 3;
	parametros->num_max_trabajadores = 20;
	parametros->num_min_trabajadores_ociosos = 2;
	parametros->num_max_trabajadores_ociosos = 5;
	parametros->nivel_depuracion = 0;
	opterr = 0;
	while ( ( c = getopt_long_only ( argc, argv, "p:b:I:M:n:m:hg::", opciones, NULL ) ) != -1 )
	{	switch(c)
		{	case 'p':
				parametros->puerto_escucha = strtol ( optarg, &fin, 10 );
				if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				break;
			case 'b':
				parametros->directorio_base = optarg;
				break;
			case 'I':
				parametros->num_inicial_trabajadores = strtol ( optarg, &fin, 10 );
				if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				break;
			case 'M':
				parametros->num_max_trabajadores = strtol ( optarg, &fin, 10 );
				if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				break;
			case 'n':
				parametros->num_min_trabajadores_ociosos = strtol ( optarg, &fin, 10 );
				if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				break;
			case 'm':
				parametros->num_max_trabajadores_ociosos = strtol ( optarg, &fin, 10 );
				if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				break;
			case 'g':
				if ( optarg )
				{	parametros->nivel_depuracion = strtol ( optarg, &fin, 10 );
					if ( *fin != '\0' ) imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
				}else
					parametros->nivel_depuracion = 1;
				break;
			case 'h':
				imprimir_instrucciones_uso_y_salir ( argv[0], EX_OK );
			default:
				imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
		}
	}
	if ( parametros->puerto_escucha == 0
	  || strlen(parametros->directorio_base) >= MAXPATHLEN
	  || parametros->num_inicial_trabajadores > parametros->num_max_trabajadores
	  || parametros->num_min_trabajadores_ociosos > parametros->num_max_trabajadores_ociosos )
		imprimir_instrucciones_uso_y_salir ( argv[0], EX_USAGE );
}

