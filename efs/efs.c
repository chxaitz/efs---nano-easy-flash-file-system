#include "efs.h"
#include "string.h"

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * TYPE_DEFS
 */
struct xMapTableItem{
    size_t index;
    size_t length;
    uint8_t key[EFS_KEY_LENGTH_MAX];
};
struct xMapTableHead{
    size_t index[sizeof(struct xMapTableItem)/sizeof(size_t)];
};
struct xMapBlock{
    size_t index[2];
    uint8_t data[0];
};


/*********************************************************************
 * GLOBAL VARIABLES
 */
const uint8_t _efs_ver[]="efs0";
uint8_t _efs_block[2][EFS_BLOCK_SIZE];
size_t  _szApplyBlkIdCur = 0;
size_t  _szKeyBlkIdCur  = 0;
uint8_t _u8KeyTabOffCur = 0;
size_t _szInvalidMapTabItem = 0;
struct xMapTableItem _xMapTabItem;

/*********************************************************************
 * FUNCTIONS
 *********************************************************************/
uint8_t efs_gc( uint8_t n );

uint8_t efs_read( size_t index, uint8_t i, uint8_t off, uint8_t len )
{
    assert_param( EFS_BLOCK_SIZE * index < EFS_AREA_SIZE );

    size_t addr = EFS_START_ADDR + EFS_BLOCK_SIZE * index + off ;
    _szKeyBlkIdCur = index;
    return efs_port_read( addr, _efs_block[i] + off , len);
}

uint8_t efs_read_block( size_t index )
{
    assert_param( EFS_BLOCK_SIZE * index < EFS_AREA_SIZE );

    size_t addr = EFS_START_ADDR + EFS_BLOCK_SIZE * index;
    _szKeyBlkIdCur = index;
    return efs_port_read( addr, _efs_block[0], EFS_BLOCK_SIZE);
}

uint8_t efs_erase_sector( size_t index )
{
    uint8_t resp = EFS_OK;
    size_t addr;
    addr = EFS_START_ADDR + EFS_SECTOR_SIZE * index;
    resp = efs_port_erase( addr, EFS_SECTOR_SIZE );

    return resp;
}

uint8_t efs_save( size_t index, uint8_t i, uint8_t off, uint8_t len )
{
    uint8_t resp = EFS_OK;
    size_t addr;

    addr = EFS_START_ADDR + EFS_BLOCK_SIZE * index + off;
    resp = efs_port_write( addr, (const uint8_t *)_efs_block[i]+off, len) ;

    return resp;
}

uint8_t efs_save_block( size_t index, uint8_t i )
{
    uint8_t resp = EFS_OK;
    size_t addr;

    addr = EFS_START_ADDR + EFS_BLOCK_SIZE * index;
    resp = efs_port_write( addr, (const uint8_t *)_efs_block[i], EFS_BLOCK_SIZE) ;

    return resp;
}

// apply a new MapBlock, if failed return NULL(0)
size_t efs_apply_empty_block( uint8_t n )
{
    uint8_t finded = FALSE;
    uint8_t retred = FALSE;
    
    size_t index = _szApplyBlkIdCur;
    struct xMapTableHead *pHead = (struct xMapTableHead *)_efs_block[n];
    while(1){
        do{
            index = (index + 1) % EFS_BLOCK_INDEX_MAX;
            if( index < EFS_BLOCKS_IN_SECTOR )
              index = EFS_BLOCKS_IN_SECTOR;
            efs_read( index, n, 0, EFS_POINTER_SIZE);
            if( pHead->index[0] == EFS_POINTER_DEFAULT ){
                memset(_efs_block[n], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE );
                pHead->index[0] = EFS_POINTER_NULL;
                efs_save( index, n, 0, EFS_POINTER_SIZE);
                _szApplyBlkIdCur = index;
                finded = TRUE;
                break;
            }
        }while( index != _szApplyBlkIdCur );

        if( finded == FALSE ){
            // retry and exist
            if( retred == FALSE ){ // if failed, try to gc and reget
                efs_gc( n ); // when call this the _szApplyBlkIdCur will set EFS_BLOCK_INDEX_MAX
                retred = TRUE;
            }else{
              break;
            }
        }else{
          break;
        }
    }
    
    if( finded == TRUE ){
      return _szApplyBlkIdCur;
    }
    else{
      return NULL;
    }
}

