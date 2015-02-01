/*********************************************************************
 * M�dulo para la gestion de una conexion HTTP lado servidor y para el intercambio de datos 
 * con las aplicaciones clientes.
 *
 * Laboratorio de Sistemas Operativos.
 * Departamento de Ingenier�a y Arquitecturas Telem�ticas (DIATEL)
 *********************************************************************/


#ifndef _HTTP_H_
#define _HTTP_H_

/*
 * Estructura de datos que representa una petici�n 
 *
 *   - metodo: m�todo usado por el cliente para hacer la petici�n (GET o POST).
 *   - ip_cliente: direcci�n IP del cliente que ha hecho esta petici�n.
 *   - fichero: nombre del fichero/directorio/CGI solicitado en la petici�n.
 *   - argumentos: argumentos de la petici�n (NULL si no tiene).
 *   - content_type: tipo de los datos asociados a la petici�n (NULL si no hay).
 *   - cuerpo: datos asociados a la petici�n (NULL si no hay).
 *   - long_cuerpo: tama�o en bytes de los datos asociados a la petici�n (0 si no hay).
 *   - extra: otros campos sin inter�s para el alumno.
 *
 * El alumno no debe modificar los campos de esta estructura.
 *
 * Para liberar una petici�n, use la funci�n http_destruir_peticion()
 */

#define METODO_GET  1
#define METODO_POST 2

typedef struct {
  int metodo;
  char* ip_cliente;
  char* fichero;
  char* argumentos;
  char* accept;		/*a�adido por jl para tener compatibilidad con internet explorer */
  char* content_type;
  void* cuerpo;
  unsigned int long_cuerpo;
  void* extra;
} Peticion;

/*
 * Estos tipos de datos se utilizan para representar de forma opaca para
 * el usuario del m�dudulo la creaci�n de un punto de escucha HTTP y sus
 * conexiones con aplicaciones cliente. Su contenido no es de inter�s para
 * el alumno.
 */
typedef void GestorHTTP;
typedef void ConexionHTTP;

/*C�digos que pueden devolver las funciones */
#define HTTP_ERROR -1
#define HTTP_OK 0
#define HTTP_SEGUIR_ESPERANDO 1
#define HTTP_CLIENTE_DESCONECTADO 2
#define HTTP_ESPERA_INTERRUMPIDA 3

/*
 * Creamos un Gestor HTTP ligado al puerto indicado.
 * entrada:
 * 	puerto: puerto de escucha
 * retorno:
 * 	Devuelve un Gestor de HTTP o un valor igual a NULL en caso de error.
 */

GestorHTTP *
http_crear(int puerto);

/*
 * Devuelve el descriptor asociado al puerto de escucha del servidor
 * entrada:
 *	gestorHttp: Gestor HTTP v�lido.
 * retorno:
 * 	descriptor v�lido o -1 en caso de error.
 */
int 
http_obtener_descriptor_escucha(GestorHTTP* gestorHttp);

/*
 * Espera a que se conecte un cliente al puerto asociado al GestorHTTP y devuelve una Conexi�nHTTP
 * con el cliente.
 * entrada:
 * 	gestorHttp: GestorHTTP v�lido
 *	bloqueante: Tipo de espera de llegadas de conexiones. 1 espera bloqueante, 0 espera
 *		no bloqueante.
 * salida:
 *	cliente: Conexi�n con el cliente (solo si devuelve HTTP_OK).
 * retorno
 *	HTTP_OK si se ha conectado un cliente nuevo. En este caso habr� puesto sus datos en cliente.
 *	HTTP_SEGUIR_ESPERANDO si la espera no es bloqueante y no hay ning�n cliente nuevo.
 *	HTTP_ESPERA_INTERRUMPIDA si se recibe una se�al miestras espera la llegada de una solicitud
 *		de conexi�n.
 *	HTT_ERROR si se produce un error.	
 */
int 
http_esperar_conexion(GestorHTTP* gestorHttp, ConexionHTTP** cliente, int bloqueante);


/*
 * Devuelve el descriptor asociado al cliente
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 * retorno:
 * 	descriptor v�lido o -1 en caso de error.
 */
int 
http_obtener_descriptor_conexion(ConexionHTTP* cliente);


/*
 * Devuelve la direcci�n IP del cliente asociado a esta conexi�n
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 * retorno:
 * 	direcci�n IP o NULL en caso de error.
 */
char* 
http_obtener_ip_cliente(ConexionHTTP* conexion);

