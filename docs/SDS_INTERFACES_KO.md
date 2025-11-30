# SDS - 인터페이스 명세서

> **버전:** 1.0.0
> **상위 문서:** [SDS_KO.md](SDS_KO.md)
> **최종 수정일:** 2025-11-30

---

## 목차

- [1. 개요](#1-개요)
- [2. 공개 API 인터페이스](#2-공개-api-인터페이스)
- [3. 내부 모듈 인터페이스](#3-내부-모듈-인터페이스)
- [4. 외부 시스템 인터페이스](#4-외부-시스템-인터페이스)
- [5. 에코시스템 통합 인터페이스](#5-에코시스템-통합-인터페이스)
- [6. 오류 처리 인터페이스](#6-오류-처리-인터페이스)

---

## 1. 개요

### 1.1 인터페이스 분류

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          인터페이스 아키텍처                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                       외부 인터페이스                                    ││
│  │                                                                          ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  ││
│  │  │ DICOM SCU    │  │ DICOM SCP    │  │ 관리         │                  ││
│  │  │ (모달리티)   │  │ (뷰어)       │  │ CLI/API      │                  ││
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        공개 API 계층                                     ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │storage_scp│ │ query_scp │ │worklist_  │ │ mpps_scp  │              ││
│  │  │           │ │           │ │    scp    │ │           │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                      내부 모듈 인터페이스                                ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │   core    │ │ encoding  │ │  network  │ │  storage  │              ││
│  │  │           │ │           │ │           │ │           │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                    에코시스템 통합 계층                                   ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │ container │ │  network  │ │  thread   │ │  logger   │              ││
│  │  │  adapter  │ │  adapter  │ │  adapter  │ │  adapter  │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 공개 API 인터페이스

### INT-API-001: storage_scp 인터페이스

**추적성:** DES-SVC-002, SRS-SVC-002

```cpp
namespace pacs::services {

/**
 * @brief Storage SCP 공개 인터페이스
 *
 * C-STORE 수신 기능을 제공합니다.
 *
 * @usage
 * @code
 * storage_scp_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 *
 * storage_scp server(config);
 * server.set_handler([](const dicom_dataset& ds, const std::string& ae) {
 *     // 수신된 영상 처리
 *     return storage_status::success;
 * });
 *
 * server.start();
 * server.wait_for_shutdown();
 * @endcode
 */
class storage_scp {
public:
    // ═══════════════════════════════════════════════════════════════
    // 생성
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 설정으로 Storage SCP 생성
     * @param config 서버 설정
     * @throws std::invalid_argument 설정이 유효하지 않은 경우
     */
    explicit storage_scp(const storage_scp_config& config);

    // ═══════════════════════════════════════════════════════════════
    // 핸들러 등록
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 저장 완료 핸들러 설정
     * @param handler 각 수신 영상에 대한 콜백
     *
     * 핸들러 시그니처:
     *   storage_status(const dicom_dataset& dataset,
     *                  const std::string& calling_ae,
     *                  const std::string& sop_class_uid,
     *                  const std::string& sop_instance_uid)
     */
    void set_handler(storage_handler handler);

    /**
     * @brief 저장 전 검증 핸들러 설정
     * @param handler 수락하면 true, 거부하면 false 반환
     */
    void set_pre_store_handler(
        std::function<bool(const dicom_dataset&)> handler);

    // ═══════════════════════════════════════════════════════════════
    // 수명 주기
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 연결 수락 시작
     * @return 성공 또는 실패를 나타내는 Result
     */
    [[nodiscard]] common::Result<void> start();

    /**
     * @brief 새 연결 수락 중지
     * @note 활성 Association이 완료될 때까지 대기
     */
    void stop();

    /**
     * @brief 서버 종료까지 블록
     */
    void wait_for_shutdown();

    // ═══════════════════════════════════════════════════════════════
    // 상태
    // ═══════════════════════════════════════════════════════════════

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] size_t active_associations() const noexcept;
    [[nodiscard]] statistics get_statistics() const;
};

} // namespace pacs::services
```

### INT-API-002: query_scp 인터페이스

**추적성:** DES-SVC-004, SRS-SVC-004

```cpp
namespace pacs::services {

/**
 * @brief Query SCP 공개 인터페이스
 *
 * Patient/Study/Series/Image 레벨에서 C-FIND 조회 기능을 제공합니다.
 */
class query_scp {
public:
    /**
     * @brief 설정으로 Query SCP 생성
     */
    explicit query_scp(const query_scp_config& config);

    /**
     * @brief 사용자 정의 조회 핸들러 설정
     * @param handler 매칭되는 데이터셋을 반환하는 콜백
     *
     * 핸들러 시그니처:
     *   std::vector<dicom_dataset>(
     *       query_level level,
     *       const dicom_dataset& query_keys,
     *       const std::string& calling_ae)
     */
    void set_handler(query_handler handler);

    /**
     * @brief 조회 서비스 시작
     */
    [[nodiscard]] common::Result<void> start();

    /**
     * @brief 조회 서비스 중지
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept;
};

} // namespace pacs::services
```

### INT-API-003: storage_interface

**추적성:** DES-STOR-001, SRS-STOR-001

```cpp
namespace pacs::storage {

/**
 * @brief 추상 저장소 백엔드 인터페이스
 *
 * 구현체는 스레드 안전해야 합니다.
 */
class storage_interface {
public:
    virtual ~storage_interface() = default;

    // ═══════════════════════════════════════════════════════════════
    // CRUD 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief DICOM 데이터셋 저장
     * @param dataset 저장할 데이터셋
     * @return 성공 또는 오류 (예: 중복, 저장 공간 부족)
     */
    [[nodiscard]] virtual common::Result<void> store(
        const core::dicom_dataset& dataset) = 0;

    /**
     * @brief SOP Instance UID로 DICOM 데이터셋 조회
     * @param sop_instance_uid 고유 식별자
     * @return 데이터셋 또는 오류 (예: 찾을 수 없음)
     */
    [[nodiscard]] virtual common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid) = 0;

    /**
     * @brief 저장된 인스턴스 삭제
     * @param sop_instance_uid 고유 식별자
     * @return 성공 또는 오류
     */
    [[nodiscard]] virtual common::Result<void> remove(
        const std::string& sop_instance_uid) = 0;

    /**
     * @brief 인스턴스 존재 여부 확인
     * @param sop_instance_uid 고유 식별자
     * @return 존재하면 true
     */
    [[nodiscard]] virtual bool exists(
        const std::string& sop_instance_uid) = 0;

    // ═══════════════════════════════════════════════════════════════
    // 조회 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 조회 조건에 매칭되는 인스턴스 검색
     * @param query 매칭 값이 포함된 조회 키
     * @return 매칭되는 데이터셋 벡터 (비어있을 수 있음)
     */
    [[nodiscard]] virtual common::Result<std::vector<core::dicom_dataset>>
        find(const core::dicom_dataset& query) = 0;
};

} // namespace pacs::storage
```

---

## 3. 내부 모듈 인터페이스

### INT-MOD-001: dicom_dataset 인터페이스

**추적성:** DES-CORE-003, SRS-CORE-003

```cpp
namespace pacs::core {

/**
 * @brief DICOM Dataset - 데이터 엘리먼트의 정렬된 컬렉션
 *
 * 스레드 안전성: 스레드 안전하지 않음. 외부 동기화 필요.
 */
class dicom_dataset {
public:
    // ═══════════════════════════════════════════════════════════════
    // 엘리먼트 접근 (읽기)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 태그 존재 여부 확인
     * @param tag 확인할 DICOM 태그
     * @return 해당 태그의 엘리먼트가 존재하면 true
     */
    [[nodiscard]] bool contains(dicom_tag tag) const noexcept;

    /**
     * @brief 태그로 엘리먼트 조회 (nullable)
     * @param tag DICOM 태그
     * @return 엘리먼트 포인터 또는 찾지 못하면 nullptr
     */
    [[nodiscard]] const dicom_element* get(dicom_tag tag) const;

    /**
     * @brief 기본값이 있는 문자열 값 조회
     * @param tag DICOM 태그
     * @param default_value 찾지 못했을 때의 값
     * @return 문자열 값 또는 기본값
     */
    [[nodiscard]] std::string get_string(
        dicom_tag tag,
        std::string_view default_value = "") const;

    /**
     * @brief 기본값이 있는 숫자 값 조회
     * @tparam T 숫자 타입 (int16_t, uint16_t, int32_t, uint32_t, float, double)
     * @param tag DICOM 태그
     * @param default_value 찾지 못했거나 변환 실패 시의 값
     * @return 숫자 값 또는 기본값
     */
    template<typename T>
    [[nodiscard]] T get_numeric(dicom_tag tag, T default_value = T{}) const;

    // ═══════════════════════════════════════════════════════════════
    // 엘리먼트 수정 (쓰기)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 문자열 값 설정 (VR 자동 조회)
     * @param tag DICOM 태그
     * @param value 문자열 값
     * @throws std::runtime_error 태그가 사전에 없는 경우
     */
    void set_string(dicom_tag tag, std::string_view value);

    /**
     * @brief 숫자 값 설정 (VR 자동 조회)
     * @tparam T 숫자 타입
     * @param tag DICOM 태그
     * @param value 숫자 값
     */
    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    /**
     * @brief 엘리먼트 추가 또는 교체
     * @param element 추가할 엘리먼트
     */
    void add(dicom_element element);

    /**
     * @brief 태그로 엘리먼트 제거
     * @param tag DICOM 태그
     */
    void remove(dicom_tag tag);

    // ═══════════════════════════════════════════════════════════════
    // 반복
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 태그 순서대로 엘리먼트 반복
     *
     * @code
     * for (const auto& [tag, element] : dataset) {
     *     std::cout << tag.to_string() << std::endl;
     * }
     * @endcode
     */
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    // ═══════════════════════════════════════════════════════════════
    // 직렬화
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 바이트로 직렬화
     * @param ts 인코딩을 위한 전송 구문
     * @return 바이너리 표현
     */
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    /**
     * @brief 바이트에서 역직렬화
     * @param data 바이너리 데이터
     * @param ts 디코딩을 위한 전송 구문
     * @return 데이터셋 또는 오류
     */
    [[nodiscard]] static common::Result<dicom_dataset> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts);
};

} // namespace pacs::core
```

### INT-MOD-002: association 인터페이스

**추적성:** DES-NET-004, SRS-NET-003

```cpp
namespace pacs::network {

/**
 * @brief DICOM Association 관리
 *
 * 단일 DICOM Association(연결)을 나타냅니다.
 * 이동 전용 의미론 - 네트워크 자원을 소유합니다.
 */
class association {
public:
    // ═══════════════════════════════════════════════════════════════
    // 팩토리 메서드
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief SCU로 연결
     * @param host 원격 호스트 주소
     * @param port 원격 포트
     * @param config Association 설정
     * @param timeout 연결 타임아웃
     * @return 수립된 Association 또는 오류
     */
    [[nodiscard]] static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config,
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ═══════════════════════════════════════════════════════════════
    // 상태
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 현재 상태 조회
     * @return Association 상태 (idle, established, released, aborted)
     */
    [[nodiscard]] association_state state() const noexcept;

    /**
     * @brief Association이 수립되었는지 확인
     * @return DIMSE 연산 준비가 되었으면 true
     */
    [[nodiscard]] bool is_established() const noexcept;

    // ═══════════════════════════════════════════════════════════════
    // Presentation Context
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Abstract Syntax가 수락되었는지 확인
     * @param abstract_syntax SOP Class UID
     * @return 수락되었으면 true
     */
    [[nodiscard]] bool has_accepted_context(
        std::string_view abstract_syntax) const;

    /**
     * @brief 수락된 Presentation Context ID 조회
     * @param abstract_syntax SOP Class UID
     * @return Context ID 또는 수락되지 않았으면 nullopt
     */
    [[nodiscard]] std::optional<uint8_t> accepted_context_id(
        std::string_view abstract_syntax) const;

    // ═══════════════════════════════════════════════════════════════
    // DIMSE 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief DIMSE 메시지 전송
     * @param context_id Presentation Context ID
     * @param message 전송할 DIMSE 메시지
     * @return 성공 또는 오류 (네트워크, 타임아웃)
     */
    [[nodiscard]] common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message);

    /**
     * @brief DIMSE 메시지 수신
     * @param timeout 수신 타임아웃
     * @return 수신된 메시지 또는 오류
     */
    [[nodiscard]] common::Result<dimse::dimse_message> receive_dimse(
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ═══════════════════════════════════════════════════════════════
    // 수명 주기
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 정상적인 해제
     * @return 성공 또는 오류
     */
    [[nodiscard]] common::Result<void> release();

    /**
     * @brief 즉시 중단
     * @param source 중단 출처 (0=Service User, 2=Service Provider)
     * @param reason 중단 사유 코드
     */
    void abort(uint8_t source = 0, uint8_t reason = 0);
};

} // namespace pacs::network
```

---

## 4. 외부 시스템 인터페이스

### INT-EXT-001: DICOM 네트워크 인터페이스

**추적성:** FR-2.x, SRS-NET-xxx

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DICOM 네트워크 프로토콜 인터페이스                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  프로토콜: DICOM Upper Layer (PS3.8)                                        │
│  전송: TCP/IP (TLS 선택적)                                                   │
│  기본 포트: 11112                                                            │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 메시지 유형                                                              ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  Association:                                                            ││
│  │    • A-ASSOCIATE-RQ  →  Association 시작                                ││
│  │    • A-ASSOCIATE-AC  ←  Association 수락                                ││
│  │    • A-ASSOCIATE-RJ  ←  Association 거부                                ││
│  │    • A-RELEASE-RQ    ↔  해제 요청                                       ││
│  │    • A-RELEASE-RP    ↔  해제 확인                                       ││
│  │    • A-ABORT         ↔  Association 중단                                ││
│  │                                                                          ││
│  │  DIMSE (P-DATA-TF 내부):                                                 ││
│  │    • C-ECHO-RQ/RSP   ↔  검증                                            ││
│  │    • C-STORE-RQ/RSP  →  영상 저장                                       ││
│  │    • C-FIND-RQ/RSP   →  조회                                            ││
│  │    • C-MOVE-RQ/RSP   →  검색 (하위 연산)                                 ││
│  │    • C-GET-RQ/RSP    →  가져오기 (인라인 검색)                           ││
│  │    • N-CREATE-RQ/RSP →  MPPS 생성                                       ││
│  │    • N-SET-RQ/RSP    →  MPPS 업데이트                                   ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 지원 SOP 클래스                                                          ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  저장 SOP 클래스:                                                        ││
│  │    • CT Image Storage                    1.2.840.10008.5.1.4.1.1.2      ││
│  │    • MR Image Storage                    1.2.840.10008.5.1.4.1.1.4      ││
│  │    • CR Image Storage                    1.2.840.10008.5.1.4.1.1.1      ││
│  │    • (모든 표준 저장 클래스)                                              ││
│  │                                                                          ││
│  │  조회/검색 SOP 클래스:                                                    ││
│  │    • Patient Root Q/R Find              1.2.840.10008.5.1.4.1.2.1.1     ││
│  │    • Patient Root Q/R Move              1.2.840.10008.5.1.4.1.2.1.2     ││
│  │    • Study Root Q/R Find                1.2.840.10008.5.1.4.1.2.2.1     ││
│  │    • Study Root Q/R Move                1.2.840.10008.5.1.4.1.2.2.2     ││
│  │                                                                          ││
│  │  워크플로우 SOP 클래스:                                                   ││
│  │    • Modality Worklist                  1.2.840.10008.5.1.4.31          ││
│  │    • MPPS                               1.2.840.10008.3.1.2.3.3         ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ 전송 구문                                                                 ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  • Implicit VR Little Endian            1.2.840.10008.1.2              ││
│  │  • Explicit VR Little Endian            1.2.840.10008.1.2.1            ││
│  │  • Explicit VR Big Endian (폐지)         1.2.840.10008.1.2.2            ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### INT-EXT-002: 설정 인터페이스

**목적:** 외부 설정 파일 형식

```yaml
# pacs_config.yaml - PACS 시스템 설정

server:
  ae_title: "MY_PACS"
  port: 11112
  max_associations: 20
  max_pdu_size: 16384

storage:
  path: "/data/dicom"
  naming_scheme: "uid_hierarchical"  # uid_hierarchical | date_hierarchical | flat
  duplicate_policy: "reject"          # reject | replace | ignore

database:
  path: "/data/pacs_index.db"
  cache_size_mb: 64
  wal_mode: true

security:
  tls_enabled: false
  cert_path: "/etc/pacs/cert.pem"
  key_path: "/etc/pacs/key.pem"
  ae_whitelist:
    - "CT_01"
    - "MR_01"
    - "VIEWER_*"

logging:
  level: "info"  # trace | debug | info | warn | error
  file: "/var/log/pacs/pacs.log"
  audit_file: "/var/log/pacs/audit.log"

worklist:
  enabled: true
  source: "database"  # database | external

mpps:
  enabled: true
  forward_to: []  # MPPS를 전달할 AE 타이틀 목록
```

---

## 5. 에코시스템 통합 인터페이스

### INT-ECO-001: container_adapter 인터페이스

**추적성:** DES-INT-001, IR-1

```cpp
namespace pacs::integration {

/**
 * @brief DICOM VR과 container_system 값 간의 어댑터
 */
class container_adapter {
public:
    // ═══════════════════════════════════════════════════════════════
    // VR과 Container 매핑
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief DICOM 엘리먼트를 container 값으로 변환
     * @param element DICOM 데이터 엘리먼트
     * @return Container 시스템 값
     *
     * 매핑:
     *   문자열 VR → std::string
     *   정수 VR → int64_t
     *   실수 VR → double
     *   바이너리 VR → std::vector<uint8_t>
     *   Sequence VR → std::vector<value_container>
     */
    [[nodiscard]] static container::value to_container_value(
        const core::dicom_element& element);

    /**
     * @brief container 값을 DICOM 엘리먼트로 변환
     * @param tag 대상 DICOM 태그
     * @param vr Value Representation
     * @param val Container 값
     * @return DICOM 데이터 엘리먼트
     */
    [[nodiscard]] static core::dicom_element from_container_value(
        core::dicom_tag tag,
        encoding::vr_type vr,
        const container::value& val);

    // ═══════════════════════════════════════════════════════════════
    // 대량 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 전체 데이터셋을 container로 직렬화
     */
    [[nodiscard]] static container::value_container serialize_dataset(
        const core::dicom_dataset& dataset);

    /**
     * @brief container를 데이터셋으로 역직렬화
     */
    [[nodiscard]] static common::Result<core::dicom_dataset>
        deserialize_dataset(const container::value_container& container);
};

} // namespace pacs::integration
```

### INT-ECO-002: network_adapter 인터페이스

**추적성:** DES-INT-002, IR-2

```cpp
namespace pacs::integration {

/**
 * @brief network_system TCP/TLS 연산을 위한 어댑터
 */
class network_adapter {
public:
    // ═══════════════════════════════════════════════════════════════
    // 서버 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief network_system을 사용하여 DICOM 서버 생성
     * @param config 서버 설정
     * @return 설정된 서버 인스턴스
     */
    [[nodiscard]] static std::unique_ptr<network::dicom_server>
        create_server(const network::server_config& config);

    // ═══════════════════════════════════════════════════════════════
    // 클라이언트 연산
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 원격 DICOM 서버에 연결
     * @param host 원격 호스트
     * @param port 원격 포트
     * @param timeout 연결 타임아웃
     * @return 연결된 세션 또는 오류
     */
    [[nodiscard]] static common::Result<
        network_system::messaging_session_ptr> connect(
            const std::string& host,
            uint16_t port,
            std::chrono::milliseconds timeout);

    // ═══════════════════════════════════════════════════════════════
    // TLS 설정
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief DICOM 연결을 위한 TLS 설정
     */
    static void configure_tls(
        network_system::tls_config& config,
        const std::filesystem::path& cert_path,
        const std::filesystem::path& key_path,
        const std::filesystem::path& ca_path);
};

} // namespace pacs::integration
```

### INT-ECO-003: thread_adapter 인터페이스

**추적성:** DES-INT-003, IR-3

```cpp
namespace pacs::integration {

/**
 * @brief thread_system 작업 처리를 위한 어댑터
 */
class thread_adapter {
public:
    /**
     * @brief DIMSE 처리를 위한 공유 스레드 풀 조회
     * @return 스레드 풀 참조
     */
    [[nodiscard]] static thread_system::thread_pool& get_worker_pool();

    /**
     * @brief 작업자 풀에 작업 제출
     * @param job 실행할 작업
     * @return 결과를 위한 Future
     */
    template<typename F>
    [[nodiscard]] static auto submit(F&& job)
        -> std::future<std::invoke_result_t<F>>;

    /**
     * @brief 저장 I/O 스레드 풀 조회
     * @return 저장 스레드 풀 참조
     */
    [[nodiscard]] static thread_system::thread_pool& get_storage_pool();
};

} // namespace pacs::integration
```

---

## 6. 오류 처리 인터페이스

### INT-ERR-001: 오류 코드 인터페이스

**추적성:** 공통 오류 처리 요구사항

```cpp
namespace pacs::error_codes {

// ═══════════════════════════════════════════════════════════════════════════
// DICOM 파싱 오류 (-800 ~ -819)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int INVALID_DICOM_FILE = -800;     // 유효하지 않은 DICOM 파일
constexpr int INVALID_VR = -801;             // 유효하지 않은 VR
constexpr int MISSING_REQUIRED_TAG = -802;   // 필수 태그 누락
constexpr int INVALID_TRANSFER_SYNTAX = -803; // 유효하지 않은 전송 구문
constexpr int INVALID_DATA_ELEMENT = -804;   // 유효하지 않은 데이터 엘리먼트
constexpr int SEQUENCE_ENCODING_ERROR = -805; // 시퀀스 인코딩 오류
constexpr int INVALID_TAG = -806;            // 유효하지 않은 태그
constexpr int VALUE_LENGTH_EXCEEDED = -807;  // 값 길이 초과
constexpr int INVALID_UID_FORMAT = -808;     // 유효하지 않은 UID 형식

// ═══════════════════════════════════════════════════════════════════════════
// Association 오류 (-820 ~ -839)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int ASSOCIATION_REJECTED = -820;   // Association 거부됨
constexpr int ASSOCIATION_ABORTED = -821;    // Association 중단됨
constexpr int NO_PRESENTATION_CONTEXT = -822; // Presentation Context 없음
constexpr int INVALID_PDU = -823;            // 유효하지 않은 PDU
constexpr int PDU_TOO_LARGE = -824;          // PDU 크기 초과
constexpr int ASSOCIATION_TIMEOUT = -825;    // Association 타임아웃
constexpr int CALLED_AE_NOT_RECOGNIZED = -826; // Called AE 인식 불가
constexpr int CALLING_AE_NOT_ALLOWED = -827;  // Calling AE 허용되지 않음
constexpr int TRANSPORT_ERROR = -828;        // 전송 오류

// ═══════════════════════════════════════════════════════════════════════════
// DIMSE 오류 (-840 ~ -859)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int DIMSE_FAILURE = -840;          // DIMSE 실패
constexpr int DIMSE_TIMEOUT = -841;          // DIMSE 타임아웃
constexpr int DIMSE_INVALID_RESPONSE = -842; // 유효하지 않은 DIMSE 응답
constexpr int DIMSE_CANCELLED = -843;        // DIMSE 취소됨
constexpr int DIMSE_STATUS_ERROR = -844;     // DIMSE 상태 오류
constexpr int DIMSE_MISSING_DATASET = -845;  // 데이터셋 누락
constexpr int DIMSE_UNEXPECTED_MESSAGE = -846; // 예기치 않은 메시지

// ═══════════════════════════════════════════════════════════════════════════
// 저장 오류 (-860 ~ -879)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int STORAGE_FAILED = -860;         // 저장 실패
constexpr int DUPLICATE_SOP_INSTANCE = -861; // 중복 SOP Instance
constexpr int INVALID_SOP_CLASS = -862;      // 유효하지 않은 SOP 클래스
constexpr int STORAGE_FULL = -863;           // 저장 공간 부족
constexpr int FILE_WRITE_ERROR = -864;       // 파일 쓰기 오류
constexpr int FILE_READ_ERROR = -865;        // 파일 읽기 오류
constexpr int INDEX_UPDATE_ERROR = -866;     // 인덱스 업데이트 오류
constexpr int INSTANCE_NOT_FOUND = -867;     // 인스턴스 찾을 수 없음

// ═══════════════════════════════════════════════════════════════════════════
// 조회 오류 (-880 ~ -899)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int QUERY_FAILED = -880;           // 조회 실패
constexpr int NO_MATCHES_FOUND = -881;       // 매칭 결과 없음
constexpr int TOO_MANY_MATCHES = -882;       // 매칭 결과 너무 많음
constexpr int INVALID_QUERY_LEVEL = -883;    // 유효하지 않은 조회 레벨
constexpr int DATABASE_ERROR = -884;         // 데이터베이스 오류
constexpr int INVALID_QUERY_KEY = -885;      // 유효하지 않은 조회 키

/**
 * @brief 사람이 읽을 수 있는 오류 메시지 조회
 * @param code 오류 코드
 * @return 오류 설명
 */
[[nodiscard]] std::string_view message(int code);

/**
 * @brief 오류 카테고리 이름 조회
 * @param code 오류 코드
 * @return 카테고리 (파싱, Association, DIMSE, 저장, 조회)
 */
[[nodiscard]] std::string_view category(int code);

} // namespace pacs::error_codes
```

---

*문서 버전: 1.0.0*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
