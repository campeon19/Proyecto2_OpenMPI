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
 * @file bruteforceWorkStealing.c
 * @version 0.1
 */


#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openssl/des.h>

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
void decrypt(uint64_t key, char *ciph, int len)
{
    DES_key_schedule ks;
    DES_cblock k;
    memcpy(k, &key, 8); // copia la llave en el bloque k
    DES_set_odd_parity(&k); // establece la paridad impar
    DES_set_key_checked(&k, &ks); // establece la llave
    DES_ecb_encrypt((const_DES_cblock *)ciph, (DES_cblock *)ciph, &ks, DES_DECRYPT); // descifra el texto
}

/**
 * @brief Cifra un texto dado una llave
 * 
 * @param key Este parametro es la llave que se utilizara para cifrar el texto
 * @param ciph Este parametro es el texto que se cifrara
 */
void encrypt(uint64_t key, char *ciph)
{
    DES_key_schedule ks;
    DES_cblock k;
    memcpy(k, &key, 8);
    DES_set_odd_parity(&k);
    DES_set_key_checked(&k, &ks);
    DES_ecb_encrypt((const_DES_cblock *)ciph, (DES_cblock *)ciph, &ks, DES_ENCRYPT);
}

//palabra clave a buscar en texto descifrado para determinar si se rompio el codigo
char search[] = "es una prueba de"; // texto a buscar en el texto desifrado

/**
 * @brief Intenta descifrar un texto dado una llave y busca un texto en el texto descifrado
 * 
 * @param key   Este parametro es la llave que se utilizara para descifrar el texto
 * @param ciph Este parametro es el texto cifrado que se descifrara
 * @param len Este parametro es el tamaño del texto cifrado
 * @return int  1 si se encontro el texto, 0 si no se encontro
 */
int tryKey(uint64_t key, char *ciph, int len)
{
    char temp[len + 1]; //+1 por el caracter terminal
    memcpy(temp, ciph, len);
    temp[len] = 0; // caracter terminal
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

/**
 * @brief Envia un mensaje a un proceso, Esta funcion se encarga de pedir trabajo a un proceso
 * 
 * @param work_source 
 * @param comm 
 * @return int 
 */
int request_work(int *work_source, MPI_Comm comm)
{
    MPI_Status status;
    int work_received;
    MPI_Recv(&work_received, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);
    *work_source = status.MPI_SOURCE;
    return work_received;
}


int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <nombre_archivo> <llave_privada>\n", argv[0]); // nombre del archivo y llave privada
        exit(EXIT_FAILURE); // termina el programa
    }

    const char *filename = argv[1]; // nombre del archivo
    long the_key = strtol(argv[2], NULL, 10); // la llave que se recibe como parametro
    double start_time, end_time; // tiempo de inicio y fin

    int N, id; // numero de procesos y id del proceso
    uint64_t upper = (1ULL << 56); // upper bound DES keys 2^56
    uint64_t mylower, myupper; 
    MPI_Status st;
    MPI_Request req;

    char *eltexto = read_file(filename);
    int ciphlen = strlen(eltexto);

    MPI_Comm comm = MPI_COMM_WORLD;

    // cifrar el texto
    char cipher[ciphlen + 1]; //+1 por el caracter terminal
    memcpy(cipher, eltexto, ciphlen); // copia el texto cifrado
    cipher[ciphlen] = 0; // caracter terminal
    encrypt(the_key, cipher); // cifra el texto

    MPI_Init(NULL, NULL); // inicializa MPI
    MPI_Comm_size(comm, &N); // obtiene el numero de procesos
    MPI_Comm_rank(comm, &id); // obtiene el id del proceso
    start_time = MPI_Wtime(); // tiempo de inicio

    // Configurar work stealing
    int chunk_size = 1000; // tamaño del chunk
    long work_counter = id * chunk_size; // contador de trabajo
    long max_work = upper / N * (id + 1); // maximo de trabajo
    int work_source; // fuente de trabajo

    long found = 0L; 
    int ready = 0;
    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req); // recibe el trabajo

    while (!ready && work_counter < max_work) // mientras no este listo y el contador de trabajo sea menor al maximo de trabajo
    {
        for (long i = work_counter; i < work_counter + chunk_size; ++i) // recorre el contador de trabajo
        {
            MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
            if (ready)
                break;

            if (tryKey(i, cipher, ciphlen)) // Verificando si logra desifrar el texto con una llave dada
            {
                found = i;
                end_time = MPI_Wtime();  // tiempo de finalizacion
                printf("Process %d found the key\n", id);
                for (int node = 0; node < N; node++)
                {
                    MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // Envia el trabajo al nodo que lo necesita
                }
                break;
            }
        }

        if (!ready)
        {
            work_counter += chunk_size;
            if (work_counter >= max_work)
            {
                work_counter = request_work(&work_source, comm); // pide trabajo a un proceso
                max_work = work_counter + chunk_size; // actualiza el maximo de trabajo
            }
        }
    }

    // wait y luego imprimir el texto
    if (id == 0)
    {
        MPI_Wait(&req, &st); // espera a que el trabajo se complete
        decrypt(found, cipher, ciphlen); // desifra el texto
        printf("Key = %li\n\n", found);
        printf("%s\n", cipher);
    }
    printf("Process %d exiting\n", id);

    double elapsed_time = end_time - start_time;

    double max_elapsed_time;
    MPI_Reduce(&elapsed_time, &max_elapsed_time, 1, MPI_DOUBLE, MPI_MAX, 0, comm); // obtiene el tiempo maximo de ejecucion

    if (id == 0)
    {
        printf("Total execution time: %f seconds\n", max_elapsed_time);
    }

    // FIN entorno MPI
    MPI_Finalize();
    free(eltexto);
}
