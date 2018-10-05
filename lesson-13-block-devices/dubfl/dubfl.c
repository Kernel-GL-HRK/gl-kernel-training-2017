#include "../common.h"
#include <linux/ioctl.h>
#include "ioctl.h"

static int major = 0;           // major номер устройства 
module_param( major, int, 0 );
static char* file = NULL;       // имя файла образа диска
module_param( file, charp, 0 ); 

struct own_disk {               // Структура диска:
   path_string_t fname;         // - имя связанного файла
   unsigned int ssize;          // - размер в секторах
   loff_t bsize;                // - размер в байтах
   short media_change;          // - признак смены носителя
   u8 *data;                    // - образ диска в RAM
   struct gendisk *gd;          
   struct file *f; 
   struct request_queue *queue; 
   spinlock_t lock;
} device;

int my_open( struct block_device* dev, fmode_t mode ) { 
   struct gendisk *gd = dev->bd_disk;
   LOG( "open device /dev/%s\n", gd->disk_name );
   return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) 
int my_release( struct gendisk *gd, fmode_t mode ) { 
#else
void my_release( struct gendisk *gd, fmode_t mode ) { 
#endif
   LOG( "close device /dev/%s\n", gd->disk_name );
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) 
   return 0;
#endif
}

int my_media_changed ( struct gendisk *gd ) {   // проверка смены носителя
   return ((struct own_disk*)(gd->private_data))->media_change;
}

int my_revalidate( struct gendisk* gd ) {       // обновление носителя 
   struct own_disk* bdev = gd->private_data;
   if( bdev->media_change ) {
      // ...
      bdev->media_change = 0;
   }
   return bdev->media_change;
}

inline void transfer( struct own_disk *dev, unsigned long sector,
                      unsigned long nsect, char *buffer, int write ) { // запись данных
   unsigned long offset = sector * KERNEL_SECTOR_SIZE;
   unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
   if( write )
      memcpy( dev->data + offset, buffer, nbytes );
   else
      memcpy( buffer, dev->data + offset, nbytes );
}

static void my_request( struct request_queue *q ) {   // запрос из очереди
   struct request *req;
   unsigned long nr_sectors, sector;
   req = blk_fetch_request( q );
   while( req ) {
      struct own_disk *dev = req->rq_disk->private_data;
      if( !blk_fs_request( req ) ) {
         ERR( "skip non-fs request\n" );
         __blk_end_request_all( req, -EIO );
         req = blk_fetch_request( q );
         continue;
      }
      nr_sectors = blk_rq_cur_sectors( req );
      sector = blk_rq_pos( req );
      DBG( "entering request routine: sector=%lu length=%lu\n", sector, nr_sectors );
      if( ( sector + nr_sectors ) > dev->ssize ) {
         ERR( "beyond-end read/write (%lu %lu)\n", sector, nr_sectors );
         __blk_end_request_all( req, -EIO );
         req = blk_fetch_request( q );
         continue;
      }
      transfer( dev, sector, nr_sectors, req->buffer, rq_data_dir( req ) );
      if( !__blk_end_request_cur( req, 0 ) )
         req = blk_fetch_request( q );
   }
}

static void save_file( struct own_disk *dev ) {
   loff_t off1;
   size_t nb = 0;
   off1 = vfs_llseek( dev->f, 0, SEEK_SET );
   {  mm_segment_t fs; 
      unsigned long i = 0;
      fs = get_fs(); 
      set_fs( get_ds() ); 
      while( off1 < dev->bsize ) {
         //extern ssize_t vfs_write(struct file *, const char __user *, size_t, loff_t *);
         nb = vfs_write( dev->f, dev->data + off1, KERNEL_SECTOR_SIZE, &off1 );
         i++;
      }
      set_fs( fs ); 
      LOG( "write to file %s %lu sektors\n", file, i );         
   } 
}

static void unlink_file( struct own_disk *dev ) {
   if( dev->data ) vfree( dev->data );
   if( dev->f ) filp_close( dev->f, NULL );
   LOG( "unlink file: %s\n", dev->fname );
   dev->f = NULL; dev->data = NULL; strcpy( dev->fname, "" ); 
   dev->ssize = dev->bsize = 0;
}

