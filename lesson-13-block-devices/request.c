static void my_request( struct request_queue *q ) {
   struct request *rq;
   char *ptr;
   unsigned nr_sectors, sector;
   DBG( "entering request routine\n" );
   rq = blk_fetch_request( q );
   while( rq ) {
      int size, res = 0;
      if( !blk_fs_request( rq ) ) {
         ERR( "this was not a normal fs request, skipping\n" );
         res = 1;
         goto done;
      }
      nr_sectors = blk_rq_cur_sectors( rq );
      sector = blk_rq_pos( rq );
      ptr = my_dev + sector * KERNEL_SECTOR_SIZE;
      size = nr_sectors * KERNEL_SECTOR_SIZE;
      if( ( ptr + size ) > ( my_dev + disk_size ) ) {
         ERR( "tried to go past end of device\n" );
         res = 1;
         goto done;
      }
      if( rq_data_dir( rq ) ) {
         LOG( "writing at sector %d, %u sectors \n", sector, nr_sectors );
         memcpy( ptr, rq->buffer, size );
      } else {
         LOG( "reading at sector %d, %u sectors \n", sector, nr_sectors );
         memcpy( rq->buffer, ptr, size );
      }
done:
      if( !__blk_end_request_cur( rq, res ) )
         rq = blk_fetch_request( q );
   }
   DBG( "leaving request\n" );
}

