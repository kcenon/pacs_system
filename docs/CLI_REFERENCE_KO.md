# CLI 레퍼런스

> **언어:** [English](CLI_REFERENCE.md) | **한국어**

PACS System에 포함된 32개 CLI 도구의 전체 문서입니다. 간단한 개요는 [README](../README_KO.md#cli-도구--예제)를 참조하세요.

## 예제 빌드

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

---

## 목차

### 파일 유틸리티
- [DCM Dump](#dcm-dump) - DICOM 파일 검사
- [DCM Info](#dcm-info) - DICOM 파일 요약
- [DCM Modify](#dcm-modify) - 태그 수정
- [DCM Conv](#dcm-conv) - Transfer Syntax 변환
- [DCM Anonymize](#dcm-anonymize) - 비식별화 (PS3.15)
- [DCM Dir](#dcm-dir) - DICOMDIR 생성/관리 (PS3.10)
- [DCM to JSON](#dcm-to-json) - DICOM → JSON (PS3.18)
- [JSON to DCM](#json-to-dcm) - JSON → DICOM (PS3.18)
- [DCM to XML](#dcm-to-xml) - DICOM → XML (PS3.19)
- [XML to DCM](#xml-to-dcm) - XML → DICOM (PS3.19)
- [Img to DCM](#img-to-dcm) - 이미지 → DICOM 변환
- [DCM Extract](#dcm-extract) - 픽셀 데이터 추출
- [DB Browser](#db-browser) - PACS 인덱스 데이터베이스 뷰어

### 네트워크 서비스
- [Echo SCP](#echo-scp) - 검증 서버
- [Echo SCU](#echo-scu) - 검증 클라이언트
- [Secure Echo SCU/SCP](#secure-echo-scuscp) - TLS 보안 DICOM
- [Storage SCU](#storage-scu) - 이미지 전송 (C-STORE)
- [Query SCU](#query-scu) - 쿼리 클라이언트 (C-FIND)
- [Find SCU](#find-scu) - dcmtk 호환 C-FIND
- [Retrieve SCU](#retrieve-scu) - 검색 클라이언트 (C-MOVE/C-GET)
- [Move SCU](#move-scu) - dcmtk 호환 C-MOVE
- [Get SCU](#get-scu) - dcmtk 호환 C-GET
- [Print SCU](#print-scu) - Print Management 클라이언트

### 서버 애플리케이션
- [Store SCP](#store-scp) - Storage 서버
- [QR SCP](#qr-scp) - Query/Retrieve 서버 (C-FIND/C-MOVE/C-GET)
- [Worklist SCU](#worklist-scu) - Modality Worklist 조회 클라이언트
- [Worklist SCP](#worklist-scp) - Modality Worklist 서버
- [MPPS SCU](#mpps-scu) - MPPS 클라이언트
- [MPPS SCP](#mpps-scp) - MPPS 서버
- [PACS Server](#pacs-server) - 전체 PACS 서버

### 테스트 & 통합
- [Integration Tests](#integration-tests) - End-to-end 워크플로우 테스트
- [Event Bus Integration](#event-bus-integration) - 모듈 간 통신

---

## 파일 유틸리티

### DCM Dump

DICOM 파일 메타데이터 검사 유틸리티.

```bash
# DICOM 파일 메타데이터 출력
./build/bin/dcm_dump image.dcm

# 특정 태그만 필터링
./build/bin/dcm_dump image.dcm --tags PatientName,PatientID,Modality

# 픽셀 데이터 정보 표시
./build/bin/dcm_dump image.dcm --pixel-info

# JSON 형식 출력 (연동용)
./build/bin/dcm_dump image.dcm --format json

# 디렉토리 재귀 스캔 및 요약
./build/bin/dcm_dump ./dicom_folder/ --recursive --summary
```

### DCM Info

DICOM 파일 요약 유틸리티.

```bash
# DICOM 파일 요약 정보 표시
./build/bin/dcm_info image.dcm

# 상세 모드 (모든 필드 표시)
./build/bin/dcm_info image.dcm --verbose

# JSON 출력 (스크립트 연동용)
./build/bin/dcm_info image.dcm --format json

# 여러 파일 빠른 확인
./build/bin/dcm_info ./dicom_folder/ -r -q
```

### DCM Modify

dcmtk 호환 DICOM 태그 수정 유틸리티. 숫자 태그 형식 `(GGGG,EEEE)`과 키워드 형식 모두 지원.

```bash
# 태그 삽입 (존재하지 않으면 생성) - 두 가지 태그 형식 지원
./build/bin/dcm_modify -i "(0010,0010)=Anonymous" patient.dcm
./build/bin/dcm_modify -i PatientName=Anonymous -o modified.dcm patient.dcm

# 기존 태그 수정 (존재하지 않으면 오류)
./build/bin/dcm_modify -m "(0010,0020)=NEW_ID" patient.dcm

# 태그 삭제
./build/bin/dcm_modify -e "(0010,1000)" patient.dcm
./build/bin/dcm_modify -e OtherPatientIDs patient.dcm

# 모든 매칭 태그 삭제 (시퀀스 내 포함)
./build/bin/dcm_modify -ea "(0010,1001)" patient.dcm

# 모든 private 태그 삭제
./build/bin/dcm_modify -ep patient.dcm

# UID 재생성
./build/bin/dcm_modify -gst -gse -gin -o anonymized.dcm patient.dcm

# 스크립트 파일로 일괄 수정
./build/bin/dcm_modify --script modify.txt *.dcm

# 제자리 수정 (.bak 백업 생성)
./build/bin/dcm_modify -i PatientID=NEW_ID patient.dcm

# 백업 없이 제자리 수정 (주의!)
./build/bin/dcm_modify -i PatientID=NEW_ID -nb patient.dcm

# 디렉토리 재귀 처리
./build/bin/dcm_modify -i PatientName=Anonymous -r ./dicom_folder/ -o ./output/
```

스크립트 파일 형식 (`modify.txt`):
```
# 주석은 #으로 시작
i (0010,0010)=Anonymous     # 태그 삽입/수정
m (0008,0050)=ACC001        # 기존 태그 수정
e (0010,1000)               # 태그 삭제
ea (0010,1001)              # 모든 매칭 태그 삭제
```

### DCM Conv

Transfer Syntax 변환 유틸리티.

```bash
# Explicit VR Little Endian으로 변환 (기본값)
./build/bin/dcm_conv image.dcm converted.dcm --explicit

# Implicit VR Little Endian으로 변환
./build/bin/dcm_conv image.dcm output.dcm --implicit

# JPEG Baseline으로 변환 (품질 설정 포함)
./build/bin/dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 85

# 디렉토리 재귀 변환 및 검증
./build/bin/dcm_conv ./input_dir/ ./output_dir/ --recursive --verify

# 지원되는 Transfer Syntax 목록 표시
./build/bin/dcm_conv --list-syntaxes

# Transfer Syntax UID 직접 지정
./build/bin/dcm_conv image.dcm output.dcm -t 1.2.840.10008.1.2.4.50
```

### DCM Anonymize

DICOM PS3.15 보안 프로파일을 준수하는 DICOM 비식별화 유틸리티.

```bash
# 기본 익명화 (직접 식별자 제거)
./build/bin/dcm_anonymize patient.dcm anonymous.dcm

# HIPAA Safe Harbor 규정 준수 (18개 식별자 제거)
./build/bin/dcm_anonymize --profile hipaa_safe_harbor patient.dcm output.dcm

# GDPR 규정 준수 가명처리
./build/bin/dcm_anonymize --profile gdpr_compliant patient.dcm output.dcm

# 특정 태그 유지
./build/bin/dcm_anonymize -k PatientSex -k PatientAge patient.dcm output.dcm

# 사용자 정의 값으로 태그 대체
./build/bin/dcm_anonymize -r "InstitutionName=Research Hospital" patient.dcm output.dcm

# 새 환자 식별자 설정
./build/bin/dcm_anonymize --patient-id "STUDY001_001" --patient-name "Anonymous" patient.dcm

# UID 매핑 파일로 일관된 익명화 (동일 검사 파일들 간)
./build/bin/dcm_anonymize -m mapping.json patient.dcm output.dcm

# 종단 연구를 위한 날짜 이동
./build/bin/dcm_anonymize --profile retain_longitudinal --date-offset -30 patient.dcm

# 디렉토리 재귀 일괄 처리
./build/bin/dcm_anonymize --recursive -o anonymized/ ./originals/

# 미리보기 모드 (변경 사항만 표시)
./build/bin/dcm_anonymize --dry-run --verbose patient.dcm

# 익명화 완료 검증
./build/bin/dcm_anonymize --verify patient.dcm anonymous.dcm
```

사용 가능한 익명화 프로파일:
- `basic` - 직접 환자 식별자 제거 (기본값)
- `clean_pixel` - 픽셀 데이터에서 번인된 주석 제거
- `clean_descriptions` - PHI가 포함될 수 있는 자유 텍스트 필드 정리
- `retain_longitudinal` - 날짜 이동으로 시간적 관계 유지
- `retain_patient_characteristics` - 인구통계 정보 유지 (성별, 나이, 키, 체중)
- `hipaa_safe_harbor` - 완전한 HIPAA 18개 식별자 제거
- `gdpr_compliant` - GDPR 가명처리 요구사항

### DCM Dir

DICOM PS3.10 표준에 따라 DICOM 미디어 저장용 DICOMDIR 파일을 생성하고 관리.

```bash
# 디렉토리에서 DICOMDIR 생성
./build/bin/dcm_dir create ./patient_data/

# 사용자 정의 출력 경로 및 파일셋 ID로 생성
./build/bin/dcm_dir create -o DICOMDIR --file-set-id "STUDY001" ./patient_data/

# 상세 출력으로 생성
./build/bin/dcm_dir create -v ./patient_data/

# DICOMDIR 내용을 트리 형식으로 표시
./build/bin/dcm_dir list DICOMDIR

# 상세 정보와 함께 표시
./build/bin/dcm_dir list -l DICOMDIR

# 파일 목록만 표시 (플랫 형식)
./build/bin/dcm_dir list --flat DICOMDIR

# DICOMDIR 구조 검증
./build/bin/dcm_dir verify DICOMDIR

# 참조 파일 존재 확인
./build/bin/dcm_dir verify --check-files DICOMDIR

# 일관성 검사 (중복 SOP Instance UID 감지)
./build/bin/dcm_dir verify --check-consistency DICOMDIR

# 새 파일을 추가하여 DICOMDIR 업데이트
./build/bin/dcm_dir update -a ./new_study/ DICOMDIR
```

DICOMDIR 구조는 DICOM 계층을 따릅니다:
- PATIENT → STUDY → SERIES → IMAGE (계층적 레코드 구조)
- 참조 파일 ID는 ISO 9660 호환성을 위해 백슬래시 구분자 사용
- 표준 레코드 유형 지원: PATIENT, STUDY, SERIES, IMAGE

### DCM to JSON

DICOM 파일을 DICOM PS3.18 JSON 표준 형식으로 변환.

```bash
# DICOM을 JSON으로 변환 (표준출력)
./build/bin/dcm_to_json image.dcm

# 파일로 출력 (정렬된 형식)
./build/bin/dcm_to_json image.dcm output.json --pretty

# 압축 출력 (서식 없음)
./build/bin/dcm_to_json image.dcm output.json --compact

# 바이너리 데이터를 Base64로 포함
./build/bin/dcm_to_json image.dcm output.json --bulk-data inline

# 바이너리 데이터를 별도 파일로 저장하고 URI로 참조
./build/bin/dcm_to_json image.dcm output.json --bulk-data uri --bulk-data-dir ./bulk/

# 픽셀 데이터 제외
./build/bin/dcm_to_json image.dcm output.json --no-pixel

# 특정 태그만 필터링
./build/bin/dcm_to_json image.dcm -t 0010,0010 -t 0010,0020

# 디렉토리 재귀 처리
./build/bin/dcm_to_json ./dicom_folder/ --recursive --no-pixel
```

출력 형식 (DICOM PS3.18):
```json
{
  "00100010": {
    "vr": "PN",
    "Value": [{"Alphabetic": "DOE^JOHN"}]
  },
  "00100020": {
    "vr": "LO",
    "Value": ["12345678"]
  }
}
```

### JSON to DCM

DICOM PS3.18 형식의 JSON 파일을 DICOM 파일로 변환.

```bash
# JSON을 DICOM으로 변환
./build/bin/json_to_dcm metadata.json output.dcm

# 템플릿 DICOM 파일 사용 (픽셀 데이터 및 누락 태그 복사)
./build/bin/json_to_dcm metadata.json output.dcm --template original.dcm

# Transfer Syntax 지정
./build/bin/json_to_dcm metadata.json output.dcm -t 1.2.840.10008.1.2.1

# BulkDataURI 해석을 위한 디렉토리 지정
./build/bin/json_to_dcm metadata.json output.dcm --bulk-data-dir ./bulk/
```

### DCM to XML

DICOM 파일을 DICOM Native XML 형식(PS3.19)으로 변환.

```bash
# DICOM을 XML로 변환 (stdout)
./build/bin/dcm_to_xml image.dcm

# 포맷팅된 출력
./build/bin/dcm_to_xml image.dcm output.xml --pretty

# 바이너리 데이터를 Base64로 포함
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data inline

# 바이너리 데이터를 별도 파일로 저장하고 URI로 참조
./build/bin/dcm_to_xml image.dcm output.xml --bulk-data uri --bulk-data-dir ./bulk/

# 픽셀 데이터 제외
./build/bin/dcm_to_xml image.dcm output.xml --no-pixel

# 특정 태그만 필터링
./build/bin/dcm_to_xml image.dcm -t 0010,0010 -t 0010,0020

# 디렉토리 재귀 처리
./build/bin/dcm_to_xml ./dicom_folder/ --recursive --no-pixel
```

출력 형식 (DICOM Native XML PS3.19):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<NativeDicomModel xmlns="http://dicom.nema.org/PS3.19/models/NativeDICOM">
  <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
    <PersonName number="1">
      <Alphabetic>
        <FamilyName>DOE</FamilyName>
        <GivenName>JOHN</GivenName>
      </Alphabetic>
    </PersonName>
  </DicomAttribute>
</NativeDicomModel>
```

### XML to DCM

XML 파일(DICOM Native XML PS3.19 형식)을 DICOM 형식으로 변환.

```bash
# XML을 DICOM으로 변환
./build/bin/xml_to_dcm metadata.xml output.dcm

# 템플릿 DICOM 사용 (픽셀 데이터 및 누락된 태그 복사)
./build/bin/xml_to_dcm metadata.xml output.dcm --template original.dcm

# Transfer Syntax 지정
./build/bin/xml_to_dcm metadata.xml output.dcm -t 1.2.840.10008.1.2.1

# BulkData URI 해석을 위한 디렉토리 지정
./build/bin/xml_to_dcm metadata.xml output.dcm --bulk-data-dir ./bulk/
```

### Img to DCM

JPEG 이미지를 Secondary Capture SOP Class를 사용하여 DICOM 형식으로 변환.

```bash
# 기본 변환
./build/bin/img_to_dcm photo.jpg output.dcm

# 환자 메타데이터 포함
./build/bin/img_to_dcm photo.jpg output.dcm \
  --patient-name "DOE^JOHN" \
  --patient-id "12345" \
  --study-description "Photograph"

# 이미지 디렉토리 변환
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive

# 상세 출력
./build/bin/img_to_dcm photo.jpg output.dcm -v

# 기존 파일 덮어쓰기
./build/bin/img_to_dcm ./photos/ ./dicom/ --recursive --overwrite
```

주요 기능:
- JPEG 이미지를 DICOM Secondary Capture 형식으로 변환
- Study, Series, Instance에 대한 자동 UID 생성
- 환자 및 검사 메타데이터 커스터마이징
- 재귀 디렉토리 지원 일괄 처리
- libjpeg-turbo 필요

### DCM Extract

DICOM 파일에서 픽셀 데이터를 표준 이미지 형식으로 추출.

```bash
# 픽셀 데이터 정보 표시
./build/bin/dcm_extract image.dcm --info

# RAW 바이너리 형식으로 추출
./build/bin/dcm_extract image.dcm output.raw --raw

# JPEG로 추출 (libjpeg 필요)
./build/bin/dcm_extract image.dcm output.jpg --jpeg -q 90

# PNG로 추출 (libpng 필요)
./build/bin/dcm_extract image.dcm output.png --png

# PPM/PGM 형식으로 추출
./build/bin/dcm_extract image.dcm output.ppm --ppm

# Window Level 변환 적용
./build/bin/dcm_extract image.dcm output.jpg --jpeg --window 40 400

# 디렉토리 일괄 추출
./build/bin/dcm_extract ./dicom/ ./images/ --recursive --jpeg
```

주요 기능:
- RAW, JPEG, PNG, PPM/PGM 출력 형식 지원
- Window Level (center/width) 변환
- 자동 16비트 → 8비트 변환
- MONOCHROME1/MONOCHROME2 처리
- 재귀 디렉토리 지원 일괄 처리

### DB Browser

PACS 인덱스 데이터베이스 뷰어.

```bash
# 전체 환자 목록 조회
./build/bin/db_browser pacs.db patients

# 특정 환자의 스터디 목록 조회
./build/bin/db_browser pacs.db studies --patient-id "12345"

# 날짜 범위로 스터디 필터링
./build/bin/db_browser pacs.db studies --from 20240101 --to 20241231

# 특정 스터디의 시리즈 목록 조회
./build/bin/db_browser pacs.db series --study-uid "1.2.3.4.5"

# 데이터베이스 통계 표시
./build/bin/db_browser pacs.db stats

# 데이터베이스 유지보수
./build/bin/db_browser pacs.db vacuum
./build/bin/db_browser pacs.db verify
```

---

## 네트워크 서비스

### Echo SCP

DICOM Verification SCP 서버.

```bash
# Echo SCP 실행
./build/bin/echo_scp --port 11112 --ae-title MY_ECHO
```

### Echo SCU

dcmtk 호환 DICOM 연결 검증 도구.

```bash
# 기본 연결 테스트
./build/bin/echo_scu localhost 11112

# 커스텀 AE Title 지정
./build/bin/echo_scu -aet MY_SCU -aec PACS_SCP localhost 11112

# 상세 출력 및 타임아웃 설정
./build/bin/echo_scu -v -to 60 localhost 11112

# 연결 모니터링을 위한 반복 테스트
./build/bin/echo_scu -r 10 --repeat-delay 1000 localhost 11112

# 조용한 모드 (종료 코드만 반환)
./build/bin/echo_scu -q localhost 11112

# 모든 옵션 확인
./build/bin/echo_scu --help
```

### Secure Echo SCU/SCP

TLS 1.2/1.3 및 상호 TLS를 지원하는 보안 DICOM 연결 테스트.

```bash
# 먼저 테스트 인증서 생성
cd examples/secure_dicom
./generate_certs.sh

# 보안 서버 시작 (TLS)
./build/bin/secure_echo_scp 2762 MY_PACS \
    --cert certs/server.crt \
    --key certs/server.key \
    --ca certs/ca.crt

# 보안 연결 테스트 (서버 인증서만 검증)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt

# 상호 TLS 테스트 (클라이언트 인증서 사용)
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --cert certs/client.crt \
    --key certs/client.key \
    --ca certs/ca.crt

# TLS 1.3 사용
./build/bin/secure_echo_scu localhost 2762 MY_PACS \
    --ca certs/ca.crt \
    --tls-version 1.3
```

### Storage SCU

DICOM Storage SCU - 원격 SCP로 이미지 전송.

```bash
# 단일 DICOM 파일 전송
./build/bin/store_scu localhost 11112 image.dcm

# 사용자 지정 AE 타이틀로 전송
./build/bin/store_scu -aet MYSCU -aec PACS localhost 11112 image.dcm

# 디렉토리의 모든 파일 전송 (재귀) + 진행률 표시
./build/bin/store_scu -r --progress localhost 11112 ./dicom_folder/

# Transfer Syntax 선호 설정
./build/bin/store_scu --prefer-lossless localhost 11112 *.dcm

# 상세 출력 + 타임아웃 설정
./build/bin/store_scu -v -to 60 localhost 11112 image.dcm

# 전송 리포트 생성
./build/bin/store_scu --report-file transfer.log localhost 11112 ./data/

# 조용한 모드 (최소 출력)
./build/bin/store_scu -q localhost 11112 image.dcm

# 도움말 표시
./build/bin/store_scu --help
```

### Query SCU

DICOM Query SCU 클라이언트 (C-FIND).

```bash
# 환자 이름으로 Study 검색 (와일드카드 지원)
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"

# 날짜 범위로 검색
./build/bin/query_scu localhost 11112 PACS_SCP --level STUDY --study-date "20240101-20241231"

# 특정 Study의 Series 검색
./build/bin/query_scu localhost 11112 PACS_SCP --level SERIES --study-uid "1.2.3.4.5"

# 연동을 위한 JSON 출력
./build/bin/query_scu localhost 11112 PACS_SCP --patient-id "12345" --format json

# CSV로 내보내기
./build/bin/query_scu localhost 11112 PACS_SCP --modality CT --format csv > results.csv
```

### Find SCU

dcmtk 호환 C-FIND SCU 유틸리티.

```bash
# Patient Root Query - 환자의 모든 검사 조회
./build/bin/find_scu -P -L STUDY -k "0010,0010=Smith*" localhost 11112

# Study Root Query - 날짜 범위의 CT 검사 조회
./build/bin/find_scu -S -L STUDY \
  -aec PACS_SCP \
  -k "0008,0060=CT" \
  -k "0008,0020=20240101-20241231" \
  localhost 11112

# JSON 출력
./build/bin/find_scu -S -L SERIES -k "0020,000D=1.2.840..." -o json localhost 11112

# 파일에서 쿼리 키 읽기
./build/bin/find_scu -f query_keys.txt localhost 11112
```

### Retrieve SCU

DICOM Retrieve SCU 클라이언트 (C-MOVE/C-GET).

```bash
# C-GET: 로컬 머신으로 Study 직접 검색
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode get --study-uid "1.2.3.4.5" -o ./downloads

# C-MOVE: 다른 PACS/워크스테이션으로 Study 전송
./build/bin/retrieve_scu localhost 11112 PACS_SCP --mode move --dest-ae LOCAL_SCP --study-uid "1.2.3.4.5"

# 특정 Series 검색
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level SERIES --series-uid "1.2.3.4.5.6"

# 환자의 모든 Study 검색
./build/bin/retrieve_scu localhost 11112 PACS_SCP --level PATIENT --patient-id "12345"

# 플랫 저장 구조 (모든 파일을 한 디렉토리에)
./build/bin/retrieve_scu localhost 11112 PACS_SCP --study-uid "1.2.3.4.5" --structure flat
```

### Move SCU

dcmtk 호환 C-MOVE SCU 유틸리티.

```bash
# 제3자 워크스테이션으로 Study 전송
./build/bin/move_scu -aem WORKSTATION \
  -L STUDY \
  -k "0020,000D=1.2.840..." \
  pacs.example.com 104

# 진행률 표시와 함께 Series 전송
./build/bin/move_scu -aem ARCHIVE \
  --progress \
  -L SERIES \
  -k "0020,000E=1.2.840..." \
  localhost 11112
```

### Get SCU

dcmtk 호환 C-GET SCU 유틸리티.

```bash
# 전체 Study 직접 검색 (별도 Storage SCP 불필요)
./build/bin/get_scu -L STUDY \
  -k "0020,000D=1.2.840..." \
  --progress \
  -od ./study_data/ \
  pacs.example.com 104

# Lossless 선호로 단일 Instance 검색
./build/bin/get_scu --prefer-lossless \
  -L IMAGE \
  -k "0008,0018=1.2.840..." \
  -od ./retrieved/ \
  localhost 11112
```

### Print SCU

DICOM Print Management 클라이언트 (PS3.4 Annex H).

```bash
# 전체 프린트 워크플로우: Session 생성 → Film Box 생성 → 인쇄 → 삭제
./build/bin/print_scu localhost 10400 PRINTER_SCP print \
  --copies 1 \
  --priority HIGH \
  --medium "BLUE FILM" \
  --format "STANDARD\\1,1" \
  --orientation PORTRAIT \
  --film-size 8INX10IN

# 프린터 상태 조회
./build/bin/print_scu localhost 10400 PRINTER_SCP status

# 커스텀 AE 타이틀 및 상세 출력
./build/bin/print_scu -aet MY_WORKSTATION -v localhost 10400 PRINTER_SCP print

# 도움말 표시
./build/bin/print_scu --help
```

주요 기능:
- **Film Session**: 세션 수명 주기 관리 (N-CREATE/N-DELETE)
- **Film Box**: 필름 레이아웃 및 인쇄 (N-CREATE/N-ACTION)
- **Image Box**: 이미지 위치에 픽셀 데이터 설정 (N-SET)
- **Printer Status**: 프린터 상태 조회 (NORMAL, WARNING, FAILURE)
- **Meta SOP Class**: Basic Grayscale 및 Color Print Meta SOP Class 지원

---

## 서버 애플리케이션

### Store SCP

DICOM Storage SCP 서버. 전체 구현은 `store_scp` 예제를 참조하세요.

### QR SCP

저장 디렉토리에서 DICOM 파일을 제공하는 경량 Query/Retrieve SCP 서버.

```bash
# 기본 사용법 - 디렉토리에서 파일 제공
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom

# 영구 인덱스 데이터베이스 사용 (재시작 시 더 빠름)
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db

# C-MOVE 대상 피어 설정
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113

# 다중 대상 C-MOVE를 위한 여러 피어 설정
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom \
  --peer WS1:10.0.0.1:104 --peer WS2:10.0.0.2:104

# 서버 시작 없이 스토리지 스캔 및 인덱싱만 수행
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --scan-only

# 연결 제한 커스터마이징
./build/bin/qr_scp 11112 MY_PACS --storage-dir ./dicom --max-assoc 20 --timeout 600
```

기능:
- **C-FIND**: Patient, Study, Series, Image 레벨에서 조회
- **C-MOVE**: 설정된 대상 AE Title로 이미지 전송
- **C-GET**: 동일 연결을 통한 직접 이미지 검색
- **자동 인덱싱**: SQLite 기반 빠른 쿼리 응답
- **정상 종료**: 시그널 처리 (SIGINT, SIGTERM)

### Worklist SCU

Modality Worklist 조회 클라이언트.

```bash
# CT 모달리티 Worklist 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT

# 오늘 예정된 검사 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality MR --date today

# 스테이션 AE 타이틀로 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --station "CT_SCANNER_01" --date 20241215

# 환자 필터로 조회
./build/bin/worklist_scu localhost 11112 RIS_SCP --patient-name "DOE^*" --modality CT

# 연동을 위한 JSON 출력
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format json > worklist.json

# CSV로 내보내기
./build/bin/worklist_scu localhost 11112 RIS_SCP --modality CT --format csv > worklist.csv
```

### Worklist SCP

모달리티에 예약된 검사 정보를 제공하는 독립 실행형 Modality Worklist (MWL) 서버.

```bash
# JSON 워크리스트 파일로 기본 사용
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json

# 디렉토리에서 워크리스트 로드
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-dir ./worklist_data

# 결과 제한 설정
./build/bin/worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json --max-results 100
```

**샘플 워크리스트 JSON** (`worklist.json`):
```json
[
  {
    "patientId": "12345",
    "patientName": "DOE^JOHN",
    "patientBirthDate": "19800101",
    "patientSex": "M",
    "studyInstanceUid": "1.2.3.4.5.6.7.8.9",
    "accessionNumber": "ACC001",
    "scheduledStationAeTitle": "CT_01",
    "scheduledProcedureStepStartDate": "20241220",
    "scheduledProcedureStepStartTime": "100000",
    "modality": "CT",
    "scheduledProcedureStepId": "SPS001",
    "scheduledProcedureStepDescription": "CT Abdomen"
  }
]
```

기능:
- **MWL C-FIND**: Modality Worklist 쿼리 응답
- **JSON 데이터 소스**: JSON 파일에서 워크리스트 항목 로드
- **필터링**: Patient ID, 이름, 모달리티, 스테이션 AE, 예약 날짜
- **정상 종료**: 시그널 처리 (SIGINT, SIGTERM)

### MPPS SCU

검사 수행 상태 업데이트를 전송하는 MPPS 클라이언트.

```bash
# 새 MPPS 인스턴스 생성 (검사 시작)
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --patient-name "Doe^John" \
  --modality CT

# 검사 완료
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status COMPLETED \
  --series-uid "1.2.3.4.5.6.7.8.9"

# 검사 중단 (취소)
./build/bin/mpps_scu localhost 11112 RIS_SCP set \
  --mpps-uid "1.2.3.4.5.6.7.8" \
  --status DISCONTINUED \
  --reason "Patient refused"

# 디버깅을 위한 상세 출력
./build/bin/mpps_scu localhost 11112 RIS_SCP create \
  --patient-id "12345" \
  --modality MR \
  --verbose
```

### MPPS SCP

모달리티 장비로부터 검사 수행 상태 업데이트를 수신하는 독립형 MPPS 서버.

```bash
# 기본 사용법 - MPPS 메시지 수신
./build/bin/mpps_scp 11112 MY_MPPS

# MPPS 레코드를 개별 JSON 파일로 저장
./build/bin/mpps_scp 11112 MY_MPPS --output-dir ./mpps_records

# MPPS 레코드를 단일 JSON 파일에 추가
./build/bin/mpps_scp 11112 MY_MPPS --output-file ./mpps.json

# 사용자 정의 연결 제한
./build/bin/mpps_scp 11112 MY_MPPS --max-assoc 20 --timeout 600

# 도움말 표시
./build/bin/mpps_scp --help
```

주요 기능:
- **N-CREATE**: 검사 시작 알림 수신 (상태 = IN PROGRESS)
- **N-SET**: 검사 완료 (COMPLETED) 또는 취소 (DISCONTINUED) 수신
- **JSON 저장**: 통합을 위한 JSON 파일로 MPPS 레코드 저장
- **통계**: 종료 시 세션 통계 표시
- **정상 종료**: 시그널 처리 (SIGINT, SIGTERM)

### PACS Server

설정 파일을 이용한 전체 PACS 서버 예제.

```bash
# 설정 파일로 실행
./build/examples/pacs_server/pacs_server --config pacs_server.yaml
```

**샘플 설정** (`pacs_server.yaml`):
```yaml
server:
  ae_title: MY_PACS
  port: 11112
  max_associations: 50

storage:
  directory: ./archive
  naming: hierarchical

database:
  path: ./pacs.db
  wal_mode: true
```

---

## 테스트 & 통합

### Integration Tests

End-to-end 통합 테스트 스위트.

```bash
# 전체 통합 테스트 실행
./build/bin/pacs_integration_e2e

# 특정 테스트 카테고리 실행
./build/bin/pacs_integration_e2e "[connectivity]"    # 기본 C-ECHO 테스트
./build/bin/pacs_integration_e2e "[store_query]"     # 저장 및 조회 워크플로우
./build/bin/pacs_integration_e2e "[worklist]"        # Worklist 및 MPPS 워크플로우
./build/bin/pacs_integration_e2e "[workflow][multimodal]"  # 다중 모달리티 임상 워크플로우
./build/bin/pacs_integration_e2e "[xa]"              # XA Storage 테스트
./build/bin/pacs_integration_e2e "[tls]"             # TLS 통합 테스트
./build/bin/pacs_integration_e2e "[stability][smoke]"  # 빠른 안정성 스모크 테스트
./build/bin/pacs_integration_e2e "[stress]"          # 다중 연결 스트레스 테스트

# 사용 가능한 테스트 목록
./build/bin/pacs_integration_e2e --list-tests

# 상세 출력
./build/bin/pacs_integration_e2e --success

# CI/CD용 JUnit XML 리포트 생성
./build/bin/pacs_integration_e2e --reporter junit --out results.xml
```

**테스트 시나리오**:
- **Connectivity**: C-ECHO, 다중 연결, 타임아웃 처리
- **Store & Query**: 파일 저장, 환자/검사/시리즈별 조회, 와일드카드 매칭
- **XA Storage**: X선 혈관조영 이미지 저장 및 검색
- **Multi-Modal Workflow**: CT, MR, XA 모달리티를 사용한 완전한 환자 여정
- **Worklist/MPPS**: 예약된 검사, MPPS IN PROGRESS/COMPLETED 워크플로우
- **TLS Security**: 인증서 검증, 상호 TLS, 보안 통신
- **Stability**: 메모리 누수 감지, 연결 풀 고갈, 장시간 운영
- **Stress**: 동시 SCU, 빠른 연결, 대용량 데이터셋
- **Error Recovery**: 잘못된 SOP 클래스, 서버 재시작, 중단 처리

### Event Bus Integration

PACS 시스템은 모듈 간 통신과 이벤트 기반 워크플로우를 위해 `common_system` Event Bus와 통합됩니다.

```cpp
#include "pacs/core/events.hpp"
#include <kcenon/common/patterns/event_bus.h>

// 이미지 저장 이벤트 구독
auto& bus = kcenon::common::get_event_bus();
auto sub_id = bus.subscribe<pacs::events::image_received_event>(
    [](const pacs::events::image_received_event& evt) {
        std::cout << "Received image: " << evt.sop_instance_uid
                  << " from " << evt.calling_ae << std::endl;
        // 워크플로우 트리거, 캐시 업데이트, 알림 전송 등
    }
);

// 연결 이벤트 구독
bus.subscribe<pacs::events::association_established_event>(
    [](const pacs::events::association_established_event& evt) {
        std::cout << "New association: " << evt.calling_ae
                  << " -> " << evt.called_ae << std::endl;
    }
);

// 정리
bus.unsubscribe(sub_id);
```

**사용 가능한 이벤트 유형**:
- `association_established_event` / `association_released_event` / `association_aborted_event`
- `image_received_event` / `storage_failed_event`
- `query_executed_event` / `query_failed_event`
- `retrieve_started_event` / `retrieve_completed_event`
