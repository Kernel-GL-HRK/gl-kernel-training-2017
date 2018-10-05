#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include <linux/version.h>
#include <linux/init.h>

// We can tweak our hardware sector size, but the kernel talks to us in terms of small sectors, always.
#define KERNEL_SECTOR_SIZE    512

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35)
#define blk_fs_request(rq)      ((rq)->cmd_type == REQ_TYPE_FS)
#endif

static int diskmb = 4;
module_param_named( size, diskmb, int, 0 ); // размер диска в Mb, по умолчанию - 4Mb
static int debug = 0;
module_param( debug, int, 0 );              // уровень отладочных сообщений

#define ERR(...) printk( KERN_ERR "! "__VA_ARGS__ )
#define LOG(...) printk( KERN_INFO "+ "__VA_ARGS__ )
#define DBG(...) if( debug > 0 ) printk( KERN_DEBUG "# "__VA_ARGS__ )

