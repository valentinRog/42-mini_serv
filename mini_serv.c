#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

void puterrln( const char *msg ) {
    write( STDERR_FILENO, msg, strlen( msg ) );
    write( STDERR_FILENO, "\n", 1 );
}

void fatal() {
    puterrln( "Fatal error" );
    exit( EXIT_FAILURE );
}

void *xmalloc( size_t size ) {
    void *ptr = malloc( size );
    if ( !ptr ) { fatal(); }
    return ptr;
}

char *xstrdup( const char *s ) {
    char *str = xmalloc( sizeof( char ) * ( strlen( s ) + 1 ) );
    strcpy( str, s );
    return str;
}

typedef struct list_node {
    void *            data;
    struct list_node *next;
    struct list_node *prev;
} list_node;

typedef struct list {
    list_node *head;
    list_node *tail;
    size_t     size;
} list;

void list_push_back( list *l, void *data ) {
    list_node *node = xmalloc( sizeof( list_node ) );
    node->data      = data;
    node->next      = NULL;
    node->prev      = l->tail;
    if ( l->tail ) { l->tail->next = node; }
    l->tail = node;
    if ( !l->head ) { l->head = node; }
    l->size++;
}

void list_remove( list *l, list_node *node ) {
    if ( node->prev ) { node->prev->next = node->next; }
    if ( node->next ) { node->next->prev = node->prev; }
    if ( l->head == node ) { l->head = node->next; }
    if ( l->tail == node ) { l->tail = node->prev; }
    free( node );
    l->size--;
}

typedef struct client_s {
    int   id;
    int   fd;
    char  acc[4096 + 1];
    size_t size;
    list  msg_queue;
} client_t;

list clients = { 0 };

void destroy_client( client_t *client ) {
    for ( list_node *node = client->msg_queue.head; node; ) {
        free( node->data );
        list_node *tmp = node->next;
        free( node );
        node = tmp;
    }
}

void add_client( int fd ) {
    static int id;
    client_t * client = xmalloc( sizeof( client_t ) );
    client->id        = id++;
    client->fd        = fd;
    client->size      = 0;
    bzero( &client->msg_queue, sizeof( list ) );
    list_push_back( &clients, client );
}

void broadcast( int fd, const char *msg ) {
    for ( list_node *node = clients.head; node; node = node->next ) {
        client_t *client = node->data;
        if ( client->fd != fd ) {
            list_push_back( &client->msg_queue, xstrdup( msg ) );
        }
    }
}

int main( int argc, char **argv ) {
    if ( argc != 2 ) {
        puterrln( "Wrong number of arguments" );
        return EXIT_FAILURE;
    }
    int fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd == -1 ) { fatal(); }
    // set socket reusable
    int optval = 1;
    if ( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof( optval ) )
         == -1 ) {
        fatal();
    }
    //
    struct sockaddr_in addr = { 0 };
    addr.sin_family         = AF_INET;
    addr.sin_port           = htons( atoi( argv[1] ) );
    addr.sin_addr.s_addr    = htonl( 2130706433 );
    if ( bind( fd, ( struct sockaddr * ) &addr, sizeof( addr ) ) == -1 ) {
        fatal();
    }
    if ( listen( fd, 100 ) == -1 ) { fatal(); }
    fd_set fds;
    FD_ZERO( &fds );
    FD_SET( fd, &fds );
    char msg[8196];
    while ( ~0 ) {
        fd_set rfds = fds;
        fd_set wfds = fds;
        FD_CLR( fd, &wfds );
        if ( select( FD_SETSIZE, &rfds, &wfds, NULL, NULL ) == -1 ) { fatal(); }
        if ( FD_ISSET( fd, &rfds ) ) {
            int cfd = accept( fd, NULL, NULL );
            if ( cfd == -1 ) { fatal(); }
            add_client( cfd );
            FD_SET( cfd, &fds );
            sprintf( msg,
                     "server: client %d just arrived\n",
                     ( ( client_t * ) clients.tail->data )->id );
            broadcast( cfd, msg );
        }
        for ( list_node *node = clients.head; node; node = node->next ) {
            client_t *client = node->data;
            if ( FD_ISSET( client->fd, &rfds ) ) {
                char    buff[1024];
                ssize_t n = recv( client->fd, buff, sizeof( buff ), 0 );
                if ( n <= 0 ) {
                    list_node *tmp = node->prev;
                    FD_CLR( client->fd, &fds );
                    close( client->fd );
                    list_remove( &clients, node );
                    sprintf( msg, "server: client %d just left\n", client->id );
                    broadcast( client->fd, msg );
                    destroy_client( client );
                    node = tmp;
                    if ( !node ) { break; }
                    continue;
                }
                for ( const char *ptr = buff; ptr != buff + n; ptr++ ) {
                    if ( *ptr == '\n' ) {
                        sprintf( msg,
                                 "client %d: %s\n",
                                 client->id,
                                 client->acc );
                        broadcast( client->fd, msg );
                        client->size = 0;
                    } else {
                        client->acc[client->size++] = *ptr;
                        client->acc[client->size] = '\0';
                    }
                }
            }
            if ( FD_ISSET( client->fd, &wfds ) ) {
                if ( !client->msg_queue.size ) { continue; }
                char *  msg = client->msg_queue.head->data;
                ssize_t n   = send( client->fd, msg, strlen( msg ), 0 );
                if ( n == -1 ) { fatal(); }
                free( msg );
                list_remove( &client->msg_queue, client->msg_queue.head );
            }
        }
    }
}
