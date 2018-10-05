/* #include <linux/hdreg.h> 
struct hd_geometry {
   unsigned char heads;
   unsigned char sectors;
   unsigned short cylinders;
   unsigned long start;
}; */

// #define HDIO_GETGEO             0x0301  /* get device geometry */
/* 0x330 is reserved - used to be HDIO_GETGEO_BIG */

static int my_getgeo( struct block_device *bdev, struct hd_geometry *geo ) {
   unsigned long sectors = ( diskmb * 1024 ) * 2;
   DBG( KERN_INFO "getgeo\n" );
   geo->heads = 4;
   geo->sectors = 16;
   geo->cylinders = sectors / geo->heads / geo->sectors;
   geo->start = geo->sectors;
   return 0;
};

static int my_ioctl( struct block_device *bdev, fmode_t mode,
                     unsigned int cmd, unsigned long arg ) {
   LOG( "ioctl cmd=%X\n", cmd );
   switch( cmd ) {
      case HDIO_GETGEO: {
         struct hd_geometry geo;
         LOG( "ioctk HDIO_GETGEO\n" );
         my_getgeo( bdev, &geo );
         if( copy_to_user( (void __user *)arg, &geo, sizeof( geo ) ) )
            return -EFAULT;
         return 0;
      }
      default:
         ERR( "ioctl unknown command\n" );
         return -ENOTTY;
   }
}
