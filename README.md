# EFS - Nano Easy Flash File System

#### 介绍
**EFS**是一款适合于8位单片机的**key-value**型的数据存取管理库。   
该库最大能管理64K地址的空间数据，空间范围内支持创建**任意长度**和**任意数量**的对象。   
很适合小型单片机用自身的**Flash**或者**Eeprom**作为小型数据存取管理使用。   
EasyFlash在Stm32上使用的很好，就仿照移植了个8位机版本的，实现了变量的存取功能，可以当个**key-value**型的数据库使用。   
算法原生支持**磨损平衡**和**掉电保护**，地址空间内均匀磨损，压榨空间的使用**寿命**的同时注重系统运行的**可靠性**。   
   
本库适合于不想自己手动对存储空间进行管理的用户。   
本库适合于需要频繁对某些数据进行写入的用户，比如系统启动的次数，时间，需频繁更新的配置，需频繁采集并存储的数据。   

#### 软件架构
1.  软件仅包含 `efs.h` `efs.c` `efs_port.c` 3个文件；
2.  其中 `efs_port.c` 文件为用户移植时需要修改的文件，里面仅有3个函数需要实现，用户可以先在单片机内存中辟1块区域测试使用本库，便于快速掌握；
``` C
   (1) efs_port_read(size_t addr, uint8_t *buf, size_t size)        -->  读取数据接口
   (2) efs_port_erase(size_t addr, size_t size)                     -->  擦除数据接口
   (3) efs_port_write(size_t addr, const uint8_t *buf, size_t size) -->  写入数据接口
```
3.  `efs.h` 头文件用户在使用前，需要根据自身实际情况进行修改；
``` C
#define EFS_KEY_LENGHT_MAX  4          // key的最大长度，可为（4，12），这里推荐固定为4B(Bytes)，则32B的BLOCK中最多能存储3个MapTableItem条目，为12B时，则需要对应调整BLOCK为64B

#define EFS_POINTER_DEFAULT 0x0000     // 存储空间擦除之后的默认值，比如我这里，擦除后默认为0x0000
#define EFS_POINTER_NULL    0xffff     // 存储空间已使用的标记，需要与上面的 EFS_POINTER_DEFAULT 不同
#define EFS_START_ADDR      0x0000     // 存储空间的起始地址，如果用户自己管理的话，可以设置为0x0000，否则必须为1个扇区的起始位
#define EFS_AREA_SIZE       0x0300     // 存储空间的大小，代表了能够管理的最大空间大小
#define EFS_BLOCK_SIZE      0x20       // 系统的最小管理单元(块)大小，根据实际尺寸，以(8K,32K)为界，推荐设置为(32,64,128)3个参数
#define EFS_SECTOR_SIZE     0x80       // 扇区的大小，硬件能够擦除的最小区域
#define EFS_INVALID_KEY_MAX 8          // 最大无效key计数，当索引中无效key大于此值时将重建索引表，过小的值会导致频繁重建
                                       // 索引表，这里推荐为2个块的区域大小，当Block为32，Item为8，则推荐值为 2*32/8=8 
```
#### 函数方法介绍
``` C
uint8_t efs_init();  // 初始化函数，需在使用之前调用，它会检查空间格式是否满足要求，并决定是否调用efs_format();函数进行格式化
uint8_t efs_format(); // 空间格式化函数，当出现空间不足的情况时，需要用户调用重新格式化（格式化前记得读取缓存重要数据^_^）
size_t  efs_get_len( uint8_t *key ); // 根据key得到数据的长度，未找到或出错的时候，返回0
uint8_t efs_get( uint8_t *key, uint8_t *buf, size_t bufLen, size_t *dataLen); // 根据key读取数据，   
                                                                              // bufLen为缓冲区的最大长度，   
                                                                              // *dataLen用来返回实际数据长度，可为NULL
uint8_t efs_set( uint8_t *key, uint8_t *buf, size_t bufLen ); // 设置数据
```
#### 快速开始

1.  移植的接口函数   
    这里使用**RAM**作为存储区域，测试对 **EFS** 的使用
