# efs - nano easy flash system

#### 介绍
一个适合于8位单片机的key-value型的数据存取管理库。
该库最大能管理64K地址的空间数据，很适合小型单片机用自身的Flash或者Eep作小型数据存取管理使用。
EasyFlash在Stm32上使用的很好，就仿照移植了个8位机版本的，只实现了变量的存取功能，可以当个key-value型的数据库使用。
算法使用了磨损平衡，地址空间内均匀磨损，压榨空间的使用寿命。

#### 软件架构
1.  软件仅包含 efs.h efs.c efs_port.c 3个文件；
2.  其中 efs_port.c 文件为用户移植时需要修改的文件，里面仅有3个函数需要实现，用户可以先在单片机内存中辟1块区域测试使用本库，便于快速掌握；
```
   (1) efs_port_read(size_t addr, uint8_t *buf, size_t size)  -->  读取数据接口
   (2) efs_port_erase(size_t addr, size_t size) -->  擦除数据接口
   (3) efs_port_write(size_t addr, const uint8_t *buf, size_t size) -->  写入数据接口
```
3.  efs.h 头文件用户在使用前，需要根据自身实际情况进行修改；
```
#define EFS_KEY_LENGHT_MAX  4          // key的最大长度，可为（4，12），这里推荐固定为4Bytes，则32Bytes的BLOCK中最多能存储3个MapTableItem条目，为12Bytes时，则需要对应调整BLOCK为64Bytes，

#define EFS_POINTER_DEFAULT 0x0000     // 存储空间擦除之后的默认值，比如我这里，擦除后默认为0x0000
#define EFS_POINTER_NULL    0xffff     // 存储空间已使用的标记，需要与上面的 EFS_POINTER_DEFAULT 不同
#define EFS_START_ADDR      0x0000     // 存储空间的起始地址，如果用户自己管理的话，可以设置为0x0000
#define EFS_AREA_SIZE       0x0300     // 存储空间的大小，代表了能够管理的最大空间大小
#define EFS_BLOCK_SIZE      0x20       // 系统的最小管理单元(块)大小，根据实际尺寸，以(8K,32K)为界，推荐设置为(32,64,128)3个参数
#define EFS_SECTOR_SIZE     0x80       // 扇区的大小，硬件能够擦除的最小区域
#define EFS_INVALID_KEY_MAX 8          // 最大无效key计数，当计数到索引表中，无效key大于此值时，将重建索引表，过小的值会频繁的重建
                                       // 索引表，这里推荐为2个块的区域大小，即Block为32Bytes，Item为8Bytes，则推荐值为 2*32/8=8 
```

#### 快速开始

1.  移植的接口函数
    这里使用内存作为存储区域，测试对 efs 的使用
```
uint8_t _ram_data[128*6]
uint8_t efs_port_read(size_t addr, uint8_t *buf, size_t size) 
{
    memcpy( buf, _ram_data+addr, size );
    return EFS_OK;
}

extern uint8_t _efs_block[2][EFS_BLOCK_SIZE];
uint8_t efs_port_erase(size_t addr, size_t size) 
{
  uint8_t i,cnt = size / EFS_BLOCK_SIZE;
  memset( _ram_data+addr, EFS_POINTER_DEFAULT, size);
  return EFS_OK;
}

uint8_t efs_port_write(size_t addr, const uint8_t *buf, size_t size) 
{
    _count += size;
    memcpy( _ram_data+addr, buf, size );
    return EFS_OK;
}
```

