/*
1. 这里是EFS移植需要用到的函数的主要存放位置，用户的主要操作都要在这里完成。
2. 用户需要完成3个接口函数的实现，分别如下：
   (1) efs_port_read(size_t addr, uint8_t *buf, size_t size)  -->  读取数据接口
   (2) efs_port_erase(size_t addr, size_t size) -->  擦除数据接口
   (3) efs_port_write(size_t addr, const uint8_t *buf, size_t size) -->  写入数据接口
3. 注意事项
   (1) addr 这个参数是根据 efs.h 中 EFS_START_ADDR 这个宏计算出来的， 用户自己管理地址偏移的话，EFS_START_ADDR 可以设置为 0x0000
   (2) efs_port_erase() 函数是以扇区(efs.h 中 EFS_SECTOR_SIZE)来擦除的，传入的参数是扇区的起始地址和扇区的大小，
                        其中，当前版本中，扇区的大小总是1个扇区的标准大小，可以忽略。
   (3) 系统在空间不足时，会以扇区为单位对空间尝试进行回收，最恶劣的情况是每个扇区均有有效数据导致扇区均无法回收，只能重新格式化，
       下建议的方案是，存储的 key_num < sec_num-1 ,下个版本中，考虑引入深度整理功能，最后一个扇区作为空闲扇区，仅整理的时候使用，
       ，整理完之后写回，该扇区重新擦除，但因该操作会导致写入时间不可控，会发布为1个独立的版本。

4. [ global views ]
-------------------------------
 sec[0] | blk[0]   | MapHead  | the MapHead if fixed in the blk[0], and it will be erase and reupdate when the it's full
        |  ...     |----------|
        | blk[n]   |    /     | the other blks in sec[0] is not used
-------------------------------
 sec[1] | blk[n+1] | MapTable | blk[n]->xMapTable[n] is and index, and used to pointed to the xMapBlock
  ...   |  ...     |--- or ---|
 sec[n] | blk[.]   | MapBlock | blk[n]->xMapBlock is used to save the data
-------------------------------

5. [ data area views ]
----------------------------------------------
|     | blk[] | MapTable | Item[0] - Item[n] |
| sec |  ...  |----------|-------------------|
|     | blk[] | MapBlock |       Data        |
----------------------------------------------
 */

#include "efs.h"
#include "string.h"
#include "m_e2p.h"
/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPE_DEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

extern size_t _count;
/*********************************************************************
 * FUNCTIONS
 *********************************************************************/

uint8_t efs_port_read(size_t addr, uint8_t *buf, size_t size) 
{
  if(E2P_STA_OK==Eep_Read( addr, buf, size ))
    return EFS_OK;
  else
    return EFS_ERROR;
}

extern uint8_t _efs_block[2][EFS_BLOCK_SIZE];
uint8_t efs_port_erase(size_t addr, size_t size) 
{
//  addr = (addr - EFS_START_ADDR)/EFS_SECTOR_SIZE;
//  Eep_Erase( addr );
  uint8_t i,cnt = size / EFS_BLOCK_SIZE;
  memset(_efs_block[1], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE);
  for(i=0; i<cnt; i++ ){
    if(E2P_STA_OK!=EEP_Write_Word( addr, _efs_block[1], EFS_BLOCK_SIZE )){
      return EFS_ERROR;
    }
    addr += EFS_BLOCK_SIZE;
  }
  return EFS_OK;
}

uint8_t efs_port_write(size_t addr, const uint8_t *buf, size_t size) 
{
  uint8_t off;
  size = (size+3)&0xfc;
  off = addr & 0x03;
  size = (off + size + 3 )&0xfc;
//  if(addr < 0x0004)
//    _count += 1;
  _count += size;
  if(E2P_STA_OK==EEP_Write_Word( addr-off, (uint8_t *)buf-off, size ))
    return EFS_OK;
  else
    return EFS_ERROR;
}