/*
 * Lee una petici�n en modo bloqueante o no bloqueane de la conexi�n especificada.
 * entrada:
 *	cliente: Conexi�n con el cliente.
 *	bloqueante: 1 en caso de espera bloqueane y 0 en espera no bloqueane.
 * salida: 
 *	peticion: Estructura con los datos de la peticion realizada por el cliente.
 * retorno:
 *	Pueden pasar varias cosas:
 *   1. Se ha le�do una petici�n. En ese caso devuelve HTTP_OK
 *      y la petici�n en cuesti�n en el par�metro petici�n. La memoria de la estructura la crea la funci�n
 *   2. Se han le�do algunos datos, pero a�n no se tiene una petici�n completa.
 *      En ese caso devuelve HTTP_SEGUIR_ESPERANDO y no la petici�n.
 *   3. Se detecta que el cliente se ha desconectado. En ese caso devuelve
 *      HTTP_CLIENTE_DESCONECTADO y no la petici�n.
 *   4. Se ha interrumpido la lectura por una se�al. En ese caso se devuelve
 *      HTTP_ESPERA_INTERRUMPIDA y no la petici�n.
 *   5. Cualquier otra situaci�n devuelve HTTP_ERROR.
 */
int 
http_leer_peticion(ConexionHTTP* cliente, Peticion **peticion, int bloqueante);


/*
 * Destruye la estructura de datos Peticion y libera todos los
 * recursos asociados a ella. 
 * entrada:
 *	peticion: Peticion recibida del cliente.
 * retorno 
 * 	HTTP_ERROR si se produce un error en cuyo caso la petici�n se queda en un estado indeterminado.
 * 	HTTP_OK en caso de �xito.
 */
int 
http_destruir_peticion(Peticion* peticion);



/*********************************************************************
 * A continuaci�n se presentan tres funciones alternativas para enviar
 * respuestas al cliente. Son mutuamente excluyentes, por lo que a una
 * misma petici�n s�lo se puede responder con una de estas funciones.
 *********************************************************************/

/*
 * Env�ar los datos le�dos del descriptor indicado como respuesta a una cierta
 * petici�n de un cliente, dando por terminada dicha petici�n.
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 *	petici�n: Petici�n recibida.
 *	descriptor: descriptor de recurso.
 *	es_fichero: 1 en caso de que el descriptor sea un fichero, 0 otra cosa.
 * retorno:
 *	HTTP_OK si todo va bien.
 *	HTTP_CLIENTE_DESCONECTADO si el cliente se ha desconectado abruptamente.
 * 	HTTP_ERROR en caso de otros problemas (por ejemplo, error al leer
 * 	 	el fichero, cierre abrupto del descriptor, etc.).
 */
int 
http_enviar_respuesta(ConexionHTTP* cliente, Peticion* peticion, int descriptor, int es_fichero);

/*
 * Env�o de un c�digo de estado y un mensaje aclarativo al cliente, dando por terminada
 * la petici�n. Normalmente se usar� para informar sobre un error.
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 *	petici�n: Petici�n recibida.
 *	codigo: codigo de estado.
 *	mensaje: mensaje explicativo del c�digo de estado.
 * retorno:
 *	HTTP_OK si todo va bien.
 *	HTTP_CLIENTE_DESCONECTADO si el cliente se ha desconectado abruptamente.
 */
int 
http_enviar_codigo(ConexionHTTP* cliente, Peticion* peticion, int codigo, char* mensaje);

/*
 * Env�o de c�digo HTML al cliente. Se puede invocar repetidamente a esta
 * funci�n para generar poco a poco una respuesta a un cliente. Cuando se quiera dar por terminada
 * la respuesta habr� que invocar a esta funci�n pas�ndole un puntero nulo como datos en el argumento
 * html.
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 *	petici�n: Petici�n recibida.
 *	html: codigo de estado.
 * retorno:
 *	HTTP_OK si todo va bien.
 *	HTTP_CLIENTE_DESCONECTADO si el cliente se ha desconectado abruptamente.
 */
int 
http_enviar_html(ConexionHTTP* cliente, Peticion* peticion, char* html);

/*
 * Cierra la conexi�n con el cliente y libera todos los recursos asociados
 * a ella.
 * entrada:
 *	cliente: Conexi�n establecida con el cliente.
 * retorno:
 *	HTTP_OK si todo va bien.
 * 	HTTP_ERROR si hay problemas, en cuyo caso la conexi�n queda
 * 		en un estado indeterminado.
 */
int 
http_cerrar_conexion(ConexionHTTP* cliente);

/*
 * Cierra el punto de escucha y libera los recursos.
 * entrada:
 *	gestorHttp: Gestor de HTTP.
 * retorno:
 *	HTTP_OK si todo va bien.
 * 	HTTP_ERROR si hay problemas en cuyo caso el gestor queda en un estado indeterminado.
 */
int 
http_destruir(GestorHTTP* gestorHttp);

#endif
