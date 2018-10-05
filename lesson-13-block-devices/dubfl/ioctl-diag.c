#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "ioctl.h"

void show( int cmd ) {
   fprintf( stdout, "команда %X => направление=%d, тип(магик)=%c, номер=%d, размер=%d\n", 
             cmd, _IOC_DIR( cmd ), _IOC_TYPE( cmd ), _IOC_NR( cmd ), _IOC_SIZE( cmd ) ); 
};

int main( int argc, char *argv[] ) {
   int cmds[] = { 
      DUBFL_GET_INFO,
      DUBFL_SAVE_DISK,
      DUBFL_UNLINK_DISK,
      DUBFL_LOAD_DISK,
                }, i;
   fprintf( stdout, "определённые команды: \n" );
   for( i = 0; i < sizeof( cmds ) / sizeof( cmds[ 0 ] ); i++ ) show( cmds[ i ] );
   while( 1 ) { 
      char buf[ 10 ];
      fprintf( stdout, "HEX код команды: " );
      fflush( stdout ); 
      fgets( buf, sizeof( buf ), stdin );
      sscanf( buf, "%X", &i );
      show( i ); 
   }
   return EXIT_SUCCESS;
};

/* used to decode ioctl numbers.. 
#define _IOC_DIR(nr)            (((nr) >> _IOC_DIRSHIFT) & _IOC_DIRMASK)
#define _IOC_TYPE(nr)           (((nr) >> _IOC_TYPESHIFT) & _IOC_TYPEMASK)
#define _IOC_NR(nr)             (((nr) >> _IOC_NRSHIFT) & _IOC_NRMASK)
#define _IOC_SIZE(nr)           (((nr) >> _IOC_SIZESHIFT) & _IOC_SIZEMASK)
*/