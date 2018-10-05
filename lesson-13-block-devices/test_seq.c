#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define put( level, ...) if( debug >= level ) printf( __VA_ARGS__ )

int main( int argc, char *argv[] ) {
   unsigned long j = 0, k = 0, length = 10;
   int fd, rc = 0, m, debug = 0;
   unsigned char data, begn = '\0'; 
   char c, sopt[] = "l:c:v";
   while( -1 != ( c = getopt( argc, argv, sopt ) ) )
      switch( c ) {
         case 'l':              // длина последовательности
            length = atol( optarg );
            break;
         case 'c':              // начальный символ последовательности
            rc = sscanf( optarg, "%lX", &j );
            begn = (char)( j & 0xFF );
            break;
         case 'v':
            debug++;            // уровень диагностики
            break;
         default :
            printf( "usage: %s -l[<length>] [-v] <device>\n", argv[ 0 ] );
            exit( EXIT_FAILURE );
      }
   if( ( argc - optind ) != 1 ) {
      printf( "usage: %s -l[<length>] [-v] <device>\n", argv[ 0 ] );
      exit( EXIT_FAILURE );
   }
   put( 2, "length = %lu device = %s\n", length, argv[ optind ] );
   if( ( fd = open( argv[ optind ], O_RDWR ) ) < 0 ) {
      perror( "ошибка open" );
      exit( EXIT_FAILURE );
   }
   off_t off1 = -1, off2 = -1;
   if( ( off2 = lseek( fd, 0, SEEK_END ) ) < 0 ) {
      perror( "ошибка lseek" );
      exit( 1 );
   };
   put( 1, "объём устройства %lu байт\n", off2 );
   if( 0 == length || off2 - off1 < length ) length = ( off2 - off1 );  
   lseek( fd, 0, SEEK_SET );
   for( j = 0; j < length; j++ ) {
      data = ( j + begn ) & 0xFF;
      put( 2, "%02X.", (int)data );
      rc = write( fd, &data, sizeof( data ) );
      if( rc != sizeof( data ) ) {
         perror( "ошибка записи" );
         break;
      }
   }
   put( 2, "\n" );
   put( 1, "записано %lu байт\n", j );
   lseek( fd, 0, SEEK_SET );
   for( k = 0; k < length; k++ ) {
      rc = read( fd, &data, sizeof( data ) );
      if( rc <= 0 ) {
         perror( "ошибка чтения" );
         break;
      } 
      put( 2, "%02X.", (int)data );
      if( data != ( k + begn & 0xFF ) ) {
         break;
      }
   }
   put( 2, "\n" );
   put( 1, "считано %lu байт\n", k );
   if( j == k )
      printf( "считанное в точности соответствует записанному!\n" );
   else
      printf( "несоответствие в позиции %lu: %d ~ %d\n",
              k, data, (int)( k & 0xFF ) );
   close( fd );
   exit( EXIT_SUCCESS );
}


