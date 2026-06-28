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
    [ "Examples", "index.html#examples", null ],
    [ "Learning Resources", "index.html#learning_resources", null ],
    [ "Related Systems", "index.html#related", null ],
    [ "README", "md_README.html", [
      [ "PACS System", "md_README.html#autotoc_md113", [
        [ "Overview", "md_README.html#autotoc_md114", null ],
        [ "Table of Contents", "md_README.html#autotoc_md116", null ],
        [ "Project Status", "md_README.html#autotoc_md118", null ],
        [ "", "md_README.html#autotoc_md119", null ],
        [ "Architecture", "md_README.html#autotoc_md120", null ],
        [ "DICOM Conformance", "md_README.html#autotoc_md122", [
          [ "Supported SOP Classes", "md_README.html#autotoc_md123", null ],
          [ "Transfer Syntax Support", "md_README.html#autotoc_md124", null ]
        ] ],
        [ "", "md_README.html#autotoc_md125", null ],
        [ "vcpkg Features", "md_README.html#autotoc_md126", [
          [ "Feature Selection Guidance", "md_README.html#autotoc_md127", null ]
        ] ],
        [ "Getting Started", "md_README.html#autotoc_md129", [
          [ "Prerequisites", "md_README.html#autotoc_md130", null ],
          [ "Build", "md_README.html#autotoc_md131", [
            [ "Linux/macOS", "md_README.html#autotoc_md132", null ],
            [ "Windows", "md_README.html#autotoc_md133", null ]
          ] ],
          [ "Build Options", "md_README.html#autotoc_md134", null ],
          [ "Installation via vcpkg", "md_README.html#autotoc_md135", null ],
          [ "Windows Development Notes", "md_README.html#autotoc_md136", null ]
        ] ],
        [ "CLI Tools & Examples", "md_README.html#autotoc_md138", [
          [ "Tool Summary", "md_README.html#autotoc_md139", null ],
          [ "Quick Examples", "md_README.html#autotoc_md140", null ]
        ] ],
        [ "", "md_README.html#autotoc_md141", null ],
        [ "Ecosystem Dependencies", "md_README.html#autotoc_md142", [
          [ "Ecosystem Dependency Map", "md_README.html#autotoc_md143", null ]
        ] ],
        [ "Project Structure", "md_README.html#autotoc_md145", null ],
        [ "", "md_README.html#autotoc_md146", null ],
        [ "C++20 Module Support", "md_README.html#autotoc_md147", [
          [ "Requirements for Modules", "md_README.html#autotoc_md148", null ],
          [ "Building with Modules", "md_README.html#autotoc_md149", null ],
          [ "Using Modules", "md_README.html#autotoc_md150", null ],
          [ "Module Structure", "md_README.html#autotoc_md151", null ]
        ] ],
        [ "Documentation", "md_README.html#autotoc_md152", null ],
        [ "Performance", "md_README.html#autotoc_md154", [
          [ "Key Performance Metrics", "md_README.html#autotoc_md155", null ],
          [ "Running Benchmarks", "md_README.html#autotoc_md156", null ]
        ] ],
        [ "Code Statistics", "md_README.html#autotoc_md158", null ],
        [ "Contributing", "md_README.html#autotoc_md160", null ],
        [ "License", "md_README.html#autotoc_md162", null ],
        [ "Contact", "md_README.html#autotoc_md164", null ]
      ] ]
    ] ],
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
    [ "Binary Integration Tests", "md_examples_2integration__tests_2README.html", [
      [ "Overview", "md_examples_2integration__tests_2README.html#autotoc_md167", null ],
      [ "Directory Structure", "md_examples_2integration__tests_2README.html#autotoc_md168", null ],
      [ "Prerequisites", "md_examples_2integration__tests_2README.html#autotoc_md169", [
        [ "Build Requirements", "md_examples_2integration__tests_2README.html#autotoc_md170", null ],
        [ "Required Binaries", "md_examples_2integration__tests_2README.html#autotoc_md171", null ]
      ] ],
      [ "Running Tests", "md_examples_2integration__tests_2README.html#autotoc_md172", [
        [ "Quick Start", "md_examples_2integration__tests_2README.html#autotoc_md173", null ],
        [ "Individual Test Suites", "md_examples_2integration__tests_2README.html#autotoc_md174", null ],
        [ "Options", "md_examples_2integration__tests_2README.html#autotoc_md175", null ]
      ] ],
      [ "Test Descriptions", "md_examples_2integration__tests_2README.html#autotoc_md176", [
        [ "test_connectivity.sh", "md_examples_2integration__tests_2README.html#autotoc_md177", null ],
        [ "test_store_retrieve.sh", "md_examples_2integration__tests_2README.html#autotoc_md178", null ],
        [ "test_worklist_mpps.sh", "md_examples_2integration__tests_2README.html#autotoc_md179", null ],
        [ "test_secure_dicom.sh", "md_examples_2integration__tests_2README.html#autotoc_md180", null ]
      ] ],
      [ "dicom_server_v2 E2E Integration Tests (C++)", "md_examples_2integration__tests_2README.html#autotoc_md181", [
        [ "Running V2 E2E Tests", "md_examples_2integration__tests_2README.html#autotoc_md182", null ],
        [ "V2 E2E Test Scenarios", "md_examples_2integration__tests_2README.html#autotoc_md183", null ],
        [ "V2 E2E Test Fixtures", "md_examples_2integration__tests_2README.html#autotoc_md184", null ],
        [ "Prerequisites", "md_examples_2integration__tests_2README.html#autotoc_md185", null ]
      ] ],
      [ "TLS Integration Tests (C++)", "md_examples_2integration__tests_2README.html#autotoc_md186", [
        [ "Running TLS Tests", "md_examples_2integration__tests_2README.html#autotoc_md187", null ],
        [ "TLS Test Scenarios", "md_examples_2integration__tests_2README.html#autotoc_md188", null ],
        [ "Test Certificate Generation", "md_examples_2integration__tests_2README.html#autotoc_md189", null ],
        [ "TLS Test Fixtures", "md_examples_2integration__tests_2README.html#autotoc_md190", null ],
        [ "Environment Variables", "md_examples_2integration__tests_2README.html#autotoc_md191", null ]
      ] ],
      [ "C++ Test Utilities", "md_examples_2integration__tests_2README.html#autotoc_md192", [
        [ "dcmtk_tool Class (Issue #450)", "md_examples_2integration__tests_2README.html#autotoc_md193", [
          [ "DCMTK Shell Helpers (dcmtk_common.sh)", "md_examples_2integration__tests_2README.html#autotoc_md194", null ]
        ] ]
      ] ],
      [ "DCMTK Interoperability Tests", "md_examples_2integration__tests_2README.html#autotoc_md195", [
        [ "Test Scenarios", "md_examples_2integration__tests_2README.html#autotoc_md196", null ],
        [ "Running DCMTK Interoperability Tests", "md_examples_2integration__tests_2README.html#autotoc_md197", null ],
        [ "Test Directions", "md_examples_2integration__tests_2README.html#autotoc_md198", null ],
        [ "C-ECHO Test Scenarios (Issue #451)", "md_examples_2integration__tests_2README.html#autotoc_md199", null ],
        [ "C-STORE Test Scenarios (Issue #452)", "md_examples_2integration__tests_2README.html#autotoc_md200", null ],
        [ "C-FIND Test Scenarios (Issue #453)", "md_examples_2integration__tests_2README.html#autotoc_md201", null ],
        [ "C-MOVE Test Scenarios (Issue #454)", "md_examples_2integration__tests_2README.html#autotoc_md202", null ],
        [ "CI/CD Integration (Issue #455)", "md_examples_2integration__tests_2README.html#autotoc_md203", [
          [ "Workflow Jobs", "md_examples_2integration__tests_2README.html#autotoc_md204", null ],
          [ "Running Locally", "md_examples_2integration__tests_2README.html#autotoc_md205", null ]
        ] ],
        [ "test_data_generator Class", "md_examples_2integration__tests_2README.html#autotoc_md206", [
          [ "Supported Generators", "md_examples_2integration__tests_2README.html#autotoc_md207", null ]
        ] ],
        [ "process_launcher Class", "md_examples_2integration__tests_2README.html#autotoc_md208", null ],
        [ "RAII Guard", "md_examples_2integration__tests_2README.html#autotoc_md209", null ]
      ] ],
      [ "Multi-Modal Workflow Tests", "md_examples_2integration__tests_2README.html#autotoc_md210", [
        [ "Test Scenarios", "md_examples_2integration__tests_2README.html#autotoc_md211", null ],
        [ "Running Workflow Tests", "md_examples_2integration__tests_2README.html#autotoc_md212", null ],
        [ "Workflow Verification Helper", "md_examples_2integration__tests_2README.html#autotoc_md213", null ]
      ] ],
      [ "Long-Running Stability Tests", "md_examples_2integration__tests_2README.html#autotoc_md214", [
        [ "Test Scenarios", "md_examples_2integration__tests_2README.html#autotoc_md215", null ],
        [ "Running Stability Tests", "md_examples_2integration__tests_2README.html#autotoc_md216", null ],
        [ "Environment Variables", "md_examples_2integration__tests_2README.html#autotoc_md217", null ],
        [ "Stability Metrics", "md_examples_2integration__tests_2README.html#autotoc_md218", null ],
        [ "Memory Monitoring", "md_examples_2integration__tests_2README.html#autotoc_md219", null ]
      ] ],
      [ "Test Data Generation", "md_examples_2integration__tests_2README.html#autotoc_md220", null ],
      [ "Cross-Platform Considerations", "md_examples_2integration__tests_2README.html#autotoc_md221", [
        [ "Port Availability and Detection", "md_examples_2integration__tests_2README.html#autotoc_md222", null ],
        [ "Process Management", "md_examples_2integration__tests_2README.html#autotoc_md223", null ],
        [ "Timeouts", "md_examples_2integration__tests_2README.html#autotoc_md224", [
          [ "Cross-Platform Timeout Command", "md_examples_2integration__tests_2README.html#autotoc_md225", null ]
        ] ]
      ] ],
      [ "Troubleshooting", "md_examples_2integration__tests_2README.html#autotoc_md226", [
        [ "Server Not Starting", "md_examples_2integration__tests_2README.html#autotoc_md227", null ],
        [ "Tests Failing", "md_examples_2integration__tests_2README.html#autotoc_md228", null ],
        [ "TLS Certificate Issues", "md_examples_2integration__tests_2README.html#autotoc_md229", null ]
      ] ],
      [ "CI/CD Pipeline", "md_examples_2integration__tests_2README.html#autotoc_md230", [
        [ "Workflow Jobs", "md_examples_2integration__tests_2README.html#autotoc_md231", null ],
        [ "Running Tests Locally with CTest Labels", "md_examples_2integration__tests_2README.html#autotoc_md232", null ],
        [ "Test Categories (Catch2 Tags)", "md_examples_2integration__tests_2README.html#autotoc_md233", null ],
        [ "Viewing Test Results", "md_examples_2integration__tests_2README.html#autotoc_md234", null ]
      ] ],
      [ "Contributing", "md_examples_2integration__tests_2README.html#autotoc_md235", null ],
      [ "Related Documentation", "md_examples_2integration__tests_2README.html#autotoc_md236", null ],
      [ "License", "md_examples_2integration__tests_2README.html#autotoc_md237", null ]
    ] ],
    [ "Binary Integration Test Scripts", "md_examples_2integration__tests_2scripts_2README.html", [
      [ "Quick Start", "md_examples_2integration__tests_2scripts_2README.html#autotoc_md239", null ],
      [ "Script Descriptions", "md_examples_2integration__tests_2scripts_2README.html#autotoc_md240", null ],
      [ "Options", "md_examples_2integration__tests_2scripts_2README.html#autotoc_md241", null ],
      [ "Exit Codes", "md_examples_2integration__tests_2scripts_2README.html#autotoc_md242", null ],
      [ "See Also", "md_examples_2integration__tests_2scripts_2README.html#autotoc_md243", null ]
    ] ],
    [ "Integration Test Data", "md_examples_2integration__tests_2test__data_2README.html", [
      [ "Generated Test Data", "md_examples_2integration__tests_2test__data_2README.html#autotoc_md245", null ],
      [ "Optional External Test Files", "md_examples_2integration__tests_2test__data_2README.html#autotoc_md246", null ],
      [ "Obtaining Test Data", "md_examples_2integration__tests_2test__data_2README.html#autotoc_md247", null ],
      [ "Notes", "md_examples_2integration__tests_2test__data_2README.html#autotoc_md248", null ]
    ] ],
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
"Basic-example.html",
"annotation__record_8h.html#aebfc33b301775e951db1cf795494e768a2ea558023e0395f98296b929dfdcfb05",
"auto__prefetch__service_8h.html",
"classkcenon_1_1pacs_1_1ai_1_1ai__service__connector.html#aacc99a6d17c0eb756734d6b48f642d7e",
"classkcenon_1_1pacs_1_1client_1_1remote__node__manager.html#ac10682e01e59b69bef2d462b2084122c",
"classkcenon_1_1pacs_1_1core_1_1dicom__dataset.html#ad89a402af2a6ca5adcc40e93fbc97df7",
"classkcenon_1_1pacs_1_1core_1_1dicom__tag.html#af254f6256c18595969692f26f1ce048a",
"classkcenon_1_1pacs_1_1di_1_1LoggerService.html#ac027492d4cf5f4df688cdaf0b127e580",
"classkcenon_1_1pacs_1_1encoding_1_1compression_1_1htj2k__codec.html#a391f83fdf2b1d52761e2fabdec66dfa3",
"classkcenon_1_1pacs_1_1encoding_1_1compression_1_1jpeg__ls__codec.html#abe22267c3a6800c92d203e3749b4a020",
"classkcenon_1_1pacs_1_1encoding_1_1transfer__syntax.html#a5a160d925c0ed15f554818352895fdb5",
"classkcenon_1_1pacs_1_1integration_1_1dicom__session.html#abf7b8fddcf9cdee65cc98993f21eca87",
"classkcenon_1_1pacs_1_1integration_1_1network__adapter.html#a2c63cfb8792793fcbb5c5d7d0283b72f",
"classkcenon_1_1pacs_1_1integration__test_1_1process__launcher.html#a472a5a59a14b419faa2baf4f3d4e4fa1",
"classkcenon_1_1pacs_1_1monitoring_1_1dicom__metrics__collector.html#a110295dc912ac5d5c8c00e84347657d4",
"classkcenon_1_1pacs_1_1monitoring_1_1pacs__metrics.html#a0d1201522f19f1dd2863b16bfb40b431",
"classkcenon_1_1pacs_1_1network_1_1association.html#aafa5ea06946b369d25be21434a783cb5",
"classkcenon_1_1pacs_1_1network_1_1dimse_1_1dimse__message.html#a53b37a21272e79a89d88ff7b75965d05",
"classkcenon_1_1pacs_1_1network_1_1pipeline_1_1pdu__decode__job.html#a164cf613212d8dc97e99aa627f10e238",
"classkcenon_1_1pacs_1_1network_1_1pipeline_1_1pipeline__metrics.html#a46d2ba671f3d2d9a1a449a35b2eb1825",
"classkcenon_1_1pacs_1_1network_1_1tracked__pdu__pool.html#a6d9fb98c4beff62d7aa4906b419f2b59",
"classkcenon_1_1pacs_1_1network_1_1v2_1_1dicom__server__v2.html#ab6d4e59d7a97a35c5221103b2bed1ba8",
"classkcenon_1_1pacs_1_1security_1_1atna__syslog__transport.html#a20019b760811de63f87af28ee46f3f75",
"classkcenon_1_1pacs_1_1security_1_1tls__policy.html",
"classkcenon_1_1pacs_1_1services_1_1cache_1_1simple__lru__cache.html#a418aacac54486b17d5742b5475b4684c",
"classkcenon_1_1pacs_1_1services_1_1print__scp.html#a1394e115d87ffbb0a51ca91ddbddeb12",
"classkcenon_1_1pacs_1_1services_1_1retrieve__scp.html#a473fc67bdb09290f5ff8434a16b11dad",
"classkcenon_1_1pacs_1_1services_1_1storage__commitment__scp.html#af3f859382db9b83d35185a6287676968",
"classkcenon_1_1pacs_1_1services_1_1ups__push__scu.html#ae50388a2108811768ea66e377a563f34",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1dx__iod__validator.html#a0d04a4385718c111b492b7ef64a08784",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1mr__iod__validator.html#aa8fbd9529532b7ee213764a75d6b41a0",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1rt__iod__validator.html#a2bade8682e0cd5f31805f0eeaf97e37a",
"classkcenon_1_1pacs_1_1services_1_1validation_1_1us__iod__validator.html#ad7ce85387046ee00d84c51e47350bd2c",
"classkcenon_1_1pacs_1_1services_1_1xds_1_1imaging__document__consumer.html#a1189cc66bbae64b1a3d28fc5ece6e6c7",
"classkcenon_1_1pacs_1_1storage_1_1file__storage.html#ab2b8c6e03b91ede164372b2a6df08d92",
"classkcenon_1_1pacs_1_1storage_1_1index__database.html#a602b10ef89fe9fec2e61e35148ffc331",
"classkcenon_1_1pacs_1_1storage_1_1key__image__repository.html",
"classkcenon_1_1pacs_1_1storage_1_1node__repository.html#a6680c9b3c1391e1689e4264d03acb04a",
"classkcenon_1_1pacs_1_1storage_1_1s3__storage_1_1s3__client__interface.html#a0450a43cce945ead6c0484ab871dd173",
"classkcenon_1_1pacs_1_1storage_1_1viewer__state__repository.html#a0c1ec07e063d6b99c87d2916c72ac9ba",
"classkcenon_1_1pacs_1_1web_1_1rest__server.html#a584bdb5fb113ecd7e1a00bc81d9c29d3",
"classkcenon_1_1pacs_1_1workflow_1_1study__lock__manager.html#a41d76a2df3e6d295a109285ef70ff9a1",
"classquery__scu_1_1query__builder.html#a642d04c44a2fbc0f3d71bd442b47d5d0",
"dcm__anonymize_2main_8cpp_source.html",
"dicomweb__endpoints_8cpp.html#af0a8872508a571f52fd334eb726e6f78",
"dx__iod__validator_8cpp.html#a9d8c6a61bbe050c385b6187663663162",
"functions_vars_m.html",
"jpegxl__codec_8cpp.html",
"measurement__endpoints_8h_source.html",
"mpps__record_8h.html",
"namespacekcenon_1_1pacs_1_1client.html#a236d99322fbfa05e92b793402ea267d4a49e9c919d63b4df76936ebbf730761e0",
"namespacekcenon_1_1pacs_1_1encoding.html#a52497f94ab1934877284679b9fd093f8a5b26faf932d04f134a2cf70e96c4c548",
"namespacekcenon_1_1pacs_1_1events.html",
"namespacekcenon_1_1pacs_1_1network.html#ad76f1e140a835d88d7f5a16db4eee07ca9a5b0a6cc1ce4bd877fc6f751999c5ee",
"namespacekcenon_1_1pacs_1_1security.html#a28c05c49ac240c32387f11e1230a496ba9367a975f19a06750b67f719f4f08ceb",
"namespacekcenon_1_1pacs_1_1services.html#a44890e0474771eb126daf1f5c3150385",
"namespacekcenon_1_1pacs_1_1services_1_1sop__classes.html#a2780f7f671d7caf61d2f122574598d2ca23823b9401b4f5d440f61879a369ae50",
"namespacekcenon_1_1pacs_1_1services_1_1sop__classes.html#a8a593af2d2fbf4eea4adcb081a05c125",
"namespacekcenon_1_1pacs_1_1services_1_1sop__classes.html#af31947495b491d092a66d9544154b0a7",
"namespacekcenon_1_1pacs_1_1services_1_1validation_1_1label__map__seg__tags.html#a9b5fd170ced17d110adbc8dc2a466bb1",
"namespacekcenon_1_1pacs_1_1services_1_1validation_1_1wsi__iod__tags.html",
"namespacekcenon_1_1pacs_1_1web_1_1storage__commitment.html#a04ffc63366d7829ace25cf12a51dc6a6a3ee28fe1a60c95b89d29317f122c7021",
"nm__storage_8h.html#a85b8314c05c03b320fcfb5760176281ba3be411765e710adc7764f5056e44138c",
"pdu__buffer__pool_8h.html#ad8151eff9fc3ffa8c10a4e876ec03acb",
"pipeline__job__types_8h.html#ac17735513750e9daec52e2b40ca1b711aab56918f7578aa7bab9f87f95eed6cec",
"result_8h.html#a3600fb79800cbf7fb4327b9343ed15c3",
"rt__storage_8cpp.html#a6a14545c17dae9d527aa5ead18c09b24",
"seg__storage_8h.html#af17675a71e814a7d500061ce9df22906",
"sr__storage_8cpp.html#a8ba2e273ed4916ff2286ec3b233bbdac",
"storage__status_8h.html#a0e388476f627e3e35ae76a725d538b8f",
"structkcenon_1_1pacs_1_1ai_1_1segment__info.html#a3c3b3f065b3fe89cb8a3c1448c0a26a3",
"structkcenon_1_1pacs_1_1client_1_1prefetch__history.html#a5b6e1b74a05a2d86bd196be89dee0231",
"structkcenon_1_1pacs_1_1client_1_1remote__node__manager_1_1impl.html#a7f29c40aee98e15a581d3b94ced07335",
"structkcenon_1_1pacs_1_1client_1_1sync__manager_1_1impl.html#a77604c647a8590407f9aa17863781fea",
"structkcenon_1_1pacs_1_1encoding_1_1compression_1_1compression__result.html",
"structkcenon_1_1pacs_1_1events_1_1storage__failed__event.html#a1d661c0455930fe1bdb8406b5d80d06a",
"structkcenon_1_1pacs_1_1monitoring_1_1association__counters.html#a8dbe2795e5fe1a4143da3c6ea95c7cf6",
"structkcenon_1_1pacs_1_1monitoring_1_1pool__counters.html#a19af9b341806f0c5c31ecd59dae4a4a1",
"structkcenon_1_1pacs_1_1network_1_1pipeline_1_1dimse__request.html#a34464937518307dd4981700d253da2c2",
"structkcenon_1_1pacs_1_1network_1_1server__config.html#aa5f932689ae02ada1dceb3f3dcb1a9f3",
"structkcenon_1_1pacs_1_1security_1_1syslog__transport__config.html#aa880768bbb53949543a18b9ad4d3ff7e",
"structkcenon_1_1pacs_1_1services_1_1n__get__result.html#a131b68b614d4faeb2fe36260b879b4c6",
"structkcenon_1_1pacs_1_1services_1_1series__query__keys.html#a19e6e42c75cb7e2b38685d5282c03739",
"structkcenon_1_1pacs_1_1services_1_1storage__scu__config.html",
"structkcenon_1_1pacs_1_1services_1_1validation_1_1mg__validation__options.html#a3764e94c4c96581c186319a1cb7daaa5",
"structkcenon_1_1pacs_1_1services_1_1validation_1_1wsi__validation__options.html#acf770d935bd76b560300d3d8be63d09a",
"structkcenon_1_1pacs_1_1services_1_1xds_1_1xds__document__entry.html#a0b38a2294806e790bb1dfdc640a50e24",
"structkcenon_1_1pacs_1_1storage_1_1cloud__storage__config.html#ab29f6c4f7faeea671cb192a77696c82f",
"structkcenon_1_1pacs_1_1storage_1_1migration__service__config.html#a835636a18b595f44afa1820bf17d46b3",
"structkcenon_1_1pacs_1_1storage_1_1study__query.html#a0fcf96b45a69efbf4eb4a6fe433588e4",
"structkcenon_1_1pacs_1_1storage_1_1worklist__item.html#a78afa6c01b6bf32aa21b187b5483b251",
"structkcenon_1_1pacs_1_1web_1_1dicomweb_1_1store__response.html#ae63e8c2f025f835de0e400d9e1d3ccb7",
"structkcenon_1_1pacs_1_1web_1_1thumbnail__service_1_1cache__key.html#aa470d370bdee92321b7c137c534d5e4c",
"structkcenon_1_1pacs_1_1workflow_1_1prefetch__result.html#a0685f34fb64bce0f9fe2c52c51f33cc4",
"structkcenon_1_1pacs_1_1workflow_1_1verification__config.html#aeb35e8f856a35b4fdc3d79c69ebda661",
"test__data__generator__test_8cpp.html#ae4d3d32f5818e7568073cb8a8aa8b9c0",
"ups__push__scu_8h_source.html",
"waveform__storage_8h.html#ad7fb262e5e516731c2cae417e5173005a7c041058644ada2c7a1e986e08ced705"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';