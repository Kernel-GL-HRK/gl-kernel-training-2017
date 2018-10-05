#include "../common.h"
#include <linux/slab.h> 

static struct gendisk *myblkdisk;            // Representation of a disk 
static struct request_queue *myblkdev_queue; // Associated request queue 
int myblkdev_major = 0;                      // Ask the block subsystem to choose a major number 
static DEFINE_SPINLOCK( myblkdev_lock );     // Spinlock that protects myblkdev_queue from concurrent access
static char *my_dev;
static int disk_size = 0;

#include "../request.c"
#include "../ioctl.c"
#include "../common.c"

#define MY_DEVICE_NAME "xbe"

static int __init blk_init( void ) { /* Initialization */
   if( ( myblkdev_major = register_blkdev( myblkdev_major, MY_DEVICE_NAME ) ) <= 0 ) // Register this block driver with the kernel 
      return -EIO;
   disk_size = diskmb * 1024 * 1024;
   if( !( my_dev = vmalloc( disk_size ) ) ) return -ENOMEM;
   myblkdev_queue = blk_init_queue( my_request, &myblkdev_lock ); // Allocate a request_queue associated with this device 
   if( !myblkdev_queue ) return -EIO;
   blk_queue_logical_block_size( myblkdev_queue, KERNEL_SECTOR_SIZE );   // Set the hardware sector size and the max number of sectors 
   blk_queue_max_hw_sectors( myblkdev_queue, 512 );
   myblkdisk = alloc_disk( 1 );                                   // Allocate an associated gendisk 
   if( !myblkdisk ) return -EIO;
   myblkdisk->fops = &mybdrv_fops;                                // Fill in parameters associated with the gendisk
   set_capacity( myblkdisk, diskmb * 1024 * 2  );                 // Set the capacity of the storage media in terms of number of sectors 
   myblkdisk->queue = myblkdev_queue;
   myblkdisk->major = myblkdev_major;
   myblkdisk->first_minor = 0;
   sprintf( myblkdisk->disk_name, MY_DEVICE_NAME );
   add_disk( myblkdisk );                                          // Add the gendisk to the block I/O subsystem 
   return 0;
}

static void __exit blk_exit( void ) { /* Exit */
   del_gendisk( myblkdisk );                            // Invalidate partitioning information and perform cleanup 
   put_disk(myblkdisk);                                 // Drop references to the gendisk so that it can be freed
   blk_cleanup_queue( myblkdev_queue );                 // Dissociate the driver from the request_queue. Internally calls elevator_exit() 
   unregister_blkdev( myblkdev_major, MY_DEVICE_NAME ); // Unregister the block device 
}

MODULE_AUTHOR( "Sreekrishnan Venkateswaran" );
MODULE_AUTHOR( "Oleg Tsiliuric" );
