#include <linux/blkdev.h>

#include <linux/genhd.h>


static struct gendisk *myblkdisk;     /* Representation of a disk */
static struct request_queue *myblkdev_queue;
                                      /* Associated request queue */
int myblkdev_major = 0;               /* Ask the block subsystem
                                         to choose a major number */
static DEFINE_SPINLOCK(myblkdev_lock);/* Spinlock that protects
                                         myblkdev_queue from
                                         concurrent access */
int myblkdisk_size = 256*1024;        /* Disk size in kilobytes. For
                                         a PC hard disk, one way to
                                         glean this is via the BIOS */
int myblkdev_sect_size = 512;         /* Hardware sector size */


/* Initialization */
static int __init
myblkdev_init(void)
{
    /* Register this block driver with the kernel */
    if ((myblkdev_major = register_blkdev(myblkdev_major,
                                          "myblkdev")) <= 0) {
        return -EIO;
    }

    /* Allocate a request_queue associated with this device */
    myblkdev_queue = blk_init_queue(myblkdev_request, &myblkdev_lock);
    if (!myblkdev_queue) return -EIO;

    /* Set the hardware sector size and the max number of sectors */
    blk_queue_hardsect_size(myblkdev_queue, myblkdev_sect_size);
    blk_queue_max_sectors(myblkdev_queue, 512);

    /* Allocate an associated gendisk */
    myblkdisk = alloc_disk(1);
    if (!myblkdisk) return -EIO;

    /* Fill in parameters associated with the gendisk */
    myblkdisk->fops = &myblkdev_fops;

    /* Set the capacity of the storage media in terms of number of
       sectors */
    set_capacity(myblkdisk, myblkdisk_size*2);
    myblkdisk->queue = myblkdev_queue;
    myblkdisk->major = myblkdev_major;
    myblkdisk->first_minor = 0;
    sprintf(myblkdisk->disk_name, "myblkdev");

    /* Add the gendisk to the block I/O subsystem */
    add_disk(myblkdisk);

    return 0;
}

/* Exit */
static void __exit
myblkdev_exit(void)
{
    /* Invalidate partitioning information and perform cleanup */
    del_gendisk(myblkdisk);

    /* Drop references to the gendisk so that it can be freed */
    put_disk(myblkdisk);

    /* Dissociate the driver from the request_queue. Internally calls
       elevator_exit() */
    blk_cleanup_queue(myblkdev_queue);

    /* Unregister the block device */
    unregister_blkdev(myblkdev_major, "myblkdev");
}

module_init(myblkdev_init);
module_exit(myblkdev_exit);

MODULE_LICENSE("GPL"); 
