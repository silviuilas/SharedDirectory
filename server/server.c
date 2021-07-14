/* servTCPCSel.c - Exemplu de server TCP concurent 
   
   Asteapta un "nume" de la clienti multipli si intoarce clientilor sirul
   "Hello nume" corespunzator; multiplexarea intrarilor se realizeaza cu select().
   
   Cod sursa preluat din [Retele de Calculatoare,S.Buraga & G.Ciobanu, 2003] si modificat de 
   Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
   
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

/* portul folosit */

#define SERVER_ADDRES "127.0.0.1" //"192.168.0.105"
#define MAX_USERS 100
#define PORT 8080
#define MSG_SIZE 8192

int get_command(char *str);
void execute_command(int k, char *msg, char *oldmsg, int fd);

void *communicate(void *v_fd);
void addClientInfo(int fd, struct sockaddr_in address);
void removeClientInfo(int fd);
char *showAllClients(int max);
void do_files(char *files, int fd);
int find_file(char *file);
int add_file(char *file, int fd);
char *show_all_files();
char *show_all_files_from();
int find_name_index(char *name);
void remove_files(int fd);
extern int errno; /* eroarea returnata de unele apeluri */

int download_file(int to, int file);
int send_file(int from, int to, int file);
/* functie de convertire a adresei IP a clientului in sir de caractere */
char *conv_addr(struct sockaddr_in address)
{
  static char str[25];
  char port[7];

  /* adresa IP a clientului */
  strcpy(str, inet_ntoa(address.sin_addr));
  /* portul utilizat de client */
  bzero(port, 7);
  sprintf(port, ":%d", ntohs(address.sin_port));
  strcat(str, port);
  return (str);
}
char *get_word_on_pos(char *str, char *cuvant, int p, char sep)
{
  bzero(cuvant, 100);
  int k = 0;
  for (int i = 0; i < p - 1; i++)
  {
    while (str[k] != sep && str[k] != '\0')
      k++;
    k++;
  }
  int i = 0;
  while (str[k] != sep && str[k] != '\0')
  {
    cuvant[i] = str[k];
    k++;
    i++;
  }
  cuvant[i] = '\0';
  return (cuvant);
}
char clienti_activi_info[MAX_USERS][20];
char clienti_activi_nume[MAX_USERS][20];
char clienti_activi_fisiere[MAX_USERS][1024];
char fisiere_online[1000][100];
int seederi_fisiere_online[1000];
/* programul */
int fk;
int pip[2], udp_pipe[2];
fd_set actfds;  /* multimea descriptorilor activi */
fd_set readfds; /* multimea descriptorilor de citire */
char udp_msg[1024];
int sd_udp; // descriptorul de socket
int main()
{
  bzero(udp_msg, 1024);
  bzero(clienti_activi_info, 2000);
  //DECLARARE
  struct sockaddr_in server; /* structurile pentru server si clienti */
  struct sockaddr_in from;

  struct timeval tv; /* structura de timp pentru select() */
  int sd, client;    /* descriptori de socket */
  int optval = 1;    /* optiune folosita pentru setsockopt()*/
  int fd;            /* descriptor folosit pentru parcurgerea listelor de descriptori */
  int nfds;          /* numarul maxim de descriptori */
  int len;           /* lungimea structurii sockaddr_in */

  /* creare socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server] Eroare la socket().\n");
    return errno;
  }

  /*setam pentru socket optiunea SO_REUSEADDR */
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  /* pregatim structurile de date */
  bzero(&server, sizeof(server));

  /* umplem structura folosita de server */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(SERVER_ADDRES);
  server.sin_port = htons(PORT);

  /* atasam socketul */
  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server] Eroare la bind().\n");
    return errno;
  }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen(sd, MAX_USERS) == -1)
  {
    perror("[server] Eroare la listen().\n");
    return errno;
  }

  /* completam multimea de descriptori de citire */
  FD_ZERO(&actfds);    /* initial, multimea este vida */
  FD_SET(sd, &actfds); /* includem in multime socketul creat */

  tv.tv_sec = 1; /* se va astepta un timp de 1 sec. */
  tv.tv_usec = 0;

  /* valoarea maxima a descriptorilor folositi */
  nfds = sd;

  printf("[server] Asteptam la portul %d...\n", PORT);
  fflush(stdout);

  pipe(pip);
  pipe(udp_pipe);
  fcntl(pip[0], F_SETFL, O_NONBLOCK);
  fcntl(udp_pipe[0], F_SETFL, O_NONBLOCK);

  if ((sd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }
  /* servim in mod concurent clientii... */
  int iter = 0;
  while (1)
  {
    iter++;
    /* ajustam multimea descriptorilor activi (efectiv utilizati) */
    bcopy((char *)&actfds, (char *)&readfds, sizeof(readfds));

    /* apelul select() */
    if (select(nfds + 1, &readfds, NULL, NULL, &tv) < 0)
    {
      perror("[server] Eroare la select().\n");
      return errno;
    }
    /* vedem daca e pregatit socketul pentru a-i accepta pe clienti */
    if (FD_ISSET(sd, &readfds))
    {
      /* pregatirea structurii client */
      len = sizeof(from);
      bzero(&from, sizeof(from));

      /* a venit un client, acceptam conexiunea */
      client = accept(sd, (struct sockaddr *)&from, &len);

      /* eroare la acceptarea conexiunii de la un client */
      if (client < 0)
      {
        perror("[server] Eroare la accept().\n");
        continue;
      }

      if (nfds < client) /* ajusteaza valoarea maximului */
        nfds = client;

      /* includem in lista de descriptori activi si acest socket */
      FD_SET(client, &actfds);
      addClientInfo(client, from);
      printf("[conectat] Descriptorul %d, de la adresa %s.\n", client, conv_addr(from));
      fflush(stdout);
    }
    /* vedem daca e pregatit vreun socket client pentru a trimite raspunsul */
    for (fd = 0; fd <= nfds; fd++) /* parcurgem multimea de descriptori */
    {
      /* este un socket de citire pregatit? */
      if (fd != sd && FD_ISSET(fd, &readfds))
      {
        int fd_aux = fd;
        pthread_t inc_x_thread;
        if (pthread_create(&inc_x_thread, NULL, communicate, &fd_aux))
          printf("Erroare la thread ");
      }
    }
  }
  close(sd_udp);
  return 0;
} /* main */

