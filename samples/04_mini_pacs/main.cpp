/**
 * @file main.cpp
 * @brief Level 4 Sample: Mini PACS - Complete DICOM server with all services
 *
 * This sample demonstrates a complete Mini PACS implementation integrating:
 * - Verification SCP (C-ECHO) for connectivity testing
 * - Storage SCP (C-STORE) for receiving images from modalities
 * - Query SCP (C-FIND) at Patient/Study/Series/Image levels
 * - Retrieve SCP (C-MOVE/C-GET) for image retrieval
 * - Modality Worklist SCP (MWL C-FIND) for scheduled procedures
 * - MPPS SCP (N-CREATE/N-SET) for procedure tracking
 *
 * After completing this sample, you will understand:
 * 1. Service Integration - Combining multiple SCPs in one server
 * 2. Query/Retrieve (Q/R) - C-FIND and C-MOVE operations
 * 3. Modality Worklist - Scheduled procedure management
 * 4. MPPS - Modality Performed Procedure Step tracking
 * 5. Class-Based Design - Encapsulating PACS functionality
 *
 * @see DICOM PS3.4 - Storage, Query/Retrieve, Worklist, MPPS Service Classes
 */

#include "mini_pacs.hpp"
#include "console_utils.hpp"
#include "signal_handler.hpp"

#include <iostream>
#include <string>

using namespace pacs::samples;

namespace {

/**
 * @brief Generate a unique UID for worklist items
 */
std::string generate_uid() {
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    return "1.2.410.200001.4." + std::to_string(ms) + "." + std::to_string(++counter);
}

/**
 * @brief Get current date in YYYYMMDD format
 */
std::string current_date() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_time{};
    localtime_r(&time, &tm_time);

    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d", &tm_time);
    return buffer;
}

/**
 * @brief Add sample worklist items for testing
 */
void add_sample_worklist_items(mini_pacs& pacs) {
    // Sample patient 1 - CT scan
    worklist_entry wl1;
    wl1.patient_id = "PAT001";
    wl1.patient_name = "DOE^JOHN";
    wl1.patient_birth_date = "19800115";
    wl1.patient_sex = "M";
    wl1.study_uid = generate_uid();
    wl1.accession_number = "ACC001";
    wl1.modality = "CT";
    wl1.scheduled_station_ae = pacs.config().ae_title;
    wl1.scheduled_date = current_date();
    wl1.scheduled_time = "100000";
    wl1.step_id = "STEP001";
    wl1.procedure_description = "CT CHEST WITH CONTRAST";
    wl1.referring_physician = "SMITH^JANE^MD";
    pacs.add_worklist_item(wl1);

    // Sample patient 2 - MRI scan
    worklist_entry wl2;
    wl2.patient_id = "PAT002";
    wl2.patient_name = "SMITH^MARY";
    wl2.patient_birth_date = "19751220";
    wl2.patient_sex = "F";
    wl2.study_uid = generate_uid();
    wl2.accession_number = "ACC002";
    wl2.modality = "MR";
    wl2.scheduled_station_ae = pacs.config().ae_title;
    wl2.scheduled_date = current_date();
    wl2.scheduled_time = "110000";
    wl2.step_id = "STEP002";
    wl2.procedure_description = "MRI BRAIN WITHOUT CONTRAST";
    wl2.referring_physician = "JONES^ROBERT^MD";
    pacs.add_worklist_item(wl2);

    // Sample patient 3 - X-ray
    worklist_entry wl3;
    wl3.patient_id = "PAT003";
    wl3.patient_name = "WILSON^DAVID";
    wl3.patient_birth_date = "19900310";
    wl3.patient_sex = "M";
    wl3.study_uid = generate_uid();
    wl3.accession_number = "ACC003";
    wl3.modality = "CR";
    wl3.scheduled_station_ae = pacs.config().ae_title;
    wl3.scheduled_date = current_date();
    wl3.scheduled_time = "140000";
    wl3.step_id = "STEP003";
    wl3.procedure_description = "CHEST X-RAY PA AND LATERAL";
    wl3.referring_physician = "BROWN^LISA^MD";
    pacs.add_worklist_item(wl3);
}

}  // namespace

