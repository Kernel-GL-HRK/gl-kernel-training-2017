MODULE_LICENSE( "GPL v2" );
MODULE_VERSION( "2.6" );
MODULE_AUTHOR( "Oleg Tsiliuric <olej@front.ru>" );

static struct block_device_operations mybdrv_fops = {
   .owner = THIS_MODULE,
   .ioctl = my_ioctl,
   .getgeo = my_getgeo
};

static int __init blk_init( void );
module_init( blk_init );

static void __exit blk_exit( void );
module_exit( blk_exit );
