/* Feel free to use this example code in any way
   you see fit (Public Domain) */

   #include <sys/types.h>
   #ifndef _WIN32
   #include <sys/select.h>
   #include <sys/socket.h>
   #else
   #include <winsock2.h>
   #endif
   
   #include <microhttpd.h>
   #include <stdio.h>
   #include <string.h>
   #include <stdlib.h>
   #include "jsmn.h"
   #include <netinet/in.h>
   #include <arpa/inet.h>
   
   #if defined(_MSC_VER) && _MSC_VER+0 <= 1800
   /* Substitution is OK while return value is not used */
   #define snprintf _snprintf
   #endif
   
   #define PORT            4545
   #define DAEMON	   4546
   #define POSTBUFFERSIZE  1024
   #define MAXNAMESIZE     1000
   #define MAXANSWERSIZE   1024
   
   #define GET             0
   #define POST            1
   char* ip = "127.0.0.1";
   
   struct connection_info_struct
   {
     int connectiontype;
     char *answerstring;
     struct MHD_PostProcessor *postprocessor;
   };


      
   static int send_page (struct MHD_Connection *connection, const char *page){
     int ret;
     struct MHD_Response *response;
       
     response =
       MHD_create_response_from_buffer (strlen (page), (void *) page,
                       MHD_RESPMEM_PERSISTENT);
     if (!response)
       return MHD_NO;
   
     ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
     MHD_destroy_response (response);
   
     return ret;
   }
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}
      
      
   static int iterate_post (void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                    const char *filename, const char *content_type,
                    const char *transfer_encoding, const char *data, uint64_t off,
                    size_t size){
     struct connection_info_struct *con_info = coninfo_cls;
     if (0 == strcmp (key, "json")){
       printf("%s: %s\n", key, data);
   	
	int i;
	int r;
	jsmn_parser p;
	jsmntok_t t[128]; /* We expect no more than 128 tokens */

	jsmn_init(&p);
	r = jsmn_parse(&p, data, strlen(data), t, sizeof(t)/sizeof(t[0]));
	if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
		return 1;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) {
		printf("Object expected\n");
		return 1;
	}
	char* solicitud = (char *)malloc(20*sizeof(char));
       //int tamano_contenido;
     
       /* Loop over all keys of the root object */
       for (i = 1; i < r; i++) {
         if (jsoneq(data, &t[i], "solicitud") == 0) {
           /* We may use strndup() to fetch string value */
           printf("- Solicitud: %.*s\n", t[i+1].end-t[i+1].start,
                    data + t[i+1].start);
           sprintf(solicitud, "%.*s", t[i+1].end-t[i+1].start, data + t[i+1].start);
           i++;
         }
}
	int client;
struct sockaddr_in direccion_cliente;
    memset(&direccion_cliente, 0, sizeof(direccion_cliente));
  direccion_cliente.sin_family = AF_INET;		
  direccion_cliente.sin_port = htons(DAEMON);	
  direccion_cliente.sin_addr.s_addr = inet_addr(ip);

  client = socket(((struct sockaddr *)&direccion_cliente)->sa_family, SOCK_STREAM, 0);
  if (client == -1)
  {
  	printf("Error al abrir el socket\n");
  	return -1;
  }
  printf("Abierto el socket para el cliente...\n");

  //Conectamos
  int conectar = connect(client, (struct sockaddr *)&direccion_cliente, sizeof(direccion_cliente));
  if (conectar != 0)
  {
  	printf("Error : No es posible conectar\n");
  	return 1;
  }
  printf("conectado...\n");
  
  //Enviamos la ruta del archivo para que el servidor lo busque
  send(client, solicitud, strlen(solicitud), 0);
  
  //Leemos la respuesta del servidor

       char *answerstring = (char *)malloc(1000000);
	recv(client, answerstring, POSTBUFFERSIZE, 0);
	printf("%s\n",answerstring);
       if (!answerstring)
         return MHD_NO;
       con_info->answerstring = answerstring;
   
       return MHD_NO;
     }
     return MHD_YES;
   }
   
   static void request_completed (void *cls, struct MHD_Connection *connection,
                     void **con_cls, enum MHD_RequestTerminationCode toe){
     struct connection_info_struct *con_info = *con_cls;
   
     if (NULL == con_info)
       return;
   
     if (con_info->connectiontype == POST)
       {
         MHD_destroy_post_processor (con_info->postprocessor);
         if (con_info->answerstring)
           free (con_info->answerstring);
       }
   
     free (con_info);
     *con_cls = NULL;
   }
      
