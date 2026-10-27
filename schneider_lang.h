/// ==========================================
/// SCHNEIDER LANGUAGE DEFINITION (v2.2 FINAL)
/// FILE: schneider_lang.h
/// ==========================================
/// ==========================================
/// BARE METAL: USB MASS STORAGE (BOT)
/// ==========================================
#pragma once
#include <stdint.h>
/// Command Block Wrapper (The packet sent to the USB drive)
struct usb_bot_cbw {
    uint32_t signature;          /// MUST be 'USBC' (0x43425355)
    uint32_t tag;                /// Command tag identifier (e.g. 1, 2, 3...)
    uint32_t data_transfer_length; /// Data transfer length in bytes for phase 2
    uint8_t  flags;              /// Flags: 0x80 = Direction IN (read), 0x00 = Direction OUT (write)
    uint8_t  lun;                /// Logical Unit Number (typically 0)
    uint8_t  cmd_length;         /// SCSI command length (typically 10, 12, or 16 bytes)
    uint8_t  scsi_cmd[16];       /// The actual SCSI command block (e.g. "READ(10)")
} __attribute__((packed));

/// Command Status Wrapper (Response returned by the USB drive)
struct usb_bot_csw {
    uint32_t signature;          /// MUST be 'USBS' (0x53425355)
    uint32_t tag;                /// Matching tag identifier sent in the CBW
    uint32_t data_residue;       /// Data residue: bytes not transferred (should be 0)
    uint8_t  status;             /// Status: 0 = Success, 1 = Command Failed, 2 = Phase Error
} __attribute__((packed));
#ifndef SCHNEIDER_LANG_H
#define SCHNEIDER_LANG_H
/// BARE METAL DEFINITIONS (Ensures uint32_t is recognized by compiler)
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
//typedef unsigned long long uint64_t;
/// ------------------------------------------
/// 1. BASIC CONTROL FLOW & TYPES
/// ------------------------------------------
#define _15   if
#define _19   do
#define _28   case
#define _30   char
#define _37   break
#define _39   for
#define _41   else
#define _42   new
#define _43   int
#define _44   bool
#define _50   void
#define _51   delete
#define _54   float
#define _56   this
#define _57   auto
#define _63   try
#define _64   sizeof      
#define _71   const
#define _82   switch
#define _91   private
#define _96   return
#define _101  continue
#define _121  namespace   
#define _171  goto
/// ------------------------------------------
/// 2. SCHNEIDER COLLISION RESOLUTIONS
/// ------------------------------------------
#define _108  class       
#define _112  friend      
#define _114  while       
#define _126  public      
#define _202  struct
/// ------------------------------------------
/// 3. MISSING STANDARD TYPES & VALUES
/// ------------------------------------------
#define _48   long        
#define _58   signed      
#define _59   double      
#define _80   short       
#define _93   unsigned    
#define _113  nullptr
/// Values
#define _86   false       
#define _128  true
/// ------------------------------------------
/// 4. FIXED WIDTH INTEGERS
/// ------------------------------------------
#define _142  int8_t      
#define _184  uint8_t     
#define _140  int16_t     
#define _182  uint16_t    
#define _68   int32_t     
#define _89   uint32_t    
#define _146  int64_t     
#define _94   uint64_t
/// ------------------------------------------
/// 5. EXCEPTION HANDLING
/// ------------------------------------------
#define _35   catch       
#define _84   throw       
#define _102  noexcept
/// ------------------------------------------
/// 6. MEMORY & MODIFIERS
/// ------------------------------------------
#define _33   asm         
#define _72   static      
#define _74   mutable     
#define _98   explicit    
#define _134  constexpr   
#define _172  extern      
#define _189  inline      
#define _196  export      
#define _303  register    
#define _252  alignas
/// ------------------------------------------
/// 7. CLASSES, TEMPLATES & STRUCTURE
/// ------------------------------------------
#define _53   enum        
#define _69   default     
#define _70   using       
#define _73   union       
#define _81   typedef     
#define _90   decltype    
#define _92   template    
#define _99   typename    
#define _103  virtual     
#define _106  protected   
#define _192  volatile    
#define _216  operator
/// ------------------------------------------
/// 8. C++ CASTS
/// ------------------------------------------
#define _115  static_cast      
#define _191  reinterpret_cast 
#define _224  dynamic_cast     
#define _228  const_cast
/// ------------------------------------------
/// 9. HELPERS
/// ------------------------------------------
#define _79   size_t
/// ------------------------------------------
/// 10. LOGIC
/// ------------------------------------------
#define EQ    ==
#define NEQ   !=
#define AND   &&
#define OR    ||
/// ------------------------------------------
/// 11. STD TYPES
/// ------------------------------------------
#define _87   string      
#define _110  wstring
/// ------------------------------------------
/// 12. MEMORY & CONTAINERS
/// ------------------------------------------
#define _34   free        /// f+r+e+e = 34
#define _60   map         /// Base 30 (Collision with 'char') -> x2
#define _83   vector      /// v+e+c+t+o+r = 83
#define _88   pair        /// Base 44 (Collision with 'bool') -> x2
#define _168  malloc      /// Base 56 (Collision with 'this') -> x3 (Friend=112)
/// ==========================================
/// BARE METAL: MEMORY MANAGER (HEAP)
/// ==========================================
#include <stddef.h> 

struct MemoryBlock {
    uint32_t size;
    uint8_t  is_free;
    MemoryBlock* next;
} __attribute__((packed));

/// Forward declarations for compiler
_50 init_heap();
#ifndef __linux__
_50* malloc(_79 size);
_50 free(_50* ptr);
#else
#include <stdlib.h>
#endif
#endif