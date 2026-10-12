# Phase 3 검증 보고서

## 실행 정보

- 실행일: 2026-10-12
- 환경: Ubuntu 24.04.4 LTS on WSL2
- 컴파일러: GCC 13.3.0
- 빌드 도구: GNU Make 4.3
- 컴파일 옵션: `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow`

## 구현 내용

- `testshell.c` 독립 실행 파일
- `fork()`/`execv()`/`waitpid()` SSD 프로세스 어댑터
- `write`, `read`, `fullwrite`, `fullread`, `help`, `exit`
- `--ssd-path` 실행 대상 선택
- SSD 종료 코드, exec 실패, 시그널 종료 처리
- 제한된 `fgets()` 입력과 초과 입력 소진
- 터미널 입력에만 프롬프트 표시
- SSD 결과 파일 형식 재검증

## 실행 명령

```sh
make clean
make test
make test-sanitize
gcc -I. -std=c11 -Wall -Wextra -Wpedantic -Wconversion \
  -Wshadow -Werror -fsyntax-only testshell.c
```

## 테스트 결과

### 기존 회귀 테스트

- 저장 계층: 8/8 PASS
- SSD CLI: 13/13 PASS

### Test Shell 통합 테스트

| ID | 검증 대상 | 결과 |
|---|---|---:|
| TC-SH-001 | write/read와 결과 출력 | PASS |
| TC-SH-002 | fullwrite/fullread 100 LBA | PASS |
| TC-SH-003 | help 출력 | PASS |
| TC-SH-004 | 잘못된 명령 후 복구 | PASS |
| TC-SH-005 | 존재하지 않는 SSD 실행 실패 | PASS |
| TC-SH-006 | Shell 재실행 후 영속성 | PASS |
| TC-SH-007 | EOF 정상 종료 | PASS |
| TC-SH-008 | 300바이트 초과 입력 복구 | PASS |
| TC-SH-009 | 255바이트 입력 버퍼 경계 | PASS |
| TC-SH-010 | 초과 인자 거부 | PASS |
| TC-SH-011 | NAND I/O 실패 전달 | PASS |
| TC-SH-012 | 잘못된 실행 옵션 | PASS |

- 일반 빌드 전체: 33/33 PASS
- ASan/UBSan 전체: 33/33 PASS
- `-Werror` 검증: PASS
- Sanitizer 보고: 없음

## 발견한 문제와 수정

일반 테스트 32/32 PASS 후 코드 경계 검토에서 입력 버퍼가 정확히 255자로
채워지고 다음 문자가 개행일 때, 초과 입력 소진 루프가 다음 명령의 첫 문자를
읽을 가능성을 발견했다. 소진 루프가 이미 읽은 문자를 먼저 판정하도록 수정하고
`TC-SH-009`를 추가했다. 수정 후 전체 33개 일반 및 Sanitizer 테스트가 통과했다.

## Phase 3 완료 조건

| 조건 | 결과 |
|---|---:|
| 기본 여섯 명령 실행 | 충족 |
| SSD와 Test Shell 독립성 | 충족 |
| 프로세스 종료 상태 감지 | 충족 |
| 잘못된 사용자 입력 후 복구 | 충족 |
| SSD 경로 선택 | 충족 |
| 기존 기능 회귀 없음 | 충족 |

Phase 4 독립 Test Oracle 구현을 진행할 수 있다.
