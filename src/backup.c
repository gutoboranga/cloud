#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <dropboxUtil.h>
#include <sys/stat.h>
#include <dropboxServer.h>
#include <list.h>
#include <backup.h>
#include <process.h>


// void *primary_healthcheck() {
//   printf("checking\n");
// }

void connect_to_primary(char *ip, list_t **other_processes, process_t *self) {
  process_t *primary = malloc(sizeof(process_t));
  
  strcpy(primary->ip, ip);
  primary->role = PRIMARY;
  primary->port = DEFAULT_PORT;
  primary->socket_id = create_socket(primary->ip, DEFAULT_PORT, &(primary->address));
  
  // adiciona o primário na lista dos outros processos
  *(other_processes) = list_make_node(primary);
  
  // serializa a struct process_t do self
  char self_data_buffer[MAX_PACKAGE_DATA_LENGTH];
  memcpy(self_data_buffer, self, sizeof(process_t));
  
  // envia uma mensagem pro primário pedindo pra conectar e enviando seus dados
  message_t message;
  config_message(&message, _MSG_TYPE_CONNECT_PLEASE, 0, self_data_buffer, "");
  send_message2(primary->socket_id, message, &(primary->address));
  
  char buffer[MAX_PACKAGE_DATA_LENGTH];
  
  // aguarda resposta do primário dizendo OK
  receive_message2(primary->socket_id, buffer, MAX_PACKAGE_DATA_LENGTH);
  message_t *msg = malloc(sizeof(message_t));
  memcpy(msg, buffer, sizeof(message_t));

  // se deu algum erro, exit
  if (msg->type != _MSG_TYPE_CONNECTED) {
    printf(BACKUP_ERROR_CONNECTING_WITH_PRIMARY);
    exit(-1);
  }
  free(msg);
  
  printf(PRIMARY_CONNECTION_SUCCEDED);
}


void get_other_processes_data(list_t **other_processes, process_t *self) {
  int there_are_processes_remaining = TRUE;
  char buffer[MAX_PACKAGE_DATA_LENGTH];
  
  message_t *msg = malloc(sizeof(message_t));
  process_t *primary = (process_t *) ((list_t *) *(other_processes))->value;
  
  // agora este processo receberá do primário os dados dos outros processos
  while(there_are_processes_remaining) {
    // envia uma mensagem pro primário pedindo dados de um processo
    message_t message;
    config_message(&message, _MSG_TYPE_PLEASE_GIVE_ME_PROCESSESS_DATA, 0, "", "");
    send_message2(primary->socket_id, message, &(primary->address));
    
    // aguarda resposta do primário contendo os dados de um dos outros processos de backup (se houver)
    receive_message2(primary->socket_id, buffer, MAX_PACKAGE_DATA_LENGTH);

    // deserializa
    memcpy(msg, buffer, sizeof(message_t));
    
    // acabaram os processos
    if (msg->type == _MSG_TYPE_END_OF_PROCESS_DATA) {
      there_are_processes_remaining = FALSE;
    }
    
    // se veio algo no campo data da mensagem, tem processo
    // se não veio, acabaram os processos da lista do primário e acabou o laço
    if (strcmp(msg->data, "") != 0) {
      process_t *p = malloc(sizeof(process_t));
      memcpy(p, msg->data, sizeof(process_t));
      
      // se forem dados do primário, apenas atualiza o pid dele na lista
      if (p->role == PRIMARY) {
        primary->pid = p->pid;
      } else {
        // se o processo recebido não for o próprio
        if (p->pid != self->pid) {
          list_insert(other_processes, p);
        } else {
          memcpy(&(self->address), &(p->address), sizeof(p->address));
          strcpy(self->ip, p->ip);
        }
      }
    }
  }
  print_processes_list();
}

void connect_to_others(char **argv, list_t **other_processes, process_t *self) {
  char buffer[MAX_PACKAGE_DATA_LENGTH];
  list_t *aux = *(other_processes);
  process_t *p;
  
  // percorre a lista de outros processos
  while (aux != NULL) {
    p = (process_t *) aux->value;
    
    aux = aux->next;
    
    // se for o primário, pula
    if (p->role == PRIMARY) {
      continue;
    }
    
    // pra cada um, abre um socket
    p->socket_id = create_socket(p->ip, p->port, &(p->address));
		    
    char buffer[MAX_PACKAGE_DATA_LENGTH];
    memcpy(buffer, self, sizeof(process_t));
    
    // e envia uma mensagem dizendo "oi, eu sou o processo tal, vamos nos conectar?"
    message_t message;
    
    config_message(&message, _MSG_TYPE_BACKUP_TO_BACKUP_CONNECT_PLEASE, 0, buffer, "");
    send_message2(p->socket_id, message, &(p->address));
    
    // aguarda a resposta
    receive_message2(p->socket_id, buffer, MAX_PACKAGE_DATA_LENGTH);
    
    printf(BACKUP_CONNECTION_SUCCEDED, p->pid);
  }
}

process_t *get_process_from_pid(list_t **other_processes, int pid) {
  char buffer[MAX_PACKAGE_DATA_LENGTH];
  list_t *aux = *(other_processes);
  process_t *p = NULL;

  // percorre a lista dos processos
  while (aux != NULL) {
    p = (process_t *) aux->value;
    
    aux = aux->next;
    
    // se for o p com pid == ao que queremos
    if (p->pid == pid) {
      return p;
    }
  }
  return p;
}

list_t *list_remove_with_pid(list_t *head, int pid) {
  list_t *previous, *current;
  current = previous = head;
  
  if (head == NULL) {
    return NULL;
  }
  
  if ((int) ((process_t *) head->value)->pid == pid) {
    current = head->next;
    free(head);
    
    return current;
  }
  
  do {
    if ((int) ((process_t *) current->value)->pid == pid) {
      previous->next = current->next;
      
      free(current);
      current = NULL;
      
      return head;
    }
    
    previous = current;
    current = current->next;
    
  } while (current != NULL);
  
  return head;
}
