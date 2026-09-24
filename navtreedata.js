/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "PACS System", "index.html", [
    [ "System Overview", "index.html#overview", null ],
    [ "Key Features", "index.html#features", null ],
    [ "Architecture Diagram", "index.html#architecture", null ],
    [ "Quick Start", "index.html#quickstart", null ],
    [ "Installation", "index.html#installation", [
      [ "CMake FetchContent (Recommended)", "index.html#install_fetchcontent", null ],
      [ "vcpkg", "index.html#install_vcpkg", null ]
    ] ],
    [ "Module Overview", "index.html#modules", null ],
    [ "CLI Tools", "index.html#examples", null ],
    [ "Learning Resources", "index.html#learning_resources", null ],
    [ "Related Systems", "index.html#related", null ],
    [ "Tutorial: DICOM Fundamentals", "tutorial_dicom_basics.html", [
      [ "Goal", "tutorial_dicom_basics.html#dicom_goal", null ],
      [ "What is DICOM?", "tutorial_dicom_basics.html#dicom_what", null ],
      [ "SOP Classes", "tutorial_dicom_basics.html#dicom_sop", null ],
      [ "Transfer Syntaxes", "tutorial_dicom_basics.html#dicom_ts", null ],
      [ "UIDs", "tutorial_dicom_basics.html#dicom_uid", null ],
      [ "Patient/Study/Series/Image Hierarchy", "tutorial_dicom_basics.html#dicom_hierarchy", null ],
      [ "Read your first DICOM file", "tutorial_dicom_basics.html#dicom_read", null ],
      [ "Next Steps", "tutorial_dicom_basics.html#dicom_next", null ]
    ] ],
    [ "Tutorial: C-STORE Workflow", "tutorial_store_workflow.html", [
      [ "Goal", "tutorial_store_workflow.html#store_goal", null ],
      [ "Step 1: Run a C-STORE SCP (Storage Provider)", "tutorial_store_workflow.html#store_scp", null ],
      [ "Step 2: Send a file from a C-STORE SCU (Storage User)", "tutorial_store_workflow.html#store_scu", null ],
      [ "Association Negotiation", "tutorial_store_workflow.html#store_negotiation", null ],
      [ "Common Mistakes", "tutorial_store_workflow.html#store_mistakes", null ],
      [ "Next Steps", "tutorial_store_workflow.html#store_next", null ]
    ] ],
    [ "Tutorial: Query/Retrieve", "tutorial_query_retrieve.html", [
      [ "Goal", "tutorial_query_retrieve.html#qr_goal", null ],
      [ "Step 1: C-FIND query", "tutorial_query_retrieve.html#qr_find", null ],
      [ "Step 2: C-MOVE retrieval", "tutorial_query_retrieve.html#qr_move", null ],
      [ "Step 3: C-GET alternative", "tutorial_query_retrieve.html#qr_get", null ],
      [ "Query Hierarchy Levels", "tutorial_query_retrieve.html#qr_levels", null ],
      [ "Common Mistakes", "tutorial_query_retrieve.html#qr_mistakes", null ],
      [ "Next Steps", "tutorial_query_retrieve.html#qr_next", null ]
    ] ],
    [ "Tutorial: CLI Tools", "tutorial_cli_tools.html", [
      [ "Goal", "tutorial_cli_tools.html#cli_goal", null ],
      [ "dcm_info — Quick file summary", "tutorial_cli_tools.html#cli_info", null ],
      [ "dcm_dump — Full metadata dump", "tutorial_cli_tools.html#cli_dump", null ],
      [ "dcm_conv — Transfer syntax conversion", "tutorial_cli_tools.html#cli_conv", null ],
      [ "dcm_anonymize — De-identification", "tutorial_cli_tools.html#cli_anon", null ],
      [ "query_scu — Remote query", "tutorial_cli_tools.html#cli_query", null ],
      [ "More tools", "tutorial_cli_tools.html#cli_more", null ],
      [ "Common Mistakes", "tutorial_cli_tools.html#cli_mistakes", null ],
      [ "Next Steps", "tutorial_cli_tools.html#cli_next", null ]
    ] ],
    [ "Frequently Asked Questions", "faq.html", [
      [ "SOP Class Support", "faq.html#faq_sop", [
        [ "Which SOP classes are supported?", "faq.html#faq_supported_sop", null ],
        [ "How do I add a new SOP class?", "faq.html#faq_add_sop", null ]
      ] ],
      [ "Transfer Syntax Questions", "faq.html#faq_ts", [
        [ "How do I add a new transfer syntax?", "faq.html#faq_ts_add", null ],
        [ "What if the receiver doesn't support my transfer syntax?", "faq.html#faq_ts_compat", null ]
      ] ],
      [ "Security Questions", "faq.html#faq_security", [
        [ "How do I enable TLS for DICOM connections?", "faq.html#faq_tls", null ],
        [ "Does pacs_system support audit logging?", "faq.html#faq_audit", null ]
      ] ],
      [ "Integration Questions", "faq.html#faq_integration", [
        [ "How do I integrate with a hospital PACS?", "faq.html#faq_hospital_pacs", null ],
        [ "DICOMweb services?", "faq.html#faq_dicomweb", null ]
      ] ],
      [ "Storage Questions", "faq.html#faq_storage", [
        [ "What storage backends are available?", "faq.html#faq_storage_backends", null ]
      ] ],
      [ "Performance Questions", "faq.html#faq_perf", [
        [ "How do I tune for high-throughput ingestion?", "faq.html#faq_perf_tuning", null ]
      ] ],
      [ "AI Integration", "faq.html#faq_ai", [
        [ "How does AI integration work?", "faq.html#faq_ai_service", null ]
      ] ],
      [ "Conformance Testing", "faq.html#faq_conformance", [
        [ "How do I run conformance tests?", "faq.html#faq_conformance_test", null ]
      ] ]
    ] ],
    [ "Troubleshooting Guide", "troubleshooting.html", [
      [ "Association Rejection", "troubleshooting.html#ts_association", null ],
      [ "Transfer Syntax Mismatch", "troubleshooting.html#ts_ts_mismatch", null ],
      [ "Incomplete DICOM Files", "troubleshooting.html#ts_incomplete", null ],
      [ "Network Connectivity", "troubleshooting.html#ts_network", null ],
      [ "Storage Commitment Failures", "troubleshooting.html#ts_storage_commit", null ],
      [ "Build Issues", "troubleshooting.html#ts_build", null ]
    ] ],
    [ "Level 1: Hello DICOM", "md_examples_2tutorials_201__hello__dicom_2README.html", [
      [ "Learning Objectives", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md115", null ],
      [ "Prerequisites", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md116", null ],
      [ "Build & Run", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md117", null ],
      [ "Tutorial Output", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md118", null ],
      [ "Verify Output", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md119", null ],
      [ "Key Concepts Explained", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md120", [
        [ "DICOM Tags", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md121", null ],
        [ "Value Representations (VR)", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md122", null ],
        [ "Dataset Operations", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md123", null ],
        [ "File I/O", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md124", null ]
      ] ],
      [ "Next Steps", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md125", null ],
      [ "Related Documentation", "md_examples_2tutorials_201__hello__dicom_2README.html#autotoc_md126", null ]
    ] ],
    [ "Level 2: Echo Server", "md_examples_2tutorials_202__echo__server_2README.html", [
      [ "Learning Objectives", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md128", null ],
      [ "Prerequisites", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md129", null ],
      [ "Build & Run", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md130", null ],
      [ "Testing", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md131", [
        [ "Expected Output", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md132", null ]
      ] ],
      [ "Key Concepts Explained", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md133", [
        [ "DICOM Association", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md134", null ],
        [ "Server Configuration", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md135", null ],
        [ "Service Registration", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md136", null ],
        [ "Event Callbacks", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md137", null ]
      ] ],
      [ "Troubleshooting", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md138", [
        [ "Port Already in Use", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md139", null ],
        [ "Connection Refused", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md140", null ],
        [ "AE Title Mismatch", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md141", null ]
      ] ],
      [ "Next Steps", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md142", null ],
      [ "Related Documentation", "md_examples_2tutorials_202__echo__server_2README.html#autotoc_md143", null ]
    ] ],
    [ "Level 3: Storage Server", "md_examples_2tutorials_203__storage__server_2README.html", [
      [ "Learning Objectives", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md145", null ],
      [ "Prerequisites", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md146", null ],
      [ "Build & Run", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md147", null ],
      [ "Testing", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md148", [
        [ "Generate Test Data", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md149", null ],
        [ "Send Images to Storage Server", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md150", null ],
        [ "Verify Storage", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md151", null ],
        [ "Expected Output", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md152", null ]
      ] ],
      [ "Key Concepts Explained", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md153", [
        [ "C-STORE Operation", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md154", null ],
        [ "File Storage Organization", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md155", null ],
        [ "Index Database Schema", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md156", null ],
        [ "Handler Callbacks", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md157", null ]
      ] ],
      [ "Storage Status Codes", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md158", null ],
      [ "Configuration Options", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md159", [
        [ "File Storage", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md160", null ],
        [ "Naming Schemes", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md161", null ],
        [ "Duplicate Policies", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md162", null ]
      ] ],
      [ "Troubleshooting", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md163", [
        [ "Storage Rejected (Status 0xC001)", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md164", null ],
        [ "Database Errors", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md165", null ],
        [ "Disk Space Issues", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md166", null ]
      ] ],
      [ "Next Steps", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md167", null ],
      [ "Related Documentation", "md_examples_2tutorials_203__storage__server_2README.html#autotoc_md168", null ]
    ] ],
    [ "Level 4: Mini PACS", "md_examples_2tutorials_204__mini__pacs_2README.html", [
      [ "Learning Objectives", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md171", null ],
      [ "Prerequisites", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md172", null ],
      [ "Architecture", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md173", null ],
      [ "Services Overview", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md174", null ],
      [ "Build", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md175", null ],
      [ "Run", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md176", null ],
      [ "Test Commands", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md177", [
        [ "Connectivity Test (C-ECHO)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md178", null ],
        [ "Store Images (C-STORE)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md179", null ],
        [ "Query at Patient Level (C-FIND)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md180", null ],
        [ "Query at Study Level (C-FIND)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md181", null ],
        [ "Query at Series Level (C-FIND)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md182", null ],
        [ "Query Worklist (MWL C-FIND)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md183", null ],
        [ "Retrieve Study (C-MOVE)", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md184", null ]
      ] ],
      [ "Code Walkthrough", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md185", [
        [ "Configuration", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md186", null ],
        [ "Service Integration", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md187", null ],
        [ "Query Handler Implementation", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md188", null ],
        [ "Worklist Management", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md189", null ]
      ] ],
      [ "Query/Retrieve Information Model", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md190", [
        [ "Patient Root", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md191", null ],
        [ "Query Keys by Level", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md192", null ]
      ] ],
      [ "MPPS Workflow", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md193", null ],
      [ "Database Schema", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md194", null ],
      [ "File Structure", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md195", null ],
      [ "Statistics", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md196", null ],
      [ "Troubleshooting", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md197", [
        [ "Connection Refused", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md198", null ],
        [ "Association Rejected", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md199", null ],
        [ "Query Returns No Results", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md200", null ],
        [ "C-MOVE Fails", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md201", null ]
      ] ],
      [ "Next Steps", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md202", null ],
      [ "References", "md_examples_2tutorials_204__mini__pacs_2README.html#autotoc_md203", null ]
    ] ],
    [ "Level 5: Production PACS", "md_examples_2tutorials_205__production__pacs_2README.html", [
      [ "Learning Objectives", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md206", null ],
      [ "Prerequisites", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md207", null ],
      [ "Architecture", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md208", null ],
      [ "Building", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md209", null ],
      [ "Configuration", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md210", [
        [ "Default Configuration", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md211", null ],
        [ "Custom Configuration", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md212", null ],
        [ "Configuration File Format", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md213", null ]
      ] ],
      [ "Features", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md214", [
        [ "TLS Security", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md215", null ],
        [ "Role-Based Access Control", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md216", null ],
        [ "Automatic Anonymization", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md217", null ],
        [ "REST API", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md218", null ],
        [ "Health Monitoring", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md219", null ],
        [ "Event-Driven Architecture", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md220", null ]
      ] ],
      [ "Test Commands", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md221", [
        [ "DICOM Connectivity", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md222", null ],
        [ "TLS Connections", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md223", null ],
        [ "REST API", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md224", null ]
      ] ],
      [ "File Structure", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md225", null ],
      [ "Concepts Covered", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md226", [
        [ "Configuration Management", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md227", null ],
        [ "Security", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md228", null ],
        [ "Anonymization", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md229", null ],
        [ "Monitoring", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md230", null ],
        [ "Event Architecture", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md231", null ]
      ] ],
      [ "Best Practices Demonstrated", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md232", null ],
      [ "Next Steps", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md233", null ],
      [ "References", "md_examples_2tutorials_205__production__pacs_2README.html#autotoc_md234", null ]
    ] ],
    [ "Common Utilities", "md_examples_2tutorials_2common_2README.html", [
      [ "Overview", "md_examples_2tutorials_2common_2README.html#autotoc_md236", null ],
      [ "Signal Handler", "md_examples_2tutorials_2common_2README.html#autotoc_md237", [
        [ "Quick Usage", "md_examples_2tutorials_2common_2README.html#autotoc_md238", null ],
        [ "RAII Wrapper (Recommended)", "md_examples_2tutorials_2common_2README.html#autotoc_md239", null ],
        [ "API Reference", "md_examples_2tutorials_2common_2README.html#autotoc_md240", [
          [ "signal_handler (Static Interface)", "md_examples_2tutorials_2common_2README.html#autotoc_md241", null ],
          [ "scoped_signal_handler (RAII)", "md_examples_2tutorials_2common_2README.html#autotoc_md242", null ]
        ] ]
      ] ],
      [ "Console Utils", "md_examples_2tutorials_2common_2README.html#autotoc_md243", [
        [ "Message Printing", "md_examples_2tutorials_2common_2README.html#autotoc_md244", null ],
        [ "DICOM Data Display", "md_examples_2tutorials_2common_2README.html#autotoc_md245", null ],
        [ "Box Drawing", "md_examples_2tutorials_2common_2README.html#autotoc_md246", null ],
        [ "Progress Display", "md_examples_2tutorials_2common_2README.html#autotoc_md247", null ],
        [ "Formatting Utilities", "md_examples_2tutorials_2common_2README.html#autotoc_md248", null ],
        [ "Color Configuration", "md_examples_2tutorials_2common_2README.html#autotoc_md249", null ],
        [ "Available Colors", "md_examples_2tutorials_2common_2README.html#autotoc_md250", null ]
      ] ],
      [ "Test Data Generator", "md_examples_2tutorials_2common_2README.html#autotoc_md251", [
        [ "Single Dataset Generation", "md_examples_2tutorials_2common_2README.html#autotoc_md252", null ],
        [ "Study Generation", "md_examples_2tutorials_2common_2README.html#autotoc_md253", null ],
        [ "Save to Files", "md_examples_2tutorials_2common_2README.html#autotoc_md254", null ],
        [ "UID Generation", "md_examples_2tutorials_2common_2README.html#autotoc_md255", null ],
        [ "Date/Time Utilities", "md_examples_2tutorials_2common_2README.html#autotoc_md256", null ],
        [ "Data Structures", "md_examples_2tutorials_2common_2README.html#autotoc_md257", [
          [ "patient_info", "md_examples_2tutorials_2common_2README.html#autotoc_md258", null ],
          [ "study_info", "md_examples_2tutorials_2common_2README.html#autotoc_md259", null ],
          [ "image_params", "md_examples_2tutorials_2common_2README.html#autotoc_md260", null ]
        ] ]
      ] ],
      [ "Build Configuration", "md_examples_2tutorials_2common_2README.html#autotoc_md261", null ],
      [ "Thread Safety", "md_examples_2tutorials_2common_2README.html#autotoc_md262", null ],
      [ "Platform Support", "md_examples_2tutorials_2common_2README.html#autotoc_md263", null ]
    ] ],
    [ "PACS System Developer Tutorials", "md_examples_2tutorials_2README.html", [
      [ "Quick Start", "md_examples_2tutorials_2README.html#autotoc_md265", null ],
      [ "Learning Path", "md_examples_2tutorials_2README.html#autotoc_md266", null ],
      [ "Prerequisites", "md_examples_2tutorials_2README.html#autotoc_md267", [
        [ "Required", "md_examples_2tutorials_2README.html#autotoc_md268", null ],
        [ "Recommended", "md_examples_2tutorials_2README.html#autotoc_md269", null ]
      ] ],
      [ "Directory Structure", "md_examples_2tutorials_2README.html#autotoc_md270", null ],
      [ "Concepts by Level", "md_examples_2tutorials_2README.html#autotoc_md271", [
        [ "Level 1: Hello DICOM", "md_examples_2tutorials_2README.html#autotoc_md272", null ],
        [ "Level 2: Echo Server", "md_examples_2tutorials_2README.html#autotoc_md273", null ],
        [ "Level 3: Storage Server", "md_examples_2tutorials_2README.html#autotoc_md274", null ],
        [ "Level 4: Mini PACS", "md_examples_2tutorials_2README.html#autotoc_md275", null ],
        [ "Level 5: Production PACS (Coming Soon)", "md_examples_2tutorials_2README.html#autotoc_md276", null ]
      ] ],
      [ "Build Options", "md_examples_2tutorials_2README.html#autotoc_md277", null ],
      [ "Testing with DCMTK", "md_examples_2tutorials_2README.html#autotoc_md278", [
        [ "Common Commands", "md_examples_2tutorials_2README.html#autotoc_md279", null ],
        [ "Tutorial Testing Workflow", "md_examples_2tutorials_2README.html#autotoc_md280", null ]
      ] ],
      [ "Troubleshooting", "md_examples_2tutorials_2README.html#autotoc_md281", [
        [ "Connection Refused", "md_examples_2tutorials_2README.html#autotoc_md282", null ],
        [ "Association Rejected", "md_examples_2tutorials_2README.html#autotoc_md283", null ],
        [ "Query Returns No Results", "md_examples_2tutorials_2README.html#autotoc_md284", null ],
        [ "Storage Fails", "md_examples_2tutorials_2README.html#autotoc_md285", null ],
        [ "Build Errors", "md_examples_2tutorials_2README.html#autotoc_md286", null ]
      ] ],
      [ "Architecture Reference", "md_examples_2tutorials_2README.html#autotoc_md287", null ],
      [ "Common Utilities", "md_examples_2tutorials_2README.html#autotoc_md288", null ],
      [ "Further Reading", "md_examples_2tutorials_2README.html#autotoc_md289", [
        [ "DICOM Standard", "md_examples_2tutorials_2README.html#autotoc_md290", null ],
        [ "Project Resources", "md_examples_2tutorials_2README.html#autotoc_md291", null ]
      ] ],
      [ "Contributing", "md_examples_2tutorials_2README.html#autotoc_md292", null ]
    ] ],
    [ "Topics", "topics.html", "topics" ],
    [ "Modules", "modules.html", [
      [ "Modules List", "modules.html", "modules_dup" ],
      [ "Module Members", "modulemembers.html", [
        [ "All", "modulemembers.html", null ],
        [ "Variables", "modulemembers_vars.html", null ]
      ] ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"01__hello__dicom_2main_8cpp.html",
"atna__audit__logger_8h.html#a0dc6f05f0dfcd44adde46c762e4f9819",
"character__set_8h.html#aa220c9a2fdfbf605ea3ae8b4750e9ee4",
"classkcenon_1_1pacs_1_1ai_1_1assessment__manager.html#af779b496b34b92cb4247db932cf88519",
"classkcenon_1_1pacs_1_1client_1_1routing__manager.html#a78c0950244b645531e2a600d6b1b1983",
"classkcenon_1_1pacs_1_1core_1_1dicom__dictionary.html#aef9ce4a5810f935714683996a79e33cb",
"classkcenon_1_1pacs_1_1core_1_1pool__manager.html#ac231e8b497cf1f74c64c353dfeb6e439",
"classkcenon_1_1pacs_1_1di_1_1test_1_1TestContainerBuilder.html#ac7ceeb58165d076036637ade3d98dcd1",
"classkcenon_1_1pacs_1_1encoding_1_1compression_1_1jpeg__baseline__codec.html",
"classkcenon_1_1pacs_1_1encoding_1_1compression_1_1rle__codec.html#adce15543b474dca16f883d3a15d25587",
"classkcenon_1_1pacs_1_1ihe_1_1xds_1_1document__source_1_1impl.html#a78b6668acc7272e45775065ff57f1208",
"classkcenon_1_1pacs_1_1integration_1_1logger__adapter.html#a1322e8d59f77929152947fb337ccc757",
"classkcenon_1_1pacs_1_1integration_1_1thread__pool__adapter.html#a67fe7529889b8bb578f5c74b7924dc27",
"classkcenon_1_1pacs_1_1monitoring_1_1dicom__metrics__collector.html#a90470e7347c4282995f8580ff1412d8e",
"classkcenon_1_1pacs_1_1monitoring_1_1pacs__metrics.html#a4f92c4ecc0093f61c817a507a96f77ff",
"classkcenon_1_1pacs_1_1network_1_1association.html#af29fbc92855ef4db113f19d4ab40169c",
"classkcenon_1_1pacs_1_1network_1_1dimse_1_1dimse__message.html#a9d03833a4d32514833083b3f78617873",
"classkcenon_1_1pacs_1_1network_1_1pipeline_1_1pipeline__adapter.html",
"classkcenon_1_1pacs_1_1network_1_1pipeline_1_1pipeline__metrics.html#adde4fe9ed851b691ce6be819c568e469",
"classkcenon_1_1pacs_1_1network_1_1v2_1_1dicom__association__handler.html#a231cae40ca10de3e6260441e5eac0d81",
"classkcenon_1_1pacs_1_1samples_1_1config__loader.html#a1458bdec678d5f1eb3c7e3803ca8a1a1",
"classkcenon_1_1pacs_1_1samples_1_1test__data__generator.html#a36ccbf59abb5ce3b9c4c61d2b72a3bc9",
"classkcenon_1_1pacs_1_1security_1_1atna__syslog__transport.html#a0775061e9ace745a96008ffa4ed0832c",
"classkcenon_1_1pacs_1_1security_1_1private__key__impl.html#a574d9acbd74de44fadea753ff419b914",
"classkcenon_1_1pacs_1_1services_1_1cache_1_1query__cache.html#ab86ce0aaba1492770d62b23c35e5aece",
"classkcenon_1_1pacs_1_1services_1_1n__get__scu.html#a6f4edd84094b686c7a3bfd33f4d101ad",
"classkcenon_1_1pacs_1_1services_1_1query__scu.html#a8a37c19f3fa383eddd94b77d35959272",
"classkcenon_1_1pacs_1_1services_1_1sop__class__registry.html#aea261e138699832aa39e174e79600475",
"classkcenon_1_1pacs_1_1services_1_1ups__push__scu.html#a06d64889d43ce7a3fcf9e6da891d6005",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1ct__iod__validator.html#af0cbb4676cdf5da5cc5312afde5c40d4",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1mg__iod__validator.html#a9865dce01841840a79c9f551a5260e1b",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1pet__iod__validator.html#ae8c6eb8b728716389b940eafcc7fe19c",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1sr__iod__validator.html#accff73ee27d4a9049a1f056fae601a92",
"classkcenon_1_1pacs_1_1services_1_1worklist__scu.html#a57cbb0a69ce1a652603ea4b2f3bd6417",
"classkcenon_1_1pacs_1_1storage_1_1azure__blob__storage_1_1azure__client__interface.html#a9a0206ba79c45c05435474800abeac7d",
"classkcenon_1_1pacs_1_1storage_1_1index__database.html#a284c0b1339c34b584111a6917a9397cf",
"classkcenon_1_1pacs_1_1storage_1_1job__repository.html#a2db10df4b7d6f1a554f42591c221c7c0",
"classkcenon_1_1pacs_1_1storage_1_1mpps__repository.html#a1abe26be4479acf08bd5fcbeb12e2355",
"classkcenon_1_1pacs_1_1storage_1_1s3__storage.html#a155410a8f8da8d36c79fae80bc6ae2d9",
"classkcenon_1_1pacs_1_1storage_1_1ups__repository.html#a0edbeb79a9b3380b5309bc5ad1ec3fe8",
"classkcenon_1_1pacs_1_1web_1_1dicomweb_1_1multipart__parser.html#a9952b4fb924a9000752a8493eafa6138",
"classkcenon_1_1pacs_1_1workflow_1_1auto__prefetch__service.html#ac9a19e291fe35aa7077aaea2f5fcfc81",
"classkcenon_1_1pacs_1_1workflow_1_1task__scheduler.html#ae7788b22dc2db3a872d6e5fda8ee4d31",
"dicom__dictionary_8cpp.html#acf71199cdf04096d3a2fbec19c9946ad",
"dimse__message_8h.html#a2111eb8398025e16efb435fb14b57cb6",
"dx__storage_8h.html#abc943a1ae204cede4ff60d86ba9f1798",
"heightmap__seg__iod__validator_8cpp.html#a9dc6ccfe4ba167c572df7f46e562380f",
"key__image__repository_8h.html",
"md_examples_2tutorials_2common_2README.html#autotoc_md259",
"monitoring__adapter_8h.html#a8cc80f3855fcabe502a805660dde64a3ab39024efbc6de61976f585c8421c6bba",
"namespacekcenon_1_1pacs_1_1client.html#a2224cefe2f1791fceebbc935eb2d567e",
"namespacekcenon_1_1pacs_1_1encoding.html#a52497f94ab1934877284679b9fd093f8a1f72410deecd85b61b1c8ffcf6bf4c99",
"namespacekcenon_1_1pacs_1_1error__codes.html#afa8c8fac1bba94f7c0524162125b8eac",
"namespacekcenon_1_1pacs_1_1network.html#aa8b15f0e527e96507bbe20b714375a8ba547a8fe4bdfbdf149aad7a2009677f47",
"namespacekcenon_1_1pacs_1_1samples.html#a0b35752828203618c347bea56ba9161b",
"namespacekcenon_1_1pacs_1_1security_1_1atna__event__ids.html#a6b33dc6202cb2d1b6e945774ac6313f3",
"namespacekcenon_1_1pacs_1_1services_1_1print__tags.html#a17310d03c868a08de5fa9228171b903f",
"namespacekcenon_1_1pacs_1_1services_1_1sop__classes.html#a69db848dfb29e267bd0e6c27ee7b0e99",
"namespacekcenon_1_1pacs_1_1services_1_1sop__classes.html#acdad7f31b407f9097a5500fa8a9773a2a12a0852b1a5f77f797900387fca77bf4",
"namespacekcenon_1_1pacs_1_1services_1_1validation.html#aef96663e7be72585493c21442d407b85",
"namespacekcenon_1_1pacs_1_1services_1_1validation_1_1rt__tags.html#ab46e6c76b8ba1c01b5dedbf3e481db4e",
"namespacekcenon_1_1pacs_1_1web.html#ac1b5a40744f2f8cc7881560ce85592dfae6e3271d48391046f142110e7c39924b",
"nm__iod__validator_8cpp.html#a04fcc845668e941cc75db0efe1f1dc40",
"parametric__map__iod__validator_8h.html#a86cb7dea8733657302c990c9419310f0",
"pet__storage_8h.html#a5cb44b50f6abfbdde7b74654b2a6a53aa1792d5515ff967e35c32eb644a47efda",
"remote__nodes__endpoints_8h.html",
"routing__types_8h.html#a236d99322fbfa05e92b793402ea267d4abad7d818ffc0623dcd76f661f5d70fca",
"seg__iod__validator_8cpp.html#a9f915eca2517af87cf8da4d66175aeb5",
"sop__class__registry_8h.html#ade0431f49d0b98ddfeb98685757adb7b",
"storage__commitment__endpoints_8h.html#aa88f84cd6de4626298c0048c9507e4c9",
"structkcenon_1_1pacs_1_1ai_1_1cad__finding.html#ad504bf4e0b71a8c37ff7ad86713c8d56",
"structkcenon_1_1pacs_1_1client_1_1job__record.html#a43b0b539c8e23656ca928a975db1879a",
"structkcenon_1_1pacs_1_1client_1_1remote__node.html#a0a923284bd0fce144c0d6d046c9c045e",
"structkcenon_1_1pacs_1_1client_1_1sync__conflict.html#ae11b536ffe82d06f90004f3e8bc0e7ef",
"structkcenon_1_1pacs_1_1core_1_1value__multiplicity.html#a1462a23247e9df766a21b875bdf04409",
"structkcenon_1_1pacs_1_1events_1_1query__executed__event.html#ab3433844f70fe9f8ca5003148f78e8d5",
"structkcenon_1_1pacs_1_1ihe_1_1xds_1_1xds__document.html#a1518ea96e779946a2f584e25456a4442",
"structkcenon_1_1pacs_1_1monitoring_1_1dicom__metrics__snapshot.html#a43e040393a47095c6186cacc6db1218f",
"structkcenon_1_1pacs_1_1network_1_1abort__pdu.html",
"structkcenon_1_1pacs_1_1network_1_1pipeline_1_1session__context.html",
"structkcenon_1_1pacs_1_1samples_1_1events_1_1image__received__event.html#aeedcee0924cd00643e057b0739a8a238",
"structkcenon_1_1pacs_1_1security_1_1Permission.html#a0b0c15e9a9c592dea87d1db90cd9c5d9",
"structkcenon_1_1pacs_1_1security_1_1tag__action__config.html#ad0314b0d0bdecbf5d470ae61bde5c82b",
"structkcenon_1_1pacs_1_1services_1_1patient__query__keys.html#afc5efe03cbb972dd639ad4dfe79a2e71",
"structkcenon_1_1pacs_1_1services_1_1sop__classes_1_1dx__sop__class__info.html#a31c0ca68a4fa3f1d45ac3d2caae7fb2c",
"structkcenon_1_1pacs_1_1services_1_1stream__config.html#a8e979eef23bb537a3725427d79df64ab",
"structkcenon_1_1pacs_1_1services_1_1validation_1_1mr__validation__options.html",
"structkcenon_1_1pacs_1_1services_1_1worklist__item.html",
"structkcenon_1_1pacs_1_1services_1_1xds_1_1xds__document__entry.html#a80437ce98ddef64bd6cfd6516a891c4b",
"structkcenon_1_1pacs_1_1storage_1_1hsm__statistics.html#abb47d28bd5a3c74d04975698a5e6d3a4",
"structkcenon_1_1pacs_1_1storage_1_1mpps__query.html#a6a40e1ee0245cd46d090de43de21aa81",
"structkcenon_1_1pacs_1_1storage_1_1study__query.html#acf3209c0e701f1b2e1a1b9048f68b41f",
"structkcenon_1_1pacs_1_1storage_1_1worklist__item.html#afa91d6db42585bdbad283d329bbe0124",
"structkcenon_1_1pacs_1_1web_1_1frame__info.html#aa2693974d181a53448b050fc4dd4605f",
"structkcenon_1_1pacs_1_1web_1_1wado__uri_1_1validation__result.html",
"structkcenon_1_1pacs_1_1workflow_1_1prefetch__service__config.html#a1842c6db82d0f53a2d040852557f408e",
"structparsed__url.html#a9ca0a50c36935e624c80c59e70c022e6",
"tutorial_store_workflow.html",
"waveform__storage_8cpp.html#ab0b4f755abd3979132db9aad2fbd7ed9"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';