/* realizeaza primirea si retrimiterea unui mesaj unui client */
void *communicate(void *v_fd)
{
  int *a_fd = (int *)v_fd;
  int fd = *a_fd;
  char buffer[MSG_SIZE];        /* mesajul */
  int bytes;                    /* numarul de octeti cititi/scrisi */
  char msg[MSG_SIZE];           //mesajul primit de la client
  char msgrasp[MSG_SIZE] = " "; //mesaj de raspuns pentru client

  bzero(msg, MSG_SIZE);
  bytes = read(fd, msg, sizeof(buffer));
  if (bytes < 0)
  {
    perror("Eroare la read() de la client.\n");
    return NULL;
  }

  if (bytes == 0)
  {
    if (FD_ISSET(fd, &readfds))
    {
      if (clienti_activi_info[fd] != 0)
      {
        printf("[deconectat] Descriptorul %d cu adresa %s\n", fd, clienti_activi_info[fd]);
        removeClientInfo(fd);
        FD_CLR(fd, &actfds);
        remove_files(fd);
        fflush(stdout);
        close(fd);
      }
    }
    return NULL;
  }

  if (strlen(msg) < 30)
  {
    printf("[primit]%s ", msg);
    for (int i = 0; i < 30 - strlen(msg); i++)
    {
      printf(" ");
    }
    printf("Descriptorul: %d\n", fd);
  }
  else
  {
    printf("[primit]Mesaj lung ");
    for (int i = 0; i < 30 - 10; i++)
    {
      printf(" ");
    }
    printf("Descriptorul: %d\n", fd);
  }

  bzero(msgrasp, MSG_SIZE);
  execute_command(get_command(msg), msgrasp, msg, fd);

  if (strlen(msgrasp) < 30)
  {
    printf("[trimis]%s ", msgrasp);
    for (int i = 0; i < 30 - strlen(msgrasp); i++)
    {
      printf(" ");
    }
    printf("Descriptorul: %d\n", fd);
  }
  else
  {
    printf("[trimis]Mesaj lung ");
    for (int i = 0; i < 30 - 10; i++)
    {
      printf(" ");
    }
    printf("Descriptorul: %d\n", fd);
  }

  if (bytes && write(fd, msgrasp, bytes) < 0)
  {
    perror("[server] Eroare la write() catre client.\n");
    return NULL;
  }
  return NULL;
}

void addClientInfo(int fd, struct sockaddr_in address)
{
  strcpy(clienti_activi_info[fd], conv_addr(address));
}
void removeClientInfo(int fd)
{
  bzero(clienti_activi_info[fd], 20);
  bzero(clienti_activi_nume[fd], 20);
}
char *showAllClients(int max)
{
  static char activi[MAX_USERS * 50];
  bzero(activi, MAX_USERS * 50);
  char aux[100];
  int k = 0;
  for (int i = 0; i <= max; i++)
    if (clienti_activi_info[i][0] != 0)
    {
      if (clienti_activi_nume[i][0] != 0)
      {
        sprintf(aux, "\tClientul %s este activ la descriptorul %d\n", clienti_activi_nume[i], i);
        strcat(activi, aux);
      }
      else
      {
        sprintf(aux, "\tClientul %s este activ la descriptorul %d\n", clienti_activi_info[i], i);
        strcat(activi, aux);
      }
    }
  activi[strlen(activi) - 1] = 0;
  return (activi);
}

