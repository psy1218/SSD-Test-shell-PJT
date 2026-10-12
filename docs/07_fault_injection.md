# Fault Injection 설계

## 목적

정상 구현이 PASS한다는 사실만으로 테스트의 결함 검출 능력을 보장할 수 없다.
정상 배포 바이너리와 분리된 의도적 결함 바이너리를 만들고 동일한 독립
Oracle 테스트가 실제 FAIL을 발생시키는지 검증한다.

## 빌드 분리

| 바이너리 | 컴파일 정의 | 동작 |
|---|---|---|
| `ssd` | 없음 | 정상 배포 대상 |
| `ssd_fault_boundary` | `SSD_FAULT_LBA99` | LBA 99 Write 성공을 보고하지만 저장하지 않음 |
| `ssd_fault_overwrite` | `SSD_FAULT_OVERWRITE` | 값이 있는 LBA의 후속 Write를 저장하지 않음 |

Fault 코드는 컴파일 타임 조건부이며 정상 빌드에서는 활성화되지 않는다.

## 실행 절차

```sh
make fault-test
```

1. 정상 SSD 전체 9개 자동 테스트 PASS 확인
2. LBA 99 fault에서 TC-B002 FAIL 및 DATA_MISMATCH 확인
3. 같은 fault의 전체 suite 관련 실패 확인
4. Overwrite fault에서 TC-F003, TC-S002 FAIL 확인
5. 같은 fault의 전체 suite 관련 실패 확인
6. 정상 SSD 전체 9개 테스트 재실행 및 PASS 확인

Fault 테스트는 “프로세스가 실패했는가”가 아니라 지정 테스트가 기대값과
실제값의 차이를 발견했는지를 검사한다.

## Mutation Score

```text
detected mutations / injected mutations * 100
= 2 / 2 * 100
= 100%
```

이는 두 개의 정의된 교육용 mutation에 대한 결과이며 모든 가능한 결함을
검출한다는 의미가 아니다.
