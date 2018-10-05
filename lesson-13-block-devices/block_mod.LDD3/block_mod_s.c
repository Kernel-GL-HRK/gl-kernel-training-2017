#include "../common.h"
#include <linux/bio.h>

static int major = 0;
module_param( major, int, 0 );
static int hardsect_size = KERNEL_SECTOR_SIZE;
module_param( hardsect_size, int, 0 );
static int ndevices = 4;
module_param( ndevices, int, 0 );
// The different "request modes" we can use:
enum {	RM_SIMPLE  = 0,	// The extra-simple request function 
	RM_FULL    = 1,	// The full-blown version 
	RM_NOQUEUE = 2,	// Use make_request 
     };
static int mode = RM_SIMPLE;
module_param( mode, int, 0 );

static int nsectors;

struct disk_dev {               // The internal representation of our device.
   int size;                    // Device size in sectors
   u8 *data;                    // The data array 
   spinlock_t lock;             // For mutual exclusion */
   struct request_queue *queue; // The device request queue */
   struct gendisk *gd;          // The gendisk structure */
};

static struct disk_dev *Devices = NULL;

static int transfer( struct disk_dev *dev, unsigned long sector,
                     unsigned long nsect, char *buffer, int write ) { // Handle an I/O request.
   unsigned long offset = sector * KERNEL_SECTOR_SIZE;
   unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
   if( (offset + nbytes) > dev->size ) {
      ERR( "beyond-end write (%ld %ld)\n", offset, nbytes );
      return -EIO;
   }
   if( write )
      memcpy( dev->data + offset, buffer, nbytes );
   else
      memcpy( buffer, dev->data + offset, nbytes );
   return 0;
}

static int xfer_bio( struct disk_dev *dev, struct bio *bio ) {   // Передача одиночного BIO.
   int i, ret;
   struct bio_vec *bvec;
   sector_t sector = bio->bi_sector;
   DBG( "entering xfer_bio routine\n" );
   bio_for_each_segment( bvec, bio, i ) { // Работаем с каждым сегментом независимо.
      char *buffer;
      sector_t nr_sectors;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
      buffer = __bio_kmap_atomic( bio, i, KM_USER0 );
#else
      buffer = __bio_kmap_atomic( bio, i );
#endif
      nr_sectors = bio_sectors( bio );
      ret = transfer( dev, sector, nr_sectors, buffer, bio_data_dir( bio ) == WRITE );
      if( ret != 0 ) return ret;
      sector += nr_sectors;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
      __bio_kunmap_atomic( bio, KM_USER0 );
#else
      __bio_kunmap_atomic( bio );
#endif
   }
   return 0;
}

static int xfer_request( struct disk_dev *dev, struct request *req ) { // Передача всего запроса.
   struct bio *bio;
   int nsect = 0;
   DBG( "entering xfer_request routine\n" );
   __rq_for_each_bio( bio, req ) {
      xfer_bio( dev, bio );
      nsect += bio->bi_size / KERNEL_SECTOR_SIZE;
   }
   return nsect;
}

static void simple_request( struct request_queue *q ) {   // простой запрос с обработкой очереди
   struct request *req;
   unsigned nr_sectors, sector;
   DBG( "entering simple request routine\n" );
   req = blk_fetch_request( q );
   while( req ) {
      int ret = 0;
      struct disk_dev *dev = req->rq_disk->private_data;
      if( !blk_fs_request( req ) ) {                     // не валидный запрос
         ERR( "skip non-fs request\n" );
         __blk_end_request_all( req, -EIO );
         req = blk_fetch_request( q );
         continue;
      }
      nr_sectors = blk_rq_cur_sectors( req );            // валидный запрос - обработка
      sector = blk_rq_pos( req );
      ret = transfer( dev, sector, nr_sectors, req->buffer, rq_data_dir( req ) );
      if( !__blk_end_request_cur( req, ret ) )
         req = blk_fetch_request( q );
   }
}

static void full_request( struct request_queue *q ) {    // запрос с обработкой вектора BIO
   struct request *req;
   int sectors_xferred;
   DBG( "entering full request routine\n" );
   req = blk_fetch_request( q );
   while( req ) {
      struct disk_dev *dev = req->rq_disk->private_data; 
      if( !blk_fs_request( req ) ) {                     // не валидный запрос
         ERR( "skip non-fs request\n" );
         __blk_end_request_all( req, -EIO );
         req = blk_fetch_request( q );
         continue;
      }
      sectors_xferred = xfer_request( dev, req );        // валидный запрос - обработка
      if( !__blk_end_request_cur( req, 0 ) )
         req = blk_fetch_request( q );
   }
}