int get_command(char *str)
{
  char aux[100];
  if (strcmp(get_word_on_pos(str, aux, 1, ' '), "my_files_are_this") == 0)
  {
    return 1;
  }
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "showall") == 0)
    return 6;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "rename") == 0)
    return 3;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "showfiles") == 0)
    return 4;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "myfiles") == 0)
    return 5;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "get_my_addr") == 0)
    return 2;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "download") == 0)
    return 7;
  else if (strcmp(get_word_on_pos(str, aux, 1, ' '), "find_from") == 0)
    return 8;
  return 0;
}
void execute_command(int k, char *msg, char *oldmsg, int fd)
{
  bzero(msg, MSG_SIZE);
  char aux[100];
  if (k == 1)
  {
    remove_files(fd);
    do_files(oldmsg, fd);
    return;
  }
  else if (k == 2)
  {
    strcpy(msg, get_word_on_pos(clienti_activi_info[fd], aux, 1, ':'));
    strcat(msg, ":");
    int k = 2000 + fd;
    sprintf(aux, "%d", k);
    strcat(msg, aux);
  }
  else if (k == 6)
  {
    strcpy(msg, "\n");
    strcat(msg, showAllClients(MAX_USERS));
  }
  else if (k == 3)
  {
    if (get_word_on_pos(oldmsg, aux, 2, ' ')[0] != 0)
    {
      strcpy(msg, "You are now called ");
      strcat(msg, get_word_on_pos(oldmsg, aux, 2, ' '));
      strcpy(clienti_activi_nume[fd], get_word_on_pos(oldmsg, aux, 2, ' '));
      return;
    }
    {
      strcpy(msg, "You must enter a name first");
    }
  }
  else if (k == 4)
  {
    strcpy(msg, "\n");
    strcat(msg, show_all_files());
    msg[strlen(msg) - 1] = '\0';
  }
  else if (k == 5)
  {
    strcpy(msg, clienti_activi_fisiere[fd]);
  }
  else if (k == 7)
  {
    if (get_word_on_pos(oldmsg, aux, 2, ' ')[0] == 0)
    {
      strcpy(msg, "You must pick a file number");
    }
    else
    {
      int file = atoi(get_word_on_pos(oldmsg, aux, 2, ' '));
      if (download_file(fd, file) == 1)
        strcpy(msg, "File started downloading");
      else
      {
        strcpy(msg, "Coud not download file");
      }
      bzero(oldmsg, 1024);
    }
  }
  else if (k == 8)
  {
    if (get_word_on_pos(oldmsg, aux, 2, ' ')[0] == 0)
    {
      strcpy(msg, "You must pick a name first");
    }
    else
    {
      char name[100];
      strcpy(name, get_word_on_pos(oldmsg, aux, 2, ' '));
      find_name_index(name);
      strcpy(msg, show_all_files_from(find_name_index(name)));
      if (strcmp(msg, "") == 0)
        strcpy(msg, "The user does not exists or does not have any files");
      bzero(oldmsg, 1024);
    }
  }
  return;
}

void do_files(char *files, int fd)
{
  char str[1024], aux[100], aux2[100], aux3[1024];
  bzero(str, 1024);
  int ii = 2;
  strcat(str, get_word_on_pos(files, aux3, ii, ' '));
  ii = 1;
  while (get_word_on_pos(str, aux3, ii, ';')[0])
  {
    sprintf(aux, "%s", get_word_on_pos(str, aux3, ii, ';'));
    int kk;
    if ((kk = find_file(aux)) == -1)
    {
      kk = add_file(aux, fd);
      seederi_fisiere_online[kk] = 1;
      sprintf(aux, "%d;", kk);
      strcat(clienti_activi_fisiere[fd], aux);
    }
    else
    {
      seederi_fisiere_online[kk]++;
      sprintf(aux, "%d;", kk);
      strcat(clienti_activi_fisiere[fd], aux);
    }

    ii++;
  }
}

