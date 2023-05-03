#include <arpa/inet.h>
#include <stdbool.h>
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

typedef struct client_s {
    int    id;
    char   acc[4096 + 1];
    size_t size;
} client_t;

client_t clients[FD_SETSIZE] = { 0 };

void add_client( int fd ) {
    static int id;
    client_t * client = clients + fd;
    client->id        = id++;
    client->size      = 0;
}

char msg[8196];

void broadcast( int fd, fd_set *wfds ) {
    for ( int i = 0; i < FD_SETSIZE; i++ ) {
        if ( i == fd || !FD_ISSET( i, wfds ) ) { continue; }
        send( i, msg, strlen( msg ), 0 );
    }
}

int main( int argc, char **argv ) {
    if ( argc != 2 ) {
        puterrln( "Wrong number of arguments" );
        return EXIT_FAILURE;
    }
    int fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd == -1 ) { fatal(); }
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
            sprintf( msg, "server: client %d just arrived\n", clients[cfd].id );
            broadcast( cfd, &wfds );
            continue;
        }
        for ( int i = 0; i < FD_SETSIZE; i++ ) {
            if ( i == fd || !FD_ISSET( i, &rfds ) ) { continue; }
            char    buff[1024];
            ssize_t n = recv( i, buff, 1024, 0 );
            if ( n <= 0 ) {
                sprintf( msg, "server: client %d just left\n", clients[i].id );
                broadcast( i, &wfds );
                FD_CLR( i, &fds );
                continue;
            }
            for ( const char *ptr = buff; ptr != buff + n; ptr++ ) {
                client_t *c       = clients + i;
                c->acc[c->size++] = *ptr;
                if ( *ptr == '\n' ) {
                    c->acc[c->size] = '\0';
                    sprintf( msg, "client %d: %s", c->id, c->acc );
                    broadcast( i, &wfds );
                    c->size = 0;
                }
            }
        }
    }
}
