#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "ioctl.h"

int main( int argc, char *argv[] ) {
   path_string_t path = "/dev/"DEVICE_NAME;
   int fd = open( path, O_RDWR );
   if( fd < 0 ) 
      fprintf( stdout, "устройство %s : ", path ), fflush( stdout ),
      perror( "" ), exit( EXIT_FAILURE );
   while( 1 ) {
      int rc = -999, nsc;
      path_string_t buf, arg = "";
      char *pwd, c;
      fprintf( stdout, "команда (h-подсказка): " );
      fflush( stdout );
      fgets( buf, sizeof( buf ) - 1, stdin );
      buf[ strlen( buf ) - 1 ] = '\0';       // забой завершающего '\n'
      nsc = sscanf( buf, "%c%s", &c, (char*)&arg );
      switch( c ) {
         case 's':                           // сброс на диск
            rc = ioctl( fd, DUBFL_SAVE_DISK, '\0' );
            break;     
         case 'u':                           // очистка связи
            rc = ioctl( fd, DUBFL_UNLINK_DISK, '\0' );
            break;     
         case 'l': {                         // связывание диска
            struct stat* stbuf;
            path_string_t full;
            if( nsc != 2 || 0 == strlen( arg ) ) {
               fprintf( stdout, "ошибочный формат команды\n" );
               continue;
            }
            if( stat( arg, &stbuf ) ) {
               fprintf( stdout, "неверное имя файла %s : %m\n", arg );
               continue;
            }
            if( '/' == arg[ 0 ] ) {          // абсолютный путь
               strcpy( full, arg );
               goto load; 
            }
            getcwd( full, MAX_PATH_LENGTH ); // текщий каталог
            if( strlen( full ) + strlen( arg ) > MAX_PATH_LENGTH - 1 ) {
               fprintf( stdout, "слишком длинный путь: %s + %s\n", full, arg );
               continue;
            }
            sprintf( full + strlen( full ), "/%s", arg );
load:       fprintf( stdout, "полный путь файла образа: %s\n", full );
            rc = ioctl( fd, DUBFL_LOAD_DISK, full );
            break;     
         }
         case 'i': {                         // информация о диске
            info_struct_t info;
            rc = ioctl( fd, DUBFL_GET_INFO, &info );
            fprintf( stdout, "связанный файл: %s\nразмер (байт): %llu\n", info.path, info.bsize ); 
            break;
         }    
         case 'q':
            close( fd );
            return EXIT_SUCCESS;
         default:
            fprintf( stdout, "недопустимая команда!\n" );
         case 'h':
            fprintf( stdout, 
                     "\ts - сохранить содержимое диска\n"
                     "\tu - отсоединить диск от образа\n"
                     "\tl <path> - связать диск с образом\n"
                     "\ti - информация о диске\n"
                     "\th - подсказка\n"
                     "\tq - выход\n"
                    );
            continue;
      }
      fprintf( stdout, "код возврата модуля: %d\n", rc );
   }
   close( fd );
   return EXIT_SUCCESS;
};
