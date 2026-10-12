# Phase 4 검증 보고서 — Independent Test Oracle

## 구현 결과

- 100개 `uint32_t` Reference Model
- 테스트 시작 시 독립적인 0 초기 상태
- 성공한 Write만 기대값에 반영
- Read 실제값의 엄격한 파일 형식 검사와 독립 비교
- 데이터 불일치와 실행 환경 오류 분리

Oracle은 `nand.txt`를 읽지 않으며 `ssd.c`의 encode/decode 또는 저장 API를
링크하거나 호출하지 않는다. 테스트 시나리오에 입력된 값만으로 기대값을 만든다.

## 검증 결과

정상 SSD에 대해 Oracle 기반 시나리오 9개를 실행했다.

- PASS: 9
- FAIL: 0
- ERROR: 0
- Pass Rate: 100.00%
- 실행 시간: 2396.010ms

검증된 비교에는 미기록 LBA, Overwrite, 서로 다른 주소, 전체 100 LBA,
LBA 0/99, 동일 패턴, 주소별 고유 패턴이 포함된다.

## 독립성 보강

동일 구현 결함을 공유할 위험을 줄이기 위해 다음을 함께 사용한다.

- 고정 상수 기대값
- `0xA5000000 | lba` 주소 패턴
- 경계 LBA 0과 99
- 프로세스 재실행을 통한 Read
- Phase 6의 고의 결함 검출 예정

Phase 4 완료 조건을 충족했다.
