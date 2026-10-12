# Phase 8 검증 보고서 — Quality and CI

## 실행 명령

```sh
sh scripts/run_all_tests.sh
```

## 로컬 품질 게이트 결과

| 구분 | 결과 |
|---|---:|
| 저장 계층 테스트 | 8/8 PASS |
| SSD CLI 테스트 | 13/13 PASS |
| Test Shell 테스트 | 12/12 PASS |
| 자동화 프레임워크 테스트 | 8/8 PASS |
| 벤치마크 통합 테스트 | 1/1 PASS |
| 일반 테스트 합계 | 42/42 PASS |
| ASan/UBSan 테스트 합계 | 42/42 PASS |
| Fault 검출 및 회귀 | 7/7 PASS |
| Mutation Score | 2/2, 100% |
| GCC `-fanalyzer -Werror` | 3/3 대상 PASS |
| 최종 정상 SSD Oracle | 9/9 PASS |

AddressSanitizer와 UndefinedBehaviorSanitizer는 저장, SSD, Test Shell,
자동화, 벤치마크 테스트에서 오류를 보고하지 않았다.

GCC 정적 분석 대상:

- `ssd.c`
- `testshell.c`
- `tools/benchmark.c`

## CI 구성

`.github/workflows/ci.yml`은 `master`/`main` Push 및 Pull Request에서 로컬과
같은 `scripts/run_all_tests.sh`를 실행한다. 마지막에 로그, 요약, CSV,
Markdown을 `validation-evidence` artifact로 업로드한다.

워크플로 실제 실행 결과는 커밋 push 후 GitHub Actions에서 별도로 확인한다.

## Phase 8 완료 판정

- 모든 CORE 기능 회귀 PASS
- 경계·예외·파일 I/O 테스트 PASS
- 메모리 및 UB 오류 미검출
- 정적 분석 경고 없음
- Fault 검출 후 정상 회귀 PASS
- CI 정의 및 로컬 동일 명령 검증 완료

Phase 8 완료 조건을 충족했다.