// apply a new MapTableItem
uint8_t efs_apply_empty_item( size_t indexHead, size_t *index, uint8_t *off )
{
    uint8_t resp = EFS_OK;
    uint8_t i;
    uint8_t finded = FALSE;
    struct xMapTableItem *pItem = (struct xMapTableItem *)_efs_block[0];
    struct xMapTableHead *pHead = (struct xMapTableHead *)_efs_block[0];
    while(1){
        resp = efs_read_block(indexHead);
        if( EFS_OK == resp ){
            if( pHead->index[1] != EFS_POINTER_DEFAULT ){ // this is a full MapTable Block, get the next
                indexHead = pHead->index[1];
                continue; // continue to check the next MapTable Block
            }

            for( i=1; i < (EFS_BLOCK_SIZE/sizeof( struct xMapTableItem)); i++ ){
                if( pItem[i].index == EFS_POINTER_DEFAULT ){ // find an empty item
                    *index = indexHead;
                    *off = i;
                    finded = TRUE;
                    return EFS_OK;
                }
            }

            if( finded == FALSE ){ // this MapTable Block is full, and need to create a new MapTable Block
                *index = efs_apply_empty_block( 1 );
                if( NULL != *index ){
                    pHead[0].index[0] = EFS_POINTER_NULL;
                    pHead[0].index[1] = *index;
                    resp = efs_save( indexHead, 0, 0, sizeof(size_t)*2 );
                    if( EFS_OK == resp ){
                        *off = 1;
                        memcpy(_efs_block[0], _efs_block[1], EFS_BLOCK_SIZE); //save the cache_1 to cache_0
                        break;
                    }else{
                        break;
                    }
                }
            }
        }else{
            break;
        }
    }

    return resp;
}

// clear the index_chain by the first BlkIndex
uint8_t efs_clear_index_chain( size_t index, uint8_t n )
{
    uint8_t resp = EFS_OK;
    size_t indexTmp;
    struct xMapBlock *pHead = (struct xMapBlock *)_efs_block[n];
    while(1){
      resp = efs_read( index, n, 0, EFS_POINTER_SIZE*2);
      if( EFS_OK == resp ){
        if( pHead->index[1] != EFS_POINTER_DEFAULT ){
          indexTmp = index;
          index = pHead->index[1];
          pHead->index[1] = EFS_POINTER_NULL;
          efs_save( indexTmp, n, 0, sizeof(size_t)*2);
        }else{ // this is the last
          pHead->index[1] = EFS_POINTER_NULL;
          efs_save( index, n, 0, sizeof(size_t)*2 );
          break;
        }
      }
    }
    
    return resp;
}

// the Sector[0] is used for MapHead, and the MapTable and MapBlock started from the Sector[1]
uint8_t efs_format()
{
    uint8_t i;
    uint8_t resp = EFS_OK;
    // erase all the sectors
    for(i=0; i<EFS_SECTOR_INDEX_MAX; i++ )
      efs_erase_sector( i );
    
    // write the MapHead
    memset(_efs_block[0], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE );
    strcpy((char *)_efs_block[0], (const char *)_efs_ver );
    *(size_t *)(_efs_block[0]+4) = EFS_BLOCKS_IN_SECTOR;    
    resp = efs_save(0, 0, 0, 4 + sizeof(size_t));
    
    // write the MapTab
    if( EFS_OK == resp ){
      memset(_efs_block[0], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE );
      ((struct xMapTableHead *)_efs_block[0])->index[0] = EFS_POINTER_NULL;      
      resp = efs_save(EFS_BLOCKS_IN_SECTOR, 0, 0, sizeof(size_t));
    }

    return resp;
}

uint8_t efs_init()
{
    uint8_t i;
    uint8_t resp = EFS_OK;
    size_t  *ptr = (size_t  *)_efs_block[0];
    resp = efs_read_block(0);
    if( EFS_OK == resp ){
        if( 0 == strncmp((const char*)_efs_block[0], (const char*)_efs_ver, 4 ) ){
            resp = 0; // start count
            for( i=2; i< EFS_SECTOR_SIZE/EFS_POINTER_SIZE; i++){
                if((ptr[i]!=EFS_POINTER_DEFAULT) && (ptr[i]!=EFS_POINTER_NULL) ){
                    if( resp++ > 0 ){ // verify failed, and reFormat the area
                      break;
                    }
                }
            }
        }
        if( resp != 1 )
          resp = efs_format();
    }

    return resp;
}

