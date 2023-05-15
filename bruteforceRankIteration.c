//bruteforceNaive.c
//Tambien cifra un texto cualquiera con un key arbitrario.
//OJO: asegurarse que la palabra a buscar sea lo suficientemente grande
//  evitando falsas soluciones ya que sera muy improbable que tal palabra suceda de
//  forma pseudoaleatoria en el descifrado.
//>> mpicc bruteforce.c -o desBrute
//>> mpirun -np <N> desBrute

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>	//OJO: utilizar otra libreria de no poder con esta
#include <stdint.h>



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


//descifra un texto dado una llave
void decrypt(uint64_t key, char *ciph, int len) {
  DES_key_schedule ks;
  DES_cblock k;
  memcpy(k, &key, 8);
  DES_set_odd_parity(&k);
  DES_set_key_checked(&k, &ks);
  DES_ecb_encrypt((const_DES_cblock *)ciph, (DES_cblock *)ciph, &ks, DES_DECRYPT);
}

//cifra un texto dado una llave
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
int tryKey(uint64_t key, char *ciph, int len){	
  char temp[len+1]; //+1 por el caracter terminal
  memcpy(temp, ciph, len);
  temp[len]=0;	//caracter terminal
  decrypt(key, temp, len);
  return strstr((char *)temp, search) != NULL;
}

char eltexto[] = "Esta es una prueba de proyecto 2";
uint64_t the_key = 12L;
//2^56 / 4 es exactamente 18014398509481983
//uint64_t the_key = 18014398509481983L;
//long the_key = 18014398509481983L +1L;
int main(int argc, char *argv[]) {
  if (argc != 3)
  {
        fprintf(stderr, "Uso: %s <nombre_archivo> <llave_privada>\n", argv[0]);
        exit(EXIT_FAILURE);
  }

  const char *filename = argv[1];
  long the_key = strtol(argv[2], NULL, 10);
  double start_time, end_time;


  int N, id;
  uint64_t upper = (1ULL << 56); // upper bound DES keys 2^56
  MPI_Status st;
  MPI_Request req;


  char *eltexto = read_file(filename);
  int ciphlen = strlen(eltexto);
  MPI_Comm comm = MPI_COMM_WORLD;

  // Encrypt the text
  char cipher[ciphlen + 1];
  memcpy(cipher, eltexto, ciphlen);
  cipher[ciphlen] = 0;
  encrypt(the_key, cipher);

  // Initialize MPI
  MPI_Init(NULL, NULL);
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  
  uint64_t found = 0;
  int ready = 0;

  printf("Process %d starting\n", id);

  // Non-blocking receive, check in the loop if someone already found it
  MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  for (uint64_t i = id; i < upper; i+=N) {
    MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
    if (ready)
      break;

    if (tryKey(i, cipher, ciphlen)) {
      found = i;
      printf("Process %d found the key\n", id);
      for (int node = 0; node < N; node++) {
        MPI_Send(&found, 1, MPI_LONG, node, 0, comm);
      }
      break;
    }
  }

  // Wait and then print the text
  if (id == 0) {
    MPI_Wait(&req, &st);
    decrypt(found, cipher, ciphlen);
    printf("Key = %llu\n\n", found);
    printf("Decrypted text (in hexadecimal):\n");
    for (int i = 0; i < ciphlen; ++i) {
        printf("%02x ", (unsigned char)cipher[i]);
    }
    printf("\n\n");
    printf("Original text:\n%s\n", eltexto);
    printf("Decrypted text:\n%s\n", cipher);
  }
  printf("Process %d exiting\n", id);

  // Finalize MPI environment
  MPI_Finalize();
  free(eltexto);

}

