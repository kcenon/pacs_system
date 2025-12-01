# Integration Test Data

This directory is reserved for sample DICOM test files used by the integration test suite.

## Generated Test Data

The integration tests primarily use **programmatically generated** DICOM datasets
through the `test_fixtures.hpp` helper functions:

- `generate_ct_dataset()` - Creates minimal CT Image datasets
- `generate_mr_dataset()` - Creates minimal MR Image datasets
- `generate_worklist_item()` - Creates Worklist query items

This approach ensures:
- Reproducible test results
- No external file dependencies
- Tests work in CI/CD environments
- Privacy compliance (no real patient data)

## Optional External Test Files

If you want to test with real DICOM files, place them in this directory:

```
test_data/
├── ct/
│   └── *.dcm         # CT Image files
├── mr/
│   └── *.dcm         # MR Image files
├── us/
│   └── *.dcm         # US Image files
└── multiframe/
    └── *.dcm         # Multi-frame images
```

## Obtaining Test Data

Public DICOM test datasets are available from:

- [DICOM Sample Images](https://www.dicomstandard.org/dicomweb-samples)
- [Cancer Imaging Archive](https://www.cancerimagingarchive.net/)
- [OSIRIX Sample Data](https://www.osirix-viewer.com/resources/dicom-image-library/)

## Notes

- Test files should be anonymized
- Keep file sizes reasonable for CI testing
- Large datasets should be downloaded on-demand, not committed