uint8_t efs_get_maptab_head( size_t *index )
{
    uint8_t resp = EFS_OK;
    uint8_t minIndex, curIndex, maxIndex;
    size_t  *ptr = (size_t  *)_efs_block[0];
    // read the MapHead
    resp = efs_read_block(0);
    if( EFS_OK == resp ){
        minIndex = 4 / sizeof(size_t);
        maxIndex = EFS_BLOCK_SIZE / EFS_POINTER_SIZE;
        curIndex = ( minIndex + maxIndex ) / 2;
        do{
            if( ptr[curIndex] == EFS_POINTER_DEFAULT ){
                maxIndex = curIndex;
                curIndex = ( minIndex + curIndex ) / 2;
            }else if( ptr[curIndex] == EFS_POINTER_NULL ){
                minIndex = curIndex;
                curIndex = ( curIndex + maxIndex ) / 2;
            }else{
                *index = ptr[curIndex];
                return EFS_OK;
            }
        }while( (maxIndex != (4 / sizeof(size_t)) ) && (minIndex != (EFS_BLOCK_SIZE / EFS_POINTER_SIZE - 1) ));
    }

    return EFS_MAPTAB_NOT_FOUND;
}

/**
 * get data addr by key.
 *
 * @param tableHeadIndex MapTable head address index
 * @param key  the key word to search
 *
 * @return result
 */
uint8_t efs_get_mapblk( size_t tableHeadIndex, uint8_t *key )
{
    uint8_t resp = EFS_OK;
    uint8_t finded = FALSE;
    uint8_t index;
    struct xMapTableItem *pItem = (struct xMapTableItem *)_efs_block[0];
    struct xMapTableHead *pHead = (struct xMapTableHead *)_efs_block[0];
    _szInvalidMapTabItem = 0;
    // read the MapTable head
    while(TRUE){
      resp = efs_read_block(tableHeadIndex);
      if( EFS_OK == resp ){
        for( index=1; index < (EFS_BLOCK_SIZE/sizeof( struct xMapTableItem)); index++ ){
          if( (pItem + index)->index != EFS_POINTER_NULL ){
            if( 0 == strncmp((const char*)key, (const char*)pItem[index].key, EFS_KEY_LENGTH_MAX) ){
                memcpy( &_xMapTabItem, pItem+index, sizeof(struct xMapTableItem) ); // copy the MapTableItem data
                _u8KeyTabOffCur = index;
                finded = TRUE;
                return EFS_OK;
            }
          }else{
            _szInvalidMapTabItem++; // invalid MapTabItem count
          }
        }
        if( pHead->index[1] != EFS_POINTER_DEFAULT ){
          tableHeadIndex = pHead->index[1];
        }else{ // search to the last, and can't not find the aim key
          break;
        }
      }
    }
    if( finded == FALSE )
        resp = EFS_KEY_NOT_FOUND;

    return resp;
}

uint8_t efs_update_mapHead( size_t index )
{
    uint8_t resp = EFS_OK;
    uint8_t minIndex, curIndex, maxIndex;
    size_t  *ptr = (size_t  *)_efs_block[0];
    // read the MapHead
    resp = efs_read_block(0);
    // get the item
    if( EFS_OK == resp ){
        minIndex = 4 / sizeof(size_t);
        maxIndex = EFS_BLOCK_SIZE / EFS_POINTER_SIZE;
        curIndex = ( minIndex + maxIndex ) / 2;
        do{
            if( ptr[curIndex] == EFS_POINTER_DEFAULT ){
                maxIndex = curIndex;
                curIndex = ( minIndex + curIndex ) / 2;
            }else if( ptr[curIndex] == EFS_POINTER_NULL ){
                minIndex = curIndex;
                curIndex = ( curIndex + maxIndex ) / 2;
            }else{
                if( curIndex == (EFS_BLOCK_SIZE / EFS_POINTER_SIZE - 1) ){// the MapHead if full
                    efs_erase_sector(0);
                    memset(_efs_block[0], EFS_POINTER_DEFAULT, EFS_BLOCK_SIZE );
                    strcpy((char *)_efs_block[0], (const char *)_efs_ver );
                    *(size_t *)(_efs_block[0]+4) = index;
                    resp =  efs_save( 0, 0, 0, sizeof(size_t)*3);
                }else{
                    ptr[curIndex] = EFS_POINTER_NULL;
                    ptr[curIndex+1] = index;
                    resp =  efs_save( 0, 0, sizeof(size_t)*curIndex, sizeof(size_t)*2);
                }
                return resp;
            }
        }while( (maxIndex != (4 / sizeof(size_t)) ) && (minIndex != (EFS_SECTOR_SIZE / EFS_POINTER_SIZE - 1) ));
    }
    
    return EFS_MAPTAB_NOT_FOUND;
}

