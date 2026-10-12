# 요구사항 명세 및 추적성

## 범위

이 문서는 최종 설계서의 필수 계약을 구현 가능한 요구사항 ID로 고정한다.
Phase 0과 Phase 1에서는 저장 계층만 구현하며 CLI, Test Shell, Oracle,
Fault Injection은 이후 Phase의 범위다.

## Phase 1 저장 요구사항

| ID | 요구사항 | 검증 방법 |
|---|---|---|
| STOR-001 | 논리 저장 공간은 LBA 0~99의 100개 항목이다. | `TC-STOR-004`, `TC-STOR-005` |
| STOR-002 | 각 LBA는 부호 없는 32비트 값 하나를 저장한다. | `TC-STOR-003`, `TC-STOR-004` |
| STOR-003 | NAND 파일은 정확히 400바이트여야 한다. | `TC-STOR-002`, `TC-STOR-007` |
| STOR-004 | 파일의 LBA 값은 명시적인 Little Endian 4바이트로 인코딩한다. | `TC-STOR-001`, `TC-STOR-006` |
| STOR-005 | NAND 파일이 없으면 모든 LBA가 0인 새 파일을 만든다. | `TC-STOR-002` |
| STOR-006 | 기존 NAND 파일의 크기가 400바이트가 아니면 손상으로 판정하고 초기화하지 않는다. | `TC-STOR-007` |
| STOR-007 | 유효한 Write 이후 다시 로드해도 값이 유지되어야 한다. | `TC-STOR-003` |
| STOR-008 | LBA 0과 99를 포함하고 범위 밖 LBA를 거부한다. | `TC-STOR-004`, `TC-STOR-005` |
| STOR-009 | Write는 임시 파일 전체 기록 후 동일 파일시스템 내 `rename()`으로 교체한다. | 코드 검토, `TC-STOR-003` |
| STOR-010 | 잘못된 포인터나 경로 입력으로 미정의 동작을 일으키지 않는다. | `TC-STOR-008` |

## 이후 Phase에 고정된 핵심 요구사항

| ID | 요구사항 | 구현 Phase |
|---|---|---|
| CLI-001 | `ssd W [LBA] [VALUE]`, `ssd R [LBA]`를 지원한다. | `TC-CLI-001` |
| CLI-002 | 정상 Write는 stdout과 `result.txt`를 변경하지 않는다. | `TC-CLI-001`, `TC-CLI-004` |
| CLI-003 | 정상 Read는 마지막 값을 `result.txt`에만 기록한다. | `TC-CLI-001`, `TC-CLI-002`, `TC-CLI-005` |
| CLI-004 | 잘못된 명령은 `INVALID COMMAND`와 종료 코드 2를 반환한다. | `TC-CLI-006`~`009` |
| CLI-005 | 파일 I/O 오류는 종료 코드 3, 손상 NAND는 종료 코드 4로 구분한다. | `TC-CLI-010`~`012` |
| CLI-006 | 실패한 Write/Read는 기존 NAND와 `result.txt`를 정상 결과처럼 변경하지 않는다. | `TC-CLI-010`~`012` |
| SHELL-001 | `write`, `read`, `fullwrite`, `fullread`, `help`, `exit`를 지원한다. | `TC-SH-001`~`004` |
| SHELL-002 | 지나치게 긴 입력과 잘못된 인자 후에도 다음 명령을 처리한다. | `TC-SH-004`, `008`~`010` |
| SHELL-003 | `--ssd-path`로 실행 대상을 선택하고 실행 실패를 감지한다. | `TC-SH-005`, `012` |
| PROC-001 | SSD는 `fork()`/`execv()`/`waitpid()`로 실행한다. | 코드 검토, `TC-SH-001`~`006` |
| ORACLE-001 | 기대값 모델은 SSD의 실제 NAND 데이터와 독립적이어야 한다. | TC-F001~005, TC-B001~002, TC-S001~002 |
| AUTO-001 | TestApp1/2와 케이스별 PASS/FAIL 집계를 제공한다. | TC-AUTO-001~004 |
| AUTO-002 | 각 테스트는 사용자 NAND와 분리된 초기 상태에서 실행한다. | TC-AUTO-005~006 |
| AUTO-003 | 비대화형 전체/개별 실행과 0/1/2 종료 코드를 제공한다. | TC-AUTO-001~003, 007 |
| REPORT-001 | 로그, 요약, CSV, Markdown 보고서를 실제 결과로 생성한다. | TC-AUTO-001, 008 |
| PERF-001 | monotonic clock으로 동일 조건을 반복 측정하고 최소·평균·중앙값·최대를 보고한다. | TC-PERF-001 |
| PERF-002 | 프로세스 실행 방식과 직접 저장 API 방식의 차이를 실제 수치로 비교한다. | 성능 보고서 |
| FAULT-001 | 의도적 LBA 99 및 Overwrite 결함이 지정 테스트에서 검출됨을 증명한다. | TC-M001~005 |
| REG-001 | 결함 검출 후 정상 SSD 전체 테스트가 다시 PASS해야 한다. | TC-M000, TC-M006 |
| QUAL-001 | 경고, ASan, UBSan, 회귀 테스트 결과를 남긴다. | Phase 8 |

## 설계상 비기능 요구사항

- 기준 플랫폼은 Linux/GCC/C11/POSIX다.
- `ssd.c`와 `testshell.c`는 최종적으로 서로 독립된 실행 파일이어야 한다.
- 실제 실행하지 않은 테스트나 측정하지 않은 수치는 보고하지 않는다.
- 손상된 사용자 데이터를 임의로 정상 데이터로 덮어쓰지 않는다.
- 정상 빌드와 결함 주입 빌드는 명확히 분리한다.
