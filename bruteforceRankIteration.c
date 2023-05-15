/**
 * Universidad del valle de Guatemala
 * Computación paralela y distribuida
 * Proyecto 2
 * 
 * Integrantes:
 *      - Christian Perez 19710
 *      - Jose Javier Hurtarte 19707
 *      - Andrei Portales 19825
 * 
 * @file bruteforceRankIteration.c
 * @version 0.1
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>	//OJO: utilizar otra libreria de no poder con esta
#include <stdint.h>


/**
 * @brief Lee un archivo y lo guarda en un string
 * 
 * @param filename 
 * @return char* 
 */
char *read_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error abriendo el archivo");
        exit(EXIT_FAILURE);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_contents = (char *)malloc(file_size + 1);
    if (!file_contents)
    {
        perror("Error reservando memoria");
        exit(EXIT_FAILURE);
    }

    fread(file_contents, 1, file_size, file);
    file_contents[file_size] = '\0';

    fclose(file);
    return file_contents;
}


/**
 * @brief Descifra un texto dado una llave
 * 
 * @param key Este parametro es la llave que se utilizara para descifrar el texto
 * @param ciph Este parametro es el texto cifrado que se descifrara
 * @param len Este parametro es el tamaño del texto cifrado
 */
void decrypt(uint64_t key, char *ciph, int len) {
  DES_key_schedule ks;
  DES_cblock k;
  memcpy(k, &key, 8);
  DES_set_odd_parity(&k);
  DES_set_key_checked(&k, &ks);
  DES_ecb_encrypt((const_DES_cblock *)ciph, (DES_cblock *)ciph, &ks, DES_DECRYPT);
}

/**
 * @brief Cifra un texto dado una llave
 * 
 * @param key Este parametro es la llave que se utilizara para cifrar el texto
 * @param ciph Este parametro es el texto que se cifrara
 */
void encrypt(uint64_t key, char *ciph) {
  DES_key_schedule ks;
  DES_cblock k;
  memcpy(k, &key, 8);
  DES_set_odd_parity(&k);
  DES_set_key_checked(&k, &ks);
  DES_ecb_encrypt((const_DES_cblock *)ciph, (DES_cblock *)ciph, &ks, DES_ENCRYPT);
}

//palabra clave a buscar en texto descifrado para determinar si se rompio el codigo
char search[] = "es una prueba de";

/**
 * @brief Intenta descifrar un texto dado una llave y busca un texto en el texto descifrado
 * 
 * @param key   Este parametro es la llave que se utilizara para descifrar el texto
 * @param ciph Este parametro es el texto cifrado que se descifrara
 * @param len Este parametro es el tamaño del texto cifrado
 * @return int  1 si se encontro el texto, 0 si no se encontro
 */
int tryKey(uint64_t key, char *ciph, int len){	
  char temp[len+1]; //+1 por el caracter terminal
  memcpy(temp, ciph, len);
  temp[len]=0;	//caracter terminal
  decrypt(key, temp, len);
  return strstr((char *)temp, search) != NULL;
}

char eltexto[] = "Esta es una prueba de proyecto 2"; //texto a cifrar
uint64_t the_key = 12L;
int main(int argc, char *argv[]) {
  if (argc != 3)
  {
    fprintf(stderr, "Uso: %s <nombre_archivo> <llave_privada>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  const char *filename = argv[1]; // Nombre del archivo
  long the_key = strtol(argv[2], NULL, 10); // Llave privada

  int N, id;
  uint64_t upper = (1ULL << 56); // upper bound DES keys 2^56
  MPI_Status st;
  MPI_Request req;
  double start_time, end_time; // Declare the variables to hold the start and end times

  char *eltexto = read_file(filename); // Lee el archivo que contiene el texto
  int ciphlen = strlen(eltexto);
  MPI_Comm comm = MPI_COMM_WORLD; // Communicator

  // Encrypt the text
  char cipher[ciphlen + 1]; //+1 por el caracter terminal
  memcpy(cipher, eltexto, ciphlen);  // copia el texto cifrado
  cipher[ciphlen] = 0; // caracter terminal
  encrypt(the_key, cipher); // cifra el texto

  // Initialize MPI
  MPI_Init(NULL, NULL); // inicializa MPI
  MPI_Comm_size(comm, &N);  // obtiene el numero de procesos
  MPI_Comm_rank(comm, &id);// obtiene el id del proceso

  uint64_t found = 0;
  int ready = 0;

  printf("Process %d starting\n", id);

  
  MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req); // recibe el mensaje de cualquier proceso

  start_time = MPI_Wtime(); // tiempo inicial del calculo
  for (uint64_t i = id; i < upper; i+=N) {
    MPI_Test(&req, &ready, MPI_STATUS_IGNORE); // prueba si el mensaje ha llegado
    if (ready)
      break;

    if (tryKey(i, cipher, ciphlen)) // Verificando si logra desifrar el texto con una llave dada
    {
      
      found = i;
      printf("Process %d found the key\n", id);
      for (int node = 0; node < N; node++) {
        MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // envia el mensaje a todos los procesos
      }
      break;
    }
  }

  if (id == 0) {
    end_time = MPI_Wtime(); // Time it takes for the process to find the key
    printf("Time taken to find the key: %f seconds\n", end_time - start_time);
  }

  // Wait and then print the text
  if (id == 0) {
    MPI_Wait(&req, &st);
    decrypt(found, cipher, ciphlen);
    printf("Key = %llu\n\n", found);
    
    printf("\n\n");
    printf("Original text:\n%s\n", eltexto);
    printf("Decrypted text:\n%s\n", cipher);
  }
  printf("Process %d exiting\n", id);

  // Finalize MPI environment
  MPI_Finalize();
  free(eltexto);
}


