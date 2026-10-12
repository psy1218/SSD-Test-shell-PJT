# Phase 1 검증 보고서

## 실행 정보

- 실행일: 2026-10-12
- 운영체제: Ubuntu 24.04.4 LTS on WSL2
- 커널: Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
- 컴파일러: GCC 13.3.0
- 빌드 도구: GNU Make 4.3
- 컴파일 옵션: `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`

## 실행 명령

```sh
make clean
make
make test
make test-sanitize
```

## 결과

일반 빌드는 컴파일 오류와 경고 없이 완료됐다.

| 테스트 ID | 검증 대상 | 일반 빌드 | ASan/UBSan |
|---|---|---:|---:|
| TC-STOR-001 | Little Endian encode/decode | PASS | PASS |
| TC-STOR-002 | NAND 부재 시 400바이트 0 초기화 | PASS | PASS |
| TC-STOR-003 | Write, Read, 재로드 영속성 | PASS | PASS |
| TC-STOR-004 | 경계 LBA 0과 99 | PASS | PASS |
| TC-STOR-005 | 범위 밖 LBA 100 거부 | PASS | PASS |
| TC-STOR-006 | 파일의 원시 Little Endian 바이트 | PASS | PASS |
| TC-STOR-007 | 399/401바이트 손상 파일 검출 및 보존 | PASS | PASS |
| TC-STOR-008 | NULL 및 빈 경로 인자 안전 처리 | PASS | PASS |

- 일반 테스트: 총 8, PASS 8, FAIL 0
- ASan/UBSan 테스트: 총 8, PASS 8, FAIL 0
- AddressSanitizer 보고: 없음
- UndefinedBehaviorSanitizer 보고: 없음

## Phase 1 완료 조건 판정

| 완료 조건 | 결과 | 근거 |
|---|---:|---|
| 정확한 400바이트 NAND 생성 | 충족 | TC-STOR-002 |
| 32비트 데이터 저장 및 복원 | 충족 | TC-STOR-001, 003 |
| 재로드 후 데이터 유지 | 충족 | TC-STOR-003 |
| Little Endian 포맷 준수 | 충족 | TC-STOR-006 |
| LBA 0~99 경계 처리 | 충족 | TC-STOR-004, 005 |
| 잘못된 NAND 크기 감지 | 충족 | TC-STOR-007 |
| 비정상 인자 안전 처리 | 충족 | TC-STOR-008, Sanitizer |

Phase 1의 정의된 완료 조건을 모두 충족했으며 Phase 2 SSD CLI 구현을 진행할
수 있다.

## 미구현 및 후속 범위

- `ssd W/R` 명령행 인터페이스와 `result.txt`: Phase 2
- Test Shell과 독립 프로세스 실행: Phase 3
- 독립 Test Oracle: Phase 4
- Fault Injection: Phase 6

위 항목은 Phase 1 테스트 결과에 포함하거나 완료된 것으로 간주하지 않는다.
