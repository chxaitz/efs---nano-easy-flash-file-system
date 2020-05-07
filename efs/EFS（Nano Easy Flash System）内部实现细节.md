# EFS（Nano Easy Flash File System）内部实现细节

标签 ： 源码 分析 c efs

---

### 一、写在前面
**EFS**项目完成之后，为保证日后或其他人对对源码及相关逻辑和实现的理解，便有了写作本文的源码。   
本文通过对EFS文件系统的相关源代码及数据结构和运行效果进行整理分析，深度分析了EFS的内部实现机理，如何实现磨损均匀及掉电保护，便于后人对相关代码的理解及修改。

**EFS**库共包含3个文件，分别为`efs.h` `efs.c` `efs_port.c` 3个文件，本文仅分析其中的`efs.h` `efs.c` 2个文件，`efs_port.c` 为用户移植文件，这里不做分析。

### 二、总体介绍
#### 1. 系统存储空间的总体介绍
1. 内部数据管理的基础单元是**BLOCK**（块），并将管理的全部空间分成若干个BLOCK块;
2. 内部共有3种数据结构，每种数据结构均占用1个BLOCK，它们分别是**MapHead**，**MapTable**，**MapBlock**；
3. **MapHead** --> 用来管理key映射表的起始索引，它独占空间中的第1个SECTOR（扇区）,并仅使用其中的第1个BLOCK用来进行索引的管理；   
   **MapTable** --> key索引的条目，即保存在此表中，并成链状，相应key的信息便在此表中进行查找；   
   **MapBlock** --> 数据存储区，key对应的数据，即保存在此表中，并成链状，相应的data数据便在此表中进行读取。  
4. **MapTableItem** --> 作为MapTable下的子结构，用来保存key及相关的索引。  

#### 2. 数据结构的相关框图
``` Text
[ global views ]
--------------------------------
| sec[0] | blk[0]   | MapHead  | the MapHead if fixed in the blk[0], and it will be erase and reupdate when the it's full  
|        |  ...     |----------|
|        | blk[n]   |    /     | the other blks in sec[0] is not used
--------------------------------
| sec[1] | blk[n+1] | MapTable | blk[n]->xMapTable[n] store the key & index, and pointed to the xMapBlock
|  ...   |  ...     |--- or ---|
| sec[n] | blk[.]   | MapBlock | blk[n]->xMapBlock is used to save the data
--------------------------------

[ data area views ]
------------------------------------------------
|       | blk[] | MapTable | Item[0] - Item[n] |
| sec[] |  ...  |----------|-------------------|
|       | blk[] | MapBlock |       Data        |
------------------------------------------------

[ MapHead ]
--------------------------------
|"efs0"| index | index | index |
|      ...            ...      |
--------------------------------
  |
  |   [ MapTable ]
-------------------------------
| MapTableHead | MapTableItem |
|      ...            ...     |
| MapTableItem | MapTableItem |
-------------------------------
  |
  |   [ MapTableItem ]
  |   -------------------------------
  |-> | Index |  Len  |    keys     | 
      -------------------------------
         |
         |   [ MapBlockData ]
         |   -------------------------------
         |-> | Index |       Datas         | 
             -------------------------------
                |   [ MapBlockData ]
                |   -------------------------------
                |-> | Index |       Datas         | 
                    -------------------------------

```
#### 3. 数据结构体
```C
struct xMapTableItem{
    size_t index;
    size_t length;
    uint8_t key[EFS_KEY_LENGHT_MAX];
};
struct xMapTableHead{
    size_t index[sizeof(struct xMapTableItem)/sizeof(size_t)];
};
struct xMapBlock{
    size_t index[2];
    uint8_t data[0];
};
```

#### 三、系统函数的总体介绍
``` C
uint8_t efs_init();  // 初始化函数，需在使用之前调用，它会检查空间格式是否满足要求，并决定是否调用efs_format();函数进行格式化
uint8_t efs_format(); // 空间格式化函数，当出现空间不足的情况时，需要用户调用重新格式化（格式化前记得读取缓存重要数据^_^）
size_t  efs_get_len( uint8_t *key ); // 根据key得到数据的长度，未找到或出错的时候，返回0
uint8_t efs_get( uint8_t *key, uint8_t *buf, size_t bufLen, size_t *dataLen); // 根据key读取数据，   
                                                                              // bufLen为缓冲区的最大长度，   
                                                                              // *dataLen用来返回实际数据长度，可为NULL
uint8_t efs_set( uint8_t *key, uint8_t *buf, size_t bufLen ); // 设置数据
```