``` C
uint8_t _ram_data[128*6];
uint8_t efs_port_read(size_t addr, uint8_t *buf, size_t size) 
{
    memcpy( buf, _ram_data+addr, size );
    return EFS_OK;
}

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
``` C
uint8_t resp;
size_t len;
size_t _count = 0; //计数本轮写入字节数
size_t tmspn_cur, tmspn_avg, tmspn_max, tmspn_min=0xffff;
uint32 tick=0; //计数总调用次数
struct xEepData eep_config; //已在其它地方初始化
uint8_t data[sizeof(struct xEepData)];
void time_start()
{
    // 记录起始时间
}
size_t time_end()
{
    // 返回耗时
}
int main(void)
{ 
  efs_init();
  while(1){
    time_start();
    for( i=0; i<10; i++ ){
      // set key-value
      resp = efs_set("hell", (uint8_t*)&eep_data, sizeof(xEepData));
      if( EFS_OK != resp ){
        printf("efs set failed! ErrorCode: %d",resp);
        delay_ms(500);
      }
      // get value len by key
      len= efs_get_len("hell");
      // get value by key
      resp = efs_get("hell", data, sizeof(xEepData), NULL);
      if( EFS_OK != resp ){
        printf("efs get failed! ErrorCode: %d",resp);
        delay_ms(500);
      }else{
        if( 0 != memcmp( (const void*)&eep_data, (const void*)data, sizeof(struct xEepData)) ){
          printf("efs get data is not equal to the set!");
          delay_ms(500);
        }
      }
      tick++;
    }
    tmspn_cur = time_end();
    tmspn_avg = (tmspn_avg + tmspn_cur)/2;
    if(tmspn_min > tmspn_cur ) tmspn_min = tmspn_cur;
    if(tmspn_max < tmspn_cur ) tmspn_max = tmspn_cur;
    printf("efs tick:%lu cnt:%u avg:%u min:%u max:%u",tick, _count, tmspn_avg, tmspn_min, tmspn_max);
    _count = 0;
}
```

#### 内部数据存储的结构
1. 内部数据管理的基础单元是**BLOCK**（块），并将管理的全部空间分成若干个BLOCK块;
2. 内部共有3种数据结构，每种数据结构均占用1个BLOCK，它们分别是**MapHead**，**MapTable**，**MapBlock**；
3. **MapHead** --> 用来管理key映射表的起始索引，它独占空间中的第1个SECTOR（扇区）,并仅使用其中的第1个BLOCK用来进行索引的管理；   
   **MapTable** --> key索引的条目，即保存在此表中，并成链状，相应key的信息便在此表中进行查找；   
   **MapBlock** --> 数据存储区，key对应的数据，即保存在此表中，并成链状，相应的data数据便在此表中进行读取。   
4. 数据存储的相关框图结构，如下所示
```
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
```
#### 测试结果
笔者正在使用的Stm8的Eeprom。手册上标称有100K的写入次数，但是因为它的Eeprom编程时间固定为6.6ms，导致写入速度并不快，笔者为此也多次优化了写入逻辑，保证写入最小化。如下所示为笔者经过一段时间的测试之后的结果。
```
I/main  [57801844] : efs tick:605440 cnt:572 avg:938 min:856 max:942
I/main  [57802800] : efs tick:605450 cnt:572 avg:938 min:856 max:942
I/main  [57927136] : efs tick:606750 cnt:572 avg:936 min:856 max:942
I/main  [57972080] : efs tick:607220 cnt:572 avg:936 min:856 max:942
```
截止笔者截取时，共进行了60.72w次测试，仍然运行正常。待出现写入错误后，笔者会继续补充此部分内容。
测试使用数据为一个25个字节的数据，占用1个32字节的Block块，key为4个字节，占1个8字节的MapTableItem。
测试结果中，tick为测试次数，其中每10次为1轮，每轮作为基础统计周期。截止写作时已测试60.72w次，cnt为统计周期内底层的实际写入字节数572个字节，其中实际有效字节(25+4)*10=290，存储效率50.7%。
avg为每轮次平均耗时938ms，稳定且 ≈ max最大写入耗时942ms。min是最小写入耗时856ms，为第1次初始化系统后得到。

通过计算可得25个字节的平均写入耗时94ms，即3.76ms/字节，Stm8的Eeprom在写入时，需注意给看门狗留合理的超时时间。

#### 注意事项
   (1) 接口函数传入的 `addr` 这个参数是根据 `efs.h` 中 **EFS_START_ADDR** 这个宏计算出来的， 用户自己管理地址偏移的话，   
       **EFS_START_ADDR** 可以设置为 **0x0000**；   
   (2) **efs_port_erase()** 函数是以扇区(`efs.h` 中 **EFS_SECTOR_SIZE**)来擦除的，传入的参数是扇区的起始地址和扇区的大小，   
                        其中，当前版本中，扇区的大小总是1个扇区的标准大小，可以忽略；   
   (3) 系统在空间不足时，会以扇区为单位对空间尝试进行回收，最恶劣的情况是每个扇区均有有效数据导致扇区均无法回收，只能重新格式化。   
       对于此问题，给出的建议方案是：
A.存储的 `key_num < sec_num-1`保证总有空闲扇区 ；   
B.在尝试`efs_set( )`函数返回**EFS_SPACE_FULL**错误后，手动使用`efs_format()`函数对空间重新进行格式化。   
       下个版本中，考虑引入深度整理功能，将最后一个扇区作为空闲扇区，仅整理的时候使用，整理完之后写回，该扇区重新擦除，但因该操作会导致写入时间不可控，会发布为1个独立的版本，或标记为扩展支持。   
