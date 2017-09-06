#include <sys/types.h>          /* some systems still require this */
#include <sys/stat.h>
#include <stdio.h>              /* for convenience */
#include <stdlib.h>             /* for convenience */
#include <stddef.h>             /* for offsetof */
#include <string.h>             /* for convenience */
#include <unistd.h>             /* for convenience */
#include <signal.h>             /* for SIG_ERR */ 
#include <netdb.h> 
#include <errno.h> 
#include <syslog.h> 
#include <sys/socket.h> 
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <libudev.h>
#include <pthread.h>
#include <mntent.h>

/*
    while(1){
        //obtener usb hacerlo en un hilo (con sleep)
        recive()/read() //conexion con el daemon_server
    }
*/
#define PORT 4546 //para comunicarse con el servidor
#define BUFLEN 100 //para lo que recibe del servidor
#define MAXDEVICES 10
char* ip = "127.0.0.1";

struct dispositivo{
  struct udev_device* nodo;
  char *puntoMonje;
  char *id;
  char *nombre;
};

struct dispositivo dispositivos[MAXDEVICES];

const char* MountPoint(const char *dir){
	FILE *mtabfile;
	struct mntent *mt;
	
	mtabfile = setmntent("/etc/mtab", "r");
	if (mtabfile == NULL) {
		return "error en al crear FILE mtab";
	}
	
	while ((mt = getmntent(mtabfile)) != NULL){
		
		if(strstr(mt->mnt_fsname,dir)>0){
			endmntent(mtabfile);
			return  mt->mnt_dir;
		}
	}
	endmntent(mtabfile);
	return  "no se encuentra montado dicho dispositivo";
}

struct udev_device* obtener_hijo(struct udev* udev, struct udev_device* padre, const char* subsistema)
{
    struct udev_device* hijo = NULL;
    struct udev_enumerate* enumerar = udev_enumerate_new(udev);

    udev_enumerate_add_match_parent(enumerar, padre);
    udev_enumerate_add_match_subsystem(enumerar, subsistema);
    udev_enumerate_scan_devices(enumerar);

    struct udev_list_entry* dispositivos = udev_enumerate_get_list_entry(enumerar);
    struct udev_list_entry* entrada;

    udev_list_entry_foreach(entrada, dispositivos) {
        const char* ruta = udev_list_entry_get_name(entrada);
        hijo = udev_device_new_from_syspath(udev, ruta);
    }

    udev_enumerate_unref(enumerar);
    return hijo;
}

char *enumerar_disp_alm_masivo(struct udev* udev)
{
    struct udev_enumerate* enumerar = udev_enumerate_new(udev);

    udev_enumerate_add_match_subsystem(enumerar, "scsi");
    udev_enumerate_add_match_property(enumerar, "DEVTYPE", "scsi_device");
    udev_enumerate_scan_devices(enumerar);

    struct udev_list_entry* dispositivos = udev_enumerate_get_list_entry(enumerar);
    struct udev_list_entry* entrada;
	char *lista = (char *)malloc(1000000); // lista de todos los dispositivos
	int n = 0;
    udev_list_entry_foreach(entrada, dispositivos) {
	char *info = (char *)malloc(1000000); //info de cada dispositivo
	
        const char* ruta = udev_list_entry_get_name(entrada);
        struct udev_device* scsi = udev_device_new_from_syspath(udev, ruta);
        
        struct udev_device* block = obtener_hijo(udev, scsi, "block");
        struct udev_device* scsi_disk = obtener_hijo(udev, scsi, "scsi_disk");

        struct udev_device* usb 
            = udev_device_get_parent_with_subsystem_devtype(scsi, "usb", "usb_device");
        
        
        if (block && scsi_disk && usb){
		const char* nodo = udev_device_get_devnode(block);
		
            	n = sprintf(info,"{\"nodo\":\"%s\", \"nombre\":\" \",\"montaje\":\"%s\",\"Vendor:idProduct\":\"%s:%s\",\"scsi\":\"%s\"}\n",
                nodo,
		MountPoint(nodo),
                udev_device_get_sysattr_value(usb, "idVendor"),
                udev_device_get_sysattr_value(usb, "idProduct"),
                udev_device_get_sysattr_value(scsi, "vendor"));

        }

	if(strstr(lista, "nodo")!=NULL){
		char *copia = (char *)malloc(1000000);
		sprintf(copia, "%s",lista);
		sprintf(lista, "%s,%s",copia,info);
	}else{
		sprintf(lista, "%s",info);
	}
        if (block){
            udev_device_unref(block);
        }
        if (scsi_disk){
            udev_device_unref(scsi_disk);
        }
        
        udev_device_unref(scsi);
    }
	if(n==0){ 
		lista=" ";
	}
    udev_enumerate_unref(enumerar);
	return lista;
}

