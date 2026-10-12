# Phase 7 검증 보고서 — Performance Measurement

## 구현

- `tools/benchmark.c` 전용 측정 프로그램
- `CLOCK_MONOTONIC` 기반 시간 측정
- 1~20회 반복 입력 검증
- 최소·평균·중앙값·최대 집계
- 임시 작업 디렉터리 격리 및 정리
- CSV와 Markdown 성능 보고서 생성
- 프로세스 방식과 직접 저장 API 비교

## 검증

`TC-PERF-001`은 1회 측정으로 7개 metric과 CSV/Markdown 형식을 검사해 PASS했다.
이를 포함한 일반 회귀 결과는 42/42 PASS다.

## 실제 5회 측정

```sh
make benchmark
```

| Metric | Average (ms) |
|---|---:|
| process_write_100 | 625.328 |
| process_read_100 | 614.452 |
| direct_write_100 | 408.613 |
| direct_read_100 | 0.221 |
| testapp1 | 2879.205 |
| testapp2 | 379.814 |
| all_automated_tests | 5116.605 |

원시 최소·평균·중앙값·최대 데이터는 `performance_report.csv`에 저장했다.
측정 환경과 해석 한계는 `docs/08_performance.md`에 기록했다.

Phase 7 완료 조건인 반복 측정, 재현 가능한 실행 방법, 실제 수치 기반 분석을
충족했다.
