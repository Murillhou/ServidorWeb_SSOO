/*********************************************************************
 * Módulo para la gestion de una conexion HTTP lado servidor y para el intercambio de datos 
 * con las aplicaciones clientes.
 *
 * Laboratorio de Sistemas Operativos.
 * Departamento de Ingeniería y Arquitecturas Telemáticas (DIATEL)
 *********************************************************************/


#ifndef _HTTP_H_
#define _HTTP_H_

/*
 * Estructura de datos que representa una petición 
 *
 *   - metodo: método usado por el cliente para hacer la petición (GET o POST).
 *   - ip_cliente: dirección IP del cliente que ha hecho esta petición.
 *   - fichero: nombre del fichero/directorio/CGI solicitado en la petición.
 *   - argumentos: argumentos de la petición (NULL si no tiene).
 *   - content_type: tipo de los datos asociados a la petición (NULL si no hay).
 *   - cuerpo: datos asociados a la petición (NULL si no hay).
 *   - long_cuerpo: tamaño en bytes de los datos asociados a la petición (0 si no hay).
 *   - extra: otros campos sin interés para el alumno.
 *
 * El alumno no debe modificar los campos de esta estructura.
 *
 * Para liberar una petición, use la función http_destruir_peticion()
 */

#define METODO_GET  1
#define METODO_POST 2

typedef struct {
  int metodo;
  char* ip_cliente;
  char* fichero;
  char* argumentos;
  char* accept;		/*añadido por jl para tener compatibilidad con internet explorer */
  char* content_type;
  void* cuerpo;
  unsigned int long_cuerpo;
  void* extra;
} Peticion;

/*
 * Estos tipos de datos se utilizan para representar de forma opaca para
 * el usuario del módudulo la creación de un punto de escucha HTTP y sus
 * conexiones con aplicaciones cliente. Su contenido no es de interés para
 * el alumno.
 */
typedef void GestorHTTP;
typedef void ConexionHTTP;

/*Códigos que pueden devolver las funciones */
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
 *	gestorHttp: Gestor HTTP válido.
 * retorno:
 * 	descriptor válido o -1 en caso de error.
 */
int 
http_obtener_descriptor_escucha(GestorHTTP* gestorHttp);

/*
 * Espera a que se conecte un cliente al puerto asociado al GestorHTTP y devuelve una ConexiónHTTP
 * con el cliente.
 * entrada:
 * 	gestorHttp: GestorHTTP válido
 *	bloqueante: Tipo de espera de llegadas de conexiones. 1 espera bloqueante, 0 espera
 *		no bloqueante.
 * salida:
 *	cliente: Conexión con el cliente (solo si devuelve HTTP_OK).
 * retorno
 *	HTTP_OK si se ha conectado un cliente nuevo. En este caso habrá puesto sus datos en cliente.
 *	HTTP_SEGUIR_ESPERANDO si la espera no es bloqueante y no hay ningún cliente nuevo.
 *	HTTP_ESPERA_INTERRUMPIDA si se recibe una señal miestras espera la llegada de una solicitud
 *		de conexión.
 *	HTT_ERROR si se produce un error.	
 */
int 
http_esperar_conexion(GestorHTTP* gestorHttp, ConexionHTTP** cliente, int bloqueante);


/*
 * Devuelve el descriptor asociado al cliente
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 * retorno:
 * 	descriptor válido o -1 en caso de error.
 */
int 
http_obtener_descriptor_conexion(ConexionHTTP* cliente);


/*
 * Devuelve la dirección IP del cliente asociado a esta conexión
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 * retorno:
 * 	dirección IP o NULL en caso de error.
 */
char* 
http_obtener_ip_cliente(ConexionHTTP* conexion);

/*
 * Lee una petición en modo bloqueante o no bloqueane de la conexión especificada.
 * entrada:
 *	cliente: Conexión con el cliente.
 *	bloqueante: 1 en caso de espera bloqueane y 0 en espera no bloqueane.
 * salida: 
 *	peticion: Estructura con los datos de la peticion realizada por el cliente.
 * retorno:
 *	Pueden pasar varias cosas:
 *   1. Se ha leído una petición. En ese caso devuelve HTTP_OK
 *      y la petición en cuestión en el parámetro petición. La memoria de la estructura la crea la función
 *   2. Se han leído algunos datos, pero aún no se tiene una petición completa.
 *      En ese caso devuelve HTTP_SEGUIR_ESPERANDO y no la petición.
 *   3. Se detecta que el cliente se ha desconectado. En ese caso devuelve
 *      HTTP_CLIENTE_DESCONECTADO y no la petición.
 *   4. Se ha interrumpido la lectura por una señal. En ese caso se devuelve
 *      HTTP_ESPERA_INTERRUMPIDA y no la petición.
 *   5. Cualquier otra situación devuelve HTTP_ERROR.
 */
int 
http_leer_peticion(ConexionHTTP* cliente, Peticion **peticion, int bloqueante);


/*
 * Destruye la estructura de datos Peticion y libera todos los
 * recursos asociados a ella. 
 * entrada:
 *	peticion: Peticion recibida del cliente.
 * retorno 
 * 	HTTP_ERROR si se produce un error en cuyo caso la petición se queda en un estado indeterminado.
 * 	HTTP_OK en caso de éxito.
 */
int 
http_destruir_peticion(Peticion* peticion);



/*********************************************************************
 * A continuación se presentan tres funciones alternativas para enviar
 * respuestas al cliente. Son mutuamente excluyentes, por lo que a una
 * misma petición sólo se puede responder con una de estas funciones.
 *********************************************************************/

/*
 * Envíar los datos leídos del descriptor indicado como respuesta a una cierta
 * petición de un cliente, dando por terminada dicha petición.
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 *	petición: Petición recibida.
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
 * Envío de un código de estado y un mensaje aclarativo al cliente, dando por terminada
 * la petición. Normalmente se usará para informar sobre un error.
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 *	petición: Petición recibida.
 *	codigo: codigo de estado.
 *	mensaje: mensaje explicativo del código de estado.
 * retorno:
 *	HTTP_OK si todo va bien.
 *	HTTP_CLIENTE_DESCONECTADO si el cliente se ha desconectado abruptamente.
 */
int 
http_enviar_codigo(ConexionHTTP* cliente, Peticion* peticion, int codigo, char* mensaje);

/*
 * Envío de código HTML al cliente. Se puede invocar repetidamente a esta
 * función para generar poco a poco una respuesta a un cliente. Cuando se quiera dar por terminada
 * la respuesta habrá que invocar a esta función pasándole un puntero nulo como datos en el argumento
 * html.
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 *	petición: Petición recibida.
 *	html: codigo de estado.
 * retorno:
 *	HTTP_OK si todo va bien.
 *	HTTP_CLIENTE_DESCONECTADO si el cliente se ha desconectado abruptamente.
 */
int 
http_enviar_html(ConexionHTTP* cliente, Peticion* peticion, char* html);

/*
 * Cierra la conexión con el cliente y libera todos los recursos asociados
 * a ella.
 * entrada:
 *	cliente: Conexión establecida con el cliente.
 * retorno:
 *	HTTP_OK si todo va bien.
 * 	HTTP_ERROR si hay problemas, en cuyo caso la conexión queda
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