/* lee el archivo del pendrive */
char* leer_archivo(char* direccion, char* nombre_archivo){
	FILE *archivo;
	int caracter;
	char resultado[1000];
	char* texto_final=NULL;
	sprintf(resultado,"%s/%s", direccion,nombre_archivo);
	archivo = fopen(resultado,"r");
	if (archivo == NULL){
            printf("\nError ak abrir el archivo. \n\n");
    }else{
        while((caracter = fgetc(archivo)) != EOF) sprintf(texto_final,"%s%c",texto_final,caracter);
	}
    fclose(archivo);
    return texto_final;
}

/*escribir archivo en el pendrive*/
void escribir(char* direccion, char* nombre_archivo, int tamano, char* contenido){
	int MAX=tamano;	
	char resultado[1000];
	sprintf(resultado,"%s/%s", direccion,nombre_archivo);

    char cadena[MAX];
    sprintf(cadena,"%s%s", cadena,contenido);
    FILE* fichero;
    fichero = fopen(resultado, "wt");
    fputs(cadena, fichero);
    fclose(fichero);
    printf("Proceso completado");
}

int main(int argc, char** argv){

    pid_t process_id = 0;

    // Crea el proceso hijo
    process_id = fork();
    
    // Verifica retorno del fork()
    if (process_id < 0){
        printf("fork failed!\n");
        // Return failure in exit status
        exit(1);
    }

    // Mata el proceso del padre
    if (process_id > 0){
        printf("process_id of child process %d\n", process_id);
        exit(0);
    }

    umask(0);

    chdir("/");

    pthread_t hiloActualizacion;

    struct udev *p = udev_new();

////////////////////////////////S E R V I D O R///////////////////////////////////
    int daemon_server;
    //Direccion del daemon_server
    struct sockaddr_in direccion_daemon_server;
    //ponemos en 0 la estructura direccion_daemon_server
    memset(&direccion_daemon_server, 0, sizeof(direccion_daemon_server));

    //llenamos los campos
    //IPv4
    direccion_daemon_server.sin_family = AF_INET;
    //Convertimos el numero de puerto al endianness de la red
    direccion_daemon_server.sin_port = htons(PORT);
    //Nos vinculamos a la interface localhost o podemos usar INADDR_ANY para ligarnos A TODAS las interfaces
    direccion_daemon_server.sin_addr.s_addr = inet_addr(ip);

    //Abrimos un socket para el daemon
    daemon_server = socket(((struct sockaddr *)&direccion_daemon_server)->sa_family, SOCK_STREAM, 0);
    if (daemon_server == -1)
    {
        printf("Error al abrir el socket\n");
        return -1;
    }
    //Para que no haya problemas debido a que el socket siga abierto_daemon_server
    int abierto_daemon_server = 1;
    setsockopt(daemon_server, SOL_SOCKET, SO_REUSEADDR, &abierto_daemon_server, sizeof(abierto_daemon_server));

    //Enlazamos el socket
    int enlace_daemon_server = bind(daemon_server, (struct sockaddr *)&direccion_daemon_server, sizeof(direccion_daemon_server));
    if(enlace_daemon_server != 0)
    {
        printf("Error!!!\n");
        printf("No se puede enlazar al puerto : dirección ya está en uso\n");
    return -1;
    }

    //Ponemos al socket del daemon en espera
    int escuchar = listen(daemon_server,100);
    if(escuchar == -1)
    {
        printf("No es posible escuchar en ese puerto\n");
        return -1;
    }
    printf("Enlazado al puerto.\n");

    struct sockaddr_in direccion_servidor;
    memset(&direccion_daemon_server, 0, sizeof(direccion_servidor));
    unsigned int tam = sizeof(direccion_servidor);

    while(1)
    {
        int servidor = accept(daemon_server,(struct sockaddr *)&direccion_servidor,&tam);

        int pid = fork();

        if (pid==0)
        {      
            char *solicitud = (char *)malloc(BUFLEN*sizeof(char *));
            recv(servidor, solicitud, BUFLEN, 0);

		printf("%s\n", solicitud);
	  	//tratamiento tipo de solicitud
		char* is = strstr(solicitud, "escribir_archivo");
		
	  	
		struct udev *udeva;
		udeva = udev_new();
		
		char* lista =  enumerar_disp_alm_masivo(udeva);
		
		printf("%s\n",lista);
		send(servidor,lista,strlen(lista),0);
		close(servidor);
		
	
        }
	

    }
//////////////////////////////////////////////////////////////////////////////////
    return (0);
}



