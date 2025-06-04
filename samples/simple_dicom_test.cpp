#include <iostream>
#include <filesystem>
#include "common/dicom/dicom_file.h"
#include "common/dicom/dicom_object.h"
#include "common/dicom/dicom_tag.h"

using namespace pacs::common::dicom;
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "DICOM File Test with DCMTK Integration\n";
    std::cout << "======================================\n\n";
    
    // Test 1: Create a new DICOM file
    std::cout << "Test 1: Creating a new DICOM file\n";
    DicomObject obj;
    
    // Set basic patient information
    obj.setString(DicomTag::PatientName, "Test^Patient");
    obj.setString(DicomTag::PatientID, "123456");
    obj.setString(DicomTag::PatientBirthDate, "19800101");
    obj.setString(DicomTag::PatientSex, "M");
    
    // Set study information
    obj.setString(DicomTag::StudyInstanceUID, "1.2.3.4.5.6.7.8.9");
    obj.setString(DicomTag::StudyDate, "20240315");
    obj.setString(DicomTag::StudyTime, "120000");
    obj.setString(DicomTag::AccessionNumber, "ACC001");
    obj.setString(DicomTag::Modality, "CT");
    
    // Set series information
    obj.setString(DicomTag::SeriesInstanceUID, "1.2.3.4.5.6.7.8.9.1");
    obj.setInt(DicomTag::SeriesNumber, 1);
    
    // Set instance information
    obj.setString(DicomTag::SOPInstanceUID, "1.2.3.4.5.6.7.8.9.1.1");
    obj.setString(DicomTag::SOPClassUID, "1.2.840.10008.5.1.4.1.1.2"); // CT Image Storage
    obj.setInt(DicomTag::InstanceNumber, 1);
    
    // Create DICOM file
    DicomFile file(obj);
    
    // Generate filename
    std::string filename = file.generateFilename();
    std::cout << "Generated filename: " << filename << "\n";
    
    // Save file
    fs::path outputPath = fs::current_path() / "test_output" / filename;
    fs::create_directories(outputPath.parent_path());
    
    if (file.save(outputPath.string())) {
        std::cout << "File saved successfully to: " << outputPath << "\n";
    } else {
        std::cout << "Failed to save file\n";
        return 1;
    }
    
    // Test 2: Read the saved file
    std::cout << "\nTest 2: Reading the saved DICOM file\n";
    DicomFile readFile;
    
    if (readFile.load(outputPath.string())) {
        std::cout << "File loaded successfully\n";
        
        // Get and display information
        DicomObject readObj = readFile.getObject();
        
        std::cout << "\nPatient Information:\n";
        std::cout << "  Name: " << readObj.getPatientName() << "\n";
        std::cout << "  ID: " << readObj.getPatientID() << "\n";
        std::cout << "  Birth Date: " << readObj.getString(DicomTag::PatientBirthDate) << "\n";
        std::cout << "  Sex: " << readObj.getString(DicomTag::PatientSex) << "\n";
        
        std::cout << "\nStudy Information:\n";
        std::cout << "  Study UID: " << readObj.getStudyInstanceUID() << "\n";
        std::cout << "  Study Date: " << readObj.getStudyDate() << "\n";
        std::cout << "  Study Time: " << readObj.getStudyTime() << "\n";
        std::cout << "  Accession Number: " << readObj.getAccessionNumber() << "\n";
        std::cout << "  Modality: " << readObj.getModality() << "\n";
        
        std::cout << "\nSeries Information:\n";
        std::cout << "  Series UID: " << readObj.getSeriesInstanceUID() << "\n";
        auto seriesNumber = readObj.getInt(DicomTag::SeriesNumber);
        if (seriesNumber.has_value()) {
            std::cout << "  Series Number: " << seriesNumber.value() << "\n";
        }
        
        std::cout << "\nInstance Information:\n";
        std::cout << "  SOP Instance UID: " << readObj.getSOPInstanceUID() << "\n";
        std::cout << "  SOP Class UID: " << readObj.getSOPClassUID() << "\n";
        auto instanceNumber = readObj.getInt(DicomTag::InstanceNumber);
        if (instanceNumber.has_value()) {
            std::cout << "  Instance Number: " << instanceNumber.value() << "\n";
        }
        
        // Test 3: List all tags
        std::cout << "\nTest 3: Listing all tags in the file\n";
        auto allTags = readObj.getAllTags();
        std::cout << "Total tags: " << allTags.size() << "\n";
        
        for (const auto& tag : allTags) {
            std::cout << "  " << tag.toString() << " - " << tag.getName() << "\n";
        }
        
    } else {
        std::cout << "Failed to load file\n";
        return 1;
    }
    
    std::cout << "\nAll tests completed successfully!\n";
    return 0;
}