/**
 * @file main.cpp
 * @brief Level 1 Sample: Hello DICOM - Introduction to DICOM fundamentals
 *
 * This sample demonstrates the basic building blocks of DICOM:
 * - DICOM Tags: (Group, Element) pairs that identify data elements
 * - Value Representations (VR): Data types in DICOM
 * - DICOM Dataset: Collection of data elements
 * - DICOM File: Part 10 file format for storing DICOM data
 *
 * After completing this sample, you will understand how to:
 * 1. Create and manipulate DICOM tags
 * 2. Work with different VR types
 * 3. Build DICOM datasets with patient/study information
 * 4. Save and load DICOM Part 10 files
 */

#include "console_utils.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <iomanip>
#include <iostream>

namespace core = pacs::core;
namespace enc = pacs::encoding;
namespace tags = pacs::core::tags;

int main() {
    pacs::samples::print_header("Hello DICOM - Level 1 Sample");

    // ═══════════════════════════════════════════════════════════════════════════
    // Part 1: Understanding DICOM Tags
    // ═══════════════════════════════════════════════════════════════════════════
    // DICOM tags identify data elements using (Group, Element) pairs.
    // - Group 0010 = Patient information
    // - Group 0008 = Study/Series/Instance identification
    // - Group 0028 = Image pixel parameters

    pacs::samples::print_section("Part 1: DICOM Tags");

    std::cout << "DICOM tags are the fundamental identifiers for data elements.\n";
    std::cout << "Each tag is a (Group, Element) pair of 16-bit hexadecimal numbers.\n\n";

    // Create tags using different methods
    core::dicom_tag patient_name_tag{0x0010, 0x0010};  // Explicit group/element
    core::dicom_tag patient_id_tag = tags::patient_id; // Using predefined constants

    std::cout << "Patient Name Tag: " << patient_name_tag.to_string() << "\n";
    std::cout << "  Group:   0x" << std::hex << std::setw(4) << std::setfill('0')
              << patient_name_tag.group() << std::dec << "\n";
    std::cout << "  Element: 0x" << std::hex << std::setw(4) << std::setfill('0')
              << patient_name_tag.element() << std::dec << "\n";
    std::cout << "  Private? " << (patient_name_tag.is_private() ? "Yes" : "No") << "\n\n";

    // Parse tag from string representation
    auto parsed = core::dicom_tag::from_string("(0008,0020)");
    if (parsed) {
        std::cout << "Parsed Study Date tag from string: " << parsed->to_string() << "\n";
    }

    // Demonstrate tag comparison
    std::cout << "\nTag comparison:\n";
    std::cout << "  (0010,0010) == tags::patient_name? "
              << (patient_name_tag == tags::patient_name ? "Yes" : "No") << "\n";

    pacs::samples::print_success("Part 1 complete - DICOM tags understood!");

    // ═══════════════════════════════════════════════════════════════════════════
    // Part 2: Creating a DICOM Dataset
    // ═══════════════════════════════════════════════════════════════════════════
    // A dataset is an ordered collection of DICOM elements.
    // Each element has: Tag + VR (Value Representation) + Value

    pacs::samples::print_section("Part 2: DICOM Dataset");

    std::cout << "A DICOM dataset is an ordered collection of data elements.\n";
    std::cout << "Each element consists of: Tag + VR + Value\n\n";

    std::cout << "Common VR (Value Representation) types:\n";
    std::cout << "  PN = Person Name      DA = Date           TM = Time\n";
    std::cout << "  LO = Long String      SH = Short String   CS = Code String\n";
    std::cout << "  UI = Unique ID        US = Unsigned Short IS = Integer String\n\n";

    core::dicom_dataset dataset;

    // --- Patient Module (Group 0010) ---
    // PN = Person Name format: Family^Given^Middle^Prefix^Suffix
    dataset.set_string(tags::patient_name, enc::vr_type::PN, "DOE^JOHN^M");
    dataset.set_string(tags::patient_id, enc::vr_type::LO, "PAT001");
    dataset.set_string(tags::patient_birth_date, enc::vr_type::DA, "19850315");
    dataset.set_string(tags::patient_sex, enc::vr_type::CS, "M");

    // --- Study Module (Group 0008) ---
    dataset.set_string(tags::study_date, enc::vr_type::DA, "20240115");
    dataset.set_string(tags::study_time, enc::vr_type::TM, "143022");
    dataset.set_string(tags::study_description, enc::vr_type::LO, "CT CHEST W/CONTRAST");
    dataset.set_string(tags::accession_number, enc::vr_type::SH, "ACC123456");

    // --- Series Module ---
    dataset.set_string(tags::modality, enc::vr_type::CS, "CT");
    dataset.set_string(tags::series_description, enc::vr_type::LO, "AXIAL 3mm");
    dataset.set_numeric<uint32_t>(tags::series_number, enc::vr_type::IS, 1);
    dataset.set_numeric<uint32_t>(tags::instance_number, enc::vr_type::IS, 1);

    // --- UID elements (required for valid DICOM) ---
    // UIDs uniquely identify study/series/instance across the world
    dataset.set_string(tags::study_instance_uid, enc::vr_type::UI,
                       "1.2.410.200001.1.1.20240115.143022.1");
    dataset.set_string(tags::series_instance_uid, enc::vr_type::UI,
                       "1.2.410.200001.1.1.20240115.143022.1.1");
    dataset.set_string(tags::sop_class_uid, enc::vr_type::UI,
                       "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
    dataset.set_string(tags::sop_instance_uid, enc::vr_type::UI,
                       "1.2.410.200001.1.1.20240115.143022.1.1.1");

    // --- Image Pixel Module ---
    // US = Unsigned Short (16-bit integer)
    dataset.set_numeric<uint16_t>(tags::rows, enc::vr_type::US, 512);
    dataset.set_numeric<uint16_t>(tags::columns, enc::vr_type::US, 512);
    dataset.set_numeric<uint16_t>(tags::bits_allocated, enc::vr_type::US, 16);
    dataset.set_numeric<uint16_t>(tags::bits_stored, enc::vr_type::US, 12);
    dataset.set_numeric<uint16_t>(tags::high_bit, enc::vr_type::US, 11);
    dataset.set_numeric<uint16_t>(tags::pixel_representation, enc::vr_type::US, 0);
    dataset.set_numeric<uint16_t>(tags::samples_per_pixel, enc::vr_type::US, 1);
    dataset.set_string(tags::photometric_interpretation, enc::vr_type::CS, "MONOCHROME2");

    // Reading values back from the dataset
    std::cout << "Dataset created with " << dataset.size() << " elements:\n\n";

    pacs::samples::print_table("Patient Information", {
        {"Name", dataset.get_string(tags::patient_name)},
        {"ID", dataset.get_string(tags::patient_id)},
        {"Birth Date", dataset.get_string(tags::patient_birth_date)},
        {"Sex", dataset.get_string(tags::patient_sex)}
    });

    pacs::samples::print_table("Study Information", {
        {"Date", dataset.get_string(tags::study_date)},
        {"Description", dataset.get_string(tags::study_description)},
        {"Modality", dataset.get_string(tags::modality)},
        {"Accession", dataset.get_string(tags::accession_number)}
    });

    // Reading numeric values
    auto rows_val = dataset.get_numeric<uint16_t>(tags::rows);
    auto cols_val = dataset.get_numeric<uint16_t>(tags::columns);
    std::cout << "\nImage dimensions: "
              << rows_val.value_or(0) << " x " << cols_val.value_or(0) << " pixels\n";

    pacs::samples::print_success("Part 2 complete - Dataset created with patient and study info!");

    // ═══════════════════════════════════════════════════════════════════════════
    // Part 3: Saving as DICOM File
    // ═══════════════════════════════════════════════════════════════════════════
    // DICOM Part 10 file format:
    // - 128 bytes preamble (typically zeros)
    // - "DICM" magic number (4 bytes)
    // - File Meta Information (Group 0002, always Explicit VR LE)
    // - Dataset (encoded per specified Transfer Syntax)

    pacs::samples::print_section("Part 3: DICOM File I/O");

    std::cout << "DICOM Part 10 file structure:\n";
    std::cout << "  1. 128-byte preamble (typically zeros)\n";
    std::cout << "  2. 'DICM' magic number\n";
    std::cout << "  3. File Meta Information (always Explicit VR Little Endian)\n";
    std::cout << "  4. Dataset (encoded per Transfer Syntax)\n\n";

    // Create file with Explicit VR Little Endian transfer syntax
    // This is the most widely compatible format
    auto dicom_file = core::dicom_file::create(
        dataset,
        enc::transfer_syntax::explicit_vr_little_endian);

    std::cout << "Transfer Syntax: "
              << dicom_file.transfer_syntax().name() << "\n\n";

    // Save to disk
    const std::string output_path = "hello_dicom_output.dcm";
    auto save_result = dicom_file.save(output_path);

    if (save_result.is_ok()) {
        pacs::samples::print_success("File saved: " + output_path);
    } else {
        pacs::samples::print_error("Save failed: " + save_result.error().message);
        return 1;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Part 4: Reading DICOM File
    // ═══════════════════════════════════════════════════════════════════════════

    pacs::samples::print_section("Part 4: Reading DICOM File");

    auto read_result = core::dicom_file::open(output_path);

    if (read_result.is_ok()) {
        auto& loaded_file = read_result.value();
        const auto& loaded_ds = loaded_file.dataset();

        pacs::samples::print_success("File loaded successfully!");

        std::cout << "\nFile properties:\n";
        std::cout << "  Transfer Syntax: " << loaded_file.transfer_syntax().name() << "\n";
        std::cout << "  SOP Class UID:   " << loaded_file.sop_class_uid() << "\n";
        std::cout << "  Element count:   " << loaded_ds.size() << "\n\n";

        // Iterate through all elements and display them
        std::cout << "All elements in the dataset:\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& [tag, element] : loaded_ds) {
            auto value_result = element.as_string();
            std::string value_str = value_result.is_ok()
                ? value_result.value()
                : "<binary data>";

            // Truncate long values for display
            if (value_str.length() > 40) {
                value_str = value_str.substr(0, 37) + "...";
            }

            std::cout << tag.to_string() << " "
                      << enc::to_string(element.vr()) << " = "
                      << value_str << "\n";
        }
        std::cout << std::string(60, '-') << "\n";

    } else {
        pacs::samples::print_error("Read failed: " + read_result.error().message);
        return 1;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Summary
    // ═══════════════════════════════════════════════════════════════════════════

    pacs::samples::print_section("Summary");

    pacs::samples::print_box({
        "Congratulations! You have learned:",
        "",
        "1. DICOM Tags - (Group, Element) pairs that identify data",
        "2. Value Representations - Data types like PN, DA, UI, US",
        "3. DICOM Dataset - Ordered collection of elements",
        "4. DICOM File - Part 10 format for file storage",
        "",
        "Next step: Level 2 - Echo Server (DICOM networking)"
    });

    std::cout << "\nYou can verify the output file with DCMTK:\n";
    std::cout << "  dcmdump " << output_path << "\n\n";

    return 0;
}
