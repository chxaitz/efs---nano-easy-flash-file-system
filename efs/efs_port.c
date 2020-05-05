/*
1. ������EFS��ֲ��Ҫ�õ��ĺ�������Ҫ���λ�ã��û�����Ҫ������Ҫ��������ɡ�
2. �û���Ҫ���3���ӿں�����ʵ�֣��ֱ����£�
   (1) efs_port_read(size_t addr, uint8_t *buf, size_t size)  -->  ��ȡ���ݽӿ�
   (2) efs_port_erase(size_t addr, size_t size) -->  �������ݽӿ�
   (3) efs_port_write(size_t addr, const uint8_t *buf, size_t size) -->  д�����ݽӿ�
3. ע������
   (1) addr ��������Ǹ��� efs.h �� EFS_START_ADDR ������������ģ� �û��Լ������ַƫ�ƵĻ���EFS_START_ADDR ��������Ϊ 0x0000
   (2) efs_port_erase() ������������(efs.h �� EFS_SECTOR_SIZE)�������ģ�����Ĳ�������������ʼ��ַ�������Ĵ�С��
                        ���У���ǰ�汾�У������Ĵ�С����1�������ı�׼��С�����Ժ��ԡ�
   (3) ϵͳ�ڿռ䲻��ʱ����������Ϊ��λ�Կռ䳢�Խ��л��գ�����ӵ������ÿ������������Ч���ݵ����������޷����գ�ֻ�����¸�ʽ����
       �½���ķ����ǣ��洢�� key_num < sec_num-1 ,�¸��汾�У�����������������ܣ����һ��������Ϊ�����������������ʱ��ʹ�ã�
       ��������֮��д�أ����������²���������ò����ᵼ��д��ʱ�䲻�ɿأ��ᷢ��Ϊ1�������İ汾��

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
