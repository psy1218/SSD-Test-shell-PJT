# SSD 인터페이스 명세

공개 선언은 `ssd.h`, 구현은 `ssd.c`에 둔다.

## 바이트 변환

### `ssd_encode_u32_le`

- 목적: 호스트 CPU 엔디안과 무관한 NAND 바이트 포맷 생성
- 입력: `uint32_t value`, 4바이트 출력 배열
- 반환: 없음
- 예외: 출력 포인터가 NULL이면 아무 작업도 하지 않음

### `ssd_decode_u32_le`

- 목적: Little Endian 4바이트를 32비트 값으로 복원
- 입력: 4바이트 배열
- 반환: 복원 값. 입력 포인터가 NULL이면 0

## 저장 API

### `ssd_storage_load`

- 목적: 전체 NAND 이미지 로드
- 입력: 파일 경로, 100개 값 출력 배열
- 동작: 파일이 없으면 0으로 초기화해 생성하고, 있으면 크기 검증 후 디코딩
- 실패 안전성: 출력 배열은 성공했을 때만 호출자에게 복사

### `ssd_storage_save_atomic`

- 목적: 전체 NAND 이미지 안전 갱신
- 입력: 파일 경로, 100개 값 배열
- 동작: 같은 디렉터리의 임시 파일에 400바이트를 모두 기록하고 `fflush`,
  `fsync`, `fclose` 성공 후 `rename`으로 교체
- 실패 안전성: 교체 전 실패하면 원본을 유지하고 임시 파일 제거를 시도

### `ssd_storage_read_lba`

- 목적: LBA 하나를 읽음
- 입력: 파일 경로, 0~99 LBA, 결과 포인터
- 반환: `SsdStatus`

### `ssd_storage_write_lba`

- 목적: 전체 이미지를 로드해 한 LBA를 변경한 뒤 원자적으로 저장
- 입력: 파일 경로, 0~99 LBA, 32비트 값
- 반환: `SsdStatus`

## 상태 코드

| 값 | 의미 |
|---|---|
| `SSD_STATUS_OK` | 성공 |
| `SSD_STATUS_INVALID_ARGUMENT` | NULL, 빈 경로, 범위 밖 LBA |
| `SSD_STATUS_IO_ERROR` | 운영체제 또는 파일 I/O 오류 |
| `SSD_STATUS_CORRUPT_NAND` | NAND 크기/내용 계약 위반 |

## Phase 2 명령행 인터페이스

### Write

```text
ssd W [LBA] [VALUE]
```

- LBA: 숫자만 포함한 0~99의 10진수. 선행 0은 허용한다.
- VALUE: 정확히 `0x` 뒤에 `0`~`9`, `A`~`F` 8자리
- 성공: stdout 없음, `nand.txt` 갱신, `result.txt` 불변, 종료 코드 0

### Read

```text
ssd R [LBA]
```

- 성공: stdout 없음, NAND 불변
- `result.txt`: `0x` + 대문자 8자리 16진수 + 개행 한 줄로 교체
- 미기록 LBA: `0x00000000`

### 오류 출력과 종료 코드

모든 오류는 stdout에 `INVALID COMMAND` 한 줄을 출력한다. 상세 원인은
테스트 판정 계약과 분리해 stderr에 기록한다.

| 종료 코드 | 의미 |
|---:|---|
| 0 | 성공 |
| 2 | 명령 또는 인자 오류 |
| 3 | 파일 I/O 오류 |
| 4 | NAND 파일 손상 |
| 5 | 기타 내부 오류 |

Read 결과는 임시 파일 기록, `fflush()`, `fsync()`, `fclose()`가 모두
성공한 뒤 `rename()`으로 교체한다. 실패 시 기존 `result.txt`를 보존한다.
