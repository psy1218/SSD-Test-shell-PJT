# Phase 2 검증 보고서

## 실행 정보

- 실행일: 2026-10-12
- 운영체제: Ubuntu 24.04.4 LTS on WSL2
- 컴파일러: GCC 13.3.0
- 빌드 도구: GNU Make 4.3
- 컴파일 옵션: `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`

## 구현 내용

- `ssd W [LBA] [VALUE]` Write 명령
- `ssd R [LBA]` Read 명령
- LBA 0~99의 엄격한 10진수 검증
- `0x` + 대문자 8자리 16진수 VALUE 검증
- 성공한 Read의 `result.txt` 원자적 교체
- 정상, 인자 오류, I/O 오류, NAND 손상, 내부 오류 종료 코드 구분
- 오류 시 stdout `INVALID COMMAND`, 진단 정보 stderr 분리
- 기존 저장 단위 테스트와 CLI 프로세스 통합 테스트의 Makefile 통합

## 실행 명령

```sh
make clean
make
make test
make test-sanitize
```

## 일반 빌드 결과

컴파일 오류와 경고 없이 `ssd`와 테스트 실행 파일을 생성했다.

### 저장 계층 회귀 테스트

- 총 8
- PASS 8
- FAIL 0

### SSD CLI 통합 테스트

| 테스트 ID | 검증 대상 | 결과 |
|---|---|---:|
| TC-CLI-001 | 프로세스를 분리한 Write/Read 및 영속성 | PASS |
| TC-CLI-002 | 미기록 LBA의 0 반환 | PASS |
| TC-CLI-003 | LBA 0과 99 경계 | PASS |
| TC-CLI-004 | Write의 `result.txt` 불변 계약 | PASS |
| TC-CLI-005 | Read의 기존 결과 덮어쓰기 | PASS |
| TC-CLI-006 | 명령과 인자 개수 오류 | PASS |
| TC-CLI-007 | 잘못된 LBA 거부 | PASS |
| TC-CLI-008 | 선행 0이 있는 유효 LBA | PASS |
| TC-CLI-009 | 잘못된 길이·접두사·소문자·문자의 VALUE 거부 | PASS |
| TC-CLI-010 | 손상 NAND 검출과 기존 파일 보존 | PASS |
| TC-CLI-011 | NAND 경로 I/O 오류와 종료 코드 | PASS |
| TC-CLI-012 | 결과 파일 교체 오류와 기존 경로 보존 | PASS |
| TC-CLI-013 | NAND의 원시 Little Endian 바이트 | PASS |

- 총 13
- PASS 13
- FAIL 0

일반 빌드 전체 결과는 21/21 PASS다.

## Sanitizer 결과

저장 계층 8개와 CLI 13개를 AddressSanitizer 및
UndefinedBehaviorSanitizer 빌드로 다시 실행했다.

- 총 21
- PASS 21
- FAIL 0
- AddressSanitizer 보고: 없음
- UndefinedBehaviorSanitizer 보고: 없음

## 발견한 문제와 수정

### NAND 경로가 디렉터리일 때의 오류 분류

최초 일반 테스트에서 `TC-CLI-011`이 실패해 CLI 결과는 12/13이었다.
Linux에서는 디렉터리를 `fopen(..., "rb")`로 열 수 있어 기존 구현이 해당
경로를 I/O 오류가 아닌 0바이트 손상 NAND로 분류한 것이 원인이었다.

`fstat()`와 `S_ISREG()`로 NAND가 일반 파일인지 먼저 검사하도록 수정했다.
수정 후 일반 테스트와 Sanitizer 테스트 모두에서 `TC-CLI-011`을 포함한
21개 테스트가 통과했다.

## Phase 2 완료 조건 판정

| 완료 조건 | 결과 | 근거 |
|---|---:|---|
| SSD Write 정상 동작 | 충족 | TC-CLI-001, 003 |
| SSD Read 정상 동작 | 충족 | TC-CLI-001, 002, 005 |
| `nand.txt`/`result.txt` 계약 준수 | 충족 | TC-CLI-004, 005, 013 |
| 잘못된 입력 안전 처리 | 충족 | TC-CLI-006~009 |
| I/O와 손상 오류 구분 | 충족 | TC-CLI-010~012 |
| Phase 1 기능 회귀 없음 | 충족 | TC-STOR 8/8 PASS |
| Sanitizer 오류 없음 | 충족 | 전체 21/21 PASS |

Phase 2 완료 조건을 모두 충족했으며 Phase 3 Test Shell 구현을 진행할 수 있다.
