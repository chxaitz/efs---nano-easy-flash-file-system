/*

0. [ global views ]
-------------------------------
 sec[0] | blk[0]   | MapHead  | the MapHead if fixed in the blk[0], and it will be erase and reupdate when the it's full
        |  ...     |----------|
        | blk[n]   |    /     | the other blks in sec[0] is not used
-------------------------------
 sec[1] | blk[n+1] | MapTable | blk[n]->xMapTable[n] is and index, and used to pointed to the xMapBlock
  ...   |  ...     |--- or ---|
 sec[n] | blk[.]   | MapBlock | blk[n]->xMapBlock is used to save the data
-------------------------------

1. [ data area views ]
----------------------------------------------
|     | blk[] | MapTable | Item[0] - Item[n] |
| sec |  ...  |----------|-------------------|
|     | blk[] | MapBlock |       Data        |
----------------------------------------------
 */

#ifndef _EFS_HEAD_
#define _EFS_HEAD_

#include "coll_types.h"
/*********************************************************************
 * CONSTANTS
 */
#define EFS_KEY_LENGTH_MAX  4   // it is fixed to 4, and no need to change it

#define EFS_POINTER_DEFAULT 0xffff
#define EFS_POINTER_NULL    0x0000
#define EFS_START_ADDR      0x0000
#define EFS_AREA_SIZE       0x0300
#define EFS_BLOCK_SIZE      0x20
#define EFS_SECTOR_SIZE     0x80        // the min size can be erase
#define EFS_INVALID_KEY_MAX 4  //when invalid key to this val, efs will rebuild the MapTable chain to clear the invalid MapTableItem
   
#define EFS_POINTER_SIZE     sizeof(size_t)
#define EFS_BLOCK_INDEX_MAX  (EFS_AREA_SIZE/EFS_BLOCK_SIZE)
#define EFS_SECTOR_INDEX_MAX (EFS_AREA_SIZE/EFS_SECTOR_SIZE)
#define EFS_BLOCKS_IN_SECTOR (EFS_SECTOR_SIZE/EFS_BLOCK_SIZE)

// EFS_STATE_CODE
#define EFS_OK               0
#define EFS_ERROR            1
#define EFS_unINIT           2
#define EFS_MAPTAB_NOT_FOUND 4
#define EFS_KEY_NOT_FOUND    5
#define EFS_SPACE_FULL       6

/*********************************************************************
 * FUNCTIONS
 *********************************************************************/
uint8_t efs_init();
uint8_t efs_format();
size_t  efs_get_len( uint8_t *key );
uint8_t efs_get( uint8_t *key, uint8_t *buf, size_t bufLen, size_t *dataLen);
uint8_t efs_set( uint8_t *key, uint8_t *buf, size_t bufLen );

// the efs port funcs
uint8_t efs_port_read(size_t addr, uint8_t *buf, size_t size);
uint8_t efs_port_erase(size_t addr, size_t size);
uint8_t efs_port_write(size_t addr, const uint8_t *buf, size_t size);

#endif // _EFS_HEAD_