// rebuild the index map, it will not erase the old MapTable Blk, besure all the blk is erased by efs_gc() last
uint8_t efs_rebuild_index()
{
    uint8_t i,seek = 1;
    uint8_t resp = EFS_OK;
    size_t indexOldHead,indexOldCur;
    size_t indexHead,indexPre,indexNew;
    struct xMapTableItem *pItem = (struct xMapTableItem *)_efs_block[0];
    resp = efs_get_maptab_head( &indexOldHead );
    if( EFS_OK != resp )
        return resp;
    resp = efs_read_block(indexOldHead);
    indexOldCur = indexOldHead;
    indexHead = efs_apply_empty_block(1);
    indexNew = indexHead;
    if( NULL == indexOldCur )
        return EFS_ERROR;
    while(TRUE){
        if( EFS_OK == resp ){
            for( i=1; i<(EFS_BLOCK_SIZE/sizeof( struct xMapTableItem)); i++){
                if( (pItem[i].index != EFS_POINTER_NULL) && (pItem[i].index != EFS_POINTER_DEFAULT) ){
                    if( seek == (EFS_BLOCK_SIZE/sizeof(struct xMapTableItem)) ){ // the new MapTable is full
                        // move blk_1 to blk_0
                        memcpy(_efs_block[0],_efs_block[1],EFS_BLOCK_SIZE);
                        indexPre = indexNew;
                        indexNew = efs_apply_empty_block(1);
                        ((struct xMapTableHead *)_efs_block[0])->index[1] = indexNew;
                        // save the new created table block
                        resp = efs_save_block( indexPre, 1 );
                        // reload the table block
                        resp = efs_read_block(indexOldCur);
                        seek = 1;
                    }
                    // copy MapTableItem to the new Blk
                    memcpy( ((struct xMapTableItem*)_efs_block[1])+seek, pItem + i, sizeof(struct xMapTableItem));
                    seek++;
                }
            }
            // the end of MapTable list
            if( ((struct xMapTableHead *)_efs_block[0])->index[1] == EFS_POINTER_DEFAULT ){
                // all old table item has been readed
                // save the new created table block
                resp = efs_save( indexNew, 1, 0, seek * sizeof(struct xMapTableHead) );
                // update the MapHead
                resp = efs_update_mapHead( indexHead );
                if( EFS_OK == resp )
                    resp = efs_clear_index_chain( indexOldHead, 0 );
                //return
                break;
            }else{
                indexOldCur = ((struct xMapTableHead *)_efs_block[0])->index[1];
                resp = efs_read_block(indexOldCur);
            }
        }
    }
    
    return resp;
}

uint8_t efs_gc( uint8_t n )
{
    uint8_t resp = EFS_OK;
    uint8_t j;
    size_t addr;
    for(size_t i=1; i<EFS_SECTOR_INDEX_MAX; i++ ){
      addr = EFS_START_ADDR + EFS_SECTOR_SIZE * i;
      for( j=0; j< EFS_BLOCKS_IN_SECTOR; j++ ){
        resp = efs_port_read( addr, _efs_block[n], sizeof(struct xMapBlock));
        if( ((struct xMapBlock*)_efs_block[n])->index[1] != EFS_POINTER_NULL){
          break;
        }
        addr += EFS_BLOCK_SIZE;
      }
      if( j == EFS_BLOCKS_IN_SECTOR )
        resp = efs_erase_sector( i );
    }
    
    // set _szApplyBlkIdCur = EFS_BLOCK_INDEX_MAX to make the next blk search start from EFS_BLOCKS_IN_SECTOR ^_^
    _szApplyBlkIdCur = EFS_BLOCK_INDEX_MAX;

    return resp;
}

/**
 * get data from flash.
 *
 * @param key to search
 * @param buf the write data buffer
 * @param buf's max size can be write
 * @param the real write data len
 *
 * @return result
 */
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

/**
 * Read data len from flash.
 *
 * @param key to search
 * @param buf the write data buffer
 * @param size write bytes size
 *
 * @return key_data's len, when NULL or ERROR return 0
 */
size_t efs_get_len( uint8_t *key )
{
    uint8_t resp = EFS_OK;
    size_t indexHead;
    resp = efs_get_maptab_head( &indexHead );
    if( EFS_OK == resp ){
        resp = efs_get_mapblk( indexHead, key );
    }
    if( EFS_OK == resp )
      return _xMapTabItem.length;
    else
      return 0;
}

size_t  _szKeyBlkIdOld = 0;
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