### 四、系统仿真及相关源码分析
#### 1. 仿真前的相关参数介绍
如下表为本次仿真的相关参数设置，Area为768B，Sector为128B，Block为32B，BlockTableItem为8B。
``` C
#define EFS_KEY_LENGHT_MAX  4          // key的最大长度，可为（4，12），这里推荐固定为4B，则32B的BLOCK中最多能存储3个MapTableItem条目，为12B时，则需要对应调整BLOCK为64B，

#define EFS_POINTER_DEFAULT 0xffff     // 存储空间擦除之后的默认值，比如我这里，擦除后默认为0xffff
#define EFS_POINTER_NULL    0x0000     // 存储空间已使用的标记，需要与上面的 EFS_POINTER_DEFAULT 不同
#define EFS_START_ADDR      0x0000     // 存储空间的起始地址，如果用户自己管理的话，可以设置为0x0000
#define EFS_AREA_SIZE       0x0300     // 存储空间的大小，代表了能够管理的最大空间大小
#define EFS_BLOCK_SIZE      0x20       // 系统的最小管理单元(块)大小，根据实际尺寸，以(8K,32K)为界，推荐设置为(32,64,128)3个参数
#define EFS_SECTOR_SIZE     0x80       // 扇区的大小，硬件能够擦除的最小区域
#define EFS_INVALID_KEY_MAX 8          // 最大无效key计数，当索引中无效key大于此值时将重建索引表，过小的值会导致频繁重建
```

#### 全局变量及相关说明
``` C
const uint8_t _efs_ver[]="efs0";        //文件系统标识头，用来标识EFS文件系统及版本呢
uint8_t _efs_block[2][EFS_BLOCK_SIZE];  //系统采用双cache缓存写入的方式，需要2组大小相同的缓冲空间
size_t  _szApplyBlkIdCur = 0;           //当前申请的Block的Index
size_t  _szKeyBlkIdCur  = 0;            //当前key所在Block的Index
uint8_t _u8KeyTabOffCur = 0;            //当前key在MapTable表中的偏移
size_t _szInvalidMapTabItem = 0;        //当前无效key的计数，到达阈值时，则会触发rebuild_maptable_index
struct xMapTableItem _xMapTabItem;      //存储当前访问的MapTableItem的结构体实例
size_t  _szKeyBlkIdOld = 0;             //尚未删除的旧key所在的MapTable的Index
uint8_t _szKeyBlkIndexOld = 0;          //尚未删除的旧key的MapTableItem的偏移
```

#### efs_init()函数   
该函数为**EFS**的初始化函数，该函数先读取第0个块，并识别前面的4字节识别符。识别符通过后，再判断后面的索引是否有效，有效地索引应该有且只有1个。不满足条件就重新进行格式化。   
``` C
uint8_t efs_init()
{
    uint8_t i;
    uint8_t resp = EFS_OK;
    size_t  *ptr = (size_t  *)_efs_block[0];
    resp = efs_read_block(0); //读取第0个块
    if( EFS_OK == resp ){
    // 先比较头是否为4字节识别字符“efs0”
        if( 0 == strncmp((const char*)_efs_block[0], (const char*)_efs_ver, 4 ) ){
            resp = 0; // start count
            // 继续判断内部是否只有1个有效索引
            for( i=2; i< EFS_SECTOR_SIZE/EFS_POINTER_SIZE; i++){
                if((ptr[i]!=EFS_POINTER_DEFAULT) && (ptr[i]!=EFS_POINTER_NULL) ){
                    if( resp++ > 0 ){ // verify failed, and reFormat the area
                      break;
                    }
                }
            }
        }
        if( resp != 1 ) //如果不满足格式要求，重新进行格式化
          resp = efs_format();
    }

    return resp;
}
```

#### efs_get( )函数
该函数通过key获取对应的值。
``` C
uint8_t efs_get( uint8_t *key, uint8_t *buf, size_t bufLen, size_t *dataLen)
{
    uint8_t resp = EFS_OK;
    size_t indexHead;
    size_t len;
    struct xMapBlock *pBlk;
    resp = efs_get_maptab_head( &indexHead );
    if( EFS_OK == resp ){
        resp = efs_get_mapblk( indexHead, key );
        if( EFS_OK == resp ){
            bufLen = _xMapTabItem.length > bufLen ? bufLen : _xMapTabItem.length;
            pBlk = (struct xMapBlock *)_efs_block[0];
            indexHead = _xMapTabItem.index;
            if( NULL != dataLen )
              *dataLen = 0;
            while( bufLen ){
                resp = efs_read_block( indexHead );
                if( EFS_OK == resp ){
                    len = EFS_BLOCK_SIZE - EFS_POINTER_SIZE*2;
                    len = len > bufLen ? bufLen: len;
                    memcpy( buf, pBlk->data, len );
                    if( NULL != dataLen )
                      *dataLen += len;
                    bufLen -= len;
                    buf += len;
                }
            }
        }
    }

    return resp;
}
```

