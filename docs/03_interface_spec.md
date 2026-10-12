# Phase 1 C 인터페이스 명세

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
