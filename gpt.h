// gpt.h - Modern GPT partition table interface
struct GPTHeader {
    uint64_t signature;       // Must be "EFI PART"
    uint32_t revision;
    uint32_t headerSize;
    uint32_t headerCRC32;
    uint32_t reserved;
    uint64_t myLBA;
    uint64_t alternateLBA;
    uint64_t firstUsableLBA;
    uint64_t lastUsableLBA;
    uint8_t  diskGUID[16];
    uint64_t partitionEntryLBA; // Partition array start LBA (typically LBA 2)
    uint32_t numPartitionEntries;
    uint32_t partitionEntrySize; // Typically 128 bytes
};

struct GPTPartitionEntry {
    uint8_t  partitionTypeGUID[16];
    uint8_t  uniquePartitionGUID[16];
    uint64_t startingLBA;       // Starting LBA of partition
    uint64_t endingLBA;
    uint64_t attributes;
    uint16_t partitionName[36]; // UTF-16
};