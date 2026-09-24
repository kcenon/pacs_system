---
doc_id: "PAC-PROJ-009"
doc_title: "패키지 상태 - PACS System"
doc_version: "1.0.0"
doc_date: "2026-09-24"
doc_status: "Accepted"
project: "pacs_system"
category: "PROJ"
---

# 패키지 상태

> **Language:** [English](PACKAGE_STATE.md) | **한국어**

이 문서는 `pacs_system`의 게시 패키지 상태, 저장소의 패키지 메타데이터와 캐노니컬 vcpkg 레지스트리의
관계, 그리고 그 상태를 검증하는 방법을 기록합니다. 결정과 증거는
[#1175](https://github.com/kcenon/pacs_system/issues/1175)에서 추적합니다.

## 결정

| 항목 | 상태 |
|------|------|
| 최신 게시 패키지 | **0.1.0** — 태그 `v0.1.0`, GitHub 릴리스 `v0.1.0` |
| 캐노니컬 vcpkg 포트 | `kcenon-pacs-system` 0.1.0, port-version 11 |
| 캐노니컬 레지스트리 | [kcenon/vcpkg-registry](https://github.com/kcenon/vcpkg-registry) `40632164c62b2256579a27eda228c48b057cbee9` |
| v0.1.0 아카이브 SHA512 | `0db1f80c979f726efc6db5836e626d717297e6e4ead74005973d655ab63e67d093ed89afa402579d53e823a7c1bf5ac350ee199faf2f5b5e2cb70976ed4c282a` |
| v1.0 | 미출시 API 계약 마일스톤입니다. v1.0.0 태그·GitHub 릴리스·레지스트리 버전은 없습니다. 준비 상태는 [#1095](https://github.com/kcenon/pacs_system/issues/1095), 의존성 릴리스 게이트는 [#1164](https://github.com/kcenon/pacs_system/issues/1164)에서 관리합니다. |

실제 릴리스를 준비하기 전까지 저장소의 모든 버전 출처는 0.1.0을 유지합니다:
`CMakeLists.txt`의 `project(... VERSION)`, `Doxyfile`의 `PROJECT_NUMBER`, 루트 `vcpkg.json`의
`version-semver`, 그리고 `vcpkg-ports/kcenon-pacs-system/vcpkg.json`. 다음 명령으로 함께 확인합니다:

```bash
python3 scripts/verify_release_version.py 0.1.0
```

릴리스 워크플로도 태그를 만들기 전에 `version` 입력값으로 같은 검증기를 실행합니다.

## 로컬 오버레이 포트

`vcpkg-ports/kcenon-pacs-system/`은 캐노니컬 레지스트리 포트(`vcpkg.json`, `portfile.cmake`, `usage`)와
바이트 단위로 동일한 사본입니다. 캐노니컬 포트를 먼저 변경한 뒤 여기로 복사하세요.

알려진 캐노니컬 결함: 포트의 `usage` 안내문은 `pacs_system::*` 타겟을 나열하지만, 0.1.0 패키지는
`kcenon::pacs::<component>` 타겟을 export하고 헤더를 `include/pacs/` 아래에 설치합니다.
`pacs_system::*` 이름은 계획된 v1.0 계약에 속합니다. 오버레이는 레지스트리가
[kcenon/vcpkg-registry#104](https://github.com/kcenon/vcpkg-registry/issues/104)에서 정정될 때까지
캐노니컬 안내문을 그대로 유지합니다.

## 루트 매니페스트와 오버레이 의존성 집합

루트 `vcpkg.json`은 현재 소스 트리를, 오버레이는 게시된 `v0.1.0` 아카이브를 빌드합니다. 기본(core)
의존성 집합은 일치합니다: common, container, logger, network, thread 생태계 포트, ICU,
`libjpeg-turbo >= 3.0.2`. 나머지 차이는 의도된 것입니다:

| 차이 | 루트 `vcpkg.json` | 오버레이 포트 | 이유 |
|------|-------------------|---------------|------|
| `vcpkg-cmake`, `vcpkg-cmake-config` | 없음 | host 의존성 | portfile 빌드에서만 필요 |
| `overrides` | 서드파티 정확 고정 | 없음 | vcpkg는 포트의 `overrides`를 무시하며, 루트 고정값은 version-drift 검사에 사용 |
| `rest-api` 기능의 Crow 하한 | `>= 1.3.1` | `>= 1.2.1` | 오버레이는 캐노니컬 포트를 따르고, 루트 빌드는 Crow 1.3.1로 고정 |

기본 루트 빌드는 게시된 레지스트리 포트를 해석하며, 이 포트들은 레거시 패키지 이름(`NetworkSystem`,
`LoggerSystem`)을 설치합니다. `cmake/dependencies.cmake`는 현재 이름(`network_system`/`logger_system`)
다음으로 이 이름들을 허용하므로, 검토된 레지스트리 baseline으로 `cmake --preset vcpkg`가 동작합니다.

## 의존성 출처

`dependency-manifest.json`은 현재 FetchContent 고정값(SQLite `sqlite-amalgamation-3450300` / 3.45.3,
OpenJPH 0.21.0, Crow v1.3.1, ASIO `asio-1-30-2`)과
[common_system coherence 실행 35861689435](https://github.com/kcenon/common_system/actions/runs/35861689435)에서
검증된 생태계 소스 튜플을 기록합니다:

| 저장소 | 소스 SHA |
|--------|----------|
| common_system | `180a8155a96d9ef2794151b59e3fedff529c20d0` |
| thread_system | `ec5fdf3351e1bddd852f8ae4f51501a1d8dac4f1` |
| container_system | `605a0afd76ad78de97fc28ff6d416d5d08430b78` |
| logger_system | `02eafd95640bc24799a36ed043c3e6a16c688832` |
| network_system | `47cf31923ad2c883a96577fb52c7261ec191980f` |
| monitoring_system | `e8c4b2e333f9766511120a9cab063f73c7b970bb` |
| database_system | `0c0ad764a2c80ae3be5757b72f12299a1a2a7fd7` |

이 Git SHA는 빌드 출처(provenance) 고정값이며 게시된 버전 제약이 아닙니다. 게시된 v1.x 의존성 제약은
#1164가 소유하며, Tier 0-4 의존성이 모두 v1.0을 출시한 뒤에야 이 고정값을 대체합니다.

## 검증

1. vcpkg를 루트 builtin baseline(`d90a9b159c08169f39adcd1b0f1ac0ca12c4b96c`)으로 고정합니다. 더 오래된
   vcpkg 체크아웃에는 해당 baseline이 선택하는 포트 버전이 없습니다.
2. 새 캐시로 오버레이를 통해 설치합니다: `VCPKG_BINARY_SOURCES=clear`, 비어 있는 `VCPKG_DOWNLOADS`와
   `X_VCPKG_REGISTRIES_CACHE`, 그리고 `overlay-ports`가 `vcpkg-ports/`를 가리키면서
   `kcenon-pacs-system`에 의존하는 consumer 매니페스트를 사용합니다. vcpkg는 포트 SHA512로 아카이브를 검증합니다.
3. `-DVCPKG_MANIFEST_INSTALL=OFF`로 해당 설치 트리에 대해 외부 consumer를 빌드합니다.
   `find_package(pacs_system 0.1 CONFIG REQUIRED)`와 `kcenon::pacs::core`를 사용하고,
   `pacs_system_DIR`이 설치 트리 안을 가리키는지 확인합니다.
4. 저장소 루트에서 `cmake --preset vcpkg`와 `cmake --build --preset vcpkg`를 실행합니다.
