#define DEVICE_NAME "dbf"
#define MAX_PATH_LENGTH 160     

typedef char path_string_t [ MAX_PATH_LENGTH + 1 ];

typedef struct {
   path_string_t path;
   loff_t        bsize; 
} info_struct_t;

/* user </usr/include/ioctl.h>
#define _IOR(type,nr,size)      _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)      _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOWR(type,nr,size)     _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
*/

/* kernel <linux/ioctl.h>
#define _IO(type,nr)            _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)      _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type,nr,size)      _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOWR(type,nr,size)     _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
*/

#define IOC_MAGIC    'D' 

#define DUBFL_GET_INFO       _IOR( IOC_MAGIC, 1, info_struct_t ) 
#define DUBFL_SAVE_DISK      _IOW( IOC_MAGIC, 1, char[ 0 ] ) 
#define DUBFL_UNLINK_DISK    _IOW( IOC_MAGIC, 2, char[ 0 ] ) 
#define DUBFL_LOAD_DISK      _IOW( IOC_MAGIC, 3, path_string_t ) 
