---
doc_id: "PAC-PROJ-001"
doc_title: "변경 이력 - PACS System"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# 변경 이력 - PACS System

> **SSOT**: This document is the single source of truth for **변경 이력 - PACS System**.

> **언어:** [English](CHANGELOG.md) | **한국어**

PACS System 프로젝트의 모든 주요 변경 사항이 이 파일에 문서화됩니다.

형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.0.0/)를 따르며,
이 프로젝트는 [Semantic Versioning](https://semver.org/lang/ko/spec/v2.0.0.html)을 준수합니다.

---

## [미배포]

### 변경됨
- 문서 표준화 (Doxyfile, README, 에코시스템 문서)
- 디렉터리 레이아웃 통합: `samples/`를 `examples/`로 (5단계 튜토리얼), `examples/`를 `tools/`로 (32개 CLI 유틸리티 바이너리) 이름 변경하여 에코시스템 표준 역할 분리에 맞춤. CMake 옵션 이름(`PACS_BUILD_EXAMPLES`, `PACS_BUILD_SAMPLES`)은 하위 호환성을 위해 유지 ([#1139](https://github.com/kcenon/pacs_system/issues/1139))
- `cmake/*.cmake` 모듈을 `common_system/cmake/template`의 표준 에코시스템 템플릿에 맞춰 정렬. pacs 고유 모듈(`pacs_system-config.cmake.in`, `summary.cmake`) 및 의도적 설계 차이는 `cmake/DEVIATIONS.md`에 문서화하고, 정렬된 템플릿 버전은 `cmake/VERSION`에 기록 ([#1140](https://github.com/kcenon/pacs_system/issues/1140))

### 호환성 변경 (BREAKING)
- 기존 경로(`samples/...` 튜토리얼, `examples/...` CLI 유틸리티)를 참조하던 다운스트림 사용자는 새 위치로 업데이트 필요: 튜토리얼은 `examples/`, CLI 유틸리티 소스는 `tools/`. 튜토리얼 빌드 출력 경로도 `${CMAKE_BINARY_DIR}/samples`에서 `${CMAKE_BINARY_DIR}/examples`로 이동 ([#1139](https://github.com/kcenon/pacs_system/issues/1139))

### 문서
- Confirm canonical namespace is `kcenon::pacs::` across all source, with no remaining `pacs_system::` references; record ecosystem alignment ([#1138](https://github.com/kcenon/pacs_system/issues/1138))
- Document Catch2 test framework retention as an explicit ecosystem exception in README, `docs/ECOSYSTEM.md`, and the master EPIC ([#1141](https://github.com/kcenon/pacs_system/issues/1141))

---

## [0.2.0] - 2026-02-09

### 추가됨
- **IHE 통합 프로파일**: XDS-I.b, AIRA, PIR 액터 지원
- **DICOMweb API**: WADO-RS, STOW-RS, QIDO-RS 엔드포인트
- **모니터링 및 관측성**: 메트릭 수집기, 헬스 체크
- **워크플로우 서비스**: 프리페치 큐, 지능형 캐싱, 라우팅 규칙
- **보안 강화**: DICOM 연결을 위한 TLS, 입력 검증
- **연결 풀링**: 풀 기반 연결을 가진 원격 노드 매니저
- **오류 처리**: 포괄적인 Result<T> 패턴 통합

### 변경됨
- Phase 4 기능 완료 표시 (보안, 모니터링, 워크플로우, 에코시스템 통합)

---

## [0.1.0] - 2024-12-01

### 추가됨
- PACS System 초기 릴리스
- **DICOM 코어**: 모든 34개 Value Representation을 위한 완전한 Data Element 파서/인코더
- **DICOM 네트워크**: SCP/SCU 구현 (C-STORE, C-FIND, C-MOVE, C-GET, C-ECHO)
- **전송 구문**: Implicit/Explicit VR, Little/Big Endian 지원
- **멀티 코덱 지원**: JPEG, JPEG 2000, JPEG-LS, RLE, 비압축
- **스토리지 백엔드**: 파일시스템 및 클라우드 스토리지 (S3/Azure Blob)
- **데이터베이스 통합**: DICOM 메타데이터 인덱싱
- **CLI 도구**: DICOM 작업을 위한 커맨드라인 인터페이스
- **에코시스템 통합**: network_system, container_system, common_system 기반 구축
- **DICOM 적합성 선언서**: 완전한 PS3.x 준수 문서
