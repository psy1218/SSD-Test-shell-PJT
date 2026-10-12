# Defect Report

## FI-001 — LBA 99 Write Drop

- 구분: 의도적 Fault Injection
- 빌드: `ssd_fault_boundary`
- 증상: LBA 99 Write가 종료 코드 0을 반환하지만 값이 저장되지 않음
- 발견 테스트: TC-B002, TC-S001 및 관련 전체 패턴 테스트
- 재현 명령: `./testshell --ssd-path ./ssd_fault_boundary --run-test TC-B002`
- 기대값: `0xFFFFFFFF`
- 실제값: `0x00000000`
- 판정: DATA_MISMATCH, testshell 종료 코드 1
- 원인: `SSD_FAULT_LBA99` 빌드에서 LBA 99 저장을 의도적으로 생략
- 정상 빌드 수정 여부: 정상 `ssd`에는 해당 매크로가 없어 코드 경로 비활성
- 회귀: 정상 SSD 9/9 PASS

## FI-002 — Overwrite Ignored

- 구분: 의도적 Fault Injection
- 빌드: `ssd_fault_overwrite`
- 증상: 이미 값이 있는 LBA의 새 Write가 종료 코드 0을 반환하지만 반영되지 않음
- 발견 테스트: TC-F003, TC-S001, TC-S002
- 재현 명령: `./testshell --ssd-path ./ssd_fault_overwrite --run-test TC-F003`
- 기대값: `0x12345678`
- 실제값: `0xAAAABBBB`
- 판정: DATA_MISMATCH, testshell 종료 코드 1
- 원인: `SSD_FAULT_OVERWRITE` 빌드에서 기존 값이 0이 아니면 저장을 생략
- 정상 빌드 수정 여부: 정상 `ssd`에는 해당 매크로가 없어 코드 경로 비활성
- 회귀: 정상 SSD 9/9 PASS

## Mutation Summary

- 주입 결함: 2
- 검출 결함: 2
- Mutation Score: 100%
- 실행일: 2026-10-12
