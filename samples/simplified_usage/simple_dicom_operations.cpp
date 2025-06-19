#include <iostream>
#include <string>
#include <filesystem>
#include <memory>

#include "common/dicom/dicom_object.h"
#include "common/dicom/dicom_file.h"
#include "common/dicom/dicom_tag.h"
#include "thread_system/sources/logger/logger.h"

namespace fs = std::filesystem;
using namespace pacs::common::dicom;
using namespace log_module;

/**
 * This sample demonstrates basic DICOM operations using the simplified API:
 * 1. Creating a DICOM object from scratch
 * 2. Saving it to a file
 * 3. Loading a DICOM file
 * 4. Modifying DICOM attributes
 * 5. Checking DICOM tags
 */
int main(int argc, char* argv[]) {
    try {
        // Initialize logger
        set_title("SIMPLE_DICOM_OPERATIONS");
        console_target(log_types::Information | log_types::Error);
        start();
        
        write_information("Starting DICOM Operations Sample...");
        
        // Create output directory if it doesn't exist
        std::string outputDir = "./dicom_output";
        if (!fs::exists(outputDir)) {
            fs::create_directories(outputDir);
        }
        
        // PART 1: Create a new DICOM object from scratch
        write_information("Creating a new DICOM object from scratch...");
        
        DicomObject newObj;
        
        // Set patient information
        newObj.setString(DicomTag::PatientName, "DOE^JOHN");
        newObj.setString(DicomTag::PatientID, "12345");
        newObj.setString(DicomTag::PatientBirthDate, "19700101");
        newObj.setString(DicomTag::PatientSex, "M");
        
        // Set study information
        newObj.setString(DicomTag::StudyInstanceUID, "1.2.3.4.5.6.7.8.9.0");
        newObj.setString(DicomTag::StudyDate, "20250619");
        newObj.setString(DicomTag::StudyTime, "112500");
        newObj.setString(DicomTag::AccessionNumber, "A12345");
        newObj.setString(DicomTag::Modality, "CT");
        
        // Set series information
        newObj.setString(DicomTag::SeriesInstanceUID, "1.2.3.4.5.6.7.8.9.0.1");
        newObj.setInt(DicomTag::SeriesNumber, 1);
        
        // Set instance information
        newObj.setString(DicomTag::SOPInstanceUID, "1.2.3.4.5.6.7.8.9.0.1.2");
        newObj.setString(DicomTag::SOPClassUID, "1.2.840.10008.5.1.4.1.1.2"); // CT Image Storage
        newObj.setInt(DicomTag::InstanceNumber, 1);
        
        // Create a DICOM file and save it
        DicomFile newFile(newObj);
        std::string outputPath = outputDir + "/simple_dicom.dcm";
        
        if (newFile.save(outputPath)) {
            write_information("Successfully saved DICOM file to: %s", outputPath.c_str());
        } else {
            write_error("Failed to save DICOM file");
        }
        
        // PART 2: Load a DICOM file and modify it
        write_information("Loading and modifying the DICOM file...");
        
        DicomFile loadedFile;
        if (loadedFile.load(outputPath)) {
            DicomObject obj = loadedFile.getObject();
            
            // Display patient information
            write_information("Patient Name: %s", obj.getPatientName().c_str());
            write_information("Patient ID: %s", obj.getPatientID().c_str());
            
            // Check if certain tags exist
            if (obj.hasTag(DicomTag::StudyDate)) {
                write_information("Study Date: %s", obj.getString(DicomTag::StudyDate).c_str());
            }
            
            if (obj.hasTag(DicomTag::Modality)) {
                write_information("Modality: %s", obj.getString(DicomTag::Modality).c_str());
            }
            
            // Modify the patient name
            write_information("Modifying patient name...");
            obj.setString(DicomTag::PatientName, "SMITH^JANE");
            
            // Get numeric values with proper type conversion
            if (auto instanceNumber = obj.getInt(DicomTag::InstanceNumber)) {
                write_information("Instance Number: %d", *instanceNumber);
            }
            
            // Save the modified file
            loadedFile.setObject(obj);
            std::string modifiedPath = outputDir + "/modified_dicom.dcm";
            
            if (loadedFile.save(modifiedPath)) {
                write_information("Successfully saved modified DICOM file to: %s", modifiedPath.c_str());
            } else {
                write_error("Failed to save modified DICOM file");
            }
            
            // Reload the modified file to verify changes
            DicomFile verifyFile;
            if (verifyFile.load(modifiedPath)) {
                DicomObject verifyObj = verifyFile.getObject();
                write_information("Verified patient name: %s", verifyObj.getPatientName().c_str());
            }
        } else {
            write_error("Failed to load DICOM file");
        }
        
        write_information("DICOM Operations Sample completed successfully");
    }
    catch (const std::exception& ex) {
        write_error("Error: %s", ex.what());
        stop();
        return 1;
    }
    
    stop();
    return 0;
}