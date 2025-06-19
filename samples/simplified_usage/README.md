# Simplified DICOM API Usage Examples

This directory contains examples showing how to use the simplified DICOM API for basic DICOM operations.

## Simple DICOM Operations

The `simple_dicom_operations` sample demonstrates basic DICOM operations using the simplified API:

1. Creating a DICOM object from scratch
2. Setting DICOM attributes with proper typing
3. Saving a DICOM object to a file
4. Loading a DICOM file
5. Retrieving and modifying DICOM attributes
6. Checking for the existence of tags

## Building

```bash
# In the build directory
cmake --build . --target simple_dicom_operations
```

## Running the Sample

```bash
./bin/simple_dicom_operations
```

The sample will:

1. Create a new DICOM object with patient, study, series, and instance information
2. Save the DICOM object to `./dicom_output/simple_dicom.dcm`
3. Load the saved DICOM file
4. Display selected attributes
5. Modify the patient name
6. Save the modified object to `./dicom_output/modified_dicom.dcm`
7. Reload and verify the modified file

## Code Highlights

### Creating a DICOM Object

```cpp
DicomObject newObj;

// Set attributes with proper types
newObj.setString(DicomTag::PatientName, "DOE^JOHN");
newObj.setString(DicomTag::PatientID, "12345");
newObj.setInt(DicomTag::SeriesNumber, 1);
```

### Loading a DICOM File

```cpp
DicomFile loadedFile;
if (loadedFile.load("/path/to/file.dcm")) {
    DicomObject obj = loadedFile.getObject();
    // Work with the DICOM object
}
```

### Working with DICOM Tags

```cpp
// Using predefined tag constants
std::string patientName = obj.getString(DicomTag::PatientName);

// Checking if a tag exists
if (obj.hasTag(DicomTag::StudyDate)) {
    // Process the study date
}

// Getting typed values with proper error handling
if (auto instanceNumber = obj.getInt(DicomTag::InstanceNumber)) {
    // Use the instance number (which is an int)
    int number = *instanceNumber;
}
```

### Saving a DICOM File

```cpp
DicomFile file(obj);
if (file.save("/path/to/output.dcm")) {
    // File saved successfully
}
```

## Benefits of the Simplified API

1. **Type Safety**: Get attributes with proper types (string, int, float)
2. **Error Handling**: Optional returns for potentially missing values
3. **Predefined Tags**: No need to remember numeric tag values
4. **Intuitive Interface**: Method names clearly describe their purpose
5. **Memory Safety**: Smart pointers handle resource management automatically