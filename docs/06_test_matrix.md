# 요구사항-테스트 매트릭스

## 자동 Oracle 시나리오

| Test ID | Requirement | 목적 | 정상 SSD 기대 결과 |
|---|---|---|---:|
| TC-F001 | CLI-001 | Write 후 Read | PASS |
| TC-F002 | STOR-005 | 미기록 LBA 0 반환 | PASS |
| TC-F003 | CLI-006 | 동일 LBA Overwrite | PASS |
| TC-F004 | STOR-001 | 서로 다른 LBA 값 분리 | PASS |
| TC-F005 | SHELL-001 | 전체 100 LBA 패턴 | PASS |
| TC-B001 | STOR-008 | LBA 0 | PASS |
| TC-B002 | STOR-008 | LBA 99 | PASS |
| TC-S001 | AUTO-001 | TestApp1 전체/주소 패턴 | PASS |
| TC-S002 | AUTO-001 | TestApp2 반복 Overwrite | PASS |

## 프레임워크 검증

| Test ID | Requirement | 목적 |
|---|---|---|
| TC-AUTO-001 | AUTO-001 | 전체 테스트 실행과 집계 |
| TC-AUTO-002 | AUTO-003 | 특정 ID 실행 |
| TC-AUTO-003 | AUTO-003 | 알 수 없는 ID 오류 |
| TC-AUTO-004 | AUTO-001 | 대화형 TestApp/runner 명령 |
| TC-AUTO-005 | AUTO-002 | 사용자 NAND 불변 |
| TC-AUTO-006 | AUTO-002 | 동일 테스트 재실행 |
| TC-AUTO-007 | AUTO-003 | 환경 ERROR 종료 코드 2 |
| TC-AUTO-008 | REPORT-001 | 로그와 보고서 필드 |

Phase 6에서는 Fault 빌드별로 어떤 Test ID가 실제 FAIL을 발생시키는지 실행
결과를 추가한다. 예상 검출과 실제 검출을 구분한다.