#### uint8_t efs_set( )函数
该函数通过key存储对应的值。   
1.该函数主要通过顺序存储，储满后擦除进行**磨损平衡**   
2.该函数先写数据，再写索引，最后擦除原索引及调用链，来实现**掉电保护**   
3.重建索引表的时候，也是先完成新表，再更新MapHead索引后，最后删除旧索引表，来实现**掉电保护**   
```C
size_t _szKeyBlkIdOld = 0;
uint8_t _szKeyBlkIndexOld = 0;
uint8_t efs_set( uint8_t *key, uint8_t *buf, size_t bufLen )
{
    uint8_t i=0;
    uint8_t resp = EFS_OK;
    uint8_t first = TRUE;
    size_t indexHead,indexPre,indexTmp;
    size_t len;
    struct xMapBlock *pBlk;
    strncpy((char*)_xMapTabItem.key, (const char *)key, EFS_KEY_LENGTH_MAX);
    _xMapTabItem.length = bufLen;

    resp = efs_get_maptab_head( &indexHead );
    if( EFS_OK == resp ){
        // find if the key is existed
        resp = efs_get_mapblk( indexHead, key );
        if( EFS_OK == resp ){
            // reset the length, because it's covered
            _xMapTabItem.length = bufLen;
            // pre save the old key's BlkId and Index
            _szKeyBlkIdOld = _szKeyBlkIdCur;
            _szKeyBlkIndexOld = _u8KeyTabOffCur;
        }else{
          _szKeyBlkIdOld = 0;
          _szKeyBlkIndexOld = 0;
        }

        // enum the empty blk, and save the data
        while(bufLen){
            i = (i+1)%2;
            indexPre = indexTmp;
            indexTmp = efs_apply_empty_block(i);
            if( NULL != indexTmp ){
                if( TRUE == first ){
                    _xMapTabItem.index = indexTmp;
                    first = FALSE;
                }else{ // need to save the pre data
                    pBlk = (struct xMapBlock *)_efs_block[(i+1)%2];
                    pBlk->index[1] = indexTmp;
                    resp = efs_save_block( indexPre, (i+1)%2 );
                }

                // fill the data area by EFS_POINTER_DEFAULT
                memset(_efs_block[i], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE );
                pBlk = (struct xMapBlock *)_efs_block[i];
                pBlk->index[0] = EFS_POINTER_NULL;
                pBlk->index[1] = EFS_POINTER_DEFAULT;

                len = bufLen > (EFS_BLOCK_SIZE-EFS_POINTER_SIZE*2) ? (EFS_BLOCK_SIZE-EFS_POINTER_SIZE*2) : bufLen;
                memcpy( pBlk->data, buf, len );
                bufLen -= len;
                if( 0 == bufLen ){
                    resp = efs_save( indexTmp, i, 0, sizeof(size_t)*2 + len ); // save the last data
                    break;
                }

            }else{ // error
                resp = EFS_SPACE_FULL;
                break;
            }
        }

        // find an empty item space, and save the MapTableItem
        if( EFS_OK == resp ){
            resp = efs_apply_empty_item( indexHead, &indexPre, &i );
            if( EFS_OK == resp ){
                memcpy( (struct xMapTableItem *)_efs_block[0] + i, &_xMapTabItem, sizeof(struct xMapTableItem) );
                if( _szKeyBlkIdOld ){ // remove the old key
                    if( _szKeyBlkIdOld == indexPre ){ //they are in the same Block
                        indexTmp = ((struct xMapTableItem *)_efs_block[0] + _szKeyBlkIndexOld)->index;
                        ((struct xMapTableItem *)_efs_block[0] + _szKeyBlkIndexOld)->index = EFS_POINTER_NULL;
                        // the i must be after the indexTmp
                        resp = efs_save( indexPre, 0, sizeof(struct xMapTableItem)*_szKeyBlkIndexOld, EFS_POINTER_SIZE );
                        resp = efs_save( indexPre, 0, sizeof(struct xMapTableItem)*i, sizeof(struct xMapTableItem) );
                    }else{
                        resp = efs_save( indexPre, 0, 0, sizeof(struct xMapTableItem)*2 ); // save the MapTabHead and the first MapTabItem
                        if( EFS_OK == resp ){
                            resp = efs_read_block(_szKeyBlkIdOld);
                            if( EFS_OK == resp ){
                                indexTmp = ((struct xMapTableItem *)_efs_block[0] + _szKeyBlkIndexOld)->index;
                                ((struct xMapTableItem *)_efs_block[0] + _szKeyBlkIndexOld)->index = EFS_POINTER_NULL;
                                resp = efs_save( _szKeyBlkIdOld, 0, sizeof(struct xMapTableItem)*_szKeyBlkIndexOld, sizeof(struct xMapTableItem) );
                            }
                        }
                    }
                    //clear the BlockData index chain
                    if( EFS_OK == resp ){
                      resp = efs_clear_index_chain( indexTmp, 0 );
                    }
                }else{
                  resp = efs_save( indexPre, 0, sizeof(struct xMapTableItem)*i, sizeof(struct xMapTableItem) );
                }
                
                if( _szInvalidMapTabItem >= EFS_INVALID_KEY_MAX )
                  efs_rebuild_index();
            }
        }
    }

    return resp;
}
```
