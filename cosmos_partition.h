#ifndef COSMOS_PARTITION_H
#define COSMOS_PARTITION_H

#include <stdint.h>
#include "schneider_lang.h"

#pragma pack(push, 1)

/// Sector 1 layout (GPT Header)
struct GPTHeader {
    uint64_t signature;         /// Must be "EFI PART"
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCRC32;
    uint32_t reserved;
    uint64_t myLBA;
    uint64_t alternateLBA;
    uint64_t firstUsableLBA;
    uint64_t lastUsableLBA;
    uint8_t  diskGUID[16];
    uint64_t partitionEntryLBA; /// Partition entries start LBA
    uint32_t numPartitionEntries;
    uint32_t partitionEntrySize;
    uint32_t partitionEntryArrayCRC32;
};

/// Sector 2+ layout (Partition entries)
struct GPTPartitionEntry {
    uint8_t  partitionTypeGUID[16];
    uint8_t  uniquePartitionGUID[16];
    uint64_t startingLBA;       /// Starting LBA of partition
    uint64_t endingLBA;
    uint64_t attributes;
    uint16_t partitionName[36]; /// UTF-16 string
};

#pragma pack(pop)

/// Bridge interface to OS1
extern "C" void scan_partitions(uint8_t port_no);

/// Storage array for detected partitions (Bridge interface to OS2)
extern uint64_t gpt_partition_starts[16];
extern int gpt_partition_count;

#endif