# Phase 6 검증 보고서 — Fault Injection

## 실행 명령

```sh
make clean
make test
make fault-test
```

## 결과

### 정상 기준선

- 자동 Oracle 시나리오: 9/9 PASS
- 전체 회귀 프레임워크: 41/41 PASS

### Fault 검출

| 테스트 | 결과 |
|---|---:|
| TC-M000 정상 기준선 | PASS |
| TC-M001 LBA 99 지정 검출 | PASS |
| TC-M002 LBA 99 전체 suite 검출 | PASS |
| TC-M003 Overwrite 지정 검출 | PASS |
| TC-M004 TestApp2 Overwrite 검출 | PASS |
| TC-M005 Overwrite 전체 suite 검출 | PASS |
| TC-M006 정상 SSD 회귀 | PASS |

Fault 검증 스크립트는 7/7 PASS다. 여기서 PASS는 고의 결함 바이너리가
성공했다는 뜻이 아니라, 지정된 자동 테스트가 예상대로 FAIL을 검출했다는 뜻이다.

### 실제 불일치

```text
TC-B002: expected=0xFFFFFFFF actual=0x00000000 exit=1
TC-F003: expected=0x12345678 actual=0xAAAABBBB exit=1
```

- 주입 결함: 2
- 검출 결함: 2
- Mutation Score: 100%
- 결함 검출 후 정상 SSD 전체 자동 테스트: 9/9 PASS

Phase 6 완료 조건을 충족했다.