static int answer_to_connection (void *cls, struct MHD_Connection *connection,
                            const char *url, const char *method,
                            const char *version, const char *upload_data,
                            size_t *upload_data_size, void **con_cls){
	char* jsonresp=malloc(10000*sizeof(char *));
     if (NULL == *con_cls){
       struct connection_info_struct *con_info;
       con_info = malloc (sizeof (struct connection_info_struct));
       if (NULL == con_info)
         return MHD_NO;
       con_info->answerstring = NULL;
       if (0 == strcmp (method, "POST")){
         con_info->postprocessor =
           MHD_create_post_processor (connection, POSTBUFFERSIZE,
                                     iterate_post, (void *) con_info);
         
         if (NULL == con_info->postprocessor){
           free (con_info);
           return MHD_NO;
         }
         con_info->connectiontype = POST;
       }
       else
         con_info->connectiontype = GET;
   
       *con_cls = (void *) con_info;
   
       return MHD_YES;
     }
   
     if (0 == strcmp(method, "GET") && 0 == strcmp(url, "/listar_dispositivos")){
       printf("Obteniendo lista de dispositivos conectados...\n");
	
       
	int client;
	struct sockaddr_in direccion_cliente;
    	memset(&direccion_cliente, 0, sizeof(direccion_cliente));
  	direccion_cliente.sin_family = AF_INET;		
  	direccion_cliente.sin_port = htons(DAEMON);	
  	direccion_cliente.sin_addr.s_addr = inet_addr(ip);

	  client = socket(((struct sockaddr *)&direccion_cliente)->sa_family, SOCK_STREAM, 0);
	  if (client == -1)
	  {
	  	printf("Error al abrir el socket\n");
	  	return -1;
	  }
	  printf("Abierto el socket para el cliente...\n");

	  //Conectamos
	  int conectar = connect(client, (struct sockaddr *)&direccion_cliente, sizeof(direccion_cliente));
	  if (conectar != 0)
	  {
	  	printf("Error : No es posible conectar\n");
	  	return 1;
	  }
	  printf("conectado...\n");
	  char* solicitud ="listar_dispositivos";
	  //Enviamos la ruta del archivo para que el servidor lo busque
	  send(client, solicitud, strlen(solicitud), 0);
	  
	  //Leemos la respuesta del servidor

	       char *answerstring = (char *)malloc(1000000);
		recv(client, answerstring, POSTBUFFERSIZE, 0);
		printf("%s\n",answerstring);

	       sprintf(jsonresp,"{\"solicitud\": \"listar_dispositivos\", \"dispositivos\": [%s ], \n"
					"\"status\": \"0\", \"str_error\" : 0}",answerstring);
	       
	       return send_page (connection, jsonresp);
	}
     else if (0 == strcmp(method, "POST")){
       struct connection_info_struct *con_info = *con_cls;
         
       if (*upload_data_size != 0){
         printf("Uploaded data: %s\n",upload_data);
         MHD_post_process (con_info->postprocessor, upload_data,
                           *upload_data_size);
         *upload_data_size = 0;
   
         return MHD_YES;
       }
       else if (NULL != con_info->answerstring)
         return send_page (connection, con_info->answerstring);
     }
     printf("Solicitud no se puede procesar...\n");
     return send_page (connection, "Metodo no existente");
}
int main (){
     struct MHD_Daemon *daemon;
   
     daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
                               &answer_to_connection, NULL,
                               MHD_OPTION_NOTIFY_COMPLETED, request_completed,
                               NULL, MHD_OPTION_END);
     if (NULL == daemon)
       return 1;
   
     (void) getchar ();
   
     MHD_stop_daemon (daemon);
   
     return 0;
}