2.  测试用例
```
uint8_t resp;
size_t len;
size_t _count = 0; //计数本轮写入字节数
size_t tmspn_cur, tmspn_avg, tmspn_max, tmspn_min=0xffff;
uint32 tick=0; //计数总调用次数
struct xEepData eep_config; //已在其它地方初始化
uint8_t data[sizeof(struct xEepData)];
int main(void)
{ 
  efs_init();
  while(1){
    time_start();
    while( i++ < 10 ){
      // set key-value
      resp = efs_set("hell", (uint8_t*)&eep_data, sizeof(xEepData));
      if( EFS_OK != resp ){
        printf("main","efs set failed! ErrorCode: %d",resp);
        delay_ms(5);
      }
      // get value len by key
      len= efs_get_len("hell");
      // get value by key
      resp = efs_get("hell", data, sizeof(xEepData), NULL);
      if( EFS_OK != resp ){
        printf("efs get failed! ErrorCode: %d",resp);
        delay_ms(5);
      }else{
        if( 0 != memcmp( (const void*)&eep_data, (const void*)data, sizeof(xEepData)) ){
          printf("efs get data is not equal to the set!");
          delay_ms(5);
        }
      }
      tick++;
    }
    i = 0;
    tmspn_cur = time_end();
    tmspn_avg = (tmspn_avg + tmspn_cur)/2;
    if(tmspn_min > tmspn_cur ) tmspn_min = tmspn_cur;
    if(tmspn_max < tmspn_cur ) tmspn_max = tmspn_cur;
    printf("efs tick:%lu cnt:%u avg:%u min:%u max:%u",tick, _count, tmspn_avg, tmspn_min, tmspn_max);
    _count = 0;
}
```

#### 内部数据存储的结构
1. 内部数据管理的基础单元是BLOCK（块），并将管理的全部空间分成若干个BLOCK块;
2. 内部共有3种数据结构，每种数据结构均占用1个BLOCK，它们分别是MapHead，MapTable，MapBlock；
3. MapHead --> 用来管理key映射表的起始索引，它独占空间中的第1个SECTOR（扇区）,并仅使用其中的第1个BLOCK用来进行索引的管理；
   MapTable --> key索引的条目，即保存在此表中，并成链状，相应key的信息便在此表中进行查找；
   MapBlock --> 数据存储区，key对应的数据，即保存在此表中，并成链状，相应的data数据便在此表中进行读取。
4. 数据存储的相关框图结构，如下所示
```
[ global views ]
--------------------------------
| sec[0] | blk[0]   | MapHead  | the MapHead if fixed in the blk[0], and it will be erase and reupdate when the it's full
|        |  ...     |----------|
|        | blk[n]   |    /     | the other blks in sec[0] is not used
--------------------------------
| sec[1] | blk[n+1] | MapTable | blk[n]->xMapTable[n] is and index, and used to pointed to the xMapBlock
|  ...   |  ...     |--- or ---|
| sec[n] | blk[.]   | MapBlock | blk[n]->xMapBlock is used to save the data
--------------------------------

[ data area views ]
------------------------------------------------
|       | blk[] | MapTable | Item[0] - Item[n] |
| sec[] |  ...  |----------|-------------------|
|       | blk[] | MapBlock |       Data        |
------------------------------------------------
```
#### 注意事项
   (1) 接口函数传入的 addr 这个参数是根据 efs.h 中 EFS_START_ADDR 这个宏计算出来的， 用户自己管理地址偏移的话，   
       EFS_START_ADDR 可以设置为 0x0000；   
   (2) efs_port_erase() 函数是以扇区(efs.h 中 EFS_SECTOR_SIZE)来擦除的，传入的参数是扇区的起始地址和扇区的大小，   
                        其中，当前版本中，扇区的大小总是1个扇区的标准大小，可以忽略；   
   (3) 系统在空间不足时，会以扇区为单位对空间尝试进行回收，最恶劣的情况是每个扇区均有有效数据导致扇区均无法回收，只能重新格式化，   
       下建议的方案是，存储的 key_num < sec_num-1 ,下个版本中，考虑引入深度整理功能，最后一个扇区作为空闲扇区，仅整理的时候使用，   
       ，整理完之后写回，该扇区重新擦除，但因该操作会导致写入时间不可控，会发布为1个独立的版本。   