static int load_file( struct own_disk* dev, char* fname ) {
   struct file *fil;
   loff_t off1, off2;
   size_t nb = 0;
   strncpy( dev->fname, fname, MAX_PATH_LENGTH );
   if( 0 == strlen( fname ) ) { 
      ERR( "undefined file name\n" ); return -ENOENT;
   }
   if( IS_ERR( fil = filp_open( dev->fname, O_RDWR, 0 ) ) ) { 
       ERR( "file open failed: %s\n", file ); 
       return -ENOENT;
   }
   if( NULL == fil ) {
       ERR( "file open failed: %s\n", file );
       return -ENOENT;
   }
   if( dev->f ) filp_close( dev->f, NULL );
   dev->f = fil;
   off2 = vfs_llseek( dev->f, 0, SEEK_END );
   LOG( "open file: %s, length %llu\n", file, off2 ); 
   off1 = vfs_llseek( dev->f, 0, SEEK_SET );
   dev->ssize = ( off2 - off1 ) / KERNEL_SECTOR_SIZE;    // ёмкость - секторов
   dev->bsize = dev->ssize * KERNEL_SECTOR_SIZE;         // ёмкость - байт
   if( dev->data ) vfree( dev->data );
   if( ( dev->data = vmalloc( dev->bsize ) ) == NULL ) { // образ диска в RAM
      ERR( "vmalloc failure\n" );
      unlink_file( dev );
      return -ENOMEM;
   }
   {  mm_segment_t fs; 
      unsigned long i = 0;
      fs = get_fs(); 
      set_fs( get_ds() ); 
      while( off1 < dev->bsize ) {
         //extern ssize_t vfs_read(struct file *, char __user *, size_t, loff_t *);
         nb = vfs_read( dev->f, dev->data + off1, KERNEL_SECTOR_SIZE, &off1 );
         i++;
      }
      set_fs( fs ); 
      LOG( "read from file %s %lu sektors\n", file, i );
   } 
   return 0;
}

#include "../ioctl.c"
static int new_ioctl( struct block_device *bdev, fmode_t mode,
                      unsigned int cmd, unsigned long arg ) {
   struct own_disk* dev = bdev->bd_disk->private_data;
   if( DUBFL_GET_INFO == cmd || DUBFL_SAVE_DISK == cmd ||
       DUBFL_UNLINK_DISK == cmd || DUBFL_LOAD_DISK == cmd ) 
      LOG( "ioctl cmd=%X\n", cmd );
   switch( cmd ) {
      case DUBFL_GET_INFO: {
         info_struct_t info;
         strncpy( info.path, dev->fname, MAX_PATH_LENGTH );
         info.bsize = dev->bsize; 
         if( copy_to_user( (void __user*)arg, &info, sizeof( info_struct_t ) ) ) return -EFAULT;
         return 0;  
      }
      case DUBFL_SAVE_DISK:
         if( NULL == dev->f || NULL== dev->data ) return -EFAULT;;
         save_file( dev );
         return 0; 
      case DUBFL_UNLINK_DISK:
         unlink_file( dev );
         return 0; 
      case DUBFL_LOAD_DISK: {
         path_string_t path;
         if( copy_from_user( &path, (void __user*)arg, sizeof( path_string_t ) ) ) return -EFAULT;
         LOG( "new loading file path = %s\n", path );
         load_file( dev, path );
         return 0;
      }
      default:
         return my_ioctl( bdev, mode, cmd, arg );
   }
}

static struct block_device_operations fops = { // device operations
   .owner           = THIS_MODULE,
   .open            = my_open,
   .release         = my_release,
   .media_changed   = my_media_changed,
   .revalidate_disk = my_revalidate,
   .ioctl           = new_ioctl,
   .getgeo          = my_getgeo
};

static int setup_device( struct own_disk* dev, char* fname ) { 
   load_file( &device, fname );                     // подключить файл образа
   spin_lock_init( &dev->lock );
   if( ( dev->queue = blk_init_queue( my_request, &dev->lock ) ) == NULL ) {
      ERR( "init queue failure\n" ); goto end0;
   }
   dev->queue->queuedata = dev;
   if( !( dev->gd = alloc_disk( 16 ) ) ) {          // Число разделов при разбиении
      ERR( "alloc_disk failure\n" ); goto end0;
   }
   set_capacity( dev->gd, dev->ssize );
   dev->gd->major = major;
   dev->gd->first_minor = 1;
   dev->gd->fops = &fops;
   dev->gd->queue = dev->queue;
   dev->gd->private_data = dev;
   strncpy( dev->gd->disk_name, DEVICE_NAME, DISK_NAME_LEN - 1 );
   add_disk( dev->gd );
   LOG( "device %s has capacity %d sectors\n", dev->gd->disk_name, dev->ssize ); 
   return 0;
end0:
   unlink_file( dev );
   return -EIO;
}

static int __init blk_init( void ) {
   int ret;
   LOG( "+ + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +" );
   if( ( major = register_blkdev( major, DEVICE_NAME ) ) <= 0 ) {
      ERR( "unable to get major number\n" );
      return -EBUSY;
   };
   memset( &device, 0, sizeof( struct own_disk ) );
   if( ( ret = setup_device( &device, file ) ) != 0 ) {
      unregister_blkdev( major, DEVICE_NAME );
   }
   return ret;
}

static void blk_exit( void ) {
   save_file( &device );
   unlink_file( &device );
   del_gendisk( device.gd );
   put_disk( device.gd );
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
   blk_put_queue( device.queue );
#endif
   unregister_blkdev( major, DEVICE_NAME );
   LOG( "+ + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + + +" );
}

module_init( blk_init );
module_exit( blk_exit );

MODULE_LICENSE( "GPL v2" );
MODULE_VERSION( "2.5" );
MODULE_AUTHOR( "Oleg Tsiliuric <olej@front.ru>" );
