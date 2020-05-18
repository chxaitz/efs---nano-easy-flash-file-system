#ifndef _COLL_TYPES_H
#define _COLL_TYPES_H

#include <stdio.h>
/*关于大小端的说明:
大端：高位在前 小端：低位在前
STM32:小端 STM8:大端*/
#define MEM_ALN_BIG     0       /*大端对齐*/
#define MEM_ALN_SML     1       /*小端对齐*/

#define MCU_TYPE_C51    1
#define MCU_TYPE_AVR    2
#define MCU_TYPE_STM8   3
#define MCU_TYPE_STM32  4
#define MCU_TYPE        MCU_TYPE_STM8
//如果不是51类型的单片机
  #if( MCU_TYPE == MCU_TYPE_C51 )
#define MEM_ALN_MODE MEM_ALN_SML
#define __code
  #elif( MCU_TYPE == MCU_TYPE_STM8 )
// #define assert_param(expr) ((expr) ? (void)0 : assert_failed((uint8_t *)__FILE__, __LINE__))
#define MEM_ALN_MODE MEM_ALN_BIG
#define __code
#include "stm8s.h"
  #elif( MCU_TYPE == MCU_TYPE_STM32 )
#define MEM_ALN_MODE MEM_ALN_SML
#define __code   
#include "stm32f10x.h"
  #endif

//规划用于存储各种集合类型
  #ifndef _MACRO_AND_CONST_H_
#define _MACRO_AND_CONST_H_
typedef unsigned short uint16;
typedef unsigned short uint;
typedef unsigned short WORD;
//typedef unsigned int u16;
typedef short int16;
typedef short INT16;
//typedef short s16;
typedef unsigned long uint32;
typedef unsigned long UINT32;
typedef unsigned long DWORD;
//typedef unsigned long u32;
typedef long int32;
typedef long INT32;
//typedef long s32;
typedef signed char int8;
//typedef signed char s8;
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned char uint8;
//typedef unsigned char u8;
typedef unsigned char BOOL;
//typedef enum {DISABLE = 0, ENABLE = !DISABLE} eState;
//typedef enum {RESET = 0, SET = !RESET} eFlag;
//typedef FuncState		eState; /*因为已经在stm32f10x.h中定义，所以上面的先注释掉*/
typedef FlagStatus	eFlag;  /*因为已经在stm32f10x.h中定义，所以上面的先注释掉*/
typedef struct _POINT{
  uint16 x;
  uint16 y;
} Point;
typedef struct _SIZE{
  uint16 height;
  uint16 width;
} Size;
    #if( MEM_ALN_MODE == MEM_ALN_BIG )
typedef union MWORD{
  uint16 u16;
  uint8  u8[2];
  struct{
  uint8  u8h;
  uint8  u8l;
  };
} MWORD;
typedef union MDWORD{
  uint32 u32;
  uint16 u16[2];
  uint8  u8[4];
  struct{
  uint16 u16h;
  uint16 u16l;
  };
  struct{
  uint8 u8hh;
  uint8 u8hl;
  uint8 u8lh;
  uint8 u8ll;
  };
} MDWORD;
    #elif( MEM_ALN_MODE == MEM_ALN_SML )
typedef union MWORD{
  uint16 u16;
  uint8  u8[2];
  struct{
  uint8  u8l;
  uint8  u8h;
  };
} MWORD;
typedef union MDWORD{
  uint32 u32;
  uint16 u16[2];
  uint8  u8[4];
  struct{
  uint16 u16l;
  uint16 u16h;
  };
  struct{
  uint8 u8ll;
  uint8 u8lh;
  uint8 u8hl;
  uint8 u8hh;
  };
} MDWORD;
    #endif  //MEM_ALN_MODE

    #ifndef NOP
#define NOP     0xff
    #endif  //NOP
    #ifndef FALSE
#define FALSE   0x00
    #endif  //FALSE
    #ifndef TRUE
#define TRUE    !FALSE
    #endif  //TRUE
    #ifndef NULL
#define NULL 0x00
    #endif  //NULL
  #endif  //_MACRO_AND_CONST_H_

extern volatile u8 u8_ecs_depths;
#define HAL_ENTER_CRITICAL_SECTION()   {if( 0x00==u8_ecs_depths++ ){sim();}}
#define HAL_EXIT_CRITICAL_SECTION()    {if(u8_ecs_depths>0x01){u8_ecs_depths--;}else{ u8_ecs_depths=0x00;rim(); }}

//#define HAL_ENTER_CRITICAL_SECTION()    __disable_interrupt();
//#define HAL_EXIT_CRITICAL_SECTION()     __enable_interrupt();

#define BV(n)    (1<<n)
#define CV(n)	(~(1<<n))
#define GET_BIT(m,n)    (m&n)
#define SET_BIT(m,n)    (m|=n)
#define CLR_BIT(m,n)    (m&=(~n))

#endif
