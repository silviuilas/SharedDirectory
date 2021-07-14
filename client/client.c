#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>
/* codul de eroare returnat de anumite apeluri */
extern int errno;
/* portul de conectare la server*/
int port;

#define MSG_SIZE 8192
#define TRANSFER_SIZE 950
#define SERVER_ADDRES "127.0.0.1"
#define DOWNLOAD 1
#define DEBUG 0
long int find_size(char *dir)
{
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  long int sum = 0;
  if ((dp = opendir(dir)) == NULL)
  {
    return 0;
  }
  chdir(dir);
  while ((entry = readdir(dp)) != NULL)
  {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode))
    {
      /* Found a directory, but ignore . and .. */
      if (strcmp(".", entry->d_name) == 0 ||
          strcmp("..", entry->d_name) == 0)
        continue;
      sum = sum + find_size(entry->d_name);
    }
    else
    {
      sum = sum + statbuf.st_size;
    }
  }
  chdir("..");
  return sum;
}
char *get_files()
{
  static char rasp[1000];
  strcpy(rasp, "\0");
  char aux[100];
  bzero(rasp, 0);
  char dir[100];
  strcpy(dir, "shared");
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  char temp_dir[100];
  if ((dp = opendir(dir)) == NULL)
  {
    return rasp;
  }
  chdir(dir);
  while ((entry = readdir(dp)) != NULL)
  {
    lstat(entry->d_name, &statbuf);
    if (S_ISDIR(statbuf.st_mode))
    {
      /* Found a directory, but ignore . and .. */
      if (strcmp(".", entry->d_name) == 0 ||
          strcmp("..", entry->d_name) == 0)
        continue;
      strcat(rasp, entry->d_name);
      strcat(rasp, "*");
      sprintf(aux, "%d", entry->d_type);
      strcat(rasp, aux);
      strcat(rasp, "*");
      sprintf(aux, "%ld", find_size(entry->d_name));
      strcat(rasp, aux);
      strcat(rasp, ";");
    }
    else
    {
      strcat(rasp, entry->d_name);
      strcat(rasp, "*");
      sprintf(aux, "%d", entry->d_type);
      strcat(rasp, aux);
      strcat(rasp, "*");
      sprintf(aux, "%ld", statbuf.st_size);
      strcat(rasp, aux);
      strcat(rasp, ";");
    }
  }
  chdir("..");
  return (rasp);
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
char *get_word_on_pos_and_after(char *str, char *cuvant, int p, char sep)
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
  while (str[k] != '\0')
  {
    cuvant[i] = str[k];
    k++;
    i++;
  }
  cuvant[i] = '\0';
  return (cuvant);
}
int send_file(char *msg, int sd_send, char *dir)
{
  int k = rand() % 500;
  usleep(k * 1000);
  char aux[1024], aux2[1024], aux3[1024];
  FILE *fp;
  char buff[TRANSFER_SIZE];

  struct sockaddr_in udp_client; // structura folosita pentru conectare
  int msglen = 0, length = 0, sum = 0;
  length = sizeof(udp_client);
  char msg_send[1024]; // mesajul trimis
  udp_client.sin_family = AF_INET;
  get_word_on_pos(msg, aux2, 5, '*');
  udp_client.sin_addr.s_addr = inet_addr(get_word_on_pos(aux2, aux, 1, ':'));
  udp_client.sin_port = htons(atoi(get_word_on_pos(aux2, aux, 2, ':')));

  if (atoi(get_word_on_pos(msg, aux, 1, '*')) == 0)
  {
    if (sendto(sd_send, (void *)msg, 1024, 0, (struct sockaddr *)&udp_client, length) < 1024)
    {
      return errno;
    }
    return 1;
  }
  else
  {
    bzero(aux, 1024);
    strcpy(aux, dir);
    strcat(aux, "/shared/");
    get_word_on_pos(msg, aux2, 2, '*');
    strcat(aux, aux2);
    fp = fopen(aux, "rb");
    if (fp == NULL)
    {
      perror("[client]Eroare la deschiderea fisierului().\n");
    }
  }

  int i = 0, max_pack = ceil(((float)atoi(get_word_on_pos(msg, aux, 4, '*'))) / (float)TRANSFER_SIZE);
  if (get_word_on_pos(msg, aux, 6, '*')[0])
  {
    i = atoi(aux);
    max_pack = atoi(get_word_on_pos(msg, aux, 7, '*'));
  }
  int slow = 0;
  fseek(fp, TRANSFER_SIZE * atoi(get_word_on_pos(msg, aux, 6, '*')), SEEK_SET);
  for (i; i < max_pack; i++)
  {
    usleep(25 * slow);
    strcpy(msg_send, "2*");
    strcat(msg_send, get_word_on_pos(msg, aux2, 2, '*'));
    sprintf(aux, "*%d*", i);
    strcat(msg_send, aux);
    sum = 0;
    for (int l = 0; l < TRANSFER_SIZE; l++)
    {
      buff[l] = fgetc(fp);
      if (feof(fp))
      {
        buff[l] = '\0';
        break;
      }
    }
    sprintf(aux, "%ld*", strlen(buff));
    strcat(msg_send, aux);
    strcat(msg_send, buff);
    if (sendto(sd_send, (void *)msg_send, 1024, 0, (struct sockaddr *)&udp_client, length) < 1024)
    {
      return errno;
    }
  }
}
void send_directory(char *dir_name, char *addr, int sd, char *path_dir)
{
  char dir[100];
  getcwd(dir, sizeof(dir));
  strcat(dir, "/shared/");
  strcat(dir, dir_name);
  DIR *dp;
  struct dirent *entry;
  struct stat statbuf;
  char temp_dir[1024];
  char aux[1024];
  char aux2[1024];
  if ((dp = opendir(dir)) == NULL)
  {
    return;
  }
  chdir(dir);
  int ok = 1;
  while ((entry = readdir(dp)) != NULL)
  {
    lstat(entry->d_name, &statbuf);
    if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
      continue;
    strcpy(aux, "0*");
    strcat(aux, dir_name);
    strcat(aux, "/");
    strcat(aux, entry->d_name);
    sprintf(aux2, "*%d*%ld*", entry->d_type, statbuf.st_size);
    strcat(aux, aux2);
    strcat(aux, addr);
    if (DEBUG)
      printf("%s\n", aux);
    send_file(aux, sd, path_dir);
  }
};
int send_to_seeders(char *msg_to_send, char *send_to, int sd_send)
{
  char aux[1024], aux2[1024], aux3[1024], msg_send[1024];
  int j = 1;
  get_word_on_pos(send_to, aux2, 2, '*');
  while (get_word_on_pos(aux2, aux, j, ';')[0])
  {
    struct sockaddr_in udp_client; // structura folosita pentru conectare
    int msglen = 0, length = 0;
    length = sizeof(udp_client);
    udp_client.sin_family = AF_INET;
    udp_client.sin_addr.s_addr = inet_addr(get_word_on_pos(aux, aux3, 1, ':'));
    udp_client.sin_port = htons(atoi(get_word_on_pos(aux, aux3, 2, ':')));
    if (sendto(sd_send, msg_to_send, 1024, 0, (struct sockaddr *)&udp_client, length) < 1024)
    {
      return errno;
    }
    j++;
  }
}
struct thread_time_args
{
  clock_t *time;
  struct sockaddr_in addr;
  int sd;
  int *pipe_download;
};
void *time_out_error(void *obj)
{
  struct thread_time_args *a_obj = obj;
  char msg[1024];
  strcpy(msg, "9*Didn't recive msg for a long time");
  int k;
  while (1)
  {
    if ((double)(clock() - *a_obj->time) / CLOCKS_PER_SEC > 0.5)
    {
      if (sendto(a_obj->sd, (void *)msg, 1024, 0, (struct sockaddr *)&a_obj->addr, sizeof(struct sockaddr)) < 1024)
      {
        return NULL;
      }
      *a_obj->time = clock();
    }
  }
}
int nice_printf(int last_k, int kk)
{
  for (int i = last_k; i < kk; i++)
  {
    if (i % 2 == 0)
      printf("#");
  }

  fflush(stdout);
}
int UDP(char *addr, int *pipe_download)
{
  FILE *cpy_to;
  int start_downloading;
  int finished_chunks[1000000];
  struct sockaddr_in bind_server; // structura folosita de client
  struct sockaddr_in persoana;    //de unde o sa ne vina informatiile
  int length = sizeof(persoana);
  int aux_size, last_k = 0;
  char msg[1024];           //mesajul primit de la persoana
  char msgrasp[1024] = " "; //mesaj de raspuns pentru persoana
  int sd, max_pack = 0;
  int sd_send; //descriptorul de socket
  char aux[1024];
  char aux2[1024];
  char addres_send[1024];
  char file_name[100];
  char current_size[100];
  char send_to[1024], msg_struct[1024];
  char path[1024];
  char queue[1024][100];
  char first_file[100];
  int queue_index_cur = 0, queue_index_end = 0, special_fix;
  getcwd(path, sizeof(aux));
  strcat(path, "/shared/");
  /* crearea unui socket */
  if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }

  /* pregatirea structurilor de date */
  bzero(&bind_server, sizeof(bind_server));
  bzero(&persoana, sizeof(persoana));

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
  bind_server.sin_family = AF_INET;
  /* acceptam orice adresa */
  strcpy(aux2, get_word_on_pos(addr, aux, 1, ':'));
  strcpy(addres_send, aux);
  strcat(addres_send, ":");
  bind_server.sin_addr.s_addr = inet_addr(aux2);
  /* utilizam un port utilizator */
  bind_server.sin_port = htons(atoi(get_word_on_pos(addr, aux, 2, ':')));
  strcat(addres_send, aux);
  /* atasam socketul */
  if (bind(sd, (struct sockaddr *)&bind_server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la bind().\n");
    return errno;
  }
  int i = 0;
  int retry_check = 0;
  char address_aux[100];
  int waiting_for_files = 0;
  clock_t last_msg_time = clock();
  struct thread_time_args obj;
  obj.time = &last_msg_time;
  obj.pipe_download = pipe_download;
  obj.addr = bind_server;
  obj.sd = sd;
  pthread_t inc_x_thread;
  if (pthread_create(&inc_x_thread, NULL, time_out_error, (void *)&obj))
    printf("Erroare la thread ");
  while (1)
  {
    bzero(&persoana, sizeof(persoana));
    if ((recvfrom(sd, (void *)msg, 1024, 0, (struct sockaddr *)&persoana, &length)) <= 0)
    {
      perror("[client]Eroare la recvfrom() de la client.\n");
      return errno;
    }
    if (atoi(get_word_on_pos(msg, aux, 1, '*')) == 0)
    {
      //Deciding if to add or not to queue
      //IF FILE NOT IN QUEUE ADD IT

      //ADD FILE TO QUEUE
      if (waiting_for_files == 0)
      {
        //Receving download details from server
        waiting_for_files = 1;
        last_k = 0;
        if (DEBUG)
          printf("Receving download new file \n");
        //Forming the msg to send
        strcpy(aux2, "1*");
        strcat(aux2, get_word_on_pos(msg, aux, 2, '*'));
        strcat(aux2, "*");
        strcat(aux2, get_word_on_pos(msg, aux, 3, '*'));
        strcat(aux2, "*");
        strcat(aux2, get_word_on_pos(msg, aux, 4, '*'));
        strcpy(first_file, aux2);
        strcat(aux2, "*");
        strcat(aux2, addres_send);

        strcpy(msg_struct, aux2);

        bzero(finished_chunks, 1000000);
        max_pack = ceil(((float)atoi(get_word_on_pos(msg, aux, 4, '*'))) / (float)TRANSFER_SIZE);
        strcat(aux2, "*0*");
        sprintf(aux, "%d", max_pack);
        strcat(aux2, aux);
        strcpy(send_to, get_word_on_pos(msg, aux, 5, '*'));
        strcat(send_to, "*");
        strcat(send_to, get_word_on_pos(msg, aux, 6, '*'));
        send_to_seeders(aux2, send_to, sd);
        strcpy(file_name, get_word_on_pos(msg, aux2, 2, '*'));
        strcpy(aux, path);
        get_word_on_pos(msg, aux2, 2, '*');
        strcat(aux, aux2);
        if (DOWNLOAD)
        {
          if (atoi(get_word_on_pos(msg, aux2, 3, '*')) != 4)
          {
            cpy_to = fopen(aux, "wb+");
            special_fix = 1;
          }
          else
          {
            mkdir(aux, 0777);
          }
        }
      }
      else
      {
        //IF FILE IS NOT IN QUEUE PUT IT THERE
        strcpy(aux, get_word_on_pos(msg, aux2, 2, '*'));
        strcat(aux, "*");
        strcat(aux, get_word_on_pos(msg, aux2, 3, '*'));
        strcat(aux, "*");
        strcat(aux, get_word_on_pos(msg, aux2, 4, '*'));
        int ok = 0;
        for (int pp = 0; pp < queue_index_end; pp++)
          if (strcmp(queue[pp], aux) == 0)
          {
            ok = 1;
          }
        if (ok == 0)
        {
          strcpy(queue[queue_index_end], aux);
          if (queue_index_end == 0)
          {
            //Start download
            queue_index_cur = 0;
            get_word_on_pos(queue[queue_index_cur], aux, '*', 1);
            printf("Nr:%d  Name :%s\n", queue_index_cur, queue[queue_index_cur]);
            strcpy(aux, path);
            get_word_on_pos(queue[queue_index_cur], aux2, 1, '*');
            strcat(aux, aux2);
            if (DOWNLOAD)
            {
              if (atoi(get_word_on_pos(queue[queue_index_cur], aux2, 2, '*')) != 4)
                cpy_to = fopen(aux, "wb+");
              else
              {
                mkdir(aux, 0777);
              }
            }
            for (int j = 0; j < max_pack; j++)
              finished_chunks[j] = 0;
            max_pack = ceil(((float)atoi(get_word_on_pos(queue[queue_index_end], aux, 3, '*'))) / (float)TRANSFER_SIZE);
            strcpy(aux, "1*");
            strcat(aux, queue[queue_index_end]);
            strcat(aux, "*");
            strcat(aux, addr);
            strcpy(msg_struct, aux);
            sprintf(aux2, "*0*%d*", max_pack);
            strcat(aux, aux2);
            send_to_seeders(aux, send_to, sd);
            strcpy(file_name, get_word_on_pos(queue[queue_index_end], aux2, 1, '*'));
          }
          if (DEBUG)
            printf("%s\n", queue[queue_index_end]);
          queue_index_end++;
        }
      }
      retry_check = 0;
      last_msg_time = clock();
    }
    else if (atoi(get_word_on_pos(msg, aux, 1, '*')) == 1)
    {
      //Request to send file to
      int fk = fork();
      if (fk == 0)
      {
        char dir[1024];
        getcwd(dir, sizeof(dir));
        if (atoi(get_word_on_pos(msg, aux, 3, '*')) != 4)
        {
          if (DEBUG)
            printf("Sending file %s \n", msg);
          send_file(msg, sd, dir);
        }
        else
        {
          if (DEBUG)
            printf("Sending directory %s \n", msg);
          get_word_on_pos(msg, aux2, 5, '*');
          send_directory(get_word_on_pos(msg, aux, 2, '*'), aux2, sd, dir);
        }

        exit(0);
      }
    }
    else if (atoi(get_word_on_pos(msg, aux, 1, '*')) == 2)
    {
      //Receving and verifing packet
      last_msg_time = clock();
      retry_check = 0;
      if (waiting_for_files && finished_chunks[atoi(get_word_on_pos(msg, aux, 3, '*'))] == 0 && (strcmp(get_word_on_pos(msg, aux, 2, '*'), get_word_on_pos(queue[queue_index_cur], aux2, 1, '*')) == 0 || special_fix == 1))
      {
        if (DEBUG)
          printf("Receving file nr %d \n", atoi(get_word_on_pos(msg, aux, 3, '*')));
        else
        {
          int kk = ceil((float)atoi(get_word_on_pos(msg, aux, 3, '*')) / (float)max_pack * 100.0);
          nice_printf(last_k, kk);
          last_k = kk;
        }

        bzero(aux2, 1024);
        finished_chunks[atoi(get_word_on_pos(msg, aux, 3, '*'))] = 1;
        if (DOWNLOAD)
        {
          fseek(cpy_to, TRANSFER_SIZE * (atoi(get_word_on_pos(msg, aux, 3, '*'))), SEEK_SET);
          get_word_on_pos_and_after(msg, aux2, 5, '*');
          fputs(aux2, cpy_to);
          fflush(cpy_to);
        }
        if (atoi(get_word_on_pos(msg, aux, 3, '*')) == max_pack - 1)
        { //ERROR CHECKING
          int check = 0;
          int beg = -1, end = -1;

          for (int k = 0; k < max_pack; k++)
          {
            if (finished_chunks[k] != 1 && beg == -1)
            {
              check = 1;
              if (DEBUG)
                printf("Error at %d", k);
              fflush(stdout);
              beg = k;
            }
            if ((finished_chunks[k] == 1 && beg != -1) || (k == max_pack - 1 && beg != -1))
            {
              if (DEBUG)
                printf(" till %d\n", k + 1);
              fflush(stdout);
              end = k;
              //TO DO REPAIR SENDING
              strcpy(aux2, msg_struct);
              sprintf(aux, "*%d*%d*", beg, end + 1);
              strcat(aux2, aux);
              send_to_seeders(aux2, send_to, sd);
              beg = -1;
              end = -1;
            }
          }
          if (check == 0)
          {
            queue_index_cur++;
            if (queue[queue_index_cur][0])
            {
              //Start download
              if (DEBUG)
                printf("One file finished Hurry,nr %d. Next %s\n", queue_index_cur, queue[queue_index_cur]);
              else
              {
                nice_printf(last_k, 100);
                last_k = 0;
                get_word_on_pos(queue[queue_index_cur], aux, '*', 1);
                printf("\nNr:%d  Name :%s\n", queue_index_cur, queue[queue_index_cur]);
              }
              strcpy(aux, path);
              get_word_on_pos(queue[queue_index_cur], aux2, 1, '*');
              strcat(aux, aux2);
              if (DOWNLOAD)
              {
                if (atoi(get_word_on_pos(queue[queue_index_cur], aux2, 2, '*')) != 4)
                  cpy_to = fopen(aux, "wb+");
                else
                {
                  mkdir(aux, 0777);
                }
              }
              for (int j = 0; j < max_pack; j++)
                finished_chunks[j] = 0;
              max_pack = ceil(((float)atoi(get_word_on_pos(queue[queue_index_cur], aux, 3, '*'))) / (float)TRANSFER_SIZE);
              strcpy(aux, "1*");
              strcat(aux, queue[queue_index_cur]);
              strcat(aux, "*");
              strcat(aux, addr);
              strcpy(msg_struct, aux);
              sprintf(aux2, "*0*%d*", max_pack);
              strcat(aux, aux2);
              send_to_seeders(aux, send_to, sd);
              strcpy(file_name, get_word_on_pos(queue[queue_index_cur], aux2, 1, '*'));
            }
            else
            {
              queue_index_cur--;
            }
          }
          //TO DO MAKE IT SWICH TO THE NEXT FILE BEFORE FINISHING
        }
      }
    }
    else if (atoi(get_word_on_pos(msg, aux, 1, '*')) == 9)
    {
      if (waiting_for_files)
      {
        if (retry_check == 0)
        {
          //ERROR CHECKING
          int check = 0;
          int beg = -1, end = -1;

          for (int k = 0; k < max_pack; k++)
          {
            if (finished_chunks[k] != 1 && beg == -1)
            {
              if (k == 0)
                break;
              check = 1;
              if (DEBUG)
                printf("Error at %d", k);
              fflush(stdout);
              beg = k;
            }
            if ((finished_chunks[k] == 1 && beg != -1) || (k == max_pack - 1 && beg != -1))
            {
              if (DEBUG)
                printf(" till %d\n", k + 1);
              fflush(stdout);
              end = k;
              strcpy(aux2, msg_struct);
              sprintf(aux, "*%d*%d*", beg, end + 1);
              strcat(aux2, aux);
              send_to_seeders(aux2, send_to, sd);
              beg = -1;
              end = -1;
            }
          }
          if (check == 1)
          {
            retry_check = 1;
            if (DEBUG)
              printf("Error : did not recive full msg,retrying \n");
            fflush(stdout);
          }
          else
          {
            queue_index_cur++;
            if (queue[queue_index_cur][0])
            {
              //Start download
              if (DEBUG)
                printf("One file finished Hurry,nr %d. Next %s\n", queue_index_cur, queue[queue_index_cur]);
              else
              {
                nice_printf(last_k,100);
                last_k = 0;
                get_word_on_pos(queue[queue_index_cur], aux, '*', 1);
                printf("\nNr:%d  Name :%s\n", queue_index_cur, queue[queue_index_cur]);
              }
              strcpy(aux, path);
              get_word_on_pos(queue[queue_index_cur], aux2, 1, '*');
              strcat(aux, aux2);
              if (DOWNLOAD)
              {
                if (atoi(get_word_on_pos(queue[queue_index_cur], aux2, 2, '*')) != 4)
                  cpy_to = fopen(aux, "wb+");
                else
                {
                  mkdir(aux, 0777);
                }
              }
              for (int j = 0; j < max_pack; j++)
                finished_chunks[j] = 0;
              max_pack = ceil(((float)atoi(get_word_on_pos(queue[queue_index_cur], aux, 3, '*'))) / (float)TRANSFER_SIZE);
              strcpy(aux, "1*");
              strcat(aux, queue[queue_index_cur]);
              strcat(aux, "*");
              strcat(aux, addr);
              strcpy(msg_struct, aux);
              sprintf(aux2, "*0*%d*", max_pack);
              strcat(aux, aux2);
              send_to_seeders(aux, send_to, sd);
              strcpy(file_name, get_word_on_pos(queue[queue_index_cur], aux2, 1, '*'));
            }
            else
            {
              if (atoi(get_word_on_pos(first_file, aux, 3, '*')) == 4)
              {
                chdir("shared");
                if (atoi(get_word_on_pos(first_file, aux, 4, '*')) != find_size(get_word_on_pos(first_file, aux2, 2, '*')))
                {
                  printf("Did not recive entire folder,retry.Expected %d | Got %ld \n", atoi(get_word_on_pos(first_file, aux, 4, '*')), find_size(get_word_on_pos(first_file, aux2, 2, '*')));
                }
                else
                {
                  printf("\nAll files finished downloading\n");
                }
                chdir("..");
              }
              else
              {
                nice_printf(last_k,100);
                printf("\nAll files finished downloading\n");
              }
              int kk = 1;
              write(pipe_download[1], &kk, sizeof(int));
              waiting_for_files = 0;
              special_fix = 0;
              queue_index_cur = 0;
              for (int i = 0; i < queue_index_end; i++)
                bzero(queue[i], 100);
              queue_index_end = 0;
            }
          }
        }
        else
        {
          printf("Connection to all seeders lost");
          waiting_for_files = 0;
          special_fix = 0;
          //DELETE QUEUE AND MAKE WAITING 0
          if (DOWNLOAD)
          {
            strcpy(aux, path);
            strcat(aux, file_name);
            remove(aux);
          }
          //TO DO RESET FINISHED CHUNKS
        }
      }
    }
    fflush(stdout);
  }

  return 1;
}
int main(int argc, char *argv[])
{
  int sd;                    // descriptorul de socket
  struct sockaddr_in server; // structura folosita pentru conectare
  char msg[MSG_SIZE];        // mesajul trimis
  /* stabilim portul */
  port = atoi("8080");

  /* cream socketul */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[client] Eroare la socket().\n");
    return errno;
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(SERVER_ADDRES);
  server.sin_port = htons(port);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }
  strcpy(msg, "my_files_are_this ");
  strcat(msg, get_files());
  if (write(sd, msg, MSG_SIZE) <= 0)
  {
    perror("[client]Eroare la write() spre server.\n");
    return errno;
  }
  if (read(sd, msg, MSG_SIZE) < 0)
  {
    perror("[client]Eroare la read() de la server.\n");
    return errno;
  }
  strcpy(msg, "get_my_addr");
  if (write(sd, msg, MSG_SIZE) <= 0)
  {
    perror("[client]Eroare la write() spre server.\n");
    return errno;
  }
  if (read(sd, msg, MSG_SIZE) < 0)
  {
    perror("[client]Eroare la read() de la server.\n");
    return errno;
  }
  int pipe_download[2];
  if (pipe(pipe_download) < 0)
    perror("Eroare la pipe ~560");
  int fd;
  fd = fork();
  if (fd == 0)
  {
    UDP(msg, pipe_download);
    _exit(1);
  }
  else
  {
    while (1)
    {
      /* citirea mesajului */
      bzero(msg, MSG_SIZE);
      printf("[sent]: ");
      fflush(stdout);
      read(0, msg, MSG_SIZE);
      msg[strlen(msg) - 1] = 0;
      if (strcmp(msg, "exit") == 0)
      {
        printf("Good Bye\n");
        kill(fd, SIGKILL);
        return 0;
      }
      else if (strcmp(msg, "refresh") == 0)
      {
        strcpy(msg, "my_files_are_this ");
        strcat(msg, get_files());
      }
      /* trimiterea mesajului la server */
      if (write(sd, msg, MSG_SIZE) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }
      /* citirea raspunsului dat de server 
     (apel blocant pina cind serverul raspunde) */
      if (read(sd, msg, MSG_SIZE) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }

      /* afisam mesajul primit */
      printf("[got] : %s\n", msg);
      if (strcmp(msg, "File started downloading") == 0)
      {
        int k = 1;
        read(pipe_download[0], &k, sizeof(int));
        strcpy(msg, "my_files_are_this ");
        strcat(msg, get_files());
        /* trimiterea mesajului la server */
        write(sd, msg, MSG_SIZE);
        /* citirea raspunsului dat de server 
     (apel blocant pina cind serverul raspunde) */
        read(sd, msg, MSG_SIZE);
      }
    }
    /* inchidem conexiunea, am terminat */
    close(sd);
  }
}