int main(int argc, char* argv[]) {
    print_header("Mini PACS - Level 4 Sample");

    // ==========================================================================
    // Part 1: Configuration
    // ==========================================================================
    // The Mini PACS integrates all core DICOM services:
    // - Verification: Network connectivity testing (C-ECHO)
    // - Storage: Image reception and archiving (C-STORE)
    // - Query: Searching for patients/studies/series/images (C-FIND)
    // - Retrieve: Sending images to destinations (C-MOVE/C-GET)
    // - Worklist: Providing scheduled procedures to modalities (MWL)
    // - MPPS: Tracking procedure progress from modalities

    print_section("Part 1: Configuration");

    std::cout << "Mini PACS provides a complete DICOM server with:\n";
    std::cout << "  - Verification SCP:  C-ECHO for connectivity\n";
    std::cout << "  - Storage SCP:       C-STORE for image reception\n";
    std::cout << "  - Query SCP:         C-FIND at all levels\n";
    std::cout << "  - Retrieve SCP:      C-MOVE/C-GET for retrieval\n";
    std::cout << "  - Worklist SCP:      MWL for scheduled procedures\n";
    std::cout << "  - MPPS SCP:          N-CREATE/N-SET for tracking\n\n";

    // Parse command line arguments
    mini_pacs_config config;
    config.ae_title = "MINI_PACS";
    config.port = argc > 1 ? static_cast<uint16_t>(std::stoi(argv[1])) : 11112;
    config.storage_path = "./pacs_data";
    config.max_associations = 50;
    config.enable_worklist = true;
    config.enable_mpps = true;
    config.verbose_logging = true;

    print_table("Mini PACS Configuration", {
        {"AE Title", config.ae_title},
        {"Port", std::to_string(config.port)},
        {"Storage Path", config.storage_path.string()},
        {"Max Associations", std::to_string(config.max_associations)},
        {"Worklist Enabled", config.enable_worklist ? "Yes" : "No"},
        {"MPPS Enabled", config.enable_mpps ? "Yes" : "No"}
    });

    print_success("Part 1 complete - Configuration ready!");

    // ==========================================================================
    // Part 2: Create and Start PACS
    // ==========================================================================
    // The mini_pacs class encapsulates all services and manages:
    // - File storage with hierarchical organization
    // - SQLite index database for fast queries
    // - All SCP services with proper handler registration
    // - Event handlers for association lifecycle

    print_section("Part 2: Create and Start PACS");

    mini_pacs pacs(config);

    // Add sample worklist items for testing
    add_sample_worklist_items(pacs);

    std::cout << "Added " << pacs.get_worklist_items().size()
              << " sample worklist items for MWL testing.\n\n";

    if (!pacs.start()) {
        print_error("Failed to start Mini PACS");
        return 1;
    }

    print_success("Part 2 complete - Mini PACS started!");

    // ==========================================================================
    // Part 3: Display Service Information
    // ==========================================================================

    print_section("Part 3: Running Server");

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   Mini PACS Server Started                   ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  AE Title: " << config.ae_title;
    for (size_t i = config.ae_title.length(); i < 48; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║  Port:     " << config.port;
    std::string port_str = std::to_string(config.port);
    for (size_t i = port_str.length(); i < 48; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Services:                                                   ║\n";
    std::cout << "║    [x] Verification (C-ECHO)                                 ║\n";
    std::cout << "║    [x] Storage (C-STORE)                                     ║\n";
    std::cout << "║    [x] Query (C-FIND Patient/Study/Series/Image)             ║\n";
    std::cout << "║    [x] Retrieve (C-MOVE/C-GET)                               ║\n";
    std::cout << "║    [x] Modality Worklist (MWL C-FIND)                        ║\n";
    std::cout << "║    [x] MPPS (N-CREATE/N-SET)                                 ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Test Commands:                                              ║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Connectivity:                                               ║\n";
    std::cout << "║    echoscu -aec MINI_PACS localhost " << config.port;
    for (size_t i = port_str.length(); i < 22; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Store Images:                                               ║\n";
    std::cout << "║    storescu -aec MINI_PACS localhost " << config.port << " *.dcm";
    for (size_t i = port_str.length(); i < 16; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Query Patient Level:                                        ║\n";
    std::cout << "║    findscu -aec MINI_PACS -P \\                               ║\n";
    std::cout << "║      -k QueryRetrieveLevel=PATIENT \\                         ║\n";
    std::cout << "║      -k PatientName=\"*\" localhost " << config.port;
    for (size_t i = port_str.length(); i < 27; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Query Study Level:                                          ║\n";
    std::cout << "║    findscu -aec MINI_PACS -S \\                               ║\n";
    std::cout << "║      -k QueryRetrieveLevel=STUDY \\                           ║\n";
    std::cout << "║      -k StudyDate=\"\" localhost " << config.port;
    for (size_t i = port_str.length(); i < 29; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Query Worklist:                                             ║\n";
    std::cout << "║    findscu -aec MINI_PACS -W \\                               ║\n";
    std::cout << "║      -k ScheduledProcedureStepStartDate=\"\" \\                 ║\n";
    std::cout << "║      -k Modality=\"\" localhost " << config.port;
    for (size_t i = port_str.length(); i < 30; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Retrieve Study:                                             ║\n";
    std::cout << "║    movescu -aec MINI_PACS -aem DEST \\                        ║\n";
    std::cout << "║      -k QueryRetrieveLevel=STUDY \\                           ║\n";
    std::cout << "║      -k StudyInstanceUID=\"...\" localhost " << config.port;
    for (size_t i = port_str.length(); i < 20; ++i) std::cout << " ";
    std::cout << "║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Worklist Items: 3 scheduled procedures                      ║\n";
    std::cout << "║                                                              ║\n";
    std::cout << "║  Press Ctrl+C to stop                                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    // Install signal handler for graceful shutdown
    scoped_signal_handler sig_handler([&pacs]() {
        std::cout << "\n" << colors::yellow
                  << "Shutdown signal received..."
                  << colors::reset << "\n";
        pacs.stop();
    });

    // Wait for shutdown
    sig_handler.wait();

    // ==========================================================================
    // Part 4: Statistics and Cleanup
    // ==========================================================================

    print_section("Final Statistics");

    const auto& stats = pacs.statistics();

    print_table("Association Statistics", {
        {"Total Associations", std::to_string(stats.associations_total.load())},
        {"Active Associations", std::to_string(stats.associations_active.load())}
    });

    print_table("Operation Statistics", {
        {"C-ECHO Count", std::to_string(stats.c_echo_count.load())},
        {"C-STORE Count", std::to_string(stats.c_store_count.load())},
        {"C-FIND Count", std::to_string(stats.c_find_count.load())},
        {"C-MOVE Count", std::to_string(stats.c_move_count.load())},
        {"C-GET Count", std::to_string(stats.c_get_count.load())},
        {"MWL Queries", std::to_string(stats.mwl_count.load())},
        {"MPPS N-CREATE", std::to_string(stats.mpps_create_count.load())},
        {"MPPS N-SET", std::to_string(stats.mpps_set_count.load())}
    });

    print_table("Data Statistics", {
        {"Bytes Received", std::to_string(stats.bytes_received.load()) + " bytes"}
    });

    // Summary
    print_box({
        "Congratulations! You have learned:",
        "",
        "1. Service Integration  - Combining multiple SCPs",
        "2. Query/Retrieve       - C-FIND and C-MOVE operations",
        "3. Modality Worklist    - Scheduled procedure queries",
        "4. MPPS                 - Procedure progress tracking",
        "5. Class-Based Design   - Encapsulating PACS functionality",
        "",
        "Query the database:",
        "  sqlite3 ./pacs_data/index.db \"SELECT * FROM patients;\"",
        "  sqlite3 ./pacs_data/index.db \"SELECT * FROM studies;\"",
        "",
        "Next step: Level 5 - Production PACS (TLS, RBAC, REST API)"
    });

    print_success("Mini PACS terminated successfully.");

    return 0;
}
