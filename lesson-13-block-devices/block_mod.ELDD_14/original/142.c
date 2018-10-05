
#define GET_DEVICE_ID 0xAA00 /* Ioctl command definition */


/* The ioctl operation */
static int
myblkdev_ioctl (struct inode *inode, struct file *file,
                unsigned int cmd, unsigned long arg)
{
    unsigned char status;

    switch (cmd) {
    case GET_DEVICE_ID:
        outb(GET_IDENTITY_CMD, COMMAND_REGISTER);
        /* Wait as long as the controller is busy */
        while ((status = inb(STATUS_REGISTER)) & BUSY_STATUS);
        /* Obtain ID and return it to user space */
        return put_user(inb(DATA_REGISTER), (long __user *)arg);
    default:
        return -EINVAL;
    }
}

/* Block device operations */
static struct block_device_operations myblkdev_fops = {
    .owner = THIS_MODULE, /* Owner of this structure */
    .ioctl = myblkdev_ioctl,
    /* The following operations are not implemented for our example
       storage controller: open(), release(), unlocked_ioctl(),
       compat_ioctl(), direct_access(), getgeo(), revalidate_disk(), and
       media_changed() */
};

