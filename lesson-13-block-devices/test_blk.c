#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define SIZE 2122
#define CHK  250

int main( int argc, char *argv[] ) {
   int j, length, fd, rc;
   int vector[ SIZE ];
   off_t offset;
   if( argc != 2 ) {
      printf( "usage: %s <device>\n", argv[ 0 ] );
      exit( 1 );
   }
   length = sizeof(int) * SIZE;
   offset = sizeof(int) * CHK;
//   char sdev[ 40 ];
//   sprintf( sdev, "/dev/%s", argv[ 1 ] );
//   fd = open( sdev, O_RDWR );
   fd = open( argv[ 1 ], O_RDWR );
   if( ( fd = open( argv[ optind ], O_RDWR ) ) < 0 ) {
      sprintf( (char*)&vector, "ошибка open %s", argv[ 1 ] );
      perror( (char*)&vector );
      exit( EXIT_FAILURE );
   }
   for( j = 0; j < SIZE; j++ ) vector[ j ] = j;
   rc = write( fd, vector, length );
   printf( "**** return code from write = %d\n", rc );
   rc = lseek( fd, offset, SEEK_SET );
   printf( "**** retrun code from lseek(%d) = %d \n", (int)offset, rc );
   rc = read( fd, vector + CHK, sizeof(int) );
   printf( "**** retrun code from read vec[%d] = %d, vec[%d] = %d \n", CHK, rc, CHK, vector[CHK]);
   close( fd );
   exit( EXIT_SUCCESS );
}
