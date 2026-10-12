# Virtual SSD & Automated Validation Framework

C11과 POSIX 환경에서 동작하는 가상 SSD 및 자동 검증 프레임워크입니다.
단순한 Read/Write 구현을 넘어 독립 Test Oracle, 결함 주입, 회귀 테스트,
성능 측정으로 확장하는 것을 목표로 합니다.

## 현재 진행 상태

- Phase 0: 요구사항, 아키텍처, 인터페이스, NAND 포맷 확정
- Phase 1: 400바이트 NAND 저장 계층 구현 및 단위 테스트
- Phase 2 이후: 아직 구현하지 않음

현재 단계는 SSD 명령행 인터페이스를 제공하지 않습니다. `ssd` 실행 파일과
Test Shell은 각각 Phase 2와 Phase 3에서 추가합니다.

## 기준 환경

- Linux
- GCC
- C11
- POSIX 파일 API

## Phase 1 빌드와 검증

```sh
make
make test
make test-sanitize
```

`make test`는 임시 디렉터리에서 NAND 생성, 초기값, 경계 LBA, 영속성,
Little Endian 원시 바이트, 손상 파일 검출을 검증합니다. 테스트 데이터는
프로젝트 디렉터리에 남지 않습니다.

## 문서

- `docs/01_requirements.md`: 요구사항 및 테스트 추적성
- `docs/02_architecture.md`: 시스템 구성과 책임 분리
- `docs/03_interface_spec.md`: Phase 1 C API 계약
- `docs/04_storage_format.md`: NAND 바이너리 포맷
- `reports/phase1_validation.md`: 실제 빌드 및 테스트 증적

## 저장 형식 주의

최종 실행 시 사용할 파일명은 `nand.txt`이지만 내용은 텍스트가 아니라
헤더 없는 400바이트 Little Endian 바이너리입니다.
