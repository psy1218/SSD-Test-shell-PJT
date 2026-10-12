# Phase 5 검증 보고서 — Automated Test Framework

## 구현 결과

- TestApp1 및 TestApp2
- 9개 Oracle 기반 자동 시나리오
- `testall`, `testcase`, `testsummary`, `testapp1`, `testapp2`
- `--run-tests`, `--run-test`, `--ssd-path`
- 케이스별 임시 디렉터리와 사용자 NAND 격리
- PASS/FAIL/ERROR 및 비대화형 종료 코드
- 로그, 요약, CSV, Markdown 자동 생성

## 프레임워크 테스트

자동화 기능 자체를 검증하는 통합 테스트 8개가 모두 통과했다.

- 전체 실행 및 결과 파일: PASS
- 개별 ID 실행: PASS
- 잘못된 ID: PASS
- 대화형 자동 테스트 명령: PASS
- 사용자 NAND 격리: PASS
- 반복 실행 재현성: PASS
- 실행 환경 ERROR 코드: PASS
- 보고서 필드: PASS

## 전체 회귀 및 Sanitizer

- 저장 계층: 8/8 PASS
- SSD CLI: 13/13 PASS
- Test Shell: 12/12 PASS
- 자동화 프레임워크: 8/8 PASS
- 일반 회귀: 41/41 PASS
- ASan/UBSan 회귀: 41/41 PASS
- 컴파일 경고 및 `-Werror`: PASS

## 실제 자동 테스트 실행

2026-10-12에 정상 SSD를 대상으로 실행한 결과:

```text
SUMMARY total=9 pass=9 fail=0 error=0 duration_ms=2396.010 pass_rate=100.00%
```

수치는 WSL2의 파일 기반 시뮬레이터 실행 결과이며 실제 SSD 성능을 의미하지
않는다. Phase 5 완료 조건을 충족했으며 Fault Injection을 진행할 수 있다.
