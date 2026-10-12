# Virtual SSD & Automated Validation Framework

C11과 POSIX 환경에서 동작하는 가상 SSD 및 자동 검증 프레임워크입니다.
단순한 Read/Write 구현을 넘어 독립 Test Oracle, 결함 주입, 회귀 테스트,
성능 측정으로 확장하는 것을 목표로 합니다.

## 현재 진행 상태

- Phase 0: 요구사항, 아키텍처, 인터페이스, NAND 포맷 확정
- Phase 1: 400바이트 NAND 저장 계층 구현 및 단위 테스트
- Phase 2: SSD W/R CLI, 입력 검증, 결과 파일, 종료 코드 구현
- Phase 3: 독립 프로세스 기반 Test Shell과 기본 명령 구현
- Phase 4: 독립 Reference Test Oracle 구현
- Phase 5: TestApp1/2, 테스트 러너, 격리, 자동 보고서 구현
- Phase 6: Fault Injection, Mutation 검출, 정상 회귀 구현
- Phase 7: 반복 성능 측정과 프로세스/직접 호출 비교 구현
- Phase 8 이후: 구현 진행 예정

현재 `ssd`와 `testshell` 두 독립 실행 파일을 제공합니다.

## 기준 환경

- Linux
- GCC
- C11
- POSIX 파일 API

## 빌드와 검증

```sh
make
make test
make test-sanitize
```

`make test`는 임시 디렉터리에서 NAND 생성, 초기값, 경계 LBA, 영속성,
Little Endian 원시 바이트, 손상 파일 검출과 SSD CLI 계약을 검증합니다.
테스트 데이터는 프로젝트 디렉터리에 남지 않습니다.

## SSD CLI

```sh
./ssd W 3 0x12345678
./ssd R 3
cat result.txt
```

- `W`: `nand.txt`의 지정 LBA를 갱신하며 정상 성공 시 stdout 출력이 없습니다.
- `R`: 값을 `0x`와 대문자 8자리 16진수로 `result.txt`에 덮어씁니다.
- LBA는 0~99의 10진수만 허용합니다.
- VALUE는 정확히 `0x`와 대문자 8자리 16진수만 허용합니다.
- 실패 시 stdout에는 `INVALID COMMAND`를 출력하고 원인은 stderr에 기록합니다.

종료 코드는 정상 0, 명령/인자 오류 2, 파일 I/O 오류 3, 손상 NAND 4,
기타 내부 오류 5입니다.

## Test Shell

```sh
./testshell
```

지원 명령:

```text
write <LBA> <0xVALUE>
read <LBA>
fullwrite <0xVALUE>
fullread
help
exit
```

Test Shell은 SSD 코드를 직접 호출하지 않고 매 명령마다
`fork()` → `execv()` → `waitpid()`로 `ssd` 프로세스를 실행합니다.
다른 SSD 실행 파일을 사용할 때는 다음과 같이 지정합니다.

```sh
./testshell --ssd-path ./ssd
```

## 자동 검증

```sh
./testshell --run-tests
./testshell --run-test TC-F003
```

대화형 Shell에서는 `testapp1`, `testapp2`, `testall`, `testcase <ID>`,
`testsummary`를 사용할 수 있습니다. 각 테스트는 `/tmp` 아래의 독립 디렉터리와
0으로 초기화된 Reference Model을 사용하므로 사용자 `nand.txt`를 변경하지
않습니다.

실행 결과는 다음 위치에 생성됩니다.

- `logs/test_result.log`
- `logs/test_summary.txt`
- `reports/test_report.csv`
- `reports/test_report.md`

## Fault Injection

```sh
make fault-test
```

이 타깃은 정상 SSD와 별도로 `ssd_fault_boundary`,
`ssd_fault_overwrite`를 빌드합니다. 테스트가 지정 결함을 FAIL로 검출하고,
마지막에 정상 SSD 전체 회귀가 다시 PASS하는지 확인합니다. 결함 바이너리는
배포용 `ssd`에 포함되지 않습니다.

## 성능 측정

```sh
make benchmark
```

`CLOCK_MONOTONIC`으로 각 항목을 5회 측정해 최소·평균·중앙값·최대를
`reports/performance_report.csv`와 `reports/performance_report.md`에
기록합니다. 이 값은 파일 기반 시뮬레이터의 프로세스·파일 I/O 비용이며
실제 SSD의 IOPS나 지연시간이 아닙니다.

## 문서

- `docs/01_requirements.md`: 요구사항 및 테스트 추적성
- `docs/02_architecture.md`: 시스템 구성과 책임 분리
- `docs/03_interface_spec.md`: Phase 1 C API 계약
- `docs/04_storage_format.md`: NAND 바이너리 포맷
- `docs/05_test_plan.md`: 자동 테스트 전략과 실행 계약
- `docs/06_test_matrix.md`: 요구사항과 테스트 매핑
- `docs/07_fault_injection.md`: 결함 주입 및 판정 절차
- `docs/08_performance.md`: 성능 측정 방법과 해석
- `reports/phase1_validation.md`: 실제 빌드 및 테스트 증적
- `reports/phase2_validation.md`: SSD CLI 및 회귀 테스트 증적
- `reports/phase3_validation.md`: Test Shell 및 프로세스 실행 증적
- `reports/phase4_validation.md`: 독립 Oracle 검증 증적
- `reports/phase5_validation.md`: 자동화·격리·보고서 검증 증적
- `reports/phase6_validation.md`: 결함 검출과 회귀 테스트 증적
- `reports/phase7_validation.md`: 실제 반복 성능 측정 증적

## 저장 형식 주의

최종 실행 시 사용할 파일명은 `nand.txt`이지만 내용은 텍스트가 아니라
헤더 없는 400바이트 Little Endian 바이너리입니다.
