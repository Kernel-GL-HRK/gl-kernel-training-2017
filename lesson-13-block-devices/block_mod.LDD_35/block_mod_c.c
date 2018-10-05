#include "../common.h"

static int mybdrv_ma_no = 0;
static int disk_size = 0;
static char *my_dev;
static struct gendisk *my_gd;
static spinlock_t lock;

static struct request_queue *my_request_queue;

#include "../request.c"
#include "../ioctl.c"
#include "../common.c"

#define MY_DEVICE_NAME "xbc"

static int __init blk_init( void ) {
   disk_size = diskmb * 1024 * 1024;
   spin_lock_init( &lock );
   if( !( my_dev = vmalloc( disk_size ) ) )
      return -ENOMEM;
   if( !( my_request_queue = blk_init_queue( my_request, &lock ) ) ) {
      vfree( my_dev );
      return -ENOMEM;
   }
   blk_queue_logical_block_size( my_request_queue, KERNEL_SECTOR_SIZE );
   mybdrv_ma_no = register_blkdev( mybdrv_ma_no, MY_DEVICE_NAME );
   if( mybdrv_ma_no < 0 ) {
      ERR( "failed registering block device, returned %d\n", mybdrv_ma_no );
      vfree( my_dev );
      return mybdrv_ma_no;
   }
   if( !( my_gd = alloc_disk( 16 ) ) ) {
      unregister_blkdev( mybdrv_ma_no, MY_DEVICE_NAME );
      vfree( my_dev );
      return -ENOMEM;
   }
   my_gd->major = mybdrv_ma_no;
   my_gd->first_minor = 0;
   my_gd->fops = &mybdrv_fops;
   strcpy( my_gd->disk_name, MY_DEVICE_NAME );
   my_gd->queue = my_request_queue;
   set_capacity( my_gd, disk_size / KERNEL_SECTOR_SIZE );
   add_disk( my_gd );
   LOG( "device successfully registered, major No. = %d\n", mybdrv_ma_no );
   LOG( "capacity of ram disk is: %d Mb\n", diskmb );
   return 0;
}

static void __exit blk_exit( void ) {
   del_gendisk( my_gd );
   put_disk( my_gd );
   unregister_blkdev( mybdrv_ma_no, MY_DEVICE_NAME );
   LOG( "module successfully unloaded, major No. = %d\n", mybdrv_ma_no);
   blk_cleanup_queue( my_request_queue );
   vfree( my_dev );
}

MODULE_AUTHOR( "Jerry Cooperstein" );
MODULE_AUTHOR( "Oleg Tsiliuric" );
