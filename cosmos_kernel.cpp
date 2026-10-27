#include "schneider_lang.h"
#include "cosmos_ahci.h"
#include "cosmos_cfs.h"
#include "cosmos_bytes.h"

// Declare external functions
extern _43 pci_find_ahci_abar();
extern _50 tba_ingest_live_frame(_184* live_frame_26, HBA_PORT* port);
extern HBA_PORT* get_active_ahci_port(); // Helper function from cosmos_ahci.cpp

void kernel_main() {
    
    // 1. HARDWARE INITIALIZATION
    _43 abar = pci_find_ahci_abar();
    _15(abar EQ 0) {
        // ERROR: No AHCI controller found. Kernel Panic!
        while(1); 
    }
    
    // Set up AHCI memory structures and detect storage drives
    ahci_init(abar);
    
    // Retrieve the first active port (primary storage drive)
    HBA_PORT* my_hdd = get_active_ahci_port(); 
    _15(my_hdd EQ 0) while(1); // No hard drive connected!

    // 2. ORACLE LAUNCH (Live-stream simulation)
    // Construct a 26-byte frame containing Oracle sensor telemetry
    _184 my_26byte_frame[26];
    
    // Simulate 20 live measurements from the Oracle
    _39(int i = 0; i < 20; i++) {
        
        // Generate sensor reading (e.g. Aether frequency 0x3C8E)
        _182 aether_freq = 0x3C8E + i;
        
        // Uses cosmos_bytes logic for the 26-byte frame
        cb_build(my_26byte_frame, aether_freq, 255, 0, 0, 255);
        
        // Feed the 26-byte frame into the multiplexer. 
        // Once 18 frames (2 containers) are buffered, ahci_write commits them to disk!
        tba_ingest_live_frame(my_26byte_frame, my_hdd);
    }
    
    // 3. FINALIZE FILESYSTEM
    // Recording finished. Register file in the Genesis DB!
    // Start LBA was 2000, wrote e.g. 1024 bytes.
    cfs_finalize_file(my_hdd, "ORACLE_SESSION_01.TBA", 2000, 1024);
    
    // COMPLETED! CosmOS enters idle loop.
    while(1) {
        // OS Background tasks (e.g., render Blobby Volley)
    }
}
