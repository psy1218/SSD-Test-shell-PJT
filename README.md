# Virtual SSD & Automated Validation Framework

C11과 POSIX 환경에서 동작하는 가상 SSD 및 자동 검증 프레임워크입니다.
단순한 Read/Write 구현을 넘어 독립 Test Oracle, 결함 주입, 회귀 테스트,
성능 측정으로 확장하는 것을 목표로 합니다.

## 현재 진행 상태

- Phase 0: 요구사항, 아키텍처, 인터페이스, NAND 포맷 확정
- Phase 1: 400바이트 NAND 저장 계층 구현 및 단위 테스트
- Phase 2: SSD W/R CLI, 입력 검증, 결과 파일, 종료 코드 구현
- Phase 3 이후: 아직 구현하지 않음

현재 `ssd` 실행 파일까지 제공하며 Test Shell은 Phase 3에서 추가합니다.

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

## 문서

- `docs/01_requirements.md`: 요구사항 및 테스트 추적성
- `docs/02_architecture.md`: 시스템 구성과 책임 분리
- `docs/03_interface_spec.md`: Phase 1 C API 계약
- `docs/04_storage_format.md`: NAND 바이너리 포맷
- `reports/phase1_validation.md`: 실제 빌드 및 테스트 증적
- `reports/phase2_validation.md`: SSD CLI 및 회귀 테스트 증적

## 저장 형식 주의

최종 실행 시 사용할 파일명은 `nand.txt`이지만 내용은 텍스트가 아니라
헤더 없는 400바이트 Little Endian 바이너리입니다.