#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,1,0) 
static int make_request( struct request_queue *q, struct bio *bio ) { 
#else
static void make_request( struct request_queue *q, struct bio *bio ) { 
#endif
   struct disk_dev *dev = q->queuedata; // Прямое выполнение запроса без очереди
   int status = xfer_bio( dev, bio );
   bio_endio( bio, status );
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,1,0) 
   return 0;
#endif
}

#include "../ioctl.c"
#include "../common.c"

#define MY_DEVICE_NAME "xd"
#define DEV_MINORS	16

static void setup_device( struct disk_dev *dev, int which ) { // Set up our internal device.
   memset( dev, 0, sizeof( struct disk_dev ) );
   dev->size = diskmb * 1024 * 1024;
   dev->data = vmalloc( dev->size );
   if( dev->data == NULL ) {
      ERR( "vmalloc failure.\n" );
      return;
   }
   spin_lock_init( &dev->lock );
   switch( mode ) { // The I/O queue, depending on whether we are using our own make_request function or not.
      case RM_NOQUEUE:
         dev->queue = blk_alloc_queue( GFP_KERNEL );
         if( dev->queue == NULL ) goto out_vfree;
         blk_queue_make_request( dev->queue, make_request );
         break;
      case RM_FULL:
         dev->queue = blk_init_queue( full_request, &dev->lock );
         if( dev->queue == NULL ) goto out_vfree;
         break;
      default:
         LOG( "bad request mode %d, using simple\n", mode );
         /* fall into.. */
      case RM_SIMPLE:
         dev->queue = blk_init_queue( simple_request, &dev->lock );
         if( dev->queue == NULL ) goto out_vfree;
         break;
   }
   blk_queue_logical_block_size( dev->queue, hardsect_size );  // Set the hardware sector size 
   dev->queue->queuedata = dev;
   dev->gd = alloc_disk( DEV_MINORS );                         // Число разделов при разбиении
   if( ! dev->gd ) {
      ERR( "alloc_disk failure\n" );
      goto out_vfree;
   }
   dev->gd->major = major;
   dev->gd->minors = DEV_MINORS;
   dev->gd->first_minor = which * DEV_MINORS;
   dev->gd->fops = &mybdrv_fops;
   dev->gd->queue = dev->queue;
   dev->gd->private_data = dev;
   snprintf( dev->gd->disk_name, DISK_NAME_LEN - 1, MY_DEVICE_NAME"%c", which + 'a' );
   set_capacity( dev->gd, nsectors * ( hardsect_size / KERNEL_SECTOR_SIZE ) );
   add_disk( dev->gd );
   return;
out_vfree:
   if( dev->data ) vfree( dev->data );
}

static int __init blk_init( void ) {
   int i;
   nsectors = diskmb * 1024 * 1024 / hardsect_size;
   major = register_blkdev( major, MY_DEVICE_NAME );
   if( major <= 0 ) {
      ERR( "unable to get major number\n" );
      return -EBUSY;
   }
   Devices = kmalloc( ndevices * sizeof( struct disk_dev ), GFP_KERNEL ); // Allocate the device array
   if( Devices == NULL ) goto out_unregister;
   for( i = 0; i < ndevices; i++ ) // Initialize each device
      setup_device( Devices + i, i );
   return 0;
out_unregister:
   unregister_blkdev( major, MY_DEVICE_NAME );
   return -ENOMEM;
}

static void blk_exit( void ) {
   int i;
   for( i = 0; i < ndevices; i++ ) {
      struct disk_dev *dev = Devices + i;
      if( dev->gd ) {
         del_gendisk( dev->gd );
         put_disk(dev->gd);
      }
      if( dev->queue ) {
         if( mode == RM_NOQUEUE ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
            blk_put_queue( dev->queue );
#endif
         }
         else
            blk_cleanup_queue( dev->queue );
      }
      if( dev->data ) vfree( dev->data );
   }
   unregister_blkdev( major, MY_DEVICE_NAME );
   kfree( Devices );
}

MODULE_AUTHOR( "Jonathan Corbet" );
MODULE_AUTHOR( "Oleg Tsiliuric" );