int find_file(char *file)
{
  for (int i = 0; i < 1000; i++)
    if (strcmp(file, fisiere_online[i]) == 0 && seederi_fisiere_online[i] != 0)
      return i;
  return -1;
}
int add_file(char *file, int fd)
{
  for (int i = 0; i < 1000; i++)
    if (fisiere_online[i][0] == 0 || seederi_fisiere_online[i] == 0)
    {
      strcpy(fisiere_online[i], file);
      return i;
    }
  return -1;
}
int find_name_index(char *name)
{
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (strcmp(clienti_activi_nume[i], name) == 0)
    {
      return i;
    }
  }
}
char *show_all_files_from(int nume_from)
{
  static char str[100000], aux[100], aux2[100], aux3[100], aux4[1000];
  strcpy(str, "\0");

  for (int i = 1; i < 100; i++)
  {
    if (get_word_on_pos(clienti_activi_fisiere[nume_from], aux3, i, ';')[0] != 0)
    {
      sprintf(aux4, "Nr: %d   Nume: %s   Size: %s bytes \n", atoi(aux3), get_word_on_pos(fisiere_online[atoi(aux3)], aux2, 1, '*'), get_word_on_pos(fisiere_online[atoi(aux3)], aux, 3, '*'));
      strcat(str, aux4);
    }
  }
  return str;
}
char *show_all_files()
{
  static char str[100000], aux[100], aux2[100];
  strcpy(str, "\0");
  for (int i = 0; i < 1000; i++)
    if (fisiere_online[i][0] != 0 && seederi_fisiere_online[i] != 0)
    {
      sprintf(aux, "Nr:%d", i);
      strcat(str, aux);
      sprintf(aux, "   Seeds:%d", seederi_fisiere_online[i]);
      get_word_on_pos(fisiere_online[i], aux2, 1, '*');
      strcat(str, aux);
      for (int k = 0; k < 25; k++)
      {
        aux[k] = aux2[k];
        if (!aux2[k])
          aux[k] = ' ';
        else if (k > 21)
          aux[k] = '.';
      }
      aux[25] = '\0';
      sprintf(aux2, "   Name:%s", aux);
      strcat(str, aux2);
      if (atoi(get_word_on_pos(fisiere_online[i], aux2, 3, '*')) < 1024)
        sprintf(aux, "   Size:%s Bytes\n", get_word_on_pos(fisiere_online[i], aux2, 3, '*'));
      else if (atoi(get_word_on_pos(fisiere_online[i], aux2, 3, '*')) < 1048576)
      {
        sprintf(aux, "   Size:%d KB\n", atoi(get_word_on_pos(fisiere_online[i], aux2, 3, '*')) / 1024);
      }
      else
      {
        sprintf(aux, "   Size:%d MB\n", atoi(get_word_on_pos(fisiere_online[i], aux2, 3, '*')) / 1048576);
      }

      strcat(str, aux);
    }
  return str;
}
void remove_files(int fd)
{
  int aux, aux2;
  char aux3[100];

  for (int i = 1; i < 100; i++)
  {
    if (get_word_on_pos(clienti_activi_fisiere[fd], aux3, i, ';')[0] != 0)
    {
      aux2 = atoi(get_word_on_pos(clienti_activi_fisiere[fd], aux3, i, ';'));
      if (seederi_fisiere_online[aux2] == 1)
      {
        seederi_fisiere_online[aux2]--;
        strcpy(fisiere_online[aux2], "\0");
      }
      else
      {
        if (seederi_fisiere_online[aux2] != 0)
          seederi_fisiere_online[aux2]--;
      }
    }
  }
  bzero(clienti_activi_fisiere[fd], 1024);
}

int download_file(int to, int file)
{
  //UDP TRANSFER
  //SEND TO
  char aux[1024];
  if (seederi_fisiere_online[file] == 0)
  {
    return 0;
  }
  int k = 1;
  while (get_word_on_pos(clienti_activi_fisiere[to], aux, k, ';')[0])
  {
    if (atoi(aux) == file)
      return 0;
    k++;
  }
  struct sockaddr_in udp_client; // structura folosita pentru conectare
  int msglen = 0, length = 0;
  length = sizeof(udp_client);
  char msg[1024]; // mesajul trimis
  udp_client.sin_family = AF_INET;
  udp_client.sin_addr.s_addr = inet_addr(get_word_on_pos(clienti_activi_info[to], msg, 1, ':'));
  udp_client.sin_port = htons((2000 + to));
  bzero(msg,1024);
  strcpy(msg, "0*");
  strcat(msg, fisiere_online[file]);

  int j, sum = 0;
  char seds[1024];
  strcpy(seds, "");
  for (int i = 0; i < MAX_USERS; i++)
  {
    j = 1;
    while (get_word_on_pos(clienti_activi_fisiere[i], aux, j, ';')[0])
    {
      if (atoi(aux) == file)
      {
        strcat(seds, get_word_on_pos(clienti_activi_info[i],aux,1,':'));
        sprintf(aux,":%d;",2000+i);
        strcat(seds,aux);
        sum++;
      }
      j++;
    }
  }
  sprintf(aux,"*%d*",sum);
  strcat(msg,aux);
  strcat(msg,seds);
  printf("%s \n",msg);
  length = sizeof(udp_client);
  if (sendto(sd_udp, msg, 1024, 0, (struct sockaddr *)&udp_client, length) < 0)
  {
    perror("Eroare la sendto \n");
    return errno;
  }

  return 1;
}
