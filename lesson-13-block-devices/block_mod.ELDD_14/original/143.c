
#define READ_SECTOR_CMD        1

#define WRITE_SECTOR_CMD       2

#define GET_IDENTITY_CMD       3

#define BUSY_STATUS            0x10

#define SECTOR_NUMBER_REGISTER 0x20000000

#define SECTOR_COUNT_REGISTER  0x20000001

#define COMMAND_REGISTER       0x20000002

#define STATUS_REGISTER        0x20000003

#define DATA_REGISTER          0x20000004


/* Request method */
static void
myblkdev_request(struct request_queue *rq)
{
    struct request *req;
    unsigned char status;
    int i, good = 0;

    /* Loop through the requests waiting in line */
    while ((req = elv_next_request(rq)) != NULL) {
        /* Program the start sector and the number of sectors */
        outb(req->sector, SECTOR_NUMBER_REGISTER);
        outb(req->nr_sectors, SECTOR_COUNT_REGISTER);

        /* We are interested only in filesystem requests. A SCSI command
           is another possible type of request. For the full list, look
           at the enumeration of rq_cmd_type_bits in
           include/linux/blkdev.h */
        if (blk_fs_request(req)) {
            switch(rq_data_dir(req)) {
            case READ:
                /* Issue Read Sector Command */
                outb(READ_SECTOR_CMD, COMMAND_REGISTER);

                /* Traverse all requested sectors, byte by byte */
                for (i = 0; i < 512*req->nr_sectors; i++) {
                    /* Wait until the disk is ready. Busy duration should be
                       in the order of microseconds. Sitting in a tight loop
                       for simplicity; more intelligence required in the real
                       world */
                    while ((status = inb(STATUS_REGISTER)) & BUSY_STATUS);

                    /* Read data from disk to the buffer associated with the
                       request */
                    req->buffer[i] = inb(DATA_REGISTER);
                }
                good = 1;
                break;

            case WRITE:
                /* Issue Write Sector Command */
                outb(WRITE_SECTOR_CMD, COMMAND_REGISTER);

                /* Traverse all requested sectors, byte by byte */
                for (i = 0; i < 512*req->nr_sectors; i++) {
                    /* Wait until the disk is ready. Busy duration should be
                       in the order of microseconds. Sitting in a tight loop
                       for simplicity; more intelligence required in the real
                       world */
                    while ((status = inb(STATUS_REGISTER)) & BUSY_STATUS);

                    /* Write data to disk from the buffer associated with the
                       request */
                    outb(req->buffer[i], DATA_REGISTER);
                }
                good = 1;
                break;
            }
        }

        end_request(req, good);
    }
}

