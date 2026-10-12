# 시스템 아키텍처

## 최종 구성

```text
User / Test Runner
        |
        v
  testshell process
  - command parser
  - independent oracle
  - PASS/FAIL judge
        |
        | fork + execv + waitpid
        v
      ssd process
  - CLI validation
  - storage API
        |
        +--> nand.txt   (400-byte binary persistence)
        +--> result.txt (last successful read only)
```

SSD는 테스트 대상(System Under Test)이고 Test Shell은 입력, 실행, 관찰,
판정을 담당한다. 두 프로그램을 분리하면 Shell이 SSD 내부 상태를 정답으로
사용하는 순환 검증을 피하고 프로세스 종료 코드와 파일 계약까지 검증할 수 있다.

## Phase 1 내부 책임

현재 `ssd.c`는 다음 저장 책임만 가진다.

1. 32비트 값과 Little Endian 바이트 사이의 명시적 변환
2. NAND 파일 부재 시 0으로 초기화된 정확한 크기의 파일 생성
3. 기존 NAND의 크기와 읽기 완전성 검증
4. 전체 100개 값 로드
5. 한 LBA 읽기와 쓰기
6. 임시 파일과 원자적 이름 변경을 이용한 전체 이미지 갱신

명령행 파싱과 `result.txt` 처리는 Phase 2 책임이므로 Phase 1 저장 API에
포함하지 않는다.

## 오류 정책

저장 API는 화면 문자열을 출력하지 않고 상태 코드를 호출자에게 반환한다.
향후 CLI 계층이 이를 종료 코드와 진단 메시지로 변환한다.

- 잘못된 인자: `SSD_STATUS_INVALID_ARGUMENT`
- 파일 열기/읽기/쓰기/동기화/교체 실패: `SSD_STATUS_IO_ERROR`
- NAND 크기 불일치 또는 불완전한 데이터: `SSD_STATUS_CORRUPT_NAND`

손상 파일은 자동으로 지우거나 다시 만들지 않는다. 증거와 기존 데이터를
보존한 상태로 상위 계층에 오류를 보고한다.